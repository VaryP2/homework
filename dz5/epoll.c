#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 128
#define MAX_EVENTS 64
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

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_client(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
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

    if (set_nonblocking(listener) < 0) {
        perror("fcntl(listener)");
        close(listener);
        return EXIT_FAILURE;
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(listener);
        return EXIT_FAILURE;
    }

    struct epoll_event listen_event;
    memset(&listen_event, 0, sizeof(listen_event));
    listen_event.events = EPOLLIN;
    listen_event.data.fd = listener;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &listen_event) < 0) {
        perror("epoll_ctl(listener)");
        close(epfd);
        close(listener);
        return EXIT_FAILURE;
    }

    printf("epoll server: port=%ld\n", port);

    struct epoll_event events[MAX_EVENTS];

    for (;;) {
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (nready < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;

            if (fd == listener) {
                for (;;) {
                    int client = accept(listener, NULL, NULL);

                    if (client < 0) {
                        if (errno == EINTR)
                            continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept");
                        break;
                    }

                    if (set_nonblocking(client) < 0) {
                        perror("fcntl(client)");
                        close(client);
                        continue;
                    }

                    struct epoll_event client_event;
                    memset(&client_event, 0, sizeof(client_event));
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &client_event) < 0) {
                        perror("epoll_ctl(client)");
                        close(client);
                        continue;
                    }

                    printf("client connected: fd=%d\n", client);
                }

                continue;
            }

            if (flags & (EPOLLERR | EPOLLHUP)) {
                close_client(epfd, fd);
                printf("client closed: fd=%d\n", fd);
                continue;
            }

            if (flags & EPOLLIN) {
                char buf[BUF_SIZE];
                ssize_t n = recv(fd, buf, sizeof(buf), 0);

                if (n == 0) {
                    close_client(epfd, fd);
                    printf("client disconnected: fd=%d\n", fd);
                    continue;
                }

                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;

                    perror("recv");
                    close_client(epfd, fd);
                    continue;
                }

                ssize_t sent = 0;
                while (sent < n) {
                    ssize_t w = send(fd, buf + sent, (size_t)(n - sent), 0);

                    if (w > 0) {
                        sent += w;
                    } else if (w < 0 && errno == EINTR) {
                        continue;
                    } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        fprintf(stderr, "send would block: fd=%d\n", fd);
                        break;
                    } else {
                        perror("send");
                        close_client(epfd, fd);
                        break;
                    }
                }
            }
        }
    }

    close(epfd);
    close(listener);
    return EXIT_SUCCESS;
}
