#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

struct client {
    int fd;
    struct sockaddr_in addr;
};

void *worker(void *arg) {
    struct client *c = (struct client *)arg;
    char buffer[BUFFER_SIZE];

    printf("Client connected: %s:%d\n",
           inet_ntoa(c->addr.sin_addr),
           ntohs(c->addr.sin_port));

    ssize_t n;
    while ((n = recv(c->fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("Received: %s", buffer);
        send(c->fd, buffer, (size_t)n, 0);
    }

    close(c->fd);
    free(c);

    return NULL;
}

int main(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PORT)
    };

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(srv);
        return EXIT_FAILURE;
    }

    if (listen(srv, SOMAXCONN) == -1) {
        perror("listen");
        close(srv);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d...\n", PORT);

    for (;;) {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);

        int cl = accept(srv, (struct sockaddr *)&peer, &len);
        if (cl == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }

        struct client *c = malloc(sizeof(*c));
        if (c == NULL) {
            perror("malloc");
            close(cl);
            continue;
        }

        c->fd = cl;
        c->addr = peer;

        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, c) == 0) {
            pthread_detach(tid);
        } else {
            perror("pthread_create");
            close(cl);
            free(c);
        }
    }

    close(srv);
    return EXIT_SUCCESS;
}
