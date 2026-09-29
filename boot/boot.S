.intel_syntax noprefix

.section .multiboot, "a"
.align 4

.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)


.global pml4_table
.global pdpt_table
.global pd_table

.global high_pdpt_table
.global high_pd_table

.global phys_map_pdpt_table

.section .boot.data, "aw"

.align 4096
pml4_table:
    ## index 0 -> pdpt_table (identity map, low 2 MiB)
    .quad pdpt_table + 0x03
    .fill 255, 8, 0
    ## index 256 -> phys_map_pdpt_table (direct physical map, PHYS_MAP_BASE)
    .quad phys_map_pdpt_table + 0x03
    .fill 254, 8, 0
    ## index 511 -> high_pdpt_table (high-half kernel mapping)
    .quad high_pdpt_table + 0x03
 
.align 4096
pdpt_table:
    .quad pd_table + 0x03
    .fill 511, 8, 0
 
.align 4096
pd_table:
    .quad 0x0000000000000083
    .fill 511, 8, 0
 
.align 4096
high_pdpt_table:
    .fill 510, 8, 0
    ## index 510 -> high_pd_table
    .quad high_pd_table + 0x03
    .fill 1, 8, 0
 
.align 4096
phys_map_pdpt_table:
    ## index 0 -> physical 0..1 GiB, PRESENT | WRITABLE | HUGE
    ## (paging.c extends this after physical-memory discovery)
    .quad 0x0000000000000083
    .fill 511, 8, 0
 
.section .boot.bss, "aw", @nobits

.align 4096
high_pd_table:
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


multiboot_info_address:
.long 0



## ---------------------------------
## 32-BIT ENTRY
## GRUB drops us here in 32-bit protected mode. Our job :build page
## tables, flip the cpu into 64-bit long mode, then jump into 64-bit code that calls our c kernel_main
##------------------------------------------

.section .boot.text, "ax"
.code32

.global _start
.extern kernel_main

_start:
    cli

    mov [multiboot_info_address], ebx

    #load our gdt
    lgdt gdt_descriptor

    lea edi, high_pd_table

    mov esi, offset kernel_load_start
    mov edx, offset kernel_load_end

    xor ecx, ecx

map_kernel:

    cmp esi, edx
    jae map_kernel_done

    mov eax, esi
    or eax, 0x00000083

    mov [edi + ecx * 8], eax

    mov dword ptr [edi + ecx * 8 + 4], 0

    add esi, 0x200000
    inc ecx

    cmp ecx, 512
    jae map_kernel_done

    jmp map_kernel


map_kernel_done: 
    # Load PML4 into CR3
    lea eax, pml4_table
    mov cr3, eax
 
 
    ### Enable PAE format for paging
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
    jmp 0x08:long_mode

.code64

long_mode:
    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax


    lea rsp, stack_top


    mov edi, [multiboot_info_address]

    mov rax, offset kernel_main
    jmp rax

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



## Helper functions
.section .text, "ax"
.code64


.global boot_load_start_phys
.type boot_load_start_phys, @function

boot_load_start_phys:
    movabs rax, offset boot_load_start
    ret


.global boot_load_end_phys
.type boot_load_end_phys, @function

boot_load_end_phys:
    movabs rax, offset boot_load_end
    ret


.global kernel_load_start_phys
.type kernel_load_start_phys, @function

kernel_load_start_phys:
    movabs rax, offset kernel_load_start
    ret



.global kernel_load_end_phys
.type kernel_load_end_phys, @function

kernel_load_end_phys:
    movabs rax, offset kernel_load_end
    ret
