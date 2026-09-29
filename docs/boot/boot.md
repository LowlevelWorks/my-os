# boot.S

`boot.S` is responsible for taking the CPU from the state provided by GRUB
into 64-bit long mode with paging enabled, then transferring control to
`kernel_main()`.

The important jobs are:

```text
GRUB
 │
 ▼
32-bit protected mode
 │
 ├── save Multiboot information
 ├── load GDT
 ├── prepare page tables
 ├── map the kernel
 ├── load CR3
 ├── enable PAE
 ├── enable Long Mode
 ├── enable Paging
 │
 ▼
64-bit long mode
 │
 ├── setup stack
 └── jump to kernel_main()
```

---

# 1. Small Linker Background

The linker combines object files, resolves symbols, assigns addresses, and
produces the final kernel image.

For our boot code, the important thing is that the linker gives addresses to
symbols such as:

```text
pml4_table
pdpt_table
pd_table
high_pdpt_table
high_pd_table
kernel_load_start
kernel_load_end
kernel_start
kernel_end
```

The linker script also decides where sections are placed:

```text
.boot.text
.boot.data
.boot.bss
.text
.rodata
.data
.bss
```

For example:

```text
.boot.text
    → boot code

.boot.data
    → boot page-table data

.boot.bss
    → boot page tables, stack, IDT, etc.

.text
    → normal kernel code

.data
    → initialized writable data

.bss
    → zero-initialized data
```

The important point for `boot.S` is:

```text
The linker gives our symbols their addresses.
boot.S uses those addresses to build the initial memory layout.
```

---

# 2. GDT

Before discussing paging, we first prepare the Global Descriptor Table.

```asm
lgdt gdt_descriptor
```

`lgdt` means:

```text
Load Global Descriptor Table register
```

The CPU has a special register called:

```text
GDTR
```

The GDTR tells the CPU:

```text
Where is my Global Descriptor Table?
How large is it?
```

Our GDT contains:

```text
---------------------------
Entry 0: NULL
Entry 1: 64-bit CODE
Entry 2: DATA
---------------------------
```

Each `.quad` is 8 bytes.

Therefore:

```text
Entry 0 = 8 bytes
Entry 1 = 8 bytes
Entry 2 = 8 bytes
```

---

# 3. GDT Entry 0

```asm
.quad 0x0000000000000000
```

This is the NULL descriptor.

x86 requires GDT index 0 to be a null descriptor.

We do not use this entry for normal code or data.

```text
GDT[0]
  │
  └── NULL
```

---

# 4. GDT Entry 1

```asm
.quad 0x00AF9A000000FFFF
```

This describes our code segment.

Conceptually:

```text
GDT[1]
  │
  └── 64-bit CODE
```

Each entry is 8 bytes:

```text
1 × 8 = 8 = 0x08
```

Therefore:

```text
0x08 → GDT entry #1
```

This is why we later use:

```asm
jmp 0x08:long_mode
```

The `0x08` selects GDT entry 1 as the code segment.

---

# 5. GDT Entry 2

```asm
.quad 0x00AF92000000FFFF
```

This is our data segment.

Conceptually:

```text
GDT[2]
  │
  └── DATA
```

Its selector is:

```text
2 × 8 = 16
16 = 0x10
```

Therefore:

```text
0x10 → GDT entry #2
```

Later:

```asm
mov ax, 0x10

mov ds, ax
mov es, ax
mov ss, ax
```

selects the data segment.

---

# 6. GDT Descriptor

```asm
gdt_descriptor:
    .word gdt_descriptor - gdt - 1
    .quad gdt
```

The CPU needs two pieces of information:

```text
1. How large is the GDT?
2. Where is the GDT?
```

## GDT Limit

```asm
.word gdt_descriptor - gdt - 1
```

The first value is the GDT limit.

The important part is:

```text
size - 1
```

Why `-1`?

The CPU interprets the limit as the offset of the last valid byte,
not as the number of bytes.

For example:

```text
GDT size = 24 bytes

limit = 24 - 1
      = 23
```

So the CPU understands:

```text
valid bytes = 0 ... 23
```

