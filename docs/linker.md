# Linker 

Put the boot code at low physical memory, put the kernel at a higher-half virtual address,
and keep the physical layout and virtual layout at matching offsets.

## 1. Entry point : ENTRY(_start)
- When execution starts, begin at the _start symbol. _start is our assembly boot entry point.

## 2. Two important addresses

```ld
  KERNEL_VMA = 0XFFFFFFFF80000000;
  BOOT_LMA = 0X00100000;
```

BOOT_LMA = 0X00100000 = 1 MiB -> There is where the boot code is physically loaded.

```Physical RAM

0x00000000
    |
    | reserved/other stuff
    |
0x00100000  ← boot code starts
    |
    | boot code
    |
    ↓
```

KEREL_VMA = 0XFFFFFFFF80000000 -> This is where the kernel thinks it lives in virtual memory.

```
Physical RAM                 Virtual address

0x00100000                  0xFFFFFFFF80000000
     │                              │
     │                              │
     └────── kernel in RAM ─────────┘
             mapped by paging
```

---

## 3. PHDRS 
```ld 
PHDRS
{
    boot   PT_LOAD FLAGS(5);
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

```
This tells the linker to create fou ELF loadable segments. The numbers are the permissions.
4 = READ, 2 = WRITE, 1 = EXECUTE 

- boot flags(5) = 4 + 1 = READ + EXECUTE 
- text flags(5) = 4 + 1 = READ + EXECUTE 
- rodata flags(4) = READ ONLY 
- data flags(6) = 4 + 2 = READ + WRITE

## 4. Sections

```ld
SECTIONS
{
    . = BOOT_LMA;

```

This means start placing sections at physical address 0x100000(BOOT_LMA)
0x100000
   ↓
.multiboot
.boot.text
.boot.data
.boot.bss

## 5. Multiboot section 

```ld 
.multiboot :
{
    KEEP(*(.multiboot))
} :boot
```

This collects all sections named: .multiboot from the object files. Multiboot header goes here.
KEEP() means Don't remove this section even if the linker thinks nothing references it. 
:boot means Put this section inside the boot ELF segement.

## 6. Boot Code 

```ld 
.boot.text :
{
  *(.boot.text)
} :boot

```

This collects the early assembly code.

```.section .boot.text
_start:
    ...

    ```

So it placed immediately after the multiboot header.

```
BOOT_LMA = 1 MiB
     ↓
┌───────────────────┐
│ Multiboot header  │
├───────────────────┤
│ boot assembly     │
├───────────────────┤
│ boot data         │
├───────────────────┤
│ boot BSS          │
└───────────────────┘
```

## 7. Boot data 

```ld
.boot.data :
{
    . = ALIGN(4096);

    *(.boot.data)

    . = ALIGN(4096);
} :boot
```

Align(4096) means: Move the current address to the next 4 KiB boundary because of the paging structure are paging-sized/aligned
So boot data starts on a clean page boundary.

##8. Boot BSS 
```ld
.boot.bss (NOLOAD) :
{
    . = ALIGN(4096);

    boot_bss_start = .;

    *(.boot.bss)

    . = ALIGN(4096);

    boot_bss_end = .;
} :boot
```

BSS is memory  that needs to exist but doesn't need to be stored in the kernel file.
(NOLOAD) means Reserve the memory for this , but don't put all  the zero bytes into the executable file.


- boot_bss_start 
- boot_bss_end
Two symbol so that our code can know start and end of boot BSS

## 9. Finish boot area 

````
. = ALIGN(4096);

boot_load_end = .;


This moves to the next page boundary and remembers: This is where  the boot portion ends.

`````

kernel_load_start = ALIGN(boot_load_end, 0x200000);
Put the physical kernel image at the next 2 MiB boundary .
````

````RAM

1 MiB
 ↓
┌─────────────────────┐
│ Boot area           │
└─────────────────────┘

     unused/alignment

2 MiB
 ↓
┌─────────────────────┐
│ Kernel physical     │
│ image               │
└─────────────────────┘
````

## 10. Now switch to kernel virtual addresses

```ld
. = KERNEL_VMA;
```

Before this: . = physical addresses
Now: . = virtual addresses

So,

```
0xFFFFFFFF80000000
        ↓
