#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 16
#define MAX_CLIENTS 5
#define BUF_SIZE 4096

static int make_listener(unsigned short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

static void remove_client(fd_set *master, int *clients, int *count, int index)
{
    int fd = clients[index];

    close(fd);
    FD_CLR(fd, master);

    clients[index] = clients[*count - 1];
    (*count)--;

    printf("client fd=%d disconnected; clients=%d\n", fd, *count);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    long port = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    int listener = make_listener((unsigned short)port);
    int clients[MAX_CLIENTS];
    int client_count = 0;
    int maxfd = listener;

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listener, &master);

    printf("select server: port=%ld, max clients=%d\n", port, MAX_CLIENTS);

    for (;;) {
        fd_set readfds = master;

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(listener, &readfds)) {
            int client = accept(listener, NULL, NULL);

            if (client < 0) {
                if (errno != EINTR)
                    perror("accept");
            } else if (client_count >= MAX_CLIENTS) {
                const char *msg = "server: maximum 5 clients reached\n";
                (void)send(client, msg, strlen(msg), 0);
                close(client);
                printf("connection rejected: limit reached\n");
            } else {
                clients[client_count++] = client;
                FD_SET(client, &master);
                if (client > maxfd)
                    maxfd = client;

                printf("client connected: fd=%d; clients=%d\n",
                       client, client_count);
            }
        }

        for (int i = 0; i < client_count; ) {
            int fd = clients[i];

            if (!FD_ISSET(fd, &readfds)) {
                ++i;
                continue;
            }

            char buf[BUF_SIZE];
            ssize_t n = recv(fd, buf, sizeof(buf), 0);

            if (n == 0) {
                remove_client(&master, clients, &client_count, i);
                continue;
            }

            if (n < 0) {
                if (errno == EINTR) {
                    ++i;
                    continue;
                }

                perror("recv");
                remove_client(&master, clients, &client_count, i);
                continue;
            }

            ssize_t sent = 0;
            while (sent < n) {
                ssize_t w = send(fd, buf + sent, (size_t)(n - sent), 0);
                if (w > 0) {
                    sent += w;
                } else if (w < 0 && errno == EINTR) {
                    continue;
                } else {
                    perror("send");
                    remove_client(&master, clients, &client_count, i);
                    goto next_client;
                }
            }

            ++i;
        next_client:
            ;
        }
    }

    for (int i = 0; i < client_count; ++i)
        close(clients[i]);
    close(listener);
    return EXIT_SUCCESS;
}
