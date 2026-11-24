#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 16384
#define MAX_ITEMS 100
#define TITLE_SIZE 256

typedef struct { int id; char title[TITLE_SIZE]; int used; } Item;
static Item items[MAX_ITEMS];
static int next_id = 1;

static void send_response(int fd, int status, const char *reason,
                          const char *type, const char *body) {
    char header[1024];
    size_t len = body ? strlen(body) : 0;
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, reason, type, len);
    (void)write(fd, header, (size_t)n);
    if (len) (void)write(fd, body, len);
}

static const char *content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm")) return "text/html; charset=utf-8";
    if (!strcasecmp(ext, ".css")) return "text/css; charset=utf-8";
    if (!strcasecmp(ext, ".js")) return "application/javascript; charset=utf-8";
    if (!strcasecmp(ext, ".json")) return "application/json; charset=utf-8";
    if (!strcasecmp(ext, ".png")) return "image/png";
    if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")) return "image/jpeg";
    return "application/octet-stream";
}

static void serve_static(int fd, const char *url) {
    char path[512];
    const char *relative = (strcmp(url, "/") == 0) ? "/index.html" : url;
    if (strstr(relative, "..") || strchr(relative, '\\')) {
        send_response(fd, 400, "Bad Request", "text/plain; charset=utf-8", "Invalid path\n");
        return;
    }
    snprintf(path, sizeof(path), "public%s", relative);
    FILE *f = fopen(path, "rb");
    if (!f) {
        send_response(fd, 404, "Not Found", "text/plain; charset=utf-8", "File not found\n");
        return;
    }
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size < 0 || st.st_size > 1000000) {
        fclose(f);
        send_response(fd, 404, "Not Found", "text/plain; charset=utf-8", "File not found\n");
        return;
    }
    size_t size = (size_t)st.st_size;
    char *data = malloc(size + 1);
    if (!data) { fclose(f); send_response(fd, 500, "Internal Server Error", "text/plain", "Memory error\n"); return; }
    size_t got = fread(data, 1, size, f);
    fclose(f);
    data[got] = '\0';

    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        content_type(path), got);
    (void)write(fd, header, (size_t)n);
    if (got) (void)write(fd, data, got);
    free(data);
}

static int extract_title(const char *body, char *out, size_t cap) {
    const char *p = strstr(body, "\"title\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap) {
        if (*p == '\\' && p[1]) p++;
        out[j++] = *p++;
    }
    if (*p != '"') return 0;
    out[j] = '\0';
    return j > 0;
}

static void api_items(int fd, const char *method, const char *url, const char *body) {
    if (strcmp(url, "/api/items") == 0 && strcmp(method, "GET") == 0) {
        char json[12000] = "[";
        int first = 1;
        for (int i = 0; i < MAX_ITEMS; i++) if (items[i].used) {
            char entry[400];
            snprintf(entry, sizeof(entry), "%s{\"id\":%d,\"title\":\"%s\"}",
                     first ? "" : ",", items[i].id, items[i].title);
            strncat(json, entry, sizeof(json) - strlen(json) - 1);
            first = 0;
        }
        strncat(json, "]", sizeof(json) - strlen(json) - 1);
        send_response(fd, 200, "OK", "application/json; charset=utf-8", json);
        return;
    }

    if (strcmp(url, "/api/items") == 0 && strcmp(method, "POST") == 0) {
        char title[TITLE_SIZE];
        if (!extract_title(body, title, sizeof(title))) {
            send_response(fd, 400, "Bad Request", "application/json; charset=utf-8",
                          "{\"error\":\"field 'title' is required\"}");
            return;
        }
        for (int i = 0; i < MAX_ITEMS; i++) if (!items[i].used) {
            items[i].used = 1; items[i].id = next_id++;
            snprintf(items[i].title, sizeof(items[i].title), "%s", title);
            char out[400];
            snprintf(out, sizeof(out), "{\"id\":%d,\"title\":\"%s\"}", items[i].id, items[i].title);
            send_response(fd, 201, "Created", "application/json; charset=utf-8", out);
            return;
        }
        send_response(fd, 400, "Bad Request", "application/json", "{\"error\":\"storage is full\"}");
        return;
    }

    const char *prefix = "/api/items/";
    if (strncmp(url, prefix, strlen(prefix)) == 0) {
        char *end = NULL;
        long id = strtol(url + strlen(prefix), &end, 10);
        if (id <= 0 || !end || *end != '\0') {
            send_response(fd, 400, "Bad Request", "application/json", "{\"error\":\"invalid id\"}");
            return;
        }
        int index = -1;
        for (int i = 0; i < MAX_ITEMS; i++)
            if (items[i].used && items[i].id == id) { index = i; break; }
        if (index < 0) {
            send_response(fd, 404, "Not Found", "application/json", "{\"error\":\"item not found\"}");
            return;
        }
        if (strcmp(method, "PUT") == 0) {
            char title[TITLE_SIZE];
            if (!extract_title(body, title, sizeof(title))) {
                send_response(fd, 400, "Bad Request", "application/json",
                              "{\"error\":\"field 'title' is required\"}");
                return;
            }
            snprintf(items[index].title, sizeof(items[index].title), "%s", title);
            char out[400];
            snprintf(out, sizeof(out), "{\"id\":%d,\"title\":\"%s\"}", items[index].id, items[index].title);
            send_response(fd, 200, "OK", "application/json; charset=utf-8", out);
            return;
        }
        if (strcmp(method, "DELETE") == 0) {
            items[index].used = 0;
            send_response(fd, 200, "OK", "application/json; charset=utf-8", "{\"deleted\":true}");
            return;
        }
    }
    send_response(fd, 404, "Not Found", "application/json; charset=utf-8", "{\"error\":\"route not found\"}");
}

