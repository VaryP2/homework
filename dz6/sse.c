#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BACKLOG 32
#define BUFFER_SIZE 4096

static long long counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t counter_changed = PTHREAD_COND_INITIALIZER;

static volatile sig_atomic_t running = 1;

typedef struct {
    int fd;
} client_arg_t;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);

        if (n > 0) {
            sent += (size_t)n;
            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;

        return -1;
    }

    return 0;
}

static int create_listener(unsigned short port)
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

static long long get_counter(void)
{
    long long value;

    pthread_mutex_lock(&counter_mutex);
    value = counter;
    pthread_mutex_unlock(&counter_mutex);

    return value;
}

static void *ticker_thread(void *unused)
{
    (void)unused;

    while (running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        pthread_mutex_lock(&counter_mutex);

        if (!running) {
            pthread_mutex_unlock(&counter_mutex);
            break;
        }

        ++counter;
        pthread_cond_broadcast(&counter_changed);

        long long value = counter;
        pthread_mutex_unlock(&counter_mutex);

        printf("[ticker] counter=%lld\n", value);
        fflush(stdout);

        pthread_mutex_lock(&counter_mutex);
        if (running)
            pthread_cond_timedwait(&counter_changed, &counter_mutex, &ts);
        pthread_mutex_unlock(&counter_mutex);
    }

    return NULL;
}

static void handle_http(int fd)
{
    char request[BUFFER_SIZE];
    ssize_t n = recv(fd, request, sizeof(request) - 1, 0);

    if (n <= 0)
        return;

    request[n] = '\0';

    char method[16];
    char path[256];

    if (sscanf(request, "%15s %255s", method, path) != 2)
        return;

    if (strcmp(method, "GET") != 0) {
        const char *response =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        send_all(fd, response, strlen(response));
        return;
    }

    if (strcmp(path, "/events") == 0) {
        const char *headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";

        if (send_all(fd, headers, strlen(headers)) < 0)
            return;

        long long last_sent = get_counter();

        char event[128];
        int len = snprintf(event, sizeof(event),
                           "event: counter\n"
                           "data: %lld\n\n", last_sent);

        if (len < 0 || send_all(fd, event, (size_t)len) < 0)
            return;

        while (running) {
            pthread_mutex_lock(&counter_mutex);

            while (running && counter == last_sent)
                pthread_cond_wait(&counter_changed, &counter_mutex);

            if (!running) {
                pthread_mutex_unlock(&counter_mutex);
                break;
            }

            last_sent = counter;

            len = snprintf(event, sizeof(event),
                           "event: counter\n"
                           "data: %lld\n\n", last_sent);

            pthread_mutex_unlock(&counter_mutex);

            if (len < 0 || send_all(fd, event, (size_t)len) < 0)
                break;
        }

        return;
    }

    if (strcmp(path, "/") == 0) {
        const char *html =
            "<!doctype html>\n"
            "<html lang=\"ru\"><meta charset=\"utf-8\">\n"
            "<title>SSE counter</title>\n"
            "<h1>Счётчик: <span id=\"value\">-</span></h1>\n"
            "<script>\n"
            "const source = new EventSource('/events');\n"
            "source.addEventListener('counter', e => {\n"
            "  document.getElementById('value').textContent = e.data;\n"
            "});\n"
            "source.onerror = () => console.log('SSE reconnecting...');\n"
            "</script>\n"
            "</html>\n";

        char header[256];
        int len = snprintf(
            header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            strlen(html)
        );

        if (len > 0) {
            send_all(fd, header, (size_t)len);
            send_all(fd, html, strlen(html));
        }

        return;
    }

    {
        const char *body = "404 Not Found\n";
        char header[256];

        int len = snprintf(
            header, sizeof(header),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            strlen(body)
        );

        if (len > 0) {
            send_all(fd, header, (size_t)len);
            send_all(fd, body, strlen(body));
        }
    }
}

static void *client_thread(void *arg)
{
    client_arg_t *client = (client_arg_t *)arg;
    int fd = client->fd;
    free(client);

    handle_http(fd);

    shutdown(fd, SHUT_RDWR);
    close(fd);

    return NULL;
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

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);

    int listener = create_listener((unsigned short)port);

    pthread_t ticker;
    if (pthread_create(&ticker, NULL, ticker_thread, NULL) != 0) {
        perror("pthread_create(ticker)");
        close(listener);
        return EXIT_FAILURE;
    }

    printf("SSE server started on port %ld\n", port);
    printf("Open http://127.0.0.1:%ld/ or curl -N http://127.0.0.1:%ld/events\n",
           port, port);

    while (running) {
        int client_fd = accept(listener, NULL, NULL);

        if (client_fd < 0) {
            if (errno == EINTR)
                continue;

            perror("accept");
            break;
        }

        client_arg_t *arg = malloc(sizeof(*arg));
        if (!arg) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        arg->fd = client_fd;

        pthread_t thread;
        int rc = pthread_create(&thread, NULL, client_thread, arg);

        if (rc != 0) {
            fprintf(stderr, "pthread_create(client): %s\n", strerror(rc));
            close(client_fd);
            free(arg);
            continue;
        }

        pthread_detach(thread);
    }

    close(listener);

    pthread_mutex_lock(&counter_mutex);
    running = 0;
    pthread_cond_broadcast(&counter_changed);
    pthread_mutex_unlock(&counter_mutex);

    pthread_join(ticker, NULL);

    pthread_mutex_destroy(&counter_mutex);
    pthread_cond_destroy(&counter_changed);

    printf("Server stopped. Final counter=%lld\n", counter);
    return EXIT_SUCCESS;
}
