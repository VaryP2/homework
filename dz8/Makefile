CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
BIN = http_server rest_client

all: $(BIN)

http_server: src/http_server.c
	$(CC) $(CFLAGS) $< -o $@

rest_client: src/rest_client.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(BIN)

.PHONY: all clean
