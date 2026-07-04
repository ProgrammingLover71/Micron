ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text : {
        *(.text.start)
        *(.text)
        *(.text*)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }
}