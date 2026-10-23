bits 64
default rel
%include 'sdk/context.inc'
section .text
global host_context_check, host_context_select, host_context_abi
host_context_check:
    sub rsp,40
    mov r8,rdx
    mov edx,esi
    mov rcx,rdi
    call context_frame_check
    add rsp,40
    ret
host_context_select:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov r8d,edx
    mov rdx,rsi
    mov rcx,rdi
    call context_select
    mov [rbx],rdx
    add rsp,32
    pop rbx
    ret
; Eight firmware integer nonvolatile sentinels across actual policy.
host_context_abi:
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
    mov rbx,0x1122334455667788
    mov rbp,0x2233445566778899
    mov rsi,0x33445566778899aa
    mov rdi,0x445566778899aabb
    mov r12,0x5566778899aabbcc
    mov r13,0x66778899aabbccdd
    mov r14,0x778899aabbccddee
    mov r15,0x8899aabbccddeeff
    mov rcx,[rsp+32]
    mov rdx,[rsp+40]
    mov r8,[rsp+48]
    call context_select
%macro check 2
    mov r10,%2
    cmp %1,r10
    jne .bad
%endmacro
    check rbx,0x1122334455667788
    check rbp,0x2233445566778899
    check rsi,0x33445566778899aa
    check rdi,0x445566778899aabb
    check r12,0x5566778899aabbcc
    check r13,0x66778899aabbccdd
    check r14,0x778899aabbccddee
    check r15,0x8899aabbccddeeff
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
%include 'kernel/sched/context.inc'
