org 0x8000
bits 16

STAGE2_SIZE equ 4096

; ============================================================
; Entry — real mode, jumped from stage1 at 0x7C00
; ============================================================
_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7BFF

    call vbe_init

    cli
    lgdt [gdt32_desc]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:init_pm

; ============================================================
; VBE initialization — real mode, 16-bit
; ============================================================
vbe_init:
    push es
    push ds
    pushad

    ; Get VBE Controller Info
    mov ax, 0x4F00
    mov di, 0x5100
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe

    cmp dword [0x5100], 0x41534556
    jne .no_vbe

    cmp word [0x5104], 0x0200
    jb  .no_vbe

    ; VideoModePtr at VBEInfo+0x0E (offset), +0x10 (segment)
    mov ax, [0x5110]
    mov fs, ax
    mov si, [0x510E]

.next_mode:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je  .no_vbe

    push cx
    mov ax, 0x4F01
    mov di, 0x5300
    int 0x10
    pop cx
    cmp ax, 0x004F
    jne .skip

    ; ModeAttributes[0] bit 7 = LFB
    test word [0x5300], 0x0080
    jz   .skip

    cmp word [0x5312], 1024
    jne .skip
    cmp word [0x5314], 768
    jne .skip
    cmp byte [0x5319], 32
    jne .skip

    ; Set mode with LFB bit (bit 14)
    mov ax, 0x4F02
    or  cx, 0x4000
    mov bx, cx
    int 0x10
    cmp ax, 0x004F
    jne .skip

    ; Store fb_info at 0x7E00 (below stage2, above stack)
    mov eax, [0x5328]
    mov [0x7E00], eax        ; phys_addr
    movzx eax, word [0x5312]
    mov [0x7E08], eax        ; width
    movzx eax, word [0x5314]
    mov [0x7E0C], eax        ; height
    movzx eax, byte [0x5319]
    mov [0x7E10], eax        ; bpp
    movzx eax, word [0x5310]
    mov [0x7E14], eax        ; pitch

    popad
    pop ds
    pop es
    ret

.skip:
    add si, 2
    jmp .next_mode

.no_vbe:
    xor eax, eax
    mov [0x7E00], eax
    mov [0x7E08], eax
    mov [0x7E0C], eax
    mov [0x7E10], eax
    mov [0x7E14], eax
    popad
    pop ds
    pop es
    ret

; ============================================================
; Protected mode — 32-bit
; ============================================================
bits 32
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov esp, 0x90000

    ; Copy kernel from after stage2 to 0x100000
    mov esi, 0x8000 + STAGE2_SIZE
    mov edi, 0x100000
    mov ecx, (200 * 512 - STAGE2_SIZE) / 4
    rep movsd

    ; Page tables at 0x1000
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 4096
    rep stosd

    mov dword [0x1000], 0x2003   ; PML4[0] -> PDPT at 0x2000
    mov dword [0x2000], 0x3003   ; PDPT[0] -> PD at 0x3000
    mov dword [0x3000], 0x4003   ; PD[0] -> PT at 0x4000

    ; Identity map 0-2MB
    mov edi, 0x4000
    mov ebx, 0x00000003
    mov ecx, 512
.pt0:
    mov [edi], ebx
    add edi, 8
    add ebx, 0x1000
    loop .pt0

    ; Identity map 2-4MB
    mov dword [0x3008], 0x5003
    mov edi, 0x5000
    mov ebx, 0x00200003
    xor eax, eax
    mov ecx, 512
.pt1:
    mov [edi], ebx
    mov [edi+4], eax
    add edi, 8
    add ebx, 0x1000
    loop .pt1

    ; 2MB huge pages for remaining RAM from E820
    movzx ecx, word [0x0FFC]
    test ecx, ecx
    jz .no_huge
    mov esi, 0x6000
    xor ebx, ebx
.find_max:
    cmp dword [esi + 16], 1
    jne .skip_ent
    mov eax, [esi]
    add eax, [esi + 8]
    cmp eax, ebx
    jbe .skip_ent
    mov ebx, eax
.skip_ent:
    add esi, 20
    dec ecx
    jnz .find_max
    add ebx, 0x1FFFFF
    and ebx, 0xFFE00000
    shr ebx, 21
    cmp ebx, 2
    jle .no_huge
    mov edi, 0x3010
    mov eax, 0x00400083
    mov ecx, ebx
    sub ecx, 2
.huge:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .huge
.no_huge:

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Set EFER.LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Switch to long mode
    lgdt [gdt64_desc]
    jmp 0x08:0x100000

; ============================================================
; GDT — 32-bit protected mode
; ============================================================
align 8
gdt32_start:
    dq 0x0
gdt32_code:
    dw 0xFFFF, 0x0
    db 0x0, 0x9A, 0xCF, 0x0
gdt32_data:
    dw 0xFFFF, 0x0
    db 0x0, 0x92, 0xCF, 0x0
gdt32_end:

gdt32_desc:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

; ============================================================
; GDT — 64-bit long mode
; ============================================================
align 8
gdt64_start:
    dq 0x0
gdt64_code:
    dw 0x0, 0x0
    db 0x0, 0x9A, 0x20, 0x0
gdt64_end:

gdt64_desc:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

times STAGE2_SIZE - ($ - $$) db 0
