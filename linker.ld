ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(16)
    {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(0x1000);

    .data : ALIGN(16)
    {
        *(.data .data.*)
        KEEP(*(.limine_reqs_start))
        KEEP(*(.limine_reqs))
        KEEP(*(.limine_reqs_end))
    } :data

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
    }
}