---

# 7. GDT Base

```asm
.quad gdt
```

This stores the address of the GDT.

So the descriptor contains:

```text
GDT descriptor
┌─────────────────────────────┐
│ LIMIT │       BASE          │
└─────────────────────────────┘
```

Then:

```asm
lgdt gdt_descriptor
```

loads this information into the CPU's GDTR register.

---

# 8. Paging

Paging is the most important part of `boot.S`.

Paging allows the CPU to translate:

```text
Virtual Address
       │
       ▼
   Page Tables
       │
       ▼
Physical Address
```

Before paging:

```text
CPU
 │
 └── uses physical addresses directly
```

After paging:

```text
CPU
 │
 └── uses virtual addresses
          │
          ▼
       page tables
          │
          ▼
       physical RAM
```

Our kernel needs paging because it is linked to a high virtual address:

```text
KERNEL_VMA = 0xFFFFFFFF80000000
```

---

# 9. x86-64 Page-Table Hierarchy

Our x86-64 paging hierarchy has four levels:

```text
PML4
 │
 ▼
PDPT
 │
 ▼
PD
 │
 ▼
PT
 │
 ▼
Physical Page
```

For a normal 4 KiB page:

```text
Virtual Address

┌────────┬────────┬────────┬────────┬────────────┐
│ PML4   │ PDPT   │ PD     │ PT     │ Offset     │
│ 9 bits │ 9 bits │ 9 bits │ 9 bits │ 12 bits    │
└────────┴────────┴────────┴────────┴────────────┘
```

The four indexes select:

```text
PML4 → PDPT → PD → PT
```

The final 12 bits select the byte inside the 4 KiB page.

---

# 10. Page-Table Entry

A page-table entry contains two important things:

```text
Physical address
+
Flags
```

Conceptually:

```text
┌───────────────────────────────┬──────────┐
│      physical address         │  flags   │
└───────────────────────────────┴──────────┘
```

For example:

```text
0x2003
```

can be understood as:

```text
0x2000 → physical address
0x0003 → flags
```

where:

```text
bit 0 = PRESENT
bit 1 = WRITABLE
```

---

# 11. Building the First Page-Table Entry

Conceptually, we can build:

```asm
lea eax, pml4_table
lea ebx, pdpt_table
or  ebx, 0x03
mov [eax], ebx
```

---

# 12. `lea eax, pml4_table`

```asm
lea eax, pml4_table
```

`LEA` means:

```text
Load Effective Address
```

It puts the address of `pml4_table` into `EAX`.

For example:

```text
pml4_table = 0x1000

EAX = 0x1000
```

It does NOT load the contents of the PML4.

It loads its address.

---

# 13. `lea ebx, pdpt_table`

```asm
lea ebx, pdpt_table
```

This puts the address of the PDPT into `EBX`.

For example:

```text
pdpt_table = 0x2000

EBX = 0x2000
```

---

# 14. Add Page-Table Flags

```asm
or ebx, 0x03
```

`0x03` is:

```text
00000011
```

The lower two bits are:

```text
bit 0 → PRESENT
bit 1 → WRITABLE
```

So:

```text
0x03

00000011
││
│└── Writable = 1
└─── Present  = 1
```

We are setting:

```text
bit 0 = 1 → Present
bit 1 = 1 → Writable
```

Therefore:

```text
EBX = address of PDPT + flags
```

Conceptually:

```text
┌───────────────────────────────┬─────┐
│      address of PDPT          │ P W │
└───────────────────────────────┴─────┘
                                  1 1
```

---

# 15. Store the PML4 Entry

```asm
mov [eax], ebx
```

This means:

```text
Store EBX into the memory address contained in EAX.
```

Example:

```text
EAX = 0x1000
EBX = 0x2003
```

Before:

```text
Memory:

0x1000 → ????????
0x1008 → ????????
```

After:

```text
Memory:

0x1000 → 0x2003
0x1008 → ????????
```

If:

```text
pml4_table = 0x1000
pdpt_table = 0x2000
```

then:

```text
PML4[0] = 0x2003
```