static void handle_client(int fd) {
    char request[BUFFER_SIZE];
    size_t used = 0;
    while (used + 1 < sizeof(request)) {
        ssize_t n = read(fd, request + used, sizeof(request) - used - 1);
        if (n <= 0) break;
        used += (size_t)n;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n")) {
            char *header_end = strstr(request, "\r\n\r\n") + 4;
            size_t header_len = (size_t)(header_end - request);
            long content_length = 0;
            char *cl = strcasestr(request, "Content-Length:");
            if (cl && (size_t)(cl - request) < header_len) content_length = strtol(cl + 15, NULL, 10);
            size_t body_read = used - header_len;
            while (content_length > (long)body_read && used + 1 < sizeof(request)) {
                n = read(fd, request + used, sizeof(request) - used - 1);
                if (n <= 0) break;
                used += (size_t)n;
                body_read += (size_t)n;
                request[used] = '\0';
            }
            break;
        }
    }
    request[used] = '\0';
    char method[16] = {0}, url[512] = {0}, version[16] = {0};
    if (sscanf(request, "%15s %511s %15s", method, url, version) != 3 ||
        strncmp(version, "HTTP/1.", 7) != 0) {
        send_response(fd, 400, "Bad Request", "text/plain; charset=utf-8", "Malformed request\n");
        return;
    }
    char *body = strstr(request, "\r\n\r\n");
    body = body ? body + 4 : "";
    if (strncmp(url, "/api/", 5) == 0) {
        if (strcmp(method, "GET") && strcmp(method, "POST") &&
            strcmp(method, "PUT") && strcmp(method, "DELETE")) {
            send_response(fd, 400, "Bad Request", "application/json", "{\"error\":\"unsupported method\"}");
        } else api_items(fd, method, url, body);
    } else if (strcmp(method, "GET") == 0) {
        serve_static(fd, url);
    } else {
        send_response(fd, 400, "Bad Request", "text/plain; charset=utf-8", "Only GET is supported for static files\n");
    }
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 8080;
    if (port < 1 || port > 65535) { fprintf(stderr, "Invalid port\n"); return 1; }
    signal(SIGPIPE, SIG_IGN);
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(server, 16) < 0) {
        perror("bind/listen"); close(server); return 1;
    }
    printf("HTTP server listening on http://127.0.0.1:%d\n", port);
    for (;;) {
        int client = accept(server, NULL, NULL);
        if (client < 0) { if (errno == EINTR) continue; perror("accept"); continue; }
        handle_client(client);
        close(client);
    }
}
