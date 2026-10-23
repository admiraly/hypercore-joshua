bits 64
default rel
%include 'sdk/handles.inc'
section .text
global host_handle_abi
host_handle_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,104
    mov [rsp+32],rdi
    mov [rsp+40],rsi
    mov [rsp+48],rdx
    mov [rsp+56],rcx
    mov [rsp+64],r8
    mov [rsp+72],r9
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde01
    mov rsi,0x123456789abcde02
    mov rdi,0x123456789abcde03
    mov r12,0x123456789abcde04
    mov r13,0x123456789abcde05
    mov r14,0x123456789abcde06
    mov r15,0x123456789abcde07
    mov rcx,[rsp+40]
    mov rdx,[rsp+48]
    mov r8,[rsp+56]
    mov r9,[rsp+64]
    mov rax,[rsp+32]
    cmp eax,0
    je .init
    cmp eax,1
    je .check
    cmp eax,2
    je .grant
    cmp eax,3
    je .lookup
    cmp eax,4
    je .duplicate
    cmp eax,5
    je .close
    cmp eax,6
    je .revoke
    ud2
.init:
    call handle_init
    jmp .verify
.check:
    call handle_check
    jmp .verify
.grant:
    call handle_grant
    jmp .verify
.lookup:
    call handle_lookup
    jmp .verify
.duplicate:
    call handle_duplicate
    jmp .verify
.close:
    call handle_close
    jmp .verify
.revoke:
    call handle_revoke
    jmp .verify
.verify:
    mov [rsp+80],rax
    mov [rsp+88],rdx
    mov [rsp+96],r8
    mov r11,0x123456789abcde00
    cmp rbx,r11
    jne .bad
    mov r11,0x123456789abcde01
    cmp rbp,r11
    jne .bad
    mov r11,0x123456789abcde02
    cmp rsi,r11
    jne .bad
    mov r11,0x123456789abcde03
    cmp rdi,r11
    jne .bad
    mov r11,0x123456789abcde04
    cmp r12,r11
    jne .bad
    mov r11,0x123456789abcde05
    cmp r13,r11
    jne .bad
    mov r11,0x123456789abcde06
    cmp r14,r11
    jne .bad
    mov r11,0x123456789abcde07
    cmp r15,r11
    jne .bad
    mov r10,[rsp+72]
    mov rax,[rsp+88]
    mov [r10],rax
    mov rax,[rsp+96]
    mov [r10+8],rax
    mov rax,[rsp+80]
    jmp .return
.bad:
    mov eax,99
.return:
    add rsp,104
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
%include 'kernel/user/handles.inc'
