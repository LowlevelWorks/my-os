.intel_syntax noprefix

.section .text
.code64

.global context_switch
.type context_switch, @function

context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    test rdi, rdi
    jz 1f
    mov [rdi], rsp
1:
    mov rsp, rsi

    mov rax, cr3
    cmp rax, rdx
    je 2f
    mov cr3, rdx
2:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

.size context_switch, .-context_switch


.global resume_preempted
.type resume_preempted, @function

resume_preempted:
    cli

    mov rax, cr3
    cmp rax, rsi
    je 1f
    mov cr3, rsi
1:
    mov rsp, rdi
    add rsp, 8          

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

.size resume_preempted, .-resume_preempted
