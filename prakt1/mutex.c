#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 16
#define CLIENTS 3
#define ITERATIONS 1000000

static long long counter = 0;

static int create_listener(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

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

typedef struct {
    int client_fd;
    int id;
} client_arg_t;

static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg) {
    client_arg_t *client = (client_arg_t *)arg;

    for (long long i = 0; i < ITERATIONS; ++i) {
        pthread_mutex_lock(&counter_mutex);
        ++counter;
        pthread_mutex_unlock(&counter_mutex);
    }

    dprintf(client->client_fd,
            "thread %d finished; shared counter = %lld\n",
            client->id, counter);

    close(client->client_fd);
    free(client);
    return NULL;
}

int main(int argc, char **argv) {
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

    int listener = create_listener((unsigned short)port);
    printf("Server: waiting for %d clients on port %ld...\n", CLIENTS, port);

    pthread_t threads[CLIENTS];

    for (int i = 0; i < CLIENTS; ++i) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) { --i; continue; }
            perror("accept");
            close(listener);
            return EXIT_FAILURE;
        }

        client_arg_t *arg = malloc(sizeof(*arg));
        if (!arg) {
            perror("malloc");
            close(client_fd);
            close(listener);
            return EXIT_FAILURE;
        }

        arg->client_fd = client_fd;
        arg->id = i + 1;

        int rc = pthread_create(&threads[i], NULL, worker, arg);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            close(client_fd);
            free(arg);
            close(listener);
            return EXIT_FAILURE;
        }

        printf("client %d connected; thread created\n", i + 1);
    }

    for (int i = 0; i < CLIENTS; ++i)
        pthread_join(threads[i], NULL);

    long long expected = (long long)CLIENTS * ITERATIONS;

    printf("\n=== MUTEX RESULT ===\n");
    printf("Expected: %lld\n", expected);
    printf("Actual:   %lld\n", counter);
    printf("Lost:     %lld\n", expected - counter);

    close(listener);
    return EXIT_SUCCESS;
}
