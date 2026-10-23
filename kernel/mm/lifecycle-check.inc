; Boot regression uses real high-half translations and INVLPG, not host adapters.
; EAX=0 complete/empty/accounting restored; 1 stop. No allocation survives success.
lifecycle_smoke:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    sub rsp,40
    lea r12,[mapping_context]
    mov r13,[frame_allocator+FRAME_ALLOCATOR.free_pages]
    mov rcx,r12
    mov edx,3
    mov r8d,0x534d4f4b
    call lifecycle_alloc
    test eax,eax
    jnz .bad
    mov rbx,rdx
    mov rdi,rdx
    mov ecx,3*4096/8
    mov rax,0x4a4f534855414d4d
    rep stosq
    mov rsi,rbx
    mov ecx,3*4096/8
.read:
    cmp [rsi],rax
    jne .bad
    add rsi,8
    loop .read
    mov rcx,r12
    mov rdx,rbx
    mov r8d,3
    mov r9d,0x534d4f4c
    call lifecycle_free
    cmp eax,1
    jne .bad
    mov rcx,r12
    lea rdx,[rbx+4096]
    mov r8d,2
    mov r9d,0x534d4f4b
    call lifecycle_free
    cmp eax,1
    jne .bad
    call .release
    test eax,eax
    jnz .bad
    mov rcx,[owned_paging+OWNED_PAGING.root]
    mov rdx,rbx
    call paging_lookup
    test rax,rax
    jnz .bad
    call .release
    cmp eax,1
    jne .bad
    mov rcx,r12
    mov edx,3
    mov r8d,0x534d4f4b
    call lifecycle_alloc
    test eax,eax
    jnz .bad
    cmp rdx,rbx
    jne .bad
    mov rsi,rdx
    mov ecx,3*4096/8
.zero:
    cmp qword [rsi],0
    jne .bad
    add rsi,8
    loop .zero
    call .release
    test eax,eax
    jnz .bad
    cmp [frame_allocator+FRAME_ALLOCATOR.free_pages],r13
    jne .bad
    cmp qword [r12+MAPPING_CONTEXT.count],0
    jne .bad
    mov rsi,[r12+MAPPING_CONTEXT.pt]
    mov ecx,512
.empty:
    cmp qword [rsi],0
    jne .bad
    add rsi,8
    loop .empty
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
.release:
    ; Entered by CALL with RSP aligned as a callee; allocate fresh shadow space.
    sub rsp,40
    mov rcx,r12
    mov rdx,rbx
    mov r8d,3
    mov r9d,0x534d4f4b
    call lifecycle_free
    add rsp,40
    ret
