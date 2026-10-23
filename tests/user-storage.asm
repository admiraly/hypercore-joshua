bits 64
default rel
%include 'tests/owned-map.asm'
%include 'sdk/user.inc'
global host_user_storage, host_user_boundary
host_user_storage:
    sub rsp,40
    mov rcx,rdi
    call user_storage_prepare
    add rsp,40
    ret
host_user_boundary:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call user_code_boundary
    mov [rbx],r9
    add rsp,32
    pop rbx
    ret
%include 'kernel/user/storage.inc'

global host_user_roots
host_user_roots:
    sub rsp,40
    mov rcx,rdi
    call user_roots_prepare
    add rsp,40
    ret
%include 'kernel/mm/paging-pat.inc'
%include 'kernel/user/map.inc'

global host_user_proof
host_user_proof:
    sub rsp,40
    mov rcx,rdi
    call user_roots_check
    add rsp,40
    ret
%include 'kernel/user/proof.inc'
; Context, operation0 storage/1 roots/2 proof/3 boundary, cursor, end.
; Assert all eight firmware integer nonvolatile registers on actual helper paths.
global host_user_storage_abi
host_user_storage_abi:
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
    cmp qword [rsp+40],0
    je .storage
    cmp qword [rsp+40],1
    je .roots
    cmp qword [rsp+40],2
    je .proof
    mov rdx,[rsp+48]
    mov r8,[rsp+56]
    call user_code_boundary
    xor eax,eax
    jmp .check
.storage:
    call user_storage_prepare
    jmp .check
.roots:
    call user_roots_prepare
    jmp .check
.proof:
    call user_roots_check
.check:
%macro user_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    user_abi_check rbx,0x123456789abcde00
    user_abi_check rbp,0x123456789abcde11
    user_abi_check rsi,0x123456789abcde22
    user_abi_check rdi,0x123456789abcde33
    user_abi_check r12,0x123456789abcde44
    user_abi_check r13,0x123456789abcde55
    user_abi_check r14,0x123456789abcde66
    user_abi_check r15,0x123456789abcde77
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