---

# 16. Present Bit

The Present bit tells the CPU whether this page-table entry is valid.

```text
bit 0 = PRESENT
```

If:

```text
Present = 1
```

the entry is present.

If:

```text
Present = 0
```

the entry is not present.

If the CPU needs to follow an entry and finds:

```text
Present = 0
```

the CPU generates a page fault.

So:

```text
P = 1
    ↓
CPU can use this entry
```

---

# 17. Writable Bit

The Writable bit controls whether writes are allowed through the entry.

```text
bit 1 = WRITABLE
```

Therefore:

```text
Writable = 1
    → writing allowed

Writable = 0
    → writing not allowed
```

So:

```text
0x03
```

means:

```text
Present + Writable
```

---

# 18. Building the Next Level

The same idea is used for the next page-table level.

We create:

```text
PML4[0]
   │
   ▼
PDPT
```

Then:

```text
PDPT[0]
   │
   ▼
PD
```

So the hierarchy becomes:

```text
PML4[0]
   │
   ▼
PDPT[0]
   │
   ▼
PD[0]
```

The important idea is:

```text
Each entry points to the next page-table level.
```

---

# 19. Identity Mapping

Our boot page tables contain an identity mapping.

```text
Virtual address
      │
      │ same
      ▼
Physical address
```

For example:

```text
Virtual 0x00100000
       │
       ▼
Physical 0x00100000
```

This is useful during the transition because the CPU is initially executing
boot code from low physical addresses.

---

# 20. 2 MiB Huge Page

Our identity mapping uses a Page Directory entry:

```asm
.quad 0x0000000000000083
```

The important flags are:

```text
0x01 → PRESENT
0x02 → WRITABLE
0x80 → HUGE
```

The `HUGE` bit at the PD level means:

```text
Do not continue to a Page Table.

This PD entry directly maps a 2 MiB page.
```

So:

```text
PML4
  ↓
PDPT
  ↓
PD[0]
  ↓
2 MiB physical memory
```

instead of:

```text
PML4
  ↓
PDPT
  ↓
PD
  ↓
PT
  ↓
4 KiB page
```

---

# 21. Page Sizes

With x86-64 paging we can have:

```text
4 KiB
2 MiB
1 GiB
```

The level determines the size when the HUGE bit is used.

```text
PDPT + HUGE
    ↓
1 GiB page

PD + HUGE
    ↓
2 MiB page

PT
    ↓
4 KiB page
```

Our boot code uses:

```text
2 MiB pages
```

for the kernel mapping.

Our direct physical map can later use:

```text
1 GiB
2 MiB
4 KiB
```

depending on alignment and CPU support.

---

# 22. Initial Page-Table Layout

Our boot page tables are:

```text
PML4
 │
 ├── [0]
 │     │
 │     ▼
 │    PDPT
 │     │
 │     ▼
 │     PD
 │     │
 │     └── [0] → 2 MiB identity mapping
 │
 ├── [256]
 │     │
 │     ▼
 │   phys_map_pdpt
 │     │
 │     └── [0] → 1 GiB physical direct map
 │
 └── [511]
       │
       ▼
    high_pdpt
       │
       └── [510]
              │
              ▼
           high_pd
              │
              └── kernel mapping
```

This is the foundation used when paging is first enabled.

---

# 23. PML4[0] — Identity Mapping

```asm
.quad pdpt_table + 0x03
```

This creates:

```text
PML4[0]
   ↓
PDPT
```

Then:

```asm
.quad pd_table + 0x03
```

creates:

```text
PDPT[0]
   ↓
PD
```

Then:

```asm
.quad 0x0000000000000083
```

creates:

```text
PD[0]
   ↓
physical 0x00000000
```

Because this is a 2 MiB huge page:

```text
virtual 0x00000000
        ↓
physical 0x00000000
```

for:

```text
0x00000000 → 0x001FFFFF
```

---

# 24. PML4[256] — Physical Direct Map

```asm
.quad phys_map_pdpt_table + 0x03
```

creates:

```text
PML4[256]
    ↓
phys_map_pdpt_table
```

