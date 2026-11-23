CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
LDFLAGS = -pthread

TARGET = sse_server

all: $(TARGET)

$(TARGET): sse_server.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
