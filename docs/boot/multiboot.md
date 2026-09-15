# Multiboot

## 1. Overview

Multiboot is a boot protocol that allows a bootloader such as GRUB to load
our kernel and transfer control to it.

We are using **Multiboot 1**.

The basic flow is:

```text
GRUB
 ↓
Find Multiboot header
 ↓ 
Load kernel
 ↓
Transfer control to _start
```
---

## 2. Multiboot Header

GRUB needs to find a Multiboot header inside the kernel.

Our header is:

```asm

.section .multiboot, "a"
.align 4

.long 0x1BADB002
.long 0
.long -(0x1BADB002)
```

 ### Multiboot Section
```asm
.section .multiboot, "a"
.align 4
```

```asm
.section .multiboot, "a"
```

Creates a section named `.multiboot`.

We put the Multiboot header in its own section so that it can be easily
identified and kept in the kernel image.

The `"a"` flag means **allocatable**.

It tells the assembler/linker that this section should be part of the
loaded kernel image.

---

### `.align 4`

```asm
.align 4
```

Aligns the next data to a 4-byte boundary.

The Multiboot 1 specification requires the Multiboot header to be
**32-bit aligned**.

So the header starts at an address divisible by 4.

```text
Address
   ↓
0x....00  ← aligned
0x....04
0x....08
0x....0C
```

Then the Multiboot header is placed there:

```asm
.long 0x1BADB002
.long 0
.long -(0x1BADB002)
```

The complete structure is:

```asm
.section .multiboot, "a"
.align 4

.long 0x1BADB002
.long 0
.long -(0x1BADB002)
```

So:

```text
.multiboot section
        ↓
   4-byte aligned
        ↓
   Multiboot header
        ↓
 ┌───────────────┐
 │ Magic         │
 │ Flags         │
 │ Checksum      │
 └───────────────┘
```


### Magic Number 

```asm
.long 0x1BADB002
```

`0x1BADB002` is the Multiboot 1 magic number.

GRUB searches for this value in the kernel image to determine whether
the kernel is Multiboot-compatile.

### Flags

```asm
.long 0
```
The flags specify which  information the kernel requests from GRUB.
For our current  setup, we don't request any additional information.

### Checksum

```asm
.long -(0x1BADB002)
```
The Multiboot header requires:

```text
magic + flags + checksum = 0
```
since our flags are `0`:

```text
0x1BADB002 + 0 + (-0x1BADB002) = 0
```
---

## 3. GRUB Configuration

Our GRUB configuration contains:

```cfg
menuentry "My OS" {
    multiboot /boot/kernel.elf
    boot
}
```

The important part is:

```cfg
multiboot /boot/kernel.elf
```

This tells GRUB to load `kernel.elf` using the Multiboot 1 protocol.

---

## 4. Kernel Entry

GRUB transfers control to our `_start` symbol:

```asm
.global _start

_start:
    cli
```

At this point, our assembly startup code takes over.

GRUB starts us in **32-bit protected mode**.

From here, `boot.S` is responsible for preparing the CPU before entering
our 64-bit kernel.

---

## 5. Transition to 64-bit Mode

The startup code performs the required CPU setup:

```text
32-bit protected mode
        ↓
    Load GDT
        ↓
   Build page tables
        ↓
    Enable PAE
        ↓
 Enable Long Mode
        ↓
  Enable Paging
        ↓
  Far jump to 64-bit
        ↓
  64-bit long mode
```

The details of these components are documented separately:

- [GDT](../cpu/gdt.md)
- [Paging](../cpu/paging.md)

--- 

## 6. Entering the C Kernel

After entering 64-bit mode, we set up the stack and call:

```asm
lea rsp, stack_top

call kernel_main
```

The final transition is:

```text
GRUB
 ↓
_start
 ↓
32-bit setup
 ↓
64-bit long mode
 ↓
kernel_main()
```

---

## 7. What We Achieved

At this stage:

- GRUB recognizes our kernel as a Multiboot 1 kernel.
- GRUB successfully loads `kernel.elf`.
- GRUB transfers control to `_start`.
- Our assembly startup code takes control from GRUB.
- The CPU is transitioned from 32-bit protected mode to 64-bit long mode.
- Control eventually reaches `kernel_main()`.
