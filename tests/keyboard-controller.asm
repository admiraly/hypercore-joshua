bits 64
default rel
%include 'sdk/keyboard.inc'
section .text
global host_keyboard_controller_abi
host_keyboard_controller_abi:
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
    je .prepare
    cmp qword [rsp+56],1
    je .enable
    call keyboard_controller_disable
    jmp .check
.prepare:
    call keyboard_controller_prepare
    jmp .check
.enable:
    call keyboard_controller_enable
.check:
%macro controller_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    controller_abi_check rbx,0x123456789abcde00
    controller_abi_check rbp,0x123456789abcde11
    controller_abi_check rsi,0x123456789abcde22
    controller_abi_check rdi,0x123456789abcde33
    controller_abi_check r12,0x123456789abcde44
    controller_abi_check r13,0x123456789abcde55
    controller_abi_check r14,0x123456789abcde66
    controller_abi_check r15,0x123456789abcde77
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

%include 'kernel/input/controller.inc'
; Host-only fixed backend wrappers. Preserve MS-ABI RSI/RDI around SysV callback.
%macro keyboard_model_port 2
%1:
    push rdi
    push rsi
    sub rsp,40
    mov esi,ecx
    mov edi,%2
    call [model_callback]
    add rsp,40
    pop rsi
    pop rdi
    ret
%endmacro
keyboard_model_port keyboard_status,0
keyboard_model_port keyboard_data_read,1
keyboard_model_port keyboard_command_write,2
keyboard_model_port keyboard_data_write,3
global host_keyboard_backend
host_keyboard_backend:
    mov [model_callback],rdi
    ret
section .data
model_callback: dq 0

