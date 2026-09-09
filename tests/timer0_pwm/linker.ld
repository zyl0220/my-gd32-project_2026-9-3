ENTRY(test_entry)

MEMORY
{
    TEST (rwx) : ORIGIN = 0x00010000, LENGTH = 512K
}

SECTIONS
{
    .text :
    {
        *(.text.test_entry)
        *(.text*)
        *(.rodata*)
    } > TEST

    .data :
    {
        *(.data*)
    } > TEST

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    } > TEST
}
