#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BACKLOG 16
#define BUFFER_SIZE 1024

typedef struct client {
    int fd;
    pthread_t tid;
    int active;
    int joined;
    struct client *next;
} client_t;

static int server_fd = -1;
static volatile sig_atomic_t stop_requested = 0;

static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int client_count = 0;
static client_t *clients = NULL;

static void handle_signal(int signo) {
    (void)signo;
    stop_requested = 1;

    if (server_fd != -1)
        close(server_fd);
}

static void add_client(client_t *client) {
    pthread_mutex_lock(&clients_mutex);

    client->active = 1;
    client->next = clients;
    clients = client;
    client_count++;

    printf("[server] clients=%d\n", client_count);
    fflush(stdout);

    pthread_mutex_unlock(&clients_mutex);
}

static void mark_client_finished(client_t *client) {
    pthread_mutex_lock(&clients_mutex);

    if (client->active) {
        client->active = 0;
        client_count--;

        printf("[server] client disconnected, clients=%d\n", client_count);
        fflush(stdout);
    }

    pthread_mutex_unlock(&clients_mutex);
}

static void shutdown_all_clients(void) {
    pthread_mutex_lock(&clients_mutex);

    for (client_t *c = clients; c != NULL; c = c->next) {
        if (c->active)
            shutdown(c->fd, SHUT_RDWR);
    }

    pthread_mutex_unlock(&clients_mutex);
}

static void join_all_clients(void) {
    for (;;) {
        client_t *target = NULL;

        pthread_mutex_lock(&clients_mutex);

        for (client_t *c = clients; c != NULL; c = c->next) {
            if (!c->joined) {
                c->joined = 1;
                target = c;
                break;
            }
        }

        pthread_mutex_unlock(&clients_mutex);

        if (target == NULL)
            break;

        pthread_join(target->tid, NULL);
    }
}

static void remove_client_entry(client_t *target) {
    pthread_mutex_lock(&clients_mutex);

    client_t **link = &clients;

    while (*link != NULL) {
        if (*link == target) {
            *link = target->next;
            break;
        }
        link = &(*link)->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

static void free_clients(void) {
    pthread_mutex_lock(&clients_mutex);

    client_t *c = clients;
    while (c != NULL) {
        client_t *next = c->next;
        free(c);
        c = next;
    }

    clients = NULL;

    pthread_mutex_unlock(&clients_mutex);
}

static void *worker(void *arg) {
    client_t *client = arg;
    char buffer[BUFFER_SIZE];

    char ip[INET_ADDRSTRLEN] = "unknown";
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);

    if (getpeername(client->fd, (struct sockaddr *)&peer, &peer_len) == 0)
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));

    printf("[worker] connected: %s:%d\n",
           ip, ntohs(peer.sin_port));
    fflush(stdout);

    for (;;) {
        ssize_t n = recv(client->fd, buffer, sizeof(buffer) - 1, 0);

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        buffer[n] = '\0';

        ssize_t sent = 0;

        while (sent < n) {
            ssize_t m = send(client->fd, buffer + sent, (size_t)(n - sent), 0);

            if (m < 0) {
                if (errno == EINTR)
                    continue;
                sent = n;
                break;
            }

            sent += m;
        }
    }

    shutdown(client->fd, SHUT_RDWR);
    close(client->fd);

    mark_client_finished(client);
    return NULL;
}

static int create_server_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("setsockopt");

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PORT)
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, BACKLOG) == -1) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);

    server_fd = create_server_socket();
    if (server_fd == -1)
        return EXIT_FAILURE;

    printf("[server] listening on 0.0.0.0:%d\n", PORT);
    printf("[server] press Ctrl+C to stop\n");
    fflush(stdout);

    while (!stop_requested) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&peer,
                               &peer_len);

        if (client_fd == -1) {
            if (stop_requested)
                break;

            if (errno == EINTR)
                continue;

            perror("accept");
            continue;
        }

        client_t *client = calloc(1, sizeof(*client));
        if (client == NULL) {
            perror("calloc");
            close(client_fd);
            continue;
        }

        client->fd = client_fd;
        add_client(client);

        int rc = pthread_create(&client->tid, NULL, worker, client);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));

            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);

            mark_client_finished(client);
            remove_client_entry(client);
            free(client);
            continue;
        }
    }

    printf("\n[server] shutdown requested\n");

    shutdown_all_clients();
    join_all_clients();
    free_clients();

    pthread_mutex_destroy(&clients_mutex);

    printf("[server] stopped cleanly\n");
    return EXIT_SUCCESS;
}
