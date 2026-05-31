ENTRY(kernel_entry)

SECTIONS {
    . = 0x10000;
    
    .text : {
        *(.text.entry)
        *(.text)
    }
    
    .rodata : {
        *(.rodata)
    }
    
    .data : {
        *(.data)
    }
    
    _bss_start = .;
    .bss : {
        *(.bss)
    }
    _bss_end = .;
}
