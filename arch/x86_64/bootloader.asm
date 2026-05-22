org 0x7c00
bits 16

_start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov ah, 0x02
    mov al, 26
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, 0x8000
    int 0x13
    jc disk_error

    cli
    lgdt [gdt32_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:init_pm

bits 32
init_pm:
    mov ax, DATA_SEG_32
    mov ds, ax
    mov ss, ax
    mov es, ax

    mov esi, 0x8000
    mov edi, 0x100000
    mov ecx, 3328
    rep movsd

    mov edi, 0x1000
    mov cr3, edi        
    xor eax, eax
    mov ecx, 4096
    rep stosd           

    mov dword [0x1000], 0x2003
    mov dword [0x2000], 0x3003
    mov dword [0x3000], 0x4003

    mov edi, 0x4000
    mov ebx, 0x00000003 
    mov ecx, 512        
.set_entry:
    mov [edi], ebx
    add edi, 8
    add ebx, 4096
    loop .set_entry

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

    lgdt [gdt64_descriptor]
    jmp CODE_SEG_64:0x100000

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp $

align 4
gdt32_start:
    dd 0x0, 0x0
gdt32_code:
    dw 0xffff, 0x0
    db 0x0, 10011010b, 11001111b, 0x0
gdt32_data:
    dw 0xffff, 0x0
    db 0x0, 10010010b, 11001111b, 0x0
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

CODE_SEG_32 equ gdt32_code - gdt32_start
DATA_SEG_32 equ gdt32_data - gdt32_start

align 4
gdt64_start:
    dd 0x0, 0x0         
gdt64_code:             
    dw 0x0, 0x0
    db 0x0, 10011010b, 00100000b, 0x0
gdt64_data:             
    dw 0x0, 0x0
    db 0x0, 10010010b, 00000000b, 0x0
gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

CODE_SEG_64 equ gdt64_code - gdt64_start

times 510 - ($-$$) db 0
dw 0xAA55