The first entry is:

```asm
.quad 0x0000000000000083
```

Because `HUGE` is set at the PDPT level, this maps:

```text
1 GiB
```

Therefore initially:

```text
physical 0 → 1 GiB
```

is also accessible through the physical direct-map region.

The virtual base is:

```c
#define PHYS_MAP_BASE 0xFFFF800000000000ULL
```

The general relationship is:

```text
virtual = PHYS_MAP_BASE + physical
```

For example:

```text
physical:
0x00200000

virtual:
0xFFFF800000200000
```

Later `paging.c` extends this mapping beyond the first 1 GiB.

---

# 25. PML4[511] — High-Half Kernel

```asm
.quad high_pdpt_table + 0x03
```

creates:

```text
PML4[511]
    ↓
high_pdpt_table
```

Then:

```asm
.quad high_pd_table + 0x03
```

at PDPT index 510 creates:

```text
PDPT[510]
    ↓
high_pd_table
```

This is the beginning of our kernel's high-half virtual address:

```text
KERNEL_VMA = 0xFFFFFFFF80000000
```

The structure is:

```text
PML4[511]
    ↓
PDPT[510]
    ↓
PD[0]
    ↓
kernel physical memory
```

---

# 26. Why `high_pdpt_table[510]`?

The x86-64 virtual address is divided into indexes.

For:

```text
0xFFFFFFFF80000000
```

the important indexes are:

```text
PML4 index = 511
PDPT index = 510
```

Therefore:

```text
PML4[511]
    ↓
PDPT[510]
```

selects the virtual address region beginning at:

```text
0xFFFFFFFF80000000
```

This is why the boot page tables use:

```asm
.fill 510, 8, 0
.quad high_pd_table + 0x03
```

The first 510 entries are empty.

Entry 510 points to `high_pd_table`.

---

# 27. Build the Kernel Mapping

The boot code then fills `high_pd_table`.

```asm
lea edi, high_pd_table

mov esi, offset kernel_load_start
mov edx, offset kernel_load_end

xor ecx, ecx
```

The registers are used as:

```text
EDI → high_pd_table

ESI → current kernel physical address

EDX → kernel physical end address

ECX → PD entry index
```

---

# 28. Kernel Mapping Loop

```asm
map_kernel:

    cmp esi, edx
    jae map_kernel_done
```

This checks:

```text
Have we reached the end of the kernel?
```

If:

```text
ESI >= EDX
```

the mapping is finished.

Otherwise:

```asm
mov eax, esi
or eax, 0x00000083
```

The current physical address is combined with:

```text
PRESENT
WRITABLE
HUGE
```

Then:

```asm
mov [edi + ecx * 8], eax
```

stores it into the Page Directory.

---

# 29. Why 2 MiB?

The Page Directory entry contains:

```text
HUGE = 1
```

Therefore each entry maps:

```text
2 MiB
```

Then:

```asm
add esi, 0x200000
```

moves to the next 2 MiB physical region.

And:

```asm
inc ecx
```

moves to the next Page Directory entry.

Conceptually:

```text
PD[0] → kernel physical 0 + 2 MiB
PD[1] → kernel physical + 2 MiB
PD[2] → kernel physical + 4 MiB
...
```

This creates:

```text
KERNEL_VMA
    ↓
kernel physical memory
```

using 2 MiB pages.

---

# 30. One Page Directory

A Page Directory has:

```text
512 entries
```

Each entry maps:

```text
2 MiB
```

Therefore:

```text
512 × 2 MiB
=
1 GiB
```

The boot code therefore limits the mapping to one Page Directory:

```asm
cmp ecx, 512
jae map_kernel_done
```

---

# 31. Load CR3

After the page tables are ready:

```asm
lea eax, pml4_table
mov cr3, eax
```

`CR3` tells the CPU where the current page-table hierarchy starts.

So:

```text
CR3
 │
 ▼
PML4
 │
 ▼
PDPT
 │
 ▼
PD
 │
 ▼
physical memory
```

Important:

```text
CR3 contains the physical address of the PML4.
```

It is not the virtual address of the PML4.

