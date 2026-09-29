# Page Table Hierarchy

For x86-64 paging with 4 KiB pages:

    Virtual Address
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
         PT
          │
          ▼
    Physical Page

Each level contains 512 entries.

    #define PAGE_SIZE 4096
    #define PAGE_TABLE_ENTRIES 512

    typedef uint64_t page_table_entry_t;

    typedef struct {
        page_table_entry_t entries[PAGE_TABLE_ENTRIES];
    } page_table_t;

## Page Table Size

Each entry is 8 bytes:

    uint64_t = 8 bytes

Each table has 512 entries:

    512 × 8 = 4096 bytes

Therefore:

    PML4 = 4096 bytes
    PDPT = 4096 bytes
    PD   = 4096 bytes
    PT   = 4096 bytes

Each page table fits exactly inside one 4 KiB page.

---

# Virtual Address

For 4 KiB pages, the virtual address is divided into:

    63             48 47       39 38       30 29       21 20       12 11        0
    ┌────────────────┬───────────┬───────────┬───────────┬───────────┬────────────┐
    │ sign extension │   PML4    │   PDPT    │    PD     │    PT     │   OFFSET   │
    └────────────────┴───────────┴───────────┴───────────┴───────────┴────────────┘
                      9 bits       9 bits      9 bits      9 bits      12 bits

There are four 9-bit indexes:

    PML4 index = 9 bits
    PDPT index = 9 bits
    PD index   = 9 bits
    PT index   = 9 bits

And a 12-bit page offset:

    Offset = 12 bits

The reason:

    2^9 = 512

so each level needs 9 bits to select one of 512 entries.

And:

    2^12 = 4096

so 12 bits select one byte within a 4 KiB page.

---

# Extracting the Indexes

Given:

    uint64_t virtual_address;

the indexes are:

    uint64_t pml4_index;
    uint64_t pdpt_index;
    uint64_t pd_index;
    uint64_t pt_index;
    uint64_t offset;

Extract them using:

    pml4_index = (virtual_address >> 39) & 0x1FF;
    pdpt_index = (virtual_address >> 30) & 0x1FF;
    pd_index   = (virtual_address >> 21) & 0x1FF;
    pt_index   = (virtual_address >> 12) & 0x1FF;
    offset     = virtual_address & 0xFFF;

The masks are:

    0x1FF = 9 bits set

    111111111

and:

    0xFFF = 12 bits set

    111111111111

---

# Page Table Entry

A page-table entry is 64 bits:

    typedef uint64_t page_table_entry_t;

An entry contains:

    physical address + flags

Conceptually:

    ┌──────────────────────────────┬─────────────┐
    │ physical address             │ flags       │
    └──────────────────────────────┴─────────────┘

Important flags:

    #define PAGE_PRESENT  (1ULL << 0)
    #define PAGE_WRITABLE (1ULL << 1)
    #define PAGE_USER     (1ULL << 2)

---

# 1ULL and Left Shift

    1ULL

means the number 1 represented as an unsigned long long integer.

For example:

    1ULL << 0

means:

    0001

Therefore:

    1ULL << 0 = 1
    1ULL << 1 = 2
    1ULL << 2 = 4
    1ULL << 3 = 8

This allows us to set individual bits.

For example:

    #define PAGE_PRESENT (1ULL << 0)

sets bit 0.

    #define PAGE_WRITABLE (1ULL << 1)

sets bit 1.

    #define PAGE_USER (1ULL << 2)

sets bit 2.

---

# Present Bit

Bit 0 is the Present bit.

    #define PAGE_PRESENT (1ULL << 0)

When:

    Present = 1

the entry is valid for the page-table walk.

When:

    Present = 0

the CPU does not use that entry as a valid mapping.

If translation requires a non-present entry, the CPU generates a page fault.

Conceptually:

    Present = 1
        ↓
    valid entry
        ↓
    continue page-table walk

    Present = 0
        ↓
    not present
        ↓
    page fault

---

# Writable Bit

Bit 1:

    #define PAGE_WRITABLE (1ULL << 1)

Conceptually:

    Writable = 1
        ↓
    writes are allowed

    Writable = 0
        ↓
    writes are not allowed

Actual access permission also depends on other paging rules.

---

# User Bit

Bit 2:

    #define PAGE_USER (1ULL << 2)

It controls whether user-mode code can access the page.

    PAGE_USER = 0
        ↓
    supervisor/kernel page

    PAGE_USER = 1
        ↓
    user-accessible page

This becomes important when the OS supports user processes.

---

# Combining Flags

Use bitwise OR:

    entry = PAGE_PRESENT | PAGE_WRITABLE;

For example:

    PAGE_PRESENT

    0001

    PAGE_WRITABLE

    0010

