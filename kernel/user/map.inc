; Construct a private user leaf from a proved kernel identity alias.
; RCX kernel root, RDX backing page, R8 desired user flags (5 or NX|7).
; RAX exact leaf or0. Never changes tables or cache controls.
user_leaf_build:
    push rbx
    push r12
    push r13
    sub rsp,32
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,r12
    jne .bad
    mov r9,0x8000000000000007
    and r9,rax
    mov r10,PTE_NX|3
    test r13,2
    jnz .permissions
    mov r10,PTE_NX|1
.permissions:
    cmp r9,r10
    jne .bad
    mov rcx,rax
    mov rdx,r8
    call paging_pat_index
    mov edx,eax
    and eax,3
    shl eax,3
    and edx,4
    shl edx,5
    or eax,edx
    or rax,r12
    or rax,r13
    jmp .return
.bad:
    xor eax,eax
.return:
    add rsp,32
    pop r13
    pop r12
    pop rbx
    ret

; RCX USER_CONTEXT stage1. Kernel identity mapping must be complete; builder
; remains inactive. All new table pages are exact paging-owned cursor pages.
; EAX0 stage2,1 poisoned/quarantined. No runnable/readiness publication here.
user_roots_prepare:
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
    cmp qword [rbx+USER_CONTEXT.stage],1
    jne .bad
    cmp qword [rbx+USER_CONTEXT.active],0
    jne .bad
    cmp qword [rbx+USER_CONTEXT.phase],0
    jne .bad
    mov r12,[rbx+USER_CONTEXT.builder]
    cmp qword [r12+OWNED_PAGING.ready],0
    jne .bad
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    mov r15,[r12+OWNED_PAGING.root]
    cmp r15,[rbx+USER_CONTEXT.kernel_root]
    jne .bad
    cmp qword [r15+USER_PML4_SLOT*8],0
    jne .bad
    ; Every shared subtree must remain inaccessible from CPL3.
    xor ecx,ecx
.kernel_root:
    test qword [r15+rcx*8],4
    jnz .bad
    inc ecx
    cmp ecx,512
    jb .kernel_root
    mov r13,[rbx+USER_CONTEXT.processes]
    xor ebp,ebp
.process:
    cmp qword [r13+USER_PROCESS.backing],0
    je .bad
    lea eax,[rbp+FRAME_OWNER_USER_BASE+1]
    cmp [r13+USER_PROCESS.owner],rax
    jne .bad
    xor r14d,r14d
.fresh:
    cmp qword [r13+r14*8],0
    jne .bad
    inc r14d
    cmp r14d,4
    jb .fresh
    xor r14d,r14d
.tables:
    mov rcx,r12
    call owned_table_alloc
    test rax,rax
    jz .bad
    mov [r13+r14*8],rax
    inc r14d
    cmp r14d,4
    jb .tables
    mov rsi,r15
    mov rdi,[r13+USER_PROCESS.root]
    mov ecx,512
    rep movsq
    mov rdi,[r13+USER_PROCESS.root]
    mov rax,[r13+USER_PROCESS.pdpt]
    or rax,7
    mov [rdi+USER_PML4_SLOT*8],rax
    mov rdi,[r13+USER_PROCESS.pdpt]
    mov rax,[r13+USER_PROCESS.pd]
    or rax,7
    mov [rdi],rax
    mov rdi,[r13+USER_PROCESS.pd]
    mov rax,[r13+USER_PROCESS.pt]
    or rax,7
    mov [rdi],rax
    xor r14d,r14d
.leaves:
    mov rcx,r15
    mov rdx,[r13+USER_PROCESS.backing]
    mov rax,r14
    shl rax,12
    add rdx,rax
    mov r8,PTE_NX|7
    test r14d,r14d
    jnz .leaf
    mov r8d,5
.leaf:
    call user_leaf_build
    test rax,rax
    jz .bad
    mov rdx,r14
    cmp r14d,2
    jb .slot
    inc rdx                    ; absent user guard at slot2
.slot:
    mov rdi,[r13+USER_PROCESS.pt]
    mov [rdi+rdx*8],rax
    inc r14d
    cmp r14d,6
    jb .leaves
    add r13,USER_PROCESS_size
    inc ebp
    cmp ebp,USER_PROCESS_COUNT
    jb .process
    mov qword [rbx+USER_CONTEXT.stage],2
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
