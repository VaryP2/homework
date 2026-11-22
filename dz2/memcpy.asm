global asm_memcpy
section .text
asm_memcpy:
    mov     rax, rdi        
    mov     rcx, rdx        
    cld                    
    rep     movsb           
    ret
