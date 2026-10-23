; Linux host-only SysV wrappers around the shipped firmware-ABI allocator.
bits 64
default rel
%include 'sdk/boot.inc'
global host_frames_init, host_frame_alloc, host_frame_free
section .text
host_frames_init:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call frame_allocator_init
    add rsp,40
    ret
host_frame_alloc:
    push rbx
    sub rsp,32
    mov rbx,r8
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call frame_alloc
    mov [rbx],rdx
    add rsp,32
    pop rbx
    ret
host_frame_free:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call frame_free
    add rsp,40
    ret
%include 'kernel/mm/frames.inc'

; Explicitly check the eight integer nonvolatile registers of the firmware ABI.
global host_frame_abi
%macro assert_frame_reg 2
    mov rax,%2
    cmp %1,rax
    jne .abi_bad
%endmacro
%macro assert_frame_regs 0
    assert_frame_reg rbx,0x123456789abcde00
    assert_frame_reg rbp,0x123456789abcde11
    assert_frame_reg rsi,0x123456789abcde22
    assert_frame_reg rdi,0x123456789abcde33
    assert_frame_reg r12,0x123456789abcde44
    assert_frame_reg r13,0x123456789abcde55
    assert_frame_reg r14,0x123456789abcde66
    assert_frame_reg r15,0x123456789abcde77
%endmacro
host_frame_abi:
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
    mov rcx,[rsp+32]
    mov edx,1
    mov r8d,0x55
    mov r9d,1
    call frame_alloc
    test eax,eax
    jnz .abi_bad
    mov [rsp+40],rdx
    assert_frame_regs
    mov rcx,[rsp+32]
    mov rdx,[rsp+40]
    mov r8d,1
    mov r9d,0x55
    call frame_free
    test eax,eax
    jnz .abi_bad
    assert_frame_regs
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
