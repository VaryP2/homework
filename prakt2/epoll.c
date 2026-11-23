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
#define BUF_SIZE 4096
#define MAX_EVENTS 128

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

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_fd(int epfd, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
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

    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev) < 0) {
        perror("epoll_ctl(ADD listener)");
        close(epfd);
        close(listener);
        return EXIT_FAILURE;
    }

    printf("echo server (epoll ET) listening on port %ld\n", p);

    struct epoll_event events[MAX_EVENTS];

    for (;;) {
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;

            if (flags & (EPOLLERR | EPOLLHUP)) {
                close_fd(epfd, fd);
                continue;
            }

            if (fd == listener) {
                for (;;) {
                    int client = accept(listener, NULL, NULL);
                    if (client < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }

                    if (set_nonblocking(client) < 0) {
                        perror("fcntl(client)");
                        close(client);
                        continue;
                    }

                    struct epoll_event client_ev = {0};
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.fd = client;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &client_ev) < 0) {
                        perror("epoll_ctl(ADD client)");
                        close(client);
                        continue;
                    }

                    printf("client connected: fd=%d\n", client);
                }
                continue;
            }

            if (flags & EPOLLIN) {
                for (;;) {
                    char buf[BUF_SIZE];
                    ssize_t r = recv(fd, buf, sizeof(buf), 0);

                    if (r > 0) {
                        ssize_t sent = 0;
                        while (sent < r) {
                            ssize_t w = send(fd, buf + sent, (size_t)(r - sent), 0);

                            if (w > 0) {
                                sent += w;
                            } else if (w < 0 && errno == EINTR) {
                                continue;
                            } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                fprintf(stderr, "send would block on fd=%d; "
                                                "output buffering is required for full nonblocking echo\n", fd);
                                break;
                            } else {
                                perror("send");
                                close_fd(epfd, fd);
                                sent = r; 
                                break;
                            }
                        }
                        continue;
                    }

                    if (r == 0) {
                        printf("client disconnected: fd=%d\n", fd);
                        close_fd(epfd, fd);
                        break;
                    }

                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    perror("recv");
                    close_fd(epfd, fd);
                    break;
                }
            }
        }
    }

    close(epfd);
    close(listener);
    return EXIT_SUCCESS;
}
