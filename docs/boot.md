## A linker takes one or more object files, resolves references between them , combines thier sections, assigns
 address/layout, and produce the final executable or kernel image.

## lgdt gdt_descriptor
- This is the  important instruction. 
lgdt means Load Global Descriptor Table register 
- The CPU has a special register called GDTR which tells the CPU : where is my global descriptor table 
and how big is it

- The Global Descriptor Table is a table that describes memory segments

-----------------------
Entry 0: NULL 
Entry 1 : 64-bit CODE
Entry 2: DATA
---------------------------

Each .quad is 8 bytes, so 
Entry 0 = 8 bytes
Entry 1 = 8 bytes 
Entry 2 = 8 bytes

.quad 0x0000000000000000 is null descriptor 
The x86 architecture requires GDT index 0 to be null descriptor. we dont use this entry for normal code/data

.quad 0x00AF9A000000FFFF
This describes our code segment. we are telling CPU to use GDT entry 1 as my code segment.

.quad 0x00AF92000000FFFF
- This is our data segment 

we have 
1 x 8 = 8 = 0x08 -> GDT entry #1
2 x 8 = 16 = 0x10 -> GDT entry #2

gdt_descriptor:
    .word gdt_descriptor - gdt - 1
    .quad gdt

Here CPU needs two pieces of information 
1. How big is the GDT ? (GDTR LIMIT)
why -1?: The GDT descriptor's limit means "the index of the last byte", not the "the number of bytes"
limit = size - 1 as cpu interpretes the value as
    last byte address - first byte address
The CPU interprets LIMIT as "How far does this table extend, starting from the base?"

2. Where is the GDT?
.quad gdt, this stores the address of gdt

##sections
.text -> "Here is my code"
.data -> "Here is initialized data"
.bss -> "I need this much empty/zero memory"


## Build page tables
1. lea eax, pml4_table
 -> Put the address of pml4_table into EAX 

 2. lea ebx, pdpt_table
 -> Put the address of pdpt_table into EBX 

 3. or ebx, 0x03
    0x03 = 
    00000011
          ││
          │└── Present = 1
          └─── Writable = 1 

it changes EBX = address of PDPT into
EBX = address of pdpt_table + flags 

More preciously we are setting the lower two bits 
bit 0 = 1 -> present
bit 1 = 1-> Writeable

EBX conceptually becomes 
┌───────────────────────────────┬─────┐
│      address of PDPT          │ P W │
└───────────────────────────────┴─────┘
                                  1 1

4. mov [eax], ebx 
-> store the value of ebx (adress of PDPT + flags) into memory adress
   pointed by eax register (or into address of PML4)
  
###  For example 
  Bfore: 
  EAX = 0x1000
  EBX = 0x2003 (pdpt_table adress + flags)

  Memory: 
  0x1000 -> ?????? (garbage)
  0x1008 -> ??????

  After:
  0x1000 -> 0x2003
  0x1008 -> ?????

  since pml4_table i at 0x1000. so 
  PML4[0] = 0x2003


  1. Present bit -> this is how CPU know whether PML4[0] contain a valid mapping
   or is just unused memory containing zaro (garbage), generates page fault when 
   there is no valid mapping here (present = 1(valid), present = 0 (invalid mapping))

   2. Writable -> can this memory be written to? 
     writeable = 1, Writing is allowed 
     writeable = 0, Writing  is not allowed

   0x03 -> Mark this page-table entry as valid and writeable 
     
  ### similarly we do pdpt[0] = pd (adress of pd + 0x03)

#CR4 is another CPU control register. It enables various processor features
CR4: bit 5 (counting from 0) (or 6th bit if countinf from 1)
┌──────────────────────────────────────────┐
│ ... │ PAE │ ... │ other CPU features     │
└──────────────────────────────────────────┘
        ↑
      bit 5

Here PAE means Physical Adress Ectension. one requirement for 64-bit paging is 
CR4.PAE = 1, use the extended paging mechanism (use PAE paging)

mov eax, cr4
or  eax, 0x20 (0010 0000) -> set fifth bit (1) = enable PAE
mov cr4, eax


## EFER.LME
The CPU has a special register called IA32_EFER 
Inside it is a bit called: 
LME = Long Mode Enable 


IA32_EFER
             bit 8
               ↓
... 0 0 0 0 0 1 0 0 0 ...
               ↑
              LME

When wen set it to 1, we're basically telling the CPU: I want to use Long Mode

MSR numbers 
- The CPU has  a separate collection of special registers called model-Specific Registers (MSRs)

MSR collection

index          register
────────────────────────────
0x00000000  →  some MSR
0x00000001  →  some MSR
...
0xC0000080  →  IA32_EFER
...

and we access them with special instructions:
  RDMSR
  WRMSR

  and these instructions use ECX as MSR identifier

  1. mov ecx, 0xC0000080
     rdmsr

  ->  ECX = 0xC0000080
        ↓
      IA32_EFER
        ↓
     read its value
        ↓
     EDX:EAX

    2. or eax, 0x100
    -> set the bit 8 of the IA32_EFER value.

0xC0000080  → identifies WHICH MSR/register
0x100       → bit mask used to set LME inside that register


## Enable Paging 
Set CR0.PG 
CRO is another CPU control register. One of its bits is
┌────────────────────────────────────────────┐
│ ... │ PG │ ...                             │
└────────────────────────────────────────────┘
       ↑
     bit 31

PG(Paging). We need to set bit 31

3. wrmsr
-> Write the modified value back into IA32_EFER.

