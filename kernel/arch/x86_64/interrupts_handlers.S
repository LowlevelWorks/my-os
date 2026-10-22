.intel_syntax noprefix

.section .text
.code64

.extern keyboard_handler
.extern serial_handler
.extern pit_handle_irq_frame

.extern scheduler_interrupt_switch_required
.extern scheduler_interrupt_next_rsp
.extern scheduler_interrupt_next_cr3
.extern scheduler_interrupt_next_is_preempted

.global invalid_opcode_handler
.global keyboard_handle_irq
.global serial_handle_irq
.global pit_irq_handler


invalid_opcode_handler:
    cli
    hlt
    jmp invalid_opcode_handler


keyboard_handle_irq:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbp

    cld
    mov rbp, rsp
    and rsp, -16

    call keyboard_handler

    mov rsp, rbp

    mov al, 0x20
    out 0x20, al

    pop rbp
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq


serial_handle_irq:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbp

    cld
    mov rbp, rsp
    and rsp, -16

    call serial_handler

    mov rsp, rbp

    mov al, 0x20
    out 0x20, al

    pop rbp
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq


pit_irq_handler:
    cld

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8

    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    push 32

    /* r15 = interrupt_frame_t * (r15 is callee-saved, and also in the frame) */
    mov r15, rsp

    /* Keep the stack 16-byte aligned for every C call below. */
    and rsp, -16

    mov rdi, r15
    call pit_handle_irq_frame

    call scheduler_interrupt_switch_required
    test eax, eax
    jz .return_current

    call scheduler_interrupt_next_rsp
    mov r12, rax

    call scheduler_interrupt_next_cr3
    mov r13, rax

    call scheduler_interrupt_next_is_preempted
    test eax, eax
    jnz .switch_to_preempted

    /*
     * New thread: saved_rsp points at
     *   r15 r14 r13 r12 rbx rbp ret(thread_trampoline) pad
     */
    mov cr3, r13
    mov rsp, r12

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret


.switch_to_preempted:
    mov cr3, r13
    mov rsp, r12

    add rsp, 8              /* drop synthetic vector */

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi

    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    iretq


.return_current:
    mov rsp, r15

    add rsp, 8              /* drop synthetic vector FIRST */

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi

    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    iretq
.size pit_irq_handler, .-pit_irq_handler
