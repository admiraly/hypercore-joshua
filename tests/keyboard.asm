bits 64
default rel
%include 'sdk/keyboard.inc'
section .text
global host_keyboard_abi
host_keyboard_abi:
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
    je .init
    cmp qword [rsp+56],1
    je .decode
    cmp qword [rsp+56],2
    je .pop
    call keyboard_decoder_resync
    jmp .check
.init:
    call keyboard_decoder_init
    jmp .check
.decode:
    call keyboard_decode
    jmp .check
.pop:
    call keyboard_pop
.check:
%macro keyboard_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    keyboard_abi_check rbx,0x123456789abcde00
    keyboard_abi_check rbp,0x123456789abcde11
    keyboard_abi_check rsi,0x123456789abcde22
    keyboard_abi_check rdi,0x123456789abcde33
    keyboard_abi_check r12,0x123456789abcde44
    keyboard_abi_check r13,0x123456789abcde55
    keyboard_abi_check r14,0x123456789abcde66
    keyboard_abi_check r15,0x123456789abcde77
    cmp qword [rsp+56],2
    jne .return
    mov r11,rdx
    shl r11,32
    or rax,r11
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

%include 'kernel/input/decoder.inc'
section .rodata
%include 'kernel/input/keymap.inc'
