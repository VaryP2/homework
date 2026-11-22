; System V AMD64 ABI, NASM, Linux
; void *asm_memcpy(void *dest, const void *src, size_t n)
; rdi=dest, rsi=src, rdx=n, return value in rax

global asm_memcpy
section .text
asm_memcpy:
    mov     rax, rdi        ; return original destination
    mov     rcx, rdx        ; REP count
    cld                     ; ensure forward direction
    rep     movsb           ; copy n bytes: [rsi] -> [rdi]
    ret