---

# 32. Enable PAE

`CR4` is another CPU control register.

It contains many processor-control bits.

For our transition we care about:

```text
CR4 bit 5
```

which is:

```text
PAE
```

Conceptually:

```text
CR4

┌──────────────────────────────────────────┐
│ ... │ PAE │ ... │ other CPU features    │
└──────────────────────────────────────────┘
        ↑
      bit 5
```

PAE means:

```text
Physical Address Extension
```

For x86-64 long-mode paging:

```text
CR4.PAE = 1
```

is required.

---

# 33. Set CR4.PAE

```asm
mov eax, cr4
or  eax, 0x20
mov cr4, eax
```

`0x20` is:

```text
0010 0000
```

Bit 5 is `1`.

The sequence is:

```text
read CR4
   ↓
set bit 5
   ↓
write CR4
```

After this:

```text
CR4.PAE = 1
```

---

# 34. IA32_EFER

The CPU has a collection of special registers called:

```text
Model-Specific Registers
(MSRs)
```

One important MSR is:

```text
IA32_EFER
```

Its MSR number is:

```text
0xC0000080
```

Inside this register we care about:

```text
LME
Long Mode Enable
```

LME is:

```text
bit 8
```

Conceptually:

```text
IA32_EFER

... │ LME │ ...
      ↑
    bit 8
```

Setting:

```text
LME = 1
```

tells the CPU that Long Mode should be enabled when paging is enabled.

---

# 35. Accessing an MSR

MSRs are accessed using:

```text
RDMSR
WRMSR
```

The MSR number is placed in:

```text
ECX
```

For example:

```asm
mov ecx, 0xC0000080
```

means:

```text
ECX = 0xC0000080
```

which identifies:

```text
IA32_EFER
```

---

# 36. Read IA32_EFER

```asm
rdmsr
```

reads the MSR identified by `ECX`.

The value is returned in:

```text
EDX:EAX
```

Conceptually:

```text
ECX = 0xC0000080
        │
        ▼
   IA32_EFER
        │
        │ RDMSR
        ▼
     EDX:EAX
```

---

# 37. Set LME

```asm
or eax, 0x100
```

`0x100` sets bit 8:

```text
0x100

00000001 00000000
       ↑
     bit 8
```

Therefore:

```text
IA32_EFER.LME = 1
```

Then:

```asm
wrmsr
```

writes the modified value back to `IA32_EFER`.

The complete sequence is:

```asm
mov ecx, 0xC0000080
rdmsr
or  eax, 0x100
wrmsr
```

---

# 38. Enable Paging

Now we enable paging through `CR0`.

`CR0` is another CPU control register.

The important bit is:

```text
CR0 bit 31
```

called:

```text
PG
Paging Enable
```

Conceptually:

```text
CR0

┌────────────────────────────────────────────┐
│ ... │ PG │ ...                             │
└────────────────────────────────────────────┘
       ↑
     bit 31
```

When:

```text
PG = 0
```

paging is disabled.

When:

```text
PG = 1
```

paging is enabled.

---

# 39. Set CR0.PG

```asm
mov eax, cr0
or  eax, 0x80000000
mov cr0, eax
```

`0x80000000` sets bit 31:

```text
10000000 00000000 00000000 00000000
↑
bit 31
```

The sequence is:

```text
read CR0
   ↓
set bit 31
   ↓
write CR0
```

After this:

```text
CR0.PG = 1
```

Paging is now enabled.

---

# 40. Complete Paging Transition

The complete sequence is:

```text
Build page tables
       │
       ▼
Load PML4 into CR3
       │
       ▼
CR4.PAE = 1
       │
       ▼
IA32_EFER.LME = 1
       │
       ▼
CR0.PG = 1
       │
       ▼
Far jump to 64-bit code
```

In assembly:

```asm
mov cr3, eax

mov eax, cr4
or  eax, 0x20
mov cr4, eax

mov ecx, 0xC0000080
rdmsr
or  eax, 0x100
wrmsr

mov eax, cr0
or  eax, 0x80000000
mov cr0, eax

jmp 0x08:long_mode
```

