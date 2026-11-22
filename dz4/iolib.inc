extern print_dec
extern read_dec

%macro PRINT_DEC 1
    mov rax, %1
    call print_dec
%endmacro

%macro READ_DEC 1
    call read_dec
    mov %1, rax
%endmacro
