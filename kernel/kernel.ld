SECTIONS
{
    . = 0x100000;
    .text : { *(.text*) }
    .data : { *(.data*) *(.rodata*) }
    .bss  : { *(.bss*) }
    /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}