%include "iolib.inc"

global _start

section .bss
    value resq 1

section .text
_start:
    READ_DEC r12
    mov [value], r12

%ifdef DEBUG
    mov rdi, [value]
    lea rsi, [rel value]
    call debug_value
%endif

    PRINT_DEC r12

    mov eax, 1
    mov edi, 1
    lea rsi, [rel newline]
    mov edx, 1
    syscall

    mov eax, 60             ; SYS_exit
    xor edi, edi
    syscall

section .rodata
    newline db 10
