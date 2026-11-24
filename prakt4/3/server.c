#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define ROOT "public"

const char *content_type(const char *path) {
    const char *ext = strrchr(path, '.');

    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";

    return "application/octet-stream";
}

void send_error(int client, int code, const char *text) {
    char body[256];
    int body_len = snprintf(body, sizeof(body),
                            "<h1>%d %s</h1>\n", code, text);

    char header[512];
    int header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, text, body_len
    );

    send(client, header, (size_t)header_len, 0);
    send(client, body, (size_t)body_len, 0);
}

void handle_client(int client) {
    char request[BUFFER_SIZE];

    ssize_t n = recv(client, request, sizeof(request) - 1, 0);
    if (n <= 0)
        return;

    request[n] = '\0';

    char method[16];
    char path[1024];
    char version[16];

    if (sscanf(request, "%15s %1023s %15s", method, path, version) != 3) {
        send_error(client, 400, "Bad Request");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_error(client, 405, "Method Not Allowed");
        return;
    }

    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.html");

    if (strstr(path, "..") != NULL) {
        send_error(client, 403, "Forbidden");
        return;
    }

    char file_path[1200];
    snprintf(file_path, sizeof(file_path), "%s%s", ROOT, path);

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        send_error(client, 404, "Not Found");
        return;
    }

    struct stat st;
    if (stat(file_path, &st) == -1) {
        fclose(file);
        send_error(client, 500, "Internal Server Error");
        return;
    }

    const char *type = content_type(file_path);

    char header[512];
    int header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        type, (long long)st.st_size
    );

    send(client, header, (size_t)header_len, 0);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client, buffer, bytes_read, 0);
    }

    fclose(file);
}

int main(void) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PORT)
    };

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server);
        return EXIT_FAILURE;
    }

    if (listen(server, 16) == -1) {
        perror("listen");
        close(server);
        return EXIT_FAILURE;
    }

    printf("Static server: http://localhost:%d/\n", PORT);

    for (;;) {
        int client = accept(server, NULL, NULL);

        if (client == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }

        handle_client(client);
        close(client);
    }

    close(server);
    return EXIT_SUCCESS;
}
