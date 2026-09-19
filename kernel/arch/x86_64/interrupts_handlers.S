.intel_syntax noprefix 

.section .text
.code64

.extern keyboard_handler


.global invalid_opcode_handler
.global keyboard_handle_irq

invalid_opcode_handler:
    cli 
    hlt

keyboard_handle_irq:
    push rax

    call keyboard_handler

    mov al, 0x20
    out 0x20, al

    pop rax
    iretq
