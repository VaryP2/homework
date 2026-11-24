#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define RESPONSE_SIZE 16384

static int request(const char *host, const char *port, const char *method,
                   const char *path, const char *body) {
    struct addrinfo hints, *result = NULL, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &result);
    if (err != 0) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return 1; }

    int fd = -1;
    for (rp = result; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd >= 0 && connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        if (fd >= 0) close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    if (fd < 0) { perror("connect"); return 1; }

    size_t body_len = body ? strlen(body) : 0;
    char header[2048];
    int n = snprintf(header, sizeof(header),
        "%s %s HTTP/1.1\r\nHost: %s:%s\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n",
        method, path, host, port, body_len);
    if (write(fd, header, (size_t)n) < 0 || (body_len && write(fd, body, body_len) < 0)) {
        perror("write"); close(fd); return 1;
    }

    char response[RESPONSE_SIZE];
    size_t total = 0;
    ssize_t got;
    while (total + 1 < sizeof(response) &&
           (got = read(fd, response + total, sizeof(response) - total - 1)) > 0)
        total += (size_t)got;
    response[total] = '\0';
    close(fd);
    printf("%s", response);
    return 0;
}

static void usage(const char *name) {
    fprintf(stderr,
        "Usage:\n"
        "  %s HOST PORT list\n"
        "  %s HOST PORT create \"title\"\n"
        "  %s HOST PORT update ID \"title\"\n"
        "  %s HOST PORT delete ID\n", name, name, name, name);
}

int main(int argc, char **argv) {
    if (argc < 4) { usage(argv[0]); return 1; }
    const char *host = argv[1], *port = argv[2], *command = argv[3];
    char path[512], body[1024];
    if (strcmp(command, "list") == 0 && argc == 4) {
        return request(host, port, "GET", "/api/items", NULL);
    }
    if (strcmp(command, "create") == 0 && argc == 5) {
        snprintf(body, sizeof(body), "{\"title\":\"%s\"}", argv[4]);
        return request(host, port, "POST", "/api/items", body);
    }
    if (strcmp(command, "update") == 0 && argc == 6) {
        snprintf(path, sizeof(path), "/api/items/%s", argv[4]);
        snprintf(body, sizeof(body), "{\"title\":\"%s\"}", argv[5]);
        return request(host, port, "PUT", path, body);
    }
    if (strcmp(command, "delete") == 0 && argc == 5) {
        snprintf(path, sizeof(path), "/api/items/%s", argv[4]);
        return request(host, port, "DELETE", path, NULL);
    }
    usage(argv[0]);
    return 1;
}
