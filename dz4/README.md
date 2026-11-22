# Практикум: x86-64 NASM — print_dec, read_dec, ручная сборка и Makefile

Файлы:
- `iolib.asm` — процедуры `print_dec`, `read_dec` и `debug_value`.
- `iolib.inc` — интерфейс и макросы `PRINT_DEC` / `READ_DEC`.
- `main_manual.asm` — вариант для ручной сборки двумя командами NASM + одной ld.
- `main.asm` — вариант с include, макросами и условной DEBUG-сборкой.
- `Makefile` — сборка одной командой `make`.

## Ручная сборка
nasm -f elf64 main_manual.asm -o main_manual.o
nasm -f elf64 iolib.asm -o iolib.o
ld -o prog_manual main_manual.o iolib.o

## Сборка через Makefile
make
./prog

## DEBUG
make clean
make DEBUG=1
./prog

Без DEBUG отладочная строка не выводится. С DEBUG появляется:
DEBUG: value=<число>, address=<адрес>

## Проверка
echo 731 | ./prog
Ожидается:
731

echo 731 | ./prog_manual
Ожидается:
731
