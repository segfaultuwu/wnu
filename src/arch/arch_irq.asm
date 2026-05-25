BITS 64

global irq1_stub
extern wnu_irq1_c_handler

%macro PUSH_REGS 0
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
%endmacro

%macro POP_REGS 0
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
%endmacro

section .text

irq1_stub:
    cld

    PUSH_REGS

    ; align stack before call
    sub rsp, 8
    call wnu_irq1_c_handler
    add rsp, 8

    POP_REGS

    iretq
