; RCX trusted fresh USER_CONTEXT, its builder/records/payload are caller-owned.
; Called before kernel identity mapping. EAX0 prepared,1 failed/quarantined.
; Failed partial backing remains owned; never publishes readiness or frees it.
user_storage_prepare:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov rbx,rcx
    cmp qword [rbx+USER_CONTEXT.ready],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.failed],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.stage],0
    jne .bad
    mov r12,[rbx+USER_CONTEXT.builder]
    test r12,r12
    jz .bad
    cmp qword [r12+OWNED_PAGING.ready],0
    jne .bad
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    mov rax,[r12+OWNED_PAGING.root]
    test rax,rax
    jz .bad
    mov [rbx+USER_CONTEXT.kernel_root],rax
    mov r13,[rbx+USER_CONTEXT.processes]
    test r13,r13
    jz .bad
    test r13,7
    jnz .bad
    cmp qword [rbx+USER_CONTEXT.payload],0
    je .bad
    mov rax,[rbx+USER_CONTEXT.payload_size]
    test rax,rax
    jz .bad
    cmp rax,4096
    ja .bad
    ; Both output records must be fresh before any allocation/publication.
    mov rsi,r13
    mov ecx,2*USER_PROCESS_size/8
.fresh:
    cmp qword [rsi],0
    jne .bad
    add rsi,8
    loop .fresh
    xor ebp,ebp
.allocate:
    mov rcx,[r12+OWNED_PAGING.allocator]
    mov edx,6
    lea r8d,[rbp+FRAME_OWNER_USER_BASE+1]
    mov r9d,1
    call frame_alloc
    test eax,eax
    jnz .bad
    mov [r13+USER_PROCESS.backing],rdx
    lea eax,[rbp+FRAME_OWNER_USER_BASE+1]
    mov [r13+USER_PROCESS.owner],rax
    mov rdi,rdx
    mov rsi,[rbx+USER_CONTEXT.payload]
    mov rcx,[rbx+USER_CONTEXT.payload_size]
    rep movsb
    ; Private marker in data, not an exposed physical pointer or kernel register.
    mov rax,0x4a53485500000000
    or rax,rbp
    mov [rdx+4096+8],rax
    add r13,USER_PROCESS_size
    inc ebp
    cmp ebp,USER_PROCESS_COUNT
    jb .allocate
    mov qword [rbx+USER_CONTEXT.stage],1
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+USER_CONTEXT.ready],0
    mov qword [rbx+USER_CONTEXT.failed],1
    mov eax,1
.return:
    add rsp,40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX prepared context, RDX identity-map cursor, R8 exclusive end.
; RAX next physical policy boundary, R9D1 current page must be RO/NX.
; Read-only. Two backing code pages are distinct owned aligned allocations.
user_code_boundary:
    mov rax,r8
    xor r9d,r9d
    mov r10,[rcx+USER_CONTEXT.processes]
    mov ecx,USER_PROCESS_COUNT
.next:
    mov r11,[r10+USER_PROCESS.backing]
    cmp rdx,r11
    jb .before
    add r11,4096
    cmp rdx,r11
    jae .advance
    cmp rax,r11
    cmova rax,r11
    mov r9d,1
    ret
.before:
    cmp rax,r11
    cmova rax,r11
.advance:
    add r10,USER_PROCESS_size
    dec ecx
    jnz .next
    ret
