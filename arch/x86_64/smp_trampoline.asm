[bits 16]
[org 0x8000]

start:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000

    lgdt [gdt32_ptr]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:pm_start

[bits 32]
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x8000

    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    mov eax, 0x90000
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    jmp 0x08:lm_start

[bits 64]
lm_start:
    mov rsp, 0xFFFF800000008000

    mov rax, cr0
    and ax, 0xFFFB
    mov cr0, rax

    mov rax, startup64_end - startup64
    mov rbx, startup64
    mov rcx, 0xFFFF800000000000
    add rbx, rcx

    mov rsi, 0xFFFF800000000000
    mov rdi, 0xFFFF800000080000
    rep movsb

    mov rax, 0xFFFF800000080000
    jmp rax

startup64:
    mov rsp, 0xFFFF800000080000
    call smp_ap_main
    hlt

startup64_end:

[bits 32]
align 8
gdt32:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt32_ptr:
    dw gdt32_ptr - gdt32 - 1
    dd gdt32
