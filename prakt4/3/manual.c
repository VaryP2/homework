#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "example.com";
    const char *port = "80";

    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &result) != 0) {
        perror("getaddrinfo");
        return EXIT_FAILURE;
    }

    int sock = -1;

    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1)
            continue;

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sock);
        sock = -1;
    }

    if (sock == -1) {
        fprintf(stderr, "Не удалось подключиться к %s:%s\n", host, port);
        freeaddrinfo(result);
        return EXIT_FAILURE;
    }

    char request[512];
    int length = snprintf(
        request,
        sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        host
    );

    if (send(sock, request, (size_t)length, 0) == -1) {
        perror("send");
        close(sock);
        freeaddrinfo(result);
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_SIZE];
    ssize_t n;

    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        fputs(buffer, stdout);
    }

    close(sock);
    freeaddrinfo(result);

    return n < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
