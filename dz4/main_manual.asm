extern read_dec
extern print_dec

global _start

section .text
_start:
    call read_dec
    call print_dec

    mov eax, 1
    mov edi, 1
    lea rsi, [rel newline]
    mov edx, 1
    syscall

    mov eax, 60
    xor edi, edi
    syscall

section .rodata
    newline db 10