OR:

    0001
    0010
    ----
    0011

So both flags are enabled.

---

# Physical Address in an Entry

A page-table entry can contain a physical address and flags together.

For example:

    entry = physical_address |
            PAGE_PRESENT |
            PAGE_WRITABLE;

Conceptually:

    ┌──────────────────────────────┬─────────────┐
    │ physical address             │ flags       │
    └──────────────────────────────┴─────────────┘

Page tables and physical pages are page-aligned.

A 4 KiB aligned address has its lowest 12 bits clear:

    physical address
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000
                                          ↑
                                      12 zero bits

This allows the low bits to be used for flags.

---

# Extracting the Physical Address

A page-table entry contains both address information and flags.

To extract the physical address, use:

    #define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

Then:

    physical_address = entry & PAGE_ADDRESS_MASK;

The bitwise AND removes the flag bits and keeps the physical-address bits.

---

# Page Table Relationships

The important relationships are:

    PML4[index] → PDPT
    PDPT[index] → PD
    PD[index]   → PT
    PT[index]   → physical page

The first three point to another page table.

The final PT entry points to the actual physical page.

---

# Example Mapping

Suppose we want:

    virtual address 0x400000
            ↓
    physical address 0x200000

The hierarchy must look like:

    PML4
      │
      └── PML4[index]
              │
              ▼
            PDPT
              │
              └── PDPT[index]
                      │
                      ▼
                     PD
                      │
                      └── PD[index]
                              │
                              ▼
                             PT
                              │
                              └── PT[index]
                                      │
                                      ▼
                               physical page
                               0x200000

---

# Building the Hierarchy

For learning purposes, assume we already have these physical addresses:

    PML4 = 0x100000
    PDPT = 0x101000
    PD   = 0x102000
    PT   = 0x103000

Then the connections are conceptually:

    pml4->entries[pml4_index] =
        0x101000 | PAGE_PRESENT | PAGE_WRITABLE;

This means:

    PML4[index]
        ↓
    PDPT at physical address 0x101000

Then:

    pdpt->entries[pdpt_index] =
        0x102000 | PAGE_PRESENT | PAGE_WRITABLE;

This means:

    PDPT[index]
        ↓
    PD at physical address 0x102000

Then:

    pd->entries[pd_index] =
        0x103000 | PAGE_PRESENT | PAGE_WRITABLE;

This means:

    PD[index]
        ↓
    PT at physical address 0x103000

Finally:

    pt->entries[pt_index] =
        0x200000 | PAGE_PRESENT | PAGE_WRITABLE;

This means:

    PT[index]
        ↓
    physical page at 0x200000

---

# Complete Translation

The CPU starts from CR3:

    CR3
     ↓
    PML4
     ↓
    PML4[pml4_index]
     ↓
    PDPT
     ↓
    PDPT[pdpt_index]
     ↓
    PD
     ↓
    PD[pd_index]
     ↓
    PT
     ↓
    PT[pt_index]
     ↓
    physical page
     ↓
    + offset
     ↓
    final physical address

---

# Canonical Addresses

Our current paging implementation uses the 48-bit virtual address model.

Bits 48-63 must be a sign extension of bit 47.

If bit 47 is 0:

    bits 48-63 must all be 0

If bit 47 is 1:

    bits 48-63 must all be 1

These are called canonical addresses.

Example:

    0x00007FFFFFFFFFFF

is canonical.

Our kernel address:

    0xFFFFFFFF80000000

is also canonical.

A non-canonical address is invalid for our current paging model.

The helper:

    paging_is_canonical(address);

checks whether an address is canonical.

---

# Address Range Validation

Checking only the first address of a range is not enough.

For example:

    address + size

can overflow the 64-bit address space.

The paging code therefore checks:

1. The size is valid.
2. Address arithmetic does not overflow.
3. The starting address is canonical.
4. The ending address is canonical.

For virtual addresses:

    paging_range_valid(address, size);

For physical addresses:

    paging_physical_range_valid(address, size);

---

# NX

NX means:

    No Execute

The NX flag prevents the CPU from executing instructions from a page.

For example:

    Kernel text     → executable
    Kernel data     → NX
    Kernel stack    → NX
    Heap            → NX
    Page tables     → NX

The flag is:

    PAGE_NX

---

# Enabling NX

The CPU reports NX support through:

    CPUID 0x80000001

NX support is reported in:

    EDX bit 20

If the CPU supports NX, the kernel enables:

    EFER.NXE

The function responsible for this is:

    paging_enable_nxe();

The process is:

    CPUID
      │
      ├── CPU supports NX?
      │
      ▼
    Read EFER
      │
      ▼
    Set EFER.NXE
      │
      ▼
    Write EFER

After EFER.NXE is enabled, PAGE_NX in page-table entries becomes effective.