kernel_start
```

## 11. Kernel .text 
````ld 
.text :
    AT(kernel_load_start + (ADDR(.text) - KERNEL_VMA))
{
    *(.text*)
} :text
````

There are two addresses:
- Virtual address : Where the kernel thinks the code is : 0xFFFFFFFF80000000
- Physical address : Where the kernel code actually exist in RAM : kernel_load_start 

AT(...) tells the linker 
"The section's virtual address is high, but its content are physically stored down here."

```
Virtual:

0xFFFFFFFF80000000
       │
       │ .text
       │
       ↓


Physical:

0x00200000
       │
       │ .text
       │
       ↓
```

Page tables will eventually connect these.

## 12. Higher-half layout 

```
kernel_load_start + (ADDR(.text) - KERNEL_VMA)
```
It Preserves the same offset. 

Physical                         Virtual

0x00200000                      0xFFFFFFFF80000000
     │                                │
     │ + 0x1000                       │ + 0x1000
     ↓                                ↓
0x00201000                      0xFFFFFFFF80001000


## 13. rodata 
```ld 
.rodata :
    AT(kernel_load_start + (ADDR(.rodata) - KERNEL_VMA))
{
    *(.rodata*)
    *(.eh_frame*)
} :rodata
```

This contains read-only data. 


## 14. .data 
```ld
.data :
    AT(kernel_load_start + (ADDR(.data) - KERNEL_VMA))
{
    *(.data*)
} :data

```
Initialize writeable variables.

## 15. .bss

```ld 
.bss (NOLOAD) :
    AT(kernel_load_start + (ADDR(.bss) - KERNEL_VMA))
{
    . = ALIGN(4096);

    *(COMMON)
    *(.bss*)

    . = ALIGN(4096);
} :data
```

This is uintialized global/static memory. 

## 16. Kernel end 

````ld 
kernel_end = .;

`````
This is the virtual address where the kernel ends.

```ld
kernel_load_end =
    kernel_load_start + (kernel_end - KERNEL_VMA);

```

This converts the virtual kernel size into its corresponding physical/load end.

```
Virtual kernel
────────────────────────────────────
KERNEL_VMA                         kernel_end
    │                                  │
    └────────── kernel size ───────────┘


Physical kernel
────────────────────────────────────
kernel_load_start              kernel_load_end
    │                                  │
    └──────── same size ───────────────┘

```

## 17. Discard unnecessary sections
```ld
/DISCARD/ :
{
    *(.comment)
    *(.note*)
}

```

This tells the linker: Throw these sections away. Things such as compiler comments/build metadata 
that aren't needed by our kernel.

---

```
PHYSICAL RAM
──────────────────────────────────────────────

0x00100000
    │
    ├── Multiboot header
    ├── Boot code
    ├── Boot data
    ├── Boot BSS
    │
    └── boot_load_end
             │
             │ align to 2 MiB
             ↓
0x00200000
    │
    ├── kernel .text       R-X
    ├── kernel .rodata     R--
    ├── kernel .data       RW-
    └── kernel .bss        RW-
             │
             ↓
       kernel_load_end

```

At the same time, the kernel sees itself as: 
```

VIRTUAL ADDRESS SPACE
──────────────────────────────────────────────

0xFFFFFFFF80000000
    │
    ├── .text       R-X
    ├── .rodata     R--
    ├── .data       RW-
    └── .bss        RW-
             │
             ↓
         kernel_end
```

And the paging eventually  creates the relationship:
```

Virtual                         Physical

0xFFFFFFFF80000000  ─────────→  0x00200000
0xFFFFFFFF80001000  ─────────→  0x00201000
0xFFFFFFFF80002000  ─────────→  0x00202000
       ...                           ...

```


