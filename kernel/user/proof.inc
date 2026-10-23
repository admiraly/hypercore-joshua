; RCX trusted builder, RDX aligned start, R8 pages, R9 owner, R10D exact flag.
; EAX0 iff exactly one matching live allocator record owns the entire range.
; Caller owns allocator/record metadata. Never dereferences the proposed range.
user_range_owned:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,32
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    mov r14,r9
    mov edi,r10d
    test r12,r12
    jz .bad
    test r12,4095
    jnz .bad
    test r13,r13
    jz .bad
    mov rax,[rbx+OWNED_PAGING.physical_limit]
    shr rax,12
    cmp r13,rax
    ja .bad
    shl r13,12
    add r13,r12
    jc .bad
    cmp r13,[rbx+OWNED_PAGING.physical_limit]
    ja .bad
    mov rsi,[rbx+OWNED_PAGING.allocator]
    mov rax,FRAME_ALLOCATOR_MAGIC
    cmp [rsi+FRAME_ALLOCATOR.ready],rax
    jne .bad
    mov rcx,[rsi+FRAME_ALLOCATOR.count]
    cmp rcx,[rsi+FRAME_ALLOCATOR.capacity]
    ja .bad
    cmp qword [rsi+FRAME_ALLOCATOR.capacity],FRAME_ALLOCATION_CAPACITY
    ja .bad
    mov r11,[rsi+FRAME_ALLOCATOR.records]
    test r11,r11
    jz .bad
    test r11,7
    jnz .bad
    xor r15d,r15d
.next:
    test rcx,rcx
    jz .done
    cmp [r11+16],r14
    jne .advance
    mov rax,[r11]
    test rax,4095
    jnz .bad
    cmp rax,r12
    ja .advance
    test edi,edi
    jz .end
    cmp rax,r12
    jne .advance
.end:
    mov r8,[r11+8]
    mov r10,[rbx+OWNED_PAGING.physical_limit]
    shr r10,12
    cmp r8,r10
    ja .bad
    shl r8,12
    add rax,r8
    jc .bad
    cmp rax,[rbx+OWNED_PAGING.physical_limit]
    ja .bad
    cmp rax,r13
    jb .advance
    test edi,edi
    jz .match
    cmp rax,r13
    jne .advance
.match:
    inc r15d
.advance:
    add r11,24
    dec rcx
    jmp .next
.done:
    cmp r15d,1
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
    pop rdi
    pop rsi
    pop rbx
    ret

; RCX USER_CONTEXT stage2. Prove every private frame/table before dereferencing
; its table contents. Shared kernel tables/root are the trusted builder output.
; EAX0 stage3/ready,1 invalidates readiness and quarantines all partial ownership.
user_roots_check:
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
    cmp qword [rbx+USER_CONTEXT.failed],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.ready],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.stage],2
    jne .bad
    cmp qword [rbx+USER_CONTEXT.active],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.phase],0
    jne .bad
    mov r12,[rbx+USER_CONTEXT.builder]
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    mov r15,[r12+OWNED_PAGING.root]
    cmp r15,[rbx+USER_CONTEXT.kernel_root]
    jne .bad
    mov rcx,r12
    mov rdx,r15
    mov r8d,1
    mov r9d,FRAME_OWNER_PAGING
    xor r10d,r10d
    call user_range_owned
    test eax,eax
    jnz .bad
    cmp qword [r15+USER_PML4_SLOT*8],0
    jne .bad
    mov r13,[rbx+USER_CONTEXT.processes]
    xor ebp,ebp
.backing:
    imul r14,rbp,USER_PROCESS_size
    add r14,r13
    lea r9d,[rbp+FRAME_OWNER_USER_BASE+1]
    cmp [r14+USER_PROCESS.owner],r9
    jne .bad
    mov rcx,r12
    mov rdx,[r14+USER_PROCESS.backing]
    mov r8d,6
    mov r10d,1
    call user_range_owned
    test eax,eax
    jnz .bad
    inc ebp
    cmp ebp,USER_PROCESS_COUNT
    jb .backing
    mov rax,[r13+USER_PROCESS.backing]
    mov rdx,[r13+USER_PROCESS_size+USER_PROCESS.backing]
    lea rcx,[rax+6*4096]
    cmp rdx,rcx
    jae .tables_start
    add rdx,6*4096
    cmp rax,rdx
    jb .bad
