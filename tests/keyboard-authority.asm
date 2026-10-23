bits 64
default rel
%include 'sdk/boot.inc'
%include 'sdk/keyboard.inc'
section .text
global host_keyboard_authority_abi
host_keyboard_authority_abi:
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
    call keyboard_authority_build
.check:
%macro authority_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    authority_abi_check rbx,0x123456789abcde00
    authority_abi_check rbp,0x123456789abcde11
    authority_abi_check rsi,0x123456789abcde22
    authority_abi_check rdi,0x123456789abcde33
    authority_abi_check r12,0x123456789abcde44
    authority_abi_check r13,0x123456789abcde55
    authority_abi_check r14,0x123456789abcde66
    authority_abi_check r15,0x123456789abcde77
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

%include 'kernel/input/authority.inc'
%include 'kernel/arch/x86_64/acpi.inc'
; Model only the paging lookup adapter. The actual ACPI read predicate is shared.
paging_lookup:
    cmp rdx,[model_low]
    jb .bad
    cmp rdx,[model_high]
    jae .bad
    mov rax,[model_flags]
    or rax,rdx
    mov r8d,4096
    ret
.bad:
    xor eax,eax
    ret
global host_keyboard_alias
host_keyboard_alias:
    mov [model_low],rdi
    mov [model_high],rsi
    mov [model_flags],rdx
    ret
section .data
model_low: dq 0
model_high: dq 0
model_flags: dq 0

