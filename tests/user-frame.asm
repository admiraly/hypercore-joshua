bits 64
default rel
%include 'sdk/boot.inc'
%include 'sdk/user.inc'
section .text
global host_user_frame, host_user_fault
host_user_frame:
    sub rsp,40
    mov r8,rdx
    mov rcx,rdi
    mov rdx,rsi
    call user_frame_check
    add rsp,40
    ret
host_user_fault:
    sub rsp,40
    mov r8,rdx
    mov rcx,rdi
    mov rdx,rsi
    call user_fault_check
    add rsp,40
    ret
%include 'kernel/user/frame.inc'

; SysV context, frame, current root, operation(0 returning IRQ/1 discarded fault).
; Check the actual policy helpers' eight integer nonvolatile ABI registers.
global host_user_frame_abi
host_user_frame_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,72
    mov [rsp+32],rdi
    mov [rsp+40],rsi
    mov [rsp+48],rdx
    mov [rsp+56],rcx
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde11
    mov rsi,0x123456789abcde22
    mov rdi,0x123456789abcde33
    mov r12,0x123456789abcde44
    mov r13,0x123456789abcde55
    mov r14,0x123456789abcde66
    mov r15,0x123456789abcde77
    mov rcx,[rsp+32]
    mov rdx,[rsp+40]
    mov r8,[rsp+48]
    cmp qword [rsp+56],0
    jne .fault
    call user_frame_check
    jmp .check
.fault:
    call user_fault_check
.check:
%macro user_frame_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    user_frame_abi_check rbx,0x123456789abcde00
    user_frame_abi_check rbp,0x123456789abcde11
    user_frame_abi_check rsi,0x123456789abcde22
    user_frame_abi_check rdi,0x123456789abcde33
    user_frame_abi_check r12,0x123456789abcde44
    user_frame_abi_check r13,0x123456789abcde55
    user_frame_abi_check r14,0x123456789abcde66
    user_frame_abi_check r15,0x123456789abcde77
    jmp .return
.bad:
    mov eax,99
.return:
    add rsp,72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
