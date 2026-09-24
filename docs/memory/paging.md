# Page Table Hierarchy

For x86-64 paging with 4 KiB pages:

```text
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
```

Each level contains 512 entries.

```c
#define PAGE_SIZE 4096
#define PAGE_TABLE_ENTRIES 512

typedef uint64_t page_table_entry_t;

typedef struct {
    page_table_entry_t entries[PAGE_TABLE_ENTRIES];
} page_table_t;
```

## Page Table Size

Each entry is 8 bytes:

```text
uint64_t = 8 bytes
```

Each table has 512 entries:

```text
512 × 8 = 4096 bytes
```

Therefore:

```text
PML4 = 4096 bytes
PDPT = 4096 bytes
PD   = 4096 bytes
PT   = 4096 bytes
```

Each page table fits exactly inside one 4 KiB page.

---

# Virtual Address

For 4 KiB pages, the virtual address is divided into:

```text
63             48 47       39 38       30 29       21 20       12 11        0
┌────────────────┬───────────┬───────────┬───────────┬───────────┬────────────┐
│ sign extension │   PML4    │   PDPT    │    PD     │    PT     │   OFFSET   │
└────────────────┴───────────┴───────────┴───────────┴───────────┴────────────┘
                  9 bits       9 bits      9 bits      9 bits      12 bits
```

There are four 9-bit indexes:

```text
PML4 index = 9 bits
PDPT index = 9 bits
PD index   = 9 bits
PT index   = 9 bits
```

And a 12-bit page offset:

```text
Offset = 12 bits
```

The reason:

```text
2^9 = 512
```

so each level needs 9 bits to select one of 512 entries.

And:

```text
2^12 = 4096
```

so 12 bits select one byte within a 4 KiB page.

---

# Extracting the Indexes

Given:

```c
uint64_t virtual_address;
```

the indexes are:

```c
uint64_t pml4_index;
uint64_t pdpt_index;
uint64_t pd_index;
uint64_t pt_index;
uint64_t offset;
```

Extract them using:

```c
pml4_index = (virtual_address >> 39) & 0x1FF;
pdpt_index = (virtual_address >> 30) & 0x1FF;
pd_index   = (virtual_address >> 21) & 0x1FF;
pt_index   = (virtual_address >> 12) & 0x1FF;
offset     = virtual_address & 0xFFF;
```

The masks are:

```text
0x1FF = 9 bits set

111111111
```

and:

```text
0xFFF = 12 bits set

111111111111
```

---

# Page Table Entry

A page-table entry is 64 bits:

```c
typedef uint64_t page_table_entry_t;
```

An entry contains:

```text
physical address + flags
```

Conceptually:

```text
┌──────────────────────────────┬─────────────┐
│ physical address             │ flags       │
└──────────────────────────────┴─────────────┘
```

The lower bits contain flags.

Important flags we learned:

```c
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
```

---

# 1ULL and Left Shift

```c
1ULL
```

means:

```text
the number 1 as an unsigned long long integer
```

For example:

```c
1ULL << 0
```

means:

```text
shift 1 left 0 times

0001
```

Therefore:

```c
1ULL << 0 = 1
```

And:

```c
1ULL << 1 = 2
1ULL << 2 = 4
1ULL << 3 = 8
```

This lets us set individual bits.

For example:

```c
#define PAGE_PRESENT (1ULL << 0)
```

sets bit 0.

```c
#define PAGE_WRITABLE (1ULL << 1)
```

sets bit 1.

```c
#define PAGE_USER (1ULL << 2)
```

sets bit 2.

---

# Present Bit

Bit 0 is the Present bit.

```c
#define PAGE_PRESENT (1ULL << 0)
```

When:

```text
Present = 1
```

the entry is valid for the page-table walk.

When:

```text
Present = 0
```

the CPU does not use that entry as a valid mapping.

If translation requires a non-present entry, the CPU generates a page fault.

Conceptually:

```text
Present = 1
    ↓
valid entry
    ↓
continue page-table walk
```

```text
Present = 0
    ↓
not present
    ↓
page fault
```

---

# Writable Bit

Bit 1:

```c
#define PAGE_WRITABLE (1ULL << 1)
```

Conceptually:

```text
Writable = 1
    ↓
writes are allowed
```

```text
Writable = 0
    ↓
writes are not allowed
```

Actual access permission also depends on other paging rules.

---

# User Bit

Bit 2:

```c
#define PAGE_USER (1ULL << 2)
```

It controls whether user-mode code can access the page.

```text
PAGE_USER = 0
    ↓
supervisor/kernel page
```

```text
PAGE_USER = 1
    ↓
user-accessible page
```

This becomes important when the OS supports user processes.

---

# Combining Flags

Use bitwise OR:

```c
entry = PAGE_PRESENT | PAGE_WRITABLE;
```

For example:

```text
PAGE_PRESENT

0001
```

```text
PAGE_WRITABLE

0010
```

OR:

```text
0001
0010
----
0011
```

So both flags are enabled.

---

# Physical Address in an Entry

A page-table entry can contain a physical address and flags together.

For example:

```c
entry = physical_address |
        PAGE_PRESENT |
        PAGE_WRITABLE;
```

Conceptually:

```text
┌──────────────────────────────┬─────────────┐
│ physical address             │ flags       │
└──────────────────────────────┴─────────────┘
```

Page tables and physical pages are page-aligned.

A 4 KiB aligned address has its lowest 12 bits clear:

```text
physical address
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000
                                      ↑
                                  12 zero bits
```

This allows the low bits to be used for flags.

---

# Extracting the Physical Address

A page-table entry contains both address information and flags.

To extract the physical address, use an address mask:

```c
#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
```

Then:

```c
physical_address = entry & PAGE_ADDRESS_MASK;
```

The bitwise AND removes the flag bits and keeps the physical-address bits.

---

# Page Table Relationships

The important relationships are:

```text
PML4[index] → PDPT
PDPT[index] → PD
PD[index]   → PT
PT[index]   → physical page
```

The first three point to another page table.

The final PT entry points to the actual physical page.

---

# Example Mapping

Suppose we want:

```text
virtual address 0x400000
        ↓
physical address 0x200000
```

The hierarchy must look like:

```text
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
```

---

# Building the Hierarchy

For learning purposes, assume we already have these physical addresses:

```text
PML4 = 0x100000
PDPT = 0x101000
PD   = 0x102000
PT   = 0x103000
```

Then the connections are conceptually:

```c
pml4->entries[pml4_index] =
    0x101000 | PAGE_PRESENT | PAGE_WRITABLE;
```

This means:

```text
PML4[index]
    ↓
PDPT at physical address 0x101000
```

Then:

```c
pdpt->entries[pdpt_index] =
    0x102000 | PAGE_PRESENT | PAGE_WRITABLE;
```

This means:

```text
PDPT[index]
    ↓
PD at physical address 0x102000
```

Then:

```c
pd->entries[pd_index] =
    0x103000 | PAGE_PRESENT | PAGE_WRITABLE;
```

This means:

```text
PD[index]
    ↓
PT at physical address 0x103000
```

Finally:

```c
pt->entries[pt_index] =
    0x200000 | PAGE_PRESENT | PAGE_WRITABLE;
```

This means:

```text
PT[index]
    ↓
physical page at 0x200000
```

---

# Complete Translation

The CPU starts from CR3:

```text
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
```

---
