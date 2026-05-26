bits 64
section .text

extern irq_handler
extern exception_handler

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

%macro exc_noerr 1
    global exc%1_handler
    exc%1_handler:
        push 0
        pushaq
        mov rdi, %1
        mov rsi, rsp
        call exception_handler
        popaq
        add rsp, 8
        iretq
%endmacro

%macro exc_err 1
    global exc%1_handler
    exc%1_handler:
        pushaq
        mov rdi, %1
        mov rsi, rsp
        call exception_handler
        popaq
        add rsp, 8
        iretq
%endmacro

exc_noerr 0
exc_noerr 1
exc_noerr 2
exc_noerr 3
exc_noerr 4
exc_noerr 5
exc_noerr 6
exc_noerr 7
exc_err   8
exc_noerr 9
exc_err   10
exc_err   11
exc_err   12
exc_err   13
exc_err   14
exc_noerr 15
exc_noerr 16
exc_err   17
exc_noerr 18
exc_noerr 19

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

extern syscall_entry

global isr48_handler
isr48_handler:
    pushaq
    mov rdi, rsp
    call syscall_entry
    mov [rsp + 112], rax
    popaq
    iretq