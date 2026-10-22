; Linux host-only SysV wrappers for the shipped firmware-ABI routines.
bits 64
default rel
%include 'sdk/boot.inc'
global host_range_reserve, host_image_check, host_candidates_build
section .text
host_range_reserve:
    sub rsp,40
    mov rcx,rdi
    mov r8,rdx
    mov rdx,rsi
    call range_reserve
    add rsp,40
    ret
host_image_check:
    sub rsp,40
    mov rcx,rdi
    mov edx,esi
    call memory_image_check
    add rsp,40
    ret
host_candidates_build:
    sub rsp,40
    mov rcx,rdi
    mov r8d,edx
    mov rdx,rsi
    call memory_candidates_build
    add rsp,40
    ret
%include 'kernel/mm/ranges.inc'
%include 'kernel/mm/candidates.inc'
