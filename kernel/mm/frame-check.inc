; Bring-up smoke check. Initializes the allocator only after all reservations.
; Allocates, checks zeroing/alignment, writes RAM, rejects invalid frees, releases
; all test allocations and verifies accounting. EAX=0 success, 1 fatal failure.
frame_allocator_check:
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,32
    lea rbx,[frame_allocator]
    mov rcx,rbx
    lea rdx,[candidate_set]
    lea r8,[frame_records]
    mov r9d,FRAME_ALLOCATION_CAPACITY
    call frame_allocator_init
    test eax,eax
    jnz .bad
    mov r14,[rbx+FRAME_ALLOCATOR.free_pages]
    cmp r14,4
    jb .bad
    mov rcx,rbx
    mov edx,1
    mov r8d,1
    mov r9d,1
    call frame_alloc
    test eax,eax
    jnz .bad
    mov r12,rdx
    test r12,0xfff
    jnz .bad
    mov rax,r12
    mov ecx,512
.zero_first:
    cmp qword [rax],0
    jne .bad
    add rax,8
    dec ecx
    jnz .zero_first
    mov rax,0x4a4f534855414f53
    mov [r12],rax
    cmp [r12],rax
    jne .bad
    mov rcx,rbx
    mov edx,3
    mov r8d,2
    mov r9d,4
    call frame_alloc
    test eax,eax
    jnz .bad
    mov r13,rdx
    test r13,0x3fff
    jnz .bad
    mov rax,r13
    mov ecx,1536
.zero_second:
    cmp qword [rax],0
    jne .bad
    add rax,8
    dec ecx
    jnz .zero_second
    mov rax,0x4859504552434f52
    mov [r13+3*4096-8],rax
    cmp [r13+3*4096-8],rax
    jne .bad
    mov rcx,rbx
    mov rdx,r12
    mov r8d,1
    mov r9d,2
    call frame_free
    cmp eax,1
    jne .bad
    mov rax,r14
    sub rax,4
    cmp [rbx+FRAME_ALLOCATOR.free_pages],rax
    jne .bad
    cmp qword [rbx+FRAME_ALLOCATOR.count],2
    jne .bad
    mov rcx,rbx
    mov rdx,r12
    mov r8d,1
    mov r9d,1
    call frame_free
    test eax,eax
    jnz .bad
    mov rcx,rbx
    mov rdx,r13
    mov r8d,3
    mov r9d,2
    call frame_free
    test eax,eax
    jnz .bad
    mov rcx,rbx
    mov rdx,r12
    mov r8d,1
    mov r9d,1
    call frame_free
    cmp eax,1
    jne .bad
    cmp qword [rbx+FRAME_ALLOCATOR.count],0
    jne .bad
    cmp [rbx+FRAME_ALLOCATOR.free_pages],r14
    jne .bad
    cmp [rbx+FRAME_ALLOCATOR.total_pages],r14
    jne .bad
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
