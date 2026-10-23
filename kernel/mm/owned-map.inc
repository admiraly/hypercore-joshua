; Private table-page cursor. RCX builder -> RAX zeroed physical page or zero.
; Every whole pool chunk remains an exact allocator record, including unused pages.
owned_table_alloc:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov rax,[rbx+OWNED_PAGING.pool_next]
    cmp rax,[rbx+OWNED_PAGING.pool_end]
    jb .take
    mov rcx,[rbx+OWNED_PAGING.allocator]
    mov edx,64
    mov r8d,FRAME_OWNER_PAGING
    mov r9d,1
    call frame_alloc
    test eax,eax
    jz .chunk
    cmp eax,2
    jne .bad
    mov rcx,[rbx+OWNED_PAGING.allocator]
    mov edx,1
    mov r8d,FRAME_OWNER_PAGING
    mov r9d,1
    call frame_alloc
    test eax,eax
    jnz .bad
    lea rax,[rdx+4096]
    jmp .pool
.chunk:
    lea rax,[rdx+64*4096]
.pool:
    mov [rbx+OWNED_PAGING.pool_end],rax
    mov [rbx+OWNED_PAGING.pool_next],rdx
    mov rax,rdx
.take:
    add qword [rbx+OWNED_PAGING.pool_next],4096
    inc qword [rbx+OWNED_PAGING.table_count]
    jmp .return
.bad:
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret

; RCX zeroed builder, RDX ready allocator, R8 power-of-two physical limit <=2^47.
; Caller may prefill old_root; root must be zero. EAX=0 success, 1 poisons builder.
owned_paging_init:
    push rbx
    sub rsp,32
    mov rbx,rcx
    cmp qword [rbx+OWNED_PAGING.root],0
    jne .bad
    cmp qword [rbx+OWNED_PAGING.failed],0
    jne .bad
    test rdx,rdx
    jz .bad
    mov rax,0x100000000
    cmp r8,rax
    jb .bad
    mov rax,0x0000800000000000
    cmp r8,rax
    ja .bad
    lea rax,[r8-1]
    test r8,rax
    jnz .bad
    mov [rbx+OWNED_PAGING.allocator],rdx
    mov [rbx+OWNED_PAGING.physical_limit],r8
    call owned_table_alloc
    test rax,rax
    jz .bad
    mov [rbx+OWNED_PAGING.root],rax
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+OWNED_PAGING.failed],1
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; Private: RCX builder, RDX owned parent-entry pointer -> RAX child or zero.
owned_child:
    push rbx
    push rsi
    sub rsp,40
    mov rbx,rcx
    mov rsi,rdx
    mov rax,[rsi]
    test rax,rax
    jnz .present
    call owned_table_alloc
    test rax,rax
    jz .return
    mov rdx,rax
    or rdx,3
    mov [rsi],rdx
    jmp .return
.present:
    test al,1
    jz .bad
    test al,0x80
    jnz .bad
    mov rdx,0x000ffffffffff000
    and rax,rdx
    jmp .return
.bad:
    xor eax,eax
.return:
    add rsp,40
    pop rsi
    pop rbx
    ret

; RCX builder, RDX aligned start, R8 exclusive end, R9 leaf flags in 4K encoding.
; Internal mapper: caller bounds each request by an inherited leaf and policy run.
; EAX=0 success; 1 poisons root. No replacement of existing leaves, no W+X or U/S.
owned_paging_map:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov r12,rcx
    mov r13,rdx
    mov r14,r8
    mov r15,r9
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    cmp qword [r12+OWNED_PAGING.root],0
    je .bad
    test rdx,rdx
    jz .bad
    test rdx,0xfff
    jnz .bad
    test r8,0xfff
    jnz .bad
    cmp rdx,r8
    jae .bad
    cmp r8,[r12+OWNED_PAGING.physical_limit]
    ja .bad
    mov rax,0x7fffffffffffff64
    test r9,rax
    jnz .bad
    test r9b,1
    jz .bad
    test r9b,2
    jz .page
    bt r9,63
    jnc .bad
.page:
    mov ebx,4096
    test r13d,0x1fffff
    jnz .walk
    mov rax,r14
    sub rax,r13
    cmp rax,0x200000
    jb .walk
    mov ebx,0x200000
