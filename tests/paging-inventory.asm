; Host-only SysV wrappers; tested routines are the shipped Assembly source.
bits 64
default rel
%include 'sdk/boot.inc'
global host_paging_build, host_paging_reserve
section .text
host_paging_build:
    sub rsp,40
    mov rcx,rdi
    call paging_inventory_build
    add rsp,40
    ret
host_paging_reserve:
    sub rsp,40
    mov rcx,rdi
    mov rdx,rsi
    call paging_inventory_reserve
    add rsp,40
    ret
%include 'kernel/mm/ranges.inc'
%include 'kernel/mm/paging-inventory.inc'
