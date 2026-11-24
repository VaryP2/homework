CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
BIN = ws_server frame_parser proto_benchmark

all: $(BIN)

ws_server: src/ws_server.c
	$(CC) $(CFLAGS) $< -o $@

frame_parser: src/frame_parser.c
	$(CC) $(CFLAGS) $< -o $@

proto_benchmark: src/proto_benchmark.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(BIN) src/message.pb-c.c src/message.pb-c.h

proto-gen:
	protoc-c --c_out=src -I proto proto/message.proto

.PHONY: all clean proto-gen
