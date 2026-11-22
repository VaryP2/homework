N = nasm -f elf64
L = ld
PROG = main.o iolib.o
TARGET = prog

$(TARGET): $(PROG)
	$(L) -o $@ $^

main.o: main.asm iolib.inc
	$(N) $(if $(DEBUG),-DDEBUG,) $< -o $@

iolib.o: iolib.asm iolib.inc
	$(N) $< -o $@

clean:
	rm -f *.o $(TARGET)

.PHONY: clean
