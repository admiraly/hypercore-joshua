bits 64
default rel
%include 'sdk/boot.inc'
global host_owned_init, host_owned_map, host_owned_preserved
section .text
host_owned_init:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call owned_paging_init
    add rsp,40
    ret
host_owned_map:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call owned_paging_map
    add rsp,40
    ret
host_owned_preserved:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call owned_map_preserved
    add rsp,40
    ret
; Reuse host-only allocator initialization wrapper; no privileged instructions.
global host_frames_init
host_frames_init:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call frame_allocator_init
    add rsp,40
    ret
%include 'kernel/mm/frames.inc'
%include 'kernel/mm/owned-map.inc'
