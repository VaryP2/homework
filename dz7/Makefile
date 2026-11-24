CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2
LDFLAGS = -pthread

TARGET = server
SRC = src/server.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) server_asan server_tsan

asan:
	$(CC) -Wall -Wextra -g -fsanitize=address,undefined $(SRC) -o server_asan -pthread

tsan:
	$(CC) -Wall -Wextra -g -fsanitize=thread $(SRC) -o server_tsan -pthread

.PHONY: all clean asan tsan