---

# 41. Enter 64-bit Code

```asm
jmp 0x08:long_mode
```

`0x08` selects:

```text
GDT entry 1
```

which is our 64-bit code segment.

Execution continues at:

```asm
.code64

long_mode:
```

Now we are executing 64-bit code.

---

# 42. Setup Data Segments

```asm
mov ax, 0x10

mov ds, ax
mov es, ax
mov ss, ax
```

`0x10` selects:

```text
GDT entry 2
```

which is our data segment.

---

# 43. Setup the Stack

```asm
lea rsp, stack_top
```

This sets the 64-bit stack pointer.

Our boot stack is:

```asm
stack:
    .skip 16384

stack_top:
```

So the boot stack is:

```text
16 KiB
```

The stack grows downward:

```text
stack_top
    │
    │
    │  stack grows ↓
    │
    ▼
  stack
```

---

# 44. Pass Multiboot Information

GRUB originally gives us the Multiboot information address in:

```text
EBX
```

We saved it earlier:

```asm
mov [multiboot_info_address], ebx
```

After entering 64-bit mode:

```asm
mov edi, [multiboot_info_address]
```

loads it into `EDI`.

Under the x86-64 calling convention, the first integer/pointer argument is
passed in:

```text
RDI
```

So this prepares the argument for:

```c
kernel_main(multiboot_info)
```

---

# 45. Jump to `kernel_main`

```asm
mov rax, offset kernel_main
jmp rax
```

Now control moves from the assembly boot code into the C kernel:

```text
boot.S
   │
   │ CPU setup
   │ page tables
   │ long mode
   │ stack
   │
   ▼
kernel_main()
```

At this point the boot code has completed its main job.

---

# 46. The Whole Paging Picture

Our boot paging looks like this:

```text
                         CR3
                          │
                          ▼
                        PML4
                          │
          ┌───────────────┼────────────────┐
          │               │                │
          ▼               ▼                ▼
      PML4[0]         PML4[256]        PML4[511]
          │               │                │
          ▼               ▼                ▼
        PDPT       phys_map_pdpt      high_pdpt
          │               │                │
          ▼               ▼                ▼
          PD        1 GiB mapping      PDPT[510]
          │                                │
          ▼                                ▼
       2 MiB page                       high_pd
                                           │
                                           ▼
                                    kernel 2 MiB pages
```

The three important virtual regions are:

```text
Identity mapping:

0x00000000...
       ↓
same physical address


Physical direct map:

0xFFFF800000000000 + physical
       ↓
physical address


High-half kernel:

0xFFFFFFFF80000000
       ↓
kernel physical address
```

---

# 47. Important Flags

The most important flags used during boot are:

```text
0x01 → PRESENT
0x02 → WRITABLE
0x80 → HUGE
```

Therefore:

```text
0x03
=
PRESENT | WRITABLE
```

and:

```text
0x83
=
PRESENT | WRITABLE | HUGE
```

So:

```text
0x03
    → page-table entry

0x83
    → huge-page mapping
```

---

# 48. Important Sizes

Remember these:

```text
1 page-table entry
    = 8 bytes

1 page table
    = 512 entries
    = 512 × 8
    = 4096 bytes
    = 4 KiB
```

Mapping sizes:

```text
PT entry
    → 4 KiB

PD entry + HUGE
    → 2 MiB

PDPT entry + HUGE
    → 1 GiB
```

This is why our boot code can map the kernel using:

```text
PD
 ↓
2 MiB pages
```

instead of creating thousands of 4 KiB page-table entries.

---

# 49. Main Idea to Remember

The most important thing to understand about `boot.S` is:

```text
boot.S does not create the entire final memory-management system.

It creates the minimum page-table structure required to safely enter
64-bit mode and start the kernel.
```

After entering C:

```text
boot.S
   │
   ▼
paging_init()
   │
   ▼
extend physical direct map
   │
   ▼
buddy allocator / page-table allocation
   │
   ▼
later virtual-memory management
```

So `boot.S` provides the initial foundation, while `paging.c` takes over the
more complete paging management later.
