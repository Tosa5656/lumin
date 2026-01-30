ENTRY(header_start)

SECTIONS {
    . = 1M;

    .multiboot_header :
    {
        *(.multiboot_header)
    }
    
    .text :
    {
        *(.text)
    }

    .data :
    {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}