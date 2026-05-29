bits 64
section .text

global _start
extern main
extern exit
extern __bss_start
extern _end

_start:
    ; zero BSS
    mov rdi, __bss_start
    mov rcx, _end
    sub rcx, rdi
    xor al, al
    cld
    rep stosb

    and rsp, -16
    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    call main
    mov rdi, rax
    call exit

section .note.GNU-stack noalloc noexec nowrite progbits