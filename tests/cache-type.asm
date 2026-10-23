bits 64
default rel
%include 'sdk/boot.inc'
section .text
global host_cache_uc
host_cache_uc:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call cache_controller_uc
    add rsp,40
    ret
%include 'kernel/arch/x86_64/cache-type.inc'
