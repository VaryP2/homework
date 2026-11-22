NASM ?= nasm
CC ?= gcc
CFLAGS = -O2 -Wall -Wextra -std=c11

all: benchmark

memcpy.o: memcpy.asm
	$(NASM) -f elf64 $< -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

benchmark: main.o memcpy.o
	$(CC) $^ -o $@

clean:
	rm -f *.o benchmark
.PHONY: all clean
