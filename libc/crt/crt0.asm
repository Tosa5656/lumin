bits 64
section .text

global _start
extern main
extern exit

_start:
    xor rdi, rdi
    xor rsi, rsi
    call main
    mov rdi, rax
    call exit

section .note.GNU-stack noalloc noexec nowrite progbits