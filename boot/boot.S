.intel_syntax noprefix 

.section .multiboot, "a"
.align 4

.long 0x1BADB002
.long 0
.long -(0x1BADB002)


## ------------------------------------------------------
## BSS
## section used for memory that program needs, but doesnot
## need to store an intial value for
##--------------------------------------------------------

.section .bss

.align 4096
pml4_table:
    .skip 4096

.align 4096
pdpt_table:
    .skip 4096

.align 4096
pd_table:
    .skip 4096


## Stack 
.align 16
stack:
    .skip 16384 

stack_top:

.align 16
idt:
    .skip 256 * 16

idt_end:


## ---------------------------------
## 32-BIT ENTRY
## GRUB drops us here in 32-bit protected mode. Our job :build page 
## tables, flip the cpu into 64-bit long mode, then jump into 64-bit code that calls our c kernel_main
##------------------------------------------

.section .text
.code32

.global _start 
.extern kernel_main

_start:
    cli 
    #load our gdt 
    lgdt gdt_descriptor

    ## Build page table

    ## PML4[0] =  address of PDPT + 0x03
    lea eax, pml4_table
    lea ebx, pdpt_table
    or ebx, 0x03 
    mov [eax], ebx

    ## PDPT[0] = address of PD + 0x03 
     lea eax, pdpt_table
     lea ebx, pd_table
     or ebx, 0x03
     mov [eax], ebx

    ## PD[0] -> physical address 0x00000000
    lea eax, pd_table 
    mov ebx, 0x00000083
    mov [eax], ebx

     # Load PML4 into CR3
     lea eax, pml4_table
     mov cr3, eax

    ###  Enable PAE format for paging 
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    ## Enable Long mode
    mov ecx, 0xC0000080
    rdmsr 
    or eax, 0x100
    wrmsr

   ## Enable Paging 
   mov eax, cr0
   or eax, 0x80000000
   mov cr0, eax

   ## Jump to long mode
   jmp 0x08, long_mode


.code64
long_mode:
    # now executing 64-bit code
    mov ax, 0x10
    mov ds, ax 
    mov es, ax
    mov ss, ax

    lea rsp, stack_top

    call kernel_main

hang:
    cli
    hlt
    jmp hang


.align 8

gdt:
   # Null descriptor 
   .quad 0x0000000000000000

 # 64-bit code segment 
  .quad 0x00AF9A000000FFFF

  # Data segment
    .quad 0x00AF92000000FFFF


gdt_descriptor:
.word gdt_descriptor - gdt - 1 
.quad gdt

idt_descriptor:
    .word idt_end - idt - 1
    .quad idt
