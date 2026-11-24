#include <stdint.h>
#include <stdio.h>
#include <string.h>

void parse_frame(const uint8_t *frame, size_t size) {
    if (size < 2) {
        printf("Кадр слишком короткий.\n");
        return;
    }

    uint8_t first = frame[0];
    uint8_t second = frame[1];

    int fin = (first >> 7) & 1;
    int opcode = first & 0x0F;
    int mask = (second >> 7) & 1;

    uint64_t payload_len = second & 0x7F;
    size_t pos = 2;

    if (payload_len == 126) {
        if (size < pos + 2) {
            printf("Некорректный кадр.\n");
            return;
        }

        payload_len = ((uint64_t)frame[pos] << 8) | frame[pos + 1];
        pos += 2;
    } else if (payload_len == 127) {
        if (size < pos + 8) {
            printf("Некорректный кадр.\n");
            return;
        }

        payload_len = 0;
        for (int i = 0; i < 8; ++i)
            payload_len = (payload_len << 8) | frame[pos + i];

        pos += 8;
    }

    printf("FIN: %d\n", fin);
    printf("Opcode: %d\n", opcode);
    printf("Mask: %d\n", mask);
    printf("Payload length: %llu\n",
           (unsigned long long)payload_len);

    if (mask) {
        if (size < pos + 4) {
            printf("Нет masking key.\n");
            return;
        }

        const uint8_t *key = frame + pos;
        pos += 4;

        if (size < pos + payload_len) {
            printf("Payload не помещается в кадр.\n");
            return;
        }

        printf("Payload: ");

        for (uint64_t i = 0; i < payload_len; ++i) {
            uint8_t decoded = frame[pos + i] ^ key[i % 4];
            putchar(decoded);
        }

        putchar('\n');
    } else {
        if (size < pos + payload_len) {
            printf("Payload не помещается в кадр.\n");
            return;
        }

        printf("Payload: ");
        fwrite(frame + pos, 1, payload_len, stdout);
        putchar('\n');
    }
}

int main(void) {
    const uint8_t frame[] = {
        0x81, 0x85,
        0x37, 0xFA, 0x21, 0x3D,
        0x7F, 0x9F, 0x4D, 0x51, 0x58
    };

    parse_frame(frame, sizeof(frame));
    return 0;
}
