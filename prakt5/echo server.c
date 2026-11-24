#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>

static int callback_echo(struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len) {
    (void)user;

    switch (reason) {
        case LWS_CALLBACK_RECEIVE: {
            unsigned char buffer[LWS_PRE + 4096];

            if (len > sizeof(buffer) - LWS_PRE)
                return -1;

            memcpy(buffer + LWS_PRE, in, len);

            lws_write(wsi, buffer + LWS_PRE, len, LWS_WRITE_TEXT);
            break;
        }

        case LWS_CALLBACK_ESTABLISHED:
            printf("Client connected.\n");
            break;

        case LWS_CALLBACK_CLOSED:
            printf("Client disconnected.\n");
            break;

        default:
            break;
    }

    return 0;
}

int main(void) {
    struct lws_protocols protocols[] = {
        {
            .name = "echo-protocol",
            .callback = callback_echo,
            .per_session_data_size = 0,
            .rx_buffer_size = 4096
        },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info = {0};
    info.port = 9000;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);

    if (!context) {
        fprintf(stderr, "Не удалось создать WebSocket context.\n");
        return 1;
    }

    printf("Echo server: ws://localhost:9000\n");

    while (lws_service(context, 1000) >= 0) {
    }

    lws_context_destroy(context);
    return 0;
}
