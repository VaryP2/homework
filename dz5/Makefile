CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

all: select_server epoll_server

select_server: select_server.c
	$(CC) $(CFLAGS) $< -o $@

epoll_server: epoll_server.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f select_server epoll_server

.PHONY: all clean
