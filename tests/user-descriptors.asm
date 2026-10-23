bits 64
default rel
section .text
global host_user_gdt_audit, host_user_gdt_prepare
host_user_gdt_audit:
    sub rsp,40
    mov rcx,rdi
    mov rdx,rsi
    call user_gdt_audit
    add rsp,40
    ret
host_user_gdt_prepare:
    push rbx
    sub rsp,32
    mov rbx,rdx
    mov rcx,rdi
    mov rdx,rsi
    call user_gdt_prepare
    mov [rbx],rdx
    mov [rbx+8],r8
    mov [rbx+16],r9
    mov [rbx+24],r10
    add rsp,32
    pop rbx
    ret
%include 'kernel/user/descriptors.inc'
; Eight firmware integer nonvolatile registers on both helper paths.
global host_user_gdt_abi
host_user_gdt_abi:
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
    mov [rsp+40],rsi
    mov [rsp+48],rdx
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
    cmp qword [rsp+48],0
    jne .prepare
    call user_gdt_audit
    jmp .check
.prepare:
    call user_gdt_prepare
.check:
%macro abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    abi_check rbx,0x123456789abcde00
    abi_check rbp,0x123456789abcde11
    abi_check rsi,0x123456789abcde22
    abi_check rdi,0x123456789abcde33
    abi_check r12,0x123456789abcde44
    abi_check r13,0x123456789abcde55
    abi_check r14,0x123456789abcde66
    abi_check r15,0x123456789abcde77
    jmp .return
.bad:
    mov eax,99
.return:
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