.tables_start:
    xor esi,esi
.table:
    mov eax,esi
    shr eax,2
    imul r14,rax,USER_PROCESS_size
    add r14,r13
    mov eax,esi
    and eax,3
    mov rdi,[r14+rax*8]
    cmp rdi,r15
    je .bad
    mov rcx,r12
    mov rdx,rdi
    mov r8d,1
    mov r9d,FRAME_OWNER_PAGING
    xor r10d,r10d
    call user_range_owned
    test eax,eax
    jnz .bad
    ; No private table may alias another private table or either backing run.
    xor ecx,ecx
.duplicate:
    cmp ecx,esi
    jae .not_backing
    mov eax,ecx
    shr eax,2
    imul rdx,rax,USER_PROCESS_size
    add rdx,r13
    mov eax,ecx
    and eax,3
    cmp rdi,[rdx+rax*8]
    je .bad
    inc ecx
    jmp .duplicate
.not_backing:
    xor ecx,ecx
.no_alias:
    imul rdx,rcx,USER_PROCESS_size
    mov rax,[r13+rdx+USER_PROCESS.backing]
    cmp rdi,rax
    jb .next_backing
    add rax,6*4096
    cmp rdi,rax
    jb .bad
.next_backing:
    inc ecx
    cmp ecx,USER_PROCESS_COUNT
    jb .no_alias
    inc esi
    cmp esi,USER_PROCESS_COUNT*4
    jb .table
    xor ebp,ebp
.process:
    imul r14,rbp,USER_PROCESS_size
    add r14,r13
    mov rdi,[r14+USER_PROCESS.root]
    xor esi,esi
.root:
    mov rax,[rdi+rsi*8]
    cmp esi,USER_PML4_SLOT
    je .user_root
    mov rdx,[r15+rsi*8]
    test rdx,4
    jnz .bad
    xor rax,rdx
    and rax,-33                ; independent hardware accessed bits allowed
    jnz .bad
    jmp .root_next
.user_root:
    mov rdx,[r14+USER_PROCESS.pdpt]
    or rdx,7
    xor rax,rdx
    and rax,-33
    jnz .bad
.root_next:
    inc esi
    cmp esi,512
    jb .root
    mov rdi,[r14+USER_PROCESS.pdpt]
    mov rdx,[r14+USER_PROCESS.pd]
    call .single_child
    test eax,eax
    jnz .bad
    mov rdi,[r14+USER_PROCESS.pd]
    mov rdx,[r14+USER_PROCESS.pt]
    call .single_child
    test eax,eax
    jnz .bad
    xor esi,esi
.pt:
    xor edx,edx
    cmp esi,7
    jae .expected
    cmp esi,2
    je .expected
    mov rdx,[r14+USER_PROCESS.backing]
    mov eax,esi
    cmp esi,2
    jb .physical
    dec eax
.physical:
    shl rax,12
    add rdx,rax
    mov r8,PTE_NX|7
    test esi,esi
    jnz .leaf
    mov r8d,5
.leaf:
    mov rcx,r15
    call user_leaf_build
    test rax,rax
    jz .bad
    mov rdx,rax
.expected:
    mov rdi,[r14+USER_PROCESS.pt]
    mov rax,[rdi+rsi*8]
    test rdx,rdx
    jz .absent
    xor rax,rdx
    and rax,-97                ; hardware accessed/dirty updates only
.absent:
    test rax,rax
    jnz .bad
    inc esi
    cmp esi,512
    jb .pt
    inc ebp
    cmp ebp,USER_PROCESS_COUNT
    jb .process
    mov qword [rbx+USER_CONTEXT.stage],3
    mov rax,USER_READY_MAGIC
    mov [rbx+USER_CONTEXT.ready],rax
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
; Internal leaf: RDI proved page, RDX expected child. No calls/nonvolatile changes.
.single_child:
    mov rax,[rdi]
    or rdx,7
    xor rax,rdx
    and rax,-33
    jnz .single_bad
    mov ecx,1
.single_next:
    cmp qword [rdi+rcx*8],0
    jne .single_bad
    inc ecx
    cmp ecx,512
    jb .single_next
    xor eax,eax
    ret
.single_bad:
    mov eax,1
    ret
