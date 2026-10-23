bits 64
default rel
%include 'sdk/keyboard.inc'
%include 'sdk/console.inc'
section .text
global host_model_abi
host_model_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,136
    mov [rsp+48],rdi
    mov [rsp+56],rsi
    mov [rsp+64],rdx
    mov [rsp+72],rcx
    mov [rsp+80],r8
    mov [rsp+88],r9
    mov rax,[rsp+208]
    mov [rsp+96],rax
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde11
    mov rsi,0x123456789abcde22
    mov rdi,0x123456789abcde33
    mov r12,0x123456789abcde44
    mov r13,0x123456789abcde55
    mov r14,0x123456789abcde66
    mov r15,0x123456789abcde77
    mov rcx,[rsp+48]
    mov rdx,[rsp+56]
    mov r8,[rsp+64]
    mov r9,[rsp+72]
    mov rax,[rsp+80]
    mov [rsp+32],rax
    mov rax,[rsp+88]
    mov [rsp+40],rax
    mov rax,[rsp+96]
    cmp eax,0
    je .init
    cmp eax,1
    je .validate
    cmp eax,2
    je .prompt
    cmp eax,3
    je .complete
    cmp eax,6
    je .parse
    cmp eax,7
    je .build
    mov rcx,[rsp+56]
    mov rdx,[rsp+64]
    cmp eax,4
    je .grid
    call grid_render
    jmp .packed
.parse:
    mov rcx,[rsp+72]
    mov rdx,[rsp+80]
    call command_parse
    jmp .packed
.build:
    mov rcx,[rsp+80]
    mov rdx,[rsp+72]
    mov r8,[rsp+88]
    call response_build
    jmp .packed
.grid:
    call grid_init
    jmp .packed
.init:
    call model_init
    jmp .packed
.validate:
    call model_check
    jmp .packed
.prompt:
    call model_prompt
    jmp .packed
.complete:
    call model_complete
.packed:
    shl rdx,32
    or rax,rdx
.check:
%macro editor_abi_check 2
    mov r11,%2
    cmp %1,r11
    jne .bad
%endmacro
    editor_abi_check rbx,0x123456789abcde00
    editor_abi_check rbp,0x123456789abcde11
    editor_abi_check rsi,0x123456789abcde22
    editor_abi_check rdi,0x123456789abcde33
    editor_abi_check r12,0x123456789abcde44
    editor_abi_check r13,0x123456789abcde55
    editor_abi_check r14,0x123456789abcde66
    editor_abi_check r15,0x123456789abcde77
    jmp .return
.bad:
    mov eax,99
.return:
    add rsp,136
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

%include 'kernel/console/editor.inc'
%include 'kernel/console/grid.inc'
%include 'kernel/console/command.inc'
%include 'kernel/console/response.inc'
%include 'kernel/console/model.inc'
section .rodata
%include 'kernel/console/font.inc'
%include 'kernel/console/command-data.inc'
%include 'kernel/console/response-data.inc'
%include 'kernel/console/model-data.inc'
