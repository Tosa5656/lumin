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

    call setup_paging
    
    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64.pointer]

    jmp 0x08:long_mode_entry

setup_paging:
    mov eax, pdpt_table
    or eax, 3 
    mov [pml4_table], eax

    mov eax, pdt_table
    or eax, 3
    mov [pdpt_table], eax

    mov eax, 0x000000 | 0x83 
    mov [pdt_table], eax
    ret

bits 64

extern kernel_entry

long_mode_entry:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax

    mov rsp, stack_top
    
    mov rdi, rsi
    mov rsi, rdi 

    call kernel_entry

.stop:
    hlt
    jmp .stop

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pdt_table:
    resb 4096
stack_bottom:
    resb 16384
stack_top:
