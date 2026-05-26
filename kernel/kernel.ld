SECTIONS
{
    . = 0x100000;
    .text : { *(.text*) }
    .data : { *(.data*) *(.rodata*) }
    .bss  : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }
    /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}