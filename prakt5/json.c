#include <jansson.h>
#include <protobuf-c/protobuf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.pb-c.h"

int main(void) {
    const char *name = "Alice";
    int id = 42;
    const char *text = "Hello, WebSocket!";

    /* JSON */
    json_t *root = json_object();
    json_object_set_new(root, "name", json_string(name));
    json_object_set_new(root, "id", json_integer(id));
    json_object_set_new(root, "text", json_string(text));

    char *json = json_dumps(root, JSON_COMPACT);
    if (!json) {
        json_decref(root);
        return EXIT_FAILURE;
    }

    Message message = MESSAGE__INIT;
    message.name = (char *)name;
    message.id = id;
    message.text = (char *)text;

    size_t protobuf_size = message__get_packed_size(&message);
    uint8_t *protobuf_data = malloc(protobuf_size);

    if (!protobuf_data) {
        free(json);
        json_decref(root);
        return EXIT_FAILURE;
    }

    message__pack(&message, protobuf_data);

    printf("JSON:\n%s\n", json);
    printf("JSON size: %zu bytes\n", strlen(json));

    printf("\nProtobuf size: %zu bytes\n", protobuf_size);

    if (protobuf_size < strlen(json)) {
        printf("Protobuf меньше JSON на %zu байт.\n",
               strlen(json) - protobuf_size);
    } else {
        printf("JSON меньше Protobuf на %zu байт.\n",
               protobuf_size - strlen(json));
    }

    free(protobuf_data);
    free(json);
    json_decref(root);

    return EXIT_SUCCESS;
}
