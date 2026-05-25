.intel_syntax noprefix

.global irq1_stub
.extern wildnix_irq1_c_handler

.macro PUSH_REGS
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
.endm

.macro POP_REGS
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
.endm

irq1_stub:
    cld
    PUSH_REGS
    sub rsp, 8
    call wildnix_irq1_c_handler
    add rsp, 8
    POP_REGS
    iretq