.walk:
    mov rdx,r13
    shr rdx,39
    and edx,511
    shl rdx,3
    add rdx,[r12+OWNED_PAGING.root]
    mov rcx,r12
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,r13
    shr rdx,30
    and edx,511
    lea rdx,[rax+rdx*8]
    mov rcx,r12
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,r13
    shr rdx,21
    and edx,511
    lea rdi,[rax+rdx*8]
    mov rsi,r15
    cmp ebx,0x200000
    je .large
    mov rcx,r12
    mov rdx,rdi
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,r13
    shr rdx,12
    and edx,511
    lea rdi,[rax+rdx*8]
    jmp .leaf
.large:
    and rsi,-129
    test r15b,0x80
    jz .ps
    or rsi,0x1000
.ps:
    or rsi,0x80
.leaf:
    cmp qword [rdi],0
    jne .bad
    or rsi,r13
    mov [rdi],rsi
    add r13,rbx
    cmp r13,r14
    jb .page
    xor eax,eax
    jmp .return
.bad:
    mov qword [r12+OWNED_PAGING.failed],1
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

; RCX trusted root physical address, RDX canonical low virtual address.
; RAX raw leaf, RDX translation, R8 leaf bytes (4K/2M/1G), or all zero if absent.
; Supports inherited large leaves; does not modify tables or nonvolatile registers.
paging_lookup:
    mov r9,rcx
    mov r11,rdx
    mov r10d,39
    mov r8d,4
.next:
    mov ecx,r10d
    mov rdx,r11
    shr rdx,cl
    and edx,511
    mov rax,[r9+rdx*8]
    test al,1
    jz .absent
    cmp r8d,1
    je .small
    test al,0x80
    jnz .large
    mov r9,0x000ffffffffff000
    and r9,rax
    sub r10d,9
    dec r8d
    jmp .next
.large:
    cmp r8d,3
    je .one_g
    cmp r8d,2
    jne .absent
    mov r8d,0x200000
    jmp .translate
.one_g:
    mov r8d,0x40000000
    jmp .translate
.small:
    mov r8d,4096
.translate:
    mov rdx,0x000ffffffffff000
    and rdx,rax
    mov rcx,r8
    neg rcx
    and rdx,rcx
    mov rcx,r8
    dec rcx
    and rcx,r11
    add rdx,rcx
    ret
.absent:
    xor eax,eax
    xor edx,edx
    xor r8d,r8d
    ret

; RCX builder, RDX start, R8 end, R9 permission flags (without cache selectors).
; Prove inherited identity and preserve its PAT selection without leaf promotion.
owned_map_preserved:
    push rbx
    push rsi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov r12,rcx
    mov rbx,rdx
    mov r13,r8
    mov r14,r9
    test r9b,0x98
    jnz .bad
    cmp qword [r12+OWNED_PAGING.old_root],0
    je .bad
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    test rbx,rbx
    jz .bad
    test rbx,0xfff
    jnz .bad
    test r13,0xfff
    jnz .bad
    cmp rbx,r13
    jae .bad
    cmp r13,[r12+OWNED_PAGING.physical_limit]
    ja .bad
.next:
    cmp rbx,r13
    jae .done
    mov rcx,[r12+OWNED_PAGING.old_root]
    mov rdx,rbx
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,rbx
    jne .bad
    mov rsi,rax
    and esi,0x18
    cmp r8,4096
    je .pat_small
    test eax,0x1000
    jz .boundary
    or esi,0x80
    jmp .boundary
.pat_small:
    test al,0x80
    jz .boundary
    or esi,0x80
.boundary:
    mov r15,r8
    neg r15
    and r15,rbx
    add r15,r8
    cmp r15,r13
    jbe .map
    mov r15,r13
.map:
    mov rcx,r12
    mov rdx,rbx
    mov r8,r15
    mov r9,r14
    or r9,rsi
    call owned_paging_map
    test eax,eax
    jnz .bad
    mov rbx,r15
    jmp .next
.done:
    xor eax,eax
    jmp .return
.bad:
    mov qword [r12+OWNED_PAGING.failed],1
    mov eax,1
.return:
    add rsp,40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
