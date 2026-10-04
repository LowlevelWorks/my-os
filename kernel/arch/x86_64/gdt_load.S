.intel_syntax noprefix

.section .text
.code64

.global gdt_load
.type gdt_load, @function

gdt_load:

    lgdt [rdi]

    push 0x08

    lea rax, [rip + reload_cs]
    push rax

    retfq

reload_cs:

    mov eax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax

    ret

.size gdt_load, .-gdt_load
