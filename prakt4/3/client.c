#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>

struct response {
    char *data;
    size_t size;
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct response *response = userp;

    char *new_data = realloc(response->data, response->size + total + 1);
    if (!new_data)
        return 0;

    response->data = new_data;
    memcpy(response->data + response->size, contents, total);
    response->size += total;
    response->data[response->size] = '\0';

    return total;
}

int main(void) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Не удалось создать CURL\n");
        return EXIT_FAILURE;
    }

    struct response response = {0};

    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://jsonplaceholder.typicode.com/users/1");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        fprintf(stderr, "HTTP ошибка: %s\n", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        free(response.data);
        return EXIT_FAILURE;
    }

    printf("JSON:\n%s\n\n", response.data);

    json_object *root = json_tokener_parse(response.data);
    if (!root) {
        fprintf(stderr, "Не удалось разобрать JSON\n");
        curl_easy_cleanup(curl);
        free(response.data);
        return EXIT_FAILURE;
    }

    json_object *name = NULL;

    if (json_object_object_get_ex(root, "name", &name)) {
        printf("Поле name: %s\n", json_object_get_string(name));
    } else {
        printf("Поле name отсутствует\n");
    }

    json_object_put(root);
    curl_easy_cleanup(curl);
    free(response.data);

    return EXIT_SUCCESS;
}
