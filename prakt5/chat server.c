#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>

#define MAX_CLIENTS 32
#define MAX_MESSAGE 4096

static struct lws *clients[MAX_CLIENTS];
static size_t client_count = 0;

static void add_client(struct lws *wsi) {
    if (client_count < MAX_CLIENTS)
        clients[client_count++] = wsi;
}

static void remove_client(struct lws *wsi) {
    for (size_t i = 0; i < client_count; ++i) {
        if (clients[i] == wsi) {
            clients[i] = clients[client_count - 1];
            client_count--;
            return;
        }
    }
}

static void broadcast(struct lws *sender, const void *data, size_t len) {
    unsigned char buffer[LWS_PRE + MAX_MESSAGE];

    if (len > MAX_MESSAGE)
        len = MAX_MESSAGE;

    memcpy(buffer + LWS_PRE, data, len);

    for (size_t i = 0; i < client_count; ++i) {
        if (clients[i] != sender) {
            lws_write(clients[i],
                      buffer + LWS_PRE,
                      len,
                      LWS_WRITE_TEXT);
        }
    }
}

static int callback_chat(struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len) {
    (void)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            add_client(wsi);
            printf("Client connected. Total: %zu\n", client_count);
            break;

        case LWS_CALLBACK_RECEIVE:
            printf("Message: %.*s\n", (int)len, (char *)in);
            broadcast(wsi, in, len);
            break;

        case LWS_CALLBACK_CLOSED:
            remove_client(wsi);
            printf("Client disconnected. Total: %zu\n", client_count);
            break;

        default:
            break;
    }

    return 0;
}

int main(void) {
    struct lws_protocols protocols[] = {
        {
            .name = "chat",
            .callback = callback_chat,
            .per_session_data_size = 0,
            .rx_buffer_size = MAX_MESSAGE
        },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info = {0};
    info.port = 9001;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);

    if (!context) {
        fprintf(stderr, "Не удалось создать WebSocket context.\n");
        return 1;
    }

    printf("Chat server: ws://localhost:9001\n");

    while (lws_service(context, 1000) >= 0) {
    }

    lws_context_destroy(context);
    return 0;
}