---

# Physical Memory Mapping

Our kernel has a direct physical-memory mapping:

    PHYS_MAP_BASE = 0xFFFF800000000000

Conceptually:

    Physical address
          │
          │ + PHYS_MAP_BASE
          ▼
    Kernel virtual address

For example:

    Physical:

        0x00100000

    Virtual:

        0xFFFF800000100000

This allows the kernel to access physical memory through a known virtual address.

---

# Temporary Mapping

Page tables are stored in physical memory.

When the kernel allocates a new physical page for a page table, it needs a way to access that physical page through a virtual address.

Our paging implementation provides a temporary mapping area:

    TEMP_MAP_BASE = 0xFFFFFE0000000000

Conceptually:

    Physical page
          │
          │ temporary mapping
          ▼
    Temporary virtual address
          │
          ▼
    Kernel accesses the page

This is useful when creating or modifying page tables.

---

# CR3

The CPU stores the physical address of the current top-level page table in:

    CR3

For our 4-level paging system:

    CR3
     │
     ▼
    PML4

Our helper:

    paging_current_pml4();

reads CR3 and converts the physical address into a kernel virtual address.

Conceptually:

    CR3
     │
     │ physical PML4 address
     ▼
    PML4
     │
     │ physical-to-virtual mapping
     ▼
    Kernel virtual address

---

# Page Table Allocation

Page tables themselves require physical pages.

When a new page table is required, the paging code obtains a physical frame from the physical memory allocator.

Conceptually:

    Paging
       │
       ▼
    Buddy allocator
       │
       ▼
    Physical frame
       │
       ▼
    Page table
       │
       ▼
    Page-table entry

This connects paging with the physical memory management system.

---

# Memory Management Relationship

The memory-management layers have different responsibilities.

    Physical memory
          │
          ▼
    Frame allocator
          │
          ▼
    Buddy allocator
          │
          ▼
    Physical frames
          │
          ▼
    Paging
          │
          ▼
    Virtual address mappings

The buddy allocator manages physical frames.

Paging manages how virtual addresses map to those physical frames.

---

# Current Address Layout

Important virtual addresses used by the kernel:

    Kernel:

        0xFFFFFFFF80000000

    Physical map:

        0xFFFF800000000000

    Temporary map:

        0xFFFFFE0000000000

All of these addresses are canonical under our current 48-bit virtual address model.

---

# Paging and Future Processes

The current paging system provides the foundation for separate process address spaces.

Later, each process can have its own PML4:

    Process A
        │
        ▼
       PML4 A

    Process B
        │
        ▼
       PML4 B

The CPU switches between them by changing CR3.

Conceptually:

    Process A
        │
        ▼
       CR3
        │
        ▼
      PML4 A


    Process B
        │
        ▼
       CR3
        │
        ▼
      PML4 B

This allows different processes to have different virtual address spaces.

Kernel mappings can be shared between address spaces.

---

```

0x0000_0000_0000_0000
        │
        │ USER SPACE
        │
        │  code
        │  data
        │  heap
        │  user stacks
        │
0x0000_7FFF_FFFF_FFFF
        │
        │ ─── non-canonical ───
        │
0xFFFF_8000_0000_0000
        │
        ├──────────────────────────────
        │ DIRECT PHYSICAL MAP
        │
        │ physical 0 → this address
        │ physical 1MB → +1MB
        │ ...
        │
        ├──────────────────────────────
        │ KERNEL IMAGE
        │
        │ kernel code
        │ kernel rodata
        │ kernel data
        │ kernel BSS
        │
        ├──────────────────────────────
        │ KERNEL HEAP
        │
        ├──────────────────────────────
        │ KERNEL STACKS
        │
        └──────────────────────────────

0xFFFF_FFFF_FFFF_FFFF


```


---

````
FFFF_8000_0000_0000
        │
        │ Direct physical map
        │
FFFF_8800_0000_0000
        │
        │ Kernel image
        │
FFFF_9000_0000_0000
        │
        │ Kernel heap
        │
FFFF_A000_0000_0000
        │
        │ Kernel stacks
        │
FFFF_B000_0000_0000
        │
        │ future kernel mappings
        │

````

# Current Scope

The current paging implementation is responsible for:

- 4-level page-table management
- Page-table allocation
- Virtual-to-physical mappings
- Physical-to-virtual mappings
- Temporary mappings
- Page-table entry creation
- Page-table entry address extraction
- Address validation
- Canonical address validation
- CPU paging feature detection
- NX enablement
- Large-page support

Virtual memory management is a later layer.

It will handle things such as:

- Virtual address regions
- Process address spaces
- mmap-like allocation
- Unmapping
- Copy-on-write
- Page-fault handling

These are built on top of the paging and physical memory-management layers.
