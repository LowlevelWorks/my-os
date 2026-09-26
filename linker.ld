ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;
BOOT_LMA   = 0x00100000;

PHDRS
{
    boot   PT_LOAD FLAGS(5);
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = BOOT_LMA;

    boot_load_start = .;

    .multiboot :
    {
        KEEP(*(.multiboot))
    } :boot

    .boot.text :
    {
        *(.boot.text)
    } :boot

    .boot.bss (NOLOAD) :
    {
        . = ALIGN(4096);
        *(.boot.bss)
    } :boot

    . = ALIGN(4096);

    boot_load_end = .;

    kernel_load_start = ALIGN(boot_load_end, 0x200000);

    . = KERNEL_VMA;

    kernel_start = .;

    .text :
        AT(kernel_load_start + (ADDR(.text) - KERNEL_VMA))
    {
        *(.text*)
    } :text

    .rodata :
        AT(kernel_load_start + (ADDR(.rodata) - KERNEL_VMA))
    {
        *(.rodata*)
        *(.eh_frame*)
    } :rodata

    .data :
        AT(kernel_load_start + (ADDR(.data) - KERNEL_VMA))
    {
        *(.data*)
    } :data

    .bss :
        AT(kernel_load_start + (ADDR(.bss) - KERNEL_VMA))
    {
        . = ALIGN(4096);
        *(COMMON)
        *(.bss*)
    } :data

    . = ALIGN(4096);

    kernel_end = .;

    kernel_load_end =
        kernel_load_start + (kernel_end - KERNEL_VMA);
}
