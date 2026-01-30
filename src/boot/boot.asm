bits 32

section .multiboot_header
align 8

header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + 0 + (header_end - header_start))
    
    dw 0
    dw 0
    dd 8
header_end:

section .text
global _start

_start:
    mov esp, stack_top
    
    mov edi, 0xb8000
    mov al, 'H'
    mov ah, 0x0F
    mov [edi], ax
.stop:
    hlt
    jmp .stop

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: