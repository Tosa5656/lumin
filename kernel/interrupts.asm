bits 64
section .text

extern irq_handler

%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

global isr32_handler
isr32_handler:
    pushaq
    mov rdi, 32
    call irq_handler
    popaq
    iretq

global isr33_handler
isr33_handler:
    pushaq
    mov rdi, 33
    call irq_handler
    popaq
    iretq

global isr48_handler
isr48_handler:
    pushaq
    mov rdi, 48
    call irq_handler
    popaq
    iretq
