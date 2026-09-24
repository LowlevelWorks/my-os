ENTRY(_start)

SECTIONS 
{
  . = 1M;

  kernel_start = .;

  .multiboot :
  {
   KEEP(*(.multiboot))
  }

  .text : 
  {
   *(.text)
  }

  .rodata : 
  {
   *(.rodata)
  }

  .data : 
  {
   *(.data)
  }

  .bss : 
    {
    . = ALIGN(4096);

    *(COMMON)
    *(.bss)
   }

   . = ALIGN(4096);

   kernel_end = .;
}
