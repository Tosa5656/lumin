bits 64
section .text

global _start
extern main
extern exit

_start:
    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    call main
    mov rdi, rax
    call exit

section .note.GNU-stack noalloc noexec nowrite progbits