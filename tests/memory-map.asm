; Linux host-only test executable for the exact shipped pure validator.
bits 64
default rel
%include 'sdk/boot.inc'
global _start
section .text
_start:
    lea r12,[cases]
    mov r13d,1
.next:
    mov rcx,[r12]
    mov rdx,[r12+8]
    mov r8,[r12+16]
    mov r9,[r12+24]
    call memory_map_validate
    cmp rax,[r12+32]
    jne .fail
    add r12,40
    inc r13d
    cmp r12,cases_end
    jb .next
    xor edi,edi
    jmp .exit
.fail:
    mov edi,r13d
.exit:
    mov eax,60
    syscall
%include 'kernel/mm/map.inc'
section .rodata
%macro descriptor 3
    dq %1,%2,0,%3,0
%endmacro
one: descriptor 7,0x1000,1
unknown: descriptor 0xffffffff,0x1000,1
zero: descriptor 7,0x1000,0
unaligned: descriptor 7,0x1001,1
boundary: descriptor 7,0xfffff000,1
outside: descriptor 7,0x100000000,1
wrap: descriptor 7,0xfffffffffffff000,1
huge: descriptor 7,0,0x10000000000000
adjacent:
    descriptor 7,0x2000,1
    descriptor 7,0x1000,1
intersect:
    descriptor 7,0x1000,2
    descriptor 0,0x2000,1
contained:
    descriptor 7,0x1000,4
    descriptor 7,0x2000,1
duplicate:
    descriptor 7,0x1000,1
    descriptor 7,0x1000,1
extended:
    descriptor 7,0x1000,1
    dq 0x12345678
    descriptor 7,0x3000,1
    dq 0xabcdef
align 8
cases:
    dq one,40,40,32,0
    dq unknown,40,40,32,0
    dq adjacent,80,40,32,0
    dq extended,96,48,32,0
    dq boundary,40,40,32,0
    dq 0,40,40,32,1
    dq one,0,40,32,1
    dq one,MAP_CAPACITY+1,40,32,1
    dq one,40,0,32,1
    dq one,40,32,32,1
    dq one,40,41,32,1
    dq one,40,48,32,1
    dq one,41,40,32,1
    dq one,40,40,31,1
    dq one,40,40,53,1
    dq -16,40,40,32,1
    dq zero,40,40,32,1
    dq unaligned,40,40,32,1
    dq outside,40,40,32,1
    dq wrap,40,40,52,1
    dq huge,40,40,52,1
    dq intersect,80,40,32,1
    dq contained,80,40,32,1
    dq duplicate,80,40,32,1
cases_end:
