bits 64
default rel
%include 'tests/owned-map.asm'
global host_lifecycle_prepare, host_lifecycle_alloc, host_lifecycle_free
host_lifecycle_prepare:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call lifecycle_prepare
    add rsp,40
    ret
host_lifecycle_alloc:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call lifecycle_alloc
    mov [rbx],rdx
    add rsp,32
    pop rbx
    ret
host_lifecycle_free:
    sub rsp,40
    mov r9,rcx
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call lifecycle_free
    add rsp,40
    ret
lifecycle_platform_check:
    xor eax,eax
    ret
; Log VA, current target PTE, free count and total nonempty PT leaves.
lifecycle_invalidate:
    mov rax,[host_event_count]
    cmp rax,65536
    jae .overflow
    shl rax,5
    lea r8,[host_events]
    add r8,rax
    mov [r8],rdx
    mov rax,MAPPING_ARENA_BASE
    sub rdx,rax
    shr rdx,9
    mov r9,[rcx+MAPPING_CONTEXT.pt]
    mov rax,[r9+rdx]
    mov [r8+8],rax
    mov rax,[rcx+MAPPING_CONTEXT.builder]
    mov rax,[rax+OWNED_PAGING.allocator]
    mov rax,[rax+FRAME_ALLOCATOR.free_pages]
    mov [r8+16],rax
    xor eax,eax
    xor edx,edx
.count:
    cmp qword [r9+rdx*8],0
    je .next
    inc eax
.next:
    inc edx
    cmp edx,512
    jb .count
    mov [r8+24],rax
    inc qword [host_event_count]
    ret
.overflow:
    ud2
; Explicitly check the eight integer nonvolatile registers of the firmware ABI.
global host_lifecycle_abi
%macro assert_lifecycle_reg 2
    mov rax,%2
    cmp %1,rax
    jne .abi_bad
%endmacro
%macro assert_lifecycle_regs 0
    assert_lifecycle_reg rbx,0x123456789abcde00
    assert_lifecycle_reg rbp,0x123456789abcde11
    assert_lifecycle_reg rsi,0x123456789abcde22
    assert_lifecycle_reg rdi,0x123456789abcde33
    assert_lifecycle_reg r12,0x123456789abcde44
    assert_lifecycle_reg r13,0x123456789abcde55
    assert_lifecycle_reg r14,0x123456789abcde66
    assert_lifecycle_reg r15,0x123456789abcde77
%endmacro
host_lifecycle_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    mov [rsp+32],rdi
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde11
    mov rsi,0x123456789abcde22
    mov rdi,0x123456789abcde33
    mov r12,0x123456789abcde44
    mov r13,0x123456789abcde55
    mov r14,0x123456789abcde66
    mov r15,0x123456789abcde77
    mov rcx,[rsp+32]
    mov edx,1
    mov r8d,0x55
    call lifecycle_alloc
    test eax,eax
    jnz .abi_bad
    mov [rsp+40],rdx
    assert_lifecycle_regs
    mov rcx,[rsp+32]
    mov rdx,[rsp+40]
    mov r8d,1
    mov r9d,0x55
    call lifecycle_free
    test eax,eax
    jnz .abi_bad
    assert_lifecycle_regs
    xor eax,eax
    jmp .abi_return
.abi_bad:
    mov eax,1
.abi_return:
    add rsp,56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

%include 'kernel/mm/lifecycle.inc'
global host_event_storage
host_event_storage:
    lea rax,[host_event_count]
    ret
section .bss
align 8
host_event_count: resq 1
host_events: resq 65536*4
