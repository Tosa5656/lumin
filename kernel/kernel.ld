SECTIONS
{
    . = 0x100000;
    .text : { *(.text*) }
    .data : { *(.data*) *(.rodata*) }
    .initcall : {
        __start_initcall = .;
        *(.initcall)
        __end_initcall = .;
    }
    .percpu : {
        __percpu_start = .;
        *(.percpu)
        __percpu_end = .;
    }
    .bss  : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }
    /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}