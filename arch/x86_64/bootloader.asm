org 0x7c00
bits 16

_start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; Check LBA extensions
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc disk_error

    ; Load stage2 + kernel (216 sectors from LBA 1) to 0x8000
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov si, dap2
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; Get E820 memory map
    mov di, 0x6000
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.e820_loop:
    mov eax, 0xE820
    mov ecx, 20
    int 0x15
    jc .e820_done
    inc bp
    add di, 20
    test ebx, ebx
    jnz .e820_loop
.e820_done:
    mov [0x0FFC], bp

    ; Jump to stage2 at 0x8000
    jmp 0:0x8000

align 4
dap:
    db 0x10
    db 0
    dw 127
    dw 0x0000
    dw 0x0800
    dd 1
    dd 0
dap2:
    db 0x10
    db 0
    dw 89
    dw 0xFE00
    dw 0x0800
    dd 128
    dd 0
disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp $

times 510 - ($-$$) db 0
dw 0xAA55
