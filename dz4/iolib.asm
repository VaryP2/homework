global print_dec
global read_dec
global debug_value

section .bss
    buf resb 32
    input_buf resb 32

section .rodata
    dbg_prefix db "DEBUG: value=", 0
    dbg_addr db ", address=", 0
    nl db 10

section .text

print_dec:
    push rbx
    push r12

    lea rsi, [buf + 31]
    mov byte [rsi], 0

    mov ebx, 10

.convert:
    xor edx, edx
    div rbx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test rax, rax
    jnz .convert

    lea rdx, [buf + 31]
    sub rdx, rsi
    mov eax, 1             
    mov edi, 1             
    syscall

    pop r12
    pop rbx
    ret

read_dec:
    xor eax, eax
    xor edi, edi           
    lea rsi, [input_buf]
    mov edx, 31
    syscall                 

    test rax, rax
    jle .zero

    mov r8, rax            
    xor eax, eax            
    xor ecx, ecx            

.parse:
    cmp rcx, r8
    jae .done

    movzx edx, byte [input_buf + rcx]
    cmp dl, '0'
    jb .done
    cmp dl, '9'
    ja .done

    imul rax, rax, 10
    sub edx, '0'
    add rax, rdx

    inc rcx
    jmp .parse

.zero:
    xor eax, eax
.done:
    ret

debug_value:
    push r12
    push r13

    mov r12, rdi           
    mov r13, rsi           

    ; prefix
    mov eax, 1
    mov edi, 1
    lea rsi, [rel dbg_prefix]
    mov edx, 13            
    syscall

    mov rax, r12
    call print_dec

    mov eax, 1
    mov edi, 1
    lea rsi, [rel dbg_addr]
    mov edx, 10    
    syscall

    mov rax, r13
    call print_dec

    mov eax, 1
    mov edi, 1
    lea rsi, [rel nl]
    mov edx, 1
    syscall

    pop r13
    pop r12
    ret
