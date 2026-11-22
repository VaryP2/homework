#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); return 1; }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        close(fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("bind");
        close(fd);
        return 1;
    }

    if (listen(fd, 16) == -1) {
        perror("listen");
        close(fd);
        return 1;
    }

    printf("server listening on 0.0.0.0:9000\n");

    for (int i = 0; i < 3; ++i) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof peer;
        int client;

        do {
            client = accept(fd, (struct sockaddr *)&peer, &peer_len);
        } while (client == -1 && errno == EINTR);

        if (client == -1) {
            perror("accept");
            close(fd);
            return 1;
        }

        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip) == NULL) {
            perror("inet_ntop");
            close(client);
            close(fd);
            return 1;
        }

        printf("client %s:%u\n", ip, ntohs(peer.sin_port));

        char msg[128];
        int n = snprintf(msg, sizeof msg, "hello, %s:%u\n", ip, ntohs(peer.sin_port));
        if (send(client, msg, (size_t)n, 0) == -1) {
            if (errno != EINTR) perror("send");
        }

        char buf[1024];
        for (;;) {
            ssize_t r = recv(client, buf, sizeof buf, 0);
            if (r > 0) continue;
            if (r == 0) break;
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }

        close(client);
    }

    close(fd);
    printf("server stopped after 3 clients\n");
    return 0;
}
