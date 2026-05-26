bits 64

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    mov rsp, 0x90000
    mov rbp, rsp

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    rep stosb

    call kmain
.hang:
    cli
    hlt
    jmp .hang