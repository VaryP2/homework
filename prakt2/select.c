#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 128
#define BUF_SIZE 4096

static int make_listener(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    long p = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || p < 1 || p > 65535) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    int listener = make_listener((uint16_t)p);
    printf("echo server (select) listening on port %ld\n", p);

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listener, &master);
    int maxfd = listener;

    for (;;) {
        fd_set readfds = master;  

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int fd = 0; fd <= maxfd && ready > 0; ++fd) {
            if (!FD_ISSET(fd, &readfds))
                continue;

            --ready;

            if (fd == listener) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) {
                    if (errno == EINTR) continue;
                    perror("accept");
                    continue;
                }

                FD_SET(client, &master);
                if (client > maxfd)
                    maxfd = client;

                printf("client connected: fd=%d\n", client);
            } else {
                char buf[BUF_SIZE];
                ssize_t n = recv(fd, buf, sizeof(buf), 0);

                if (n == 0) {
                    printf("client disconnected: fd=%d\n", fd);
                    close(fd);
                    FD_CLR(fd, &master);
                } else if (n < 0) {
                    if (errno != EINTR) {
                        perror("recv");
                        close(fd);
                        FD_CLR(fd, &master);
                    }
                } else {
                    ssize_t sent = 0;
                    while (sent < n) {
                        ssize_t w = send(fd, buf + sent, (size_t)(n - sent), 0);
                        if (w > 0) {
                            sent += w;
                        } else if (w < 0 && errno == EINTR) {
                            continue;
                        } else {
                            perror("send");
                            close(fd);
                            FD_CLR(fd, &master);
                            break;
                        }
                    }
                }
            }
        }
    }

    close(listener);
    return EXIT_SUCCESS;
}
