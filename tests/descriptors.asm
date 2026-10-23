bits 64
default rel
%include 'sdk/boot.inc'
section .text
global host_gdt, host_tss, host_idt, host_guard
host_gdt:
    push rbx
    sub rsp,32
    mov rbx,r8
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call descriptor_gdt_build
    mov [rbx],rdx
    mov [rbx+8],r8
    mov [rbx+16],r9
    mov [rbx+24],r10
    add rsp,32
    pop rbx
    ret
host_tss:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call descriptor_tss_init
    add rsp,40
    ret
host_idt:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call descriptor_idt_build
    add rsp,40
    ret
host_guard:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call stack_guard_boundary
    mov [rbx],r9
    add rsp,32
    pop rbx
    ret
%include 'kernel/arch/x86_64/descriptors.inc'
; Explicitly check the eight integer nonvolatile registers of the firmware ABI.
global host_descriptor_abi
%macro assert_descriptor_reg 2
    mov rax,%2
    cmp %1,rax
    jne .abi_bad
%endmacro
%macro assert_descriptor_regs 0
    assert_descriptor_reg rbx,0x123456789abcde00
    assert_descriptor_reg rbp,0x123456789abcde11
    assert_descriptor_reg rsi,0x123456789abcde22
    assert_descriptor_reg rdi,0x123456789abcde33
    assert_descriptor_reg r12,0x123456789abcde44
    assert_descriptor_reg r13,0x123456789abcde55
    assert_descriptor_reg r14,0x123456789abcde66
    assert_descriptor_reg r15,0x123456789abcde77
%endmacro
host_descriptor_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    mov [rsp+32],rdi
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde11
    mov rsi,0x123456789abcde22
    mov rdi,0x123456789abcde33
    mov r12,0x123456789abcde44
    mov r13,0x123456789abcde55
    mov r14,0x123456789abcde66
    mov r15,0x123456789abcde77
    mov rax,[rsp+32]
    mov rcx,[rax]
    mov rdx,[rax+8]
    mov r8,[rax+16]
    mov r9,[rax+24]
    call descriptor_gdt_build
    test eax,eax
    jnz .abi_bad
    assert_descriptor_regs
    mov rax,[rsp+32]
    mov rcx,[rax+24]
    mov edx,0x1234
    mov r8,[rax+32]
    call descriptor_tss_init
    assert_descriptor_regs
    mov rax,[rsp+32]
    mov rcx,[rax+40]
    mov rdx,[rax+48]
    mov r8d,0x20
    call descriptor_idt_build
    assert_descriptor_regs
    mov rax,[rsp+32]
    mov rcx,[rax+56]
    xor edx,edx
    mov r8d,4096
    call stack_guard_boundary
    assert_descriptor_regs
    xor eax,eax
    jmp .abi_return
.abi_bad:
    mov eax,1
.abi_return:
    add rsp,56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
