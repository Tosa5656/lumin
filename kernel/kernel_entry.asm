bits 64

global _start
extern kmain

_start:
    mov rsp, 0x90000
    mov rbp, rsp

    call kmain
.hang:
    cli
    hlt
    jmp .hang