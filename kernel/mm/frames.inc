; RCX zero/unready FRAME_ALLOCATOR, RDX completed excluded RANGE_SET,
; R8 owned records array, R9 capacity. EAX=0 success, 1 invalid; no failure writes.
; Caller owns all metadata and supplies RAM from the completed reservation stages.
frame_allocator_init:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov r15,rcx
    cmp qword [r15+FRAME_ALLOCATOR.ready],0
    jne .bad
    test rdx,rdx
    jz .bad
    test r8,r8
    jz .bad
    test r8,7
    jnz .bad
    test r9,r9
    jz .bad
    cmp r9,FRAME_ALLOCATION_CAPACITY
    ja .bad
    mov r14,rdx
    mov r13,r8
    mov r12,r9
    lea rax,[r9+r9*2]
    shl rax,3
    add rax,r8
    jc .bad
    mov rdi,[r14+RANGE_SET.entries]
    test rdi,rdi
    jz .bad
    mov rax,[r14+RANGE_SET.capacity]
    test rax,rax
    jz .bad
    cmp rax,CANDIDATE_CAPACITY
    ja .bad
    cmp [r14+RANGE_SET.count],rax
    ja .bad
    shl rax,4
    add rax,rdi
    jc .bad
    xor ebp,ebp
    xor r8d,r8d
.range:
    cmp rbp,[r14+RANGE_SET.count]
    jae .commit
    mov rsi,rbp
    shl rsi,4
    add rsi,rdi
    mov rbx,[rsi]
    test rbx,rbx
    jz .bad
    test rbx,0xfff
    jnz .bad
    mov r11,[rsi+8]
    test r11,0xfff
    jnz .bad
    cmp rbx,r11
    jae .bad
    mov rax,0x0000800000000000
    cmp r11,rax
    ja .bad
    mov rax,r11
    sub rax,rbx
    shr rax,12
    add r8,rax
    jc .bad
    xor r9d,r9d
.previous:
    cmp r9,rbp
    jae .next
    mov r10,r9
    shl r10,4
    add r10,rdi
    cmp rbx,[r10+8]
    jae .disjoint
    cmp r11,[r10]
    ja .bad
.disjoint:
    inc r9
    jmp .previous
.next:
    inc rbp
    jmp .range
.commit:
    mov [r15+FRAME_ALLOCATOR.ranges],r14
    mov [r15+FRAME_ALLOCATOR.records],r13
    mov qword [r15+FRAME_ALLOCATOR.count],0
    mov [r15+FRAME_ALLOCATOR.capacity],r12
    mov [r15+FRAME_ALLOCATOR.total_pages],r8
    mov [r15+FRAME_ALLOCATOR.free_pages],r8
    mov rax,FRAME_ALLOCATOR_MAGIC
    mov [r15+FRAME_ALLOCATOR.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX allocator, RDX pages, R8 nonzero owner, R9 power-of-two page alignment.
; EAX=0/RDX=base success; EAX=1 invalid, 2 exhaustion, 3 metadata; RDX=0 on failure.
; Failure does not mutate metadata or RAM. Success clears every byte before return.
frame_alloc:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,16
    mov qword [rsp],0
    mov r15,rcx
    mov r12,rdx
    mov r14,r8
    mov rax,FRAME_ALLOCATOR_MAGIC
    cmp [r15+FRAME_ALLOCATOR.ready],rax
    jne .invalid
    test rdx,rdx
    jz .invalid
    test r8,r8
    jz .invalid
    test r9,r9
    jz .invalid
    lea rax,[r9-1]
    test r9,rax
    jnz .invalid
    mov rax,0x800000000
    cmp rdx,rax
    ja .invalid
    cmp r9,rax
    ja .invalid
    mov rax,[r15+FRAME_ALLOCATOR.count]
    cmp rax,[r15+FRAME_ALLOCATOR.capacity]
    jae .metadata
    cmp rdx,[r15+FRAME_ALLOCATOR.free_pages]
    ja .exhausted
    mov r8,rdx
    shl r8,12
    shl r9,12
    dec r9
    mov rbx,[r15+FRAME_ALLOCATOR.ranges]
    mov rsi,[rbx+RANGE_SET.entries]
    xor ebp,ebp
.range:
    cmp rbp,[rbx+RANGE_SET.count]
    jae .no_fit
    mov rdi,rbp
    shl rdi,4
    add rdi,rsi
    mov r13,[rdi]
    add r13,r9
    jc .next
    mov rdx,r9
    not rdx
    and r13,rdx
    mov r11,r13
    add r11,r8
    jc .next
    cmp r11,[rdi+8]
    ja .next
    ; Reserve a range-list slot for every live allocation's eventual free.
    mov rax,[rbx+RANGE_SET.count]
    add rax,[r15+FRAME_ALLOCATOR.count]
    inc rax
    cmp r13,[rdi]
    jne .tail_capacity
    cmp r11,[rdi+8]
    jne .capacity
    dec rax
    jmp .capacity
.tail_capacity:
    cmp r11,[rdi+8]
    je .capacity
    inc rax
.capacity:
    cmp rax,[rbx+RANGE_SET.capacity]
    ja .skip_metadata
    mov r10,rdi
    mov rdi,r13
    mov rcx,r12
    shl rcx,9
    xor eax,eax
    cld
    rep stosq
    cmp r13,[r10]
    jne .tail
    cmp r11,[r10+8]
    jne .front
    dec qword [rbx+RANGE_SET.count]
    mov rax,[rbx+RANGE_SET.count]
    shl rax,4
    mov rdx,[rsi+rax]
    mov [r10],rdx
    mov rdx,[rsi+rax+8]
    mov [r10+8],rdx
    jmp .record
.front:
    mov [r10],r11
    jmp .record
.tail:
    cmp r11,[r10+8]
    je .back
    mov rax,[rbx+RANGE_SET.count]
    shl rax,4
    mov [rsi+rax],r11
    mov rdx,[r10+8]
    mov [rsi+rax+8],rdx
    inc qword [rbx+RANGE_SET.count]
.back:
    mov [r10+8],r13
.record:
    mov rax,[r15+FRAME_ALLOCATOR.count]
    lea rax,[rax+rax*2]
    shl rax,3
    add rax,[r15+FRAME_ALLOCATOR.records]
    mov [rax],r13
    mov [rax+8],r12
    mov [rax+16],r14
    inc qword [r15+FRAME_ALLOCATOR.count]
    sub [r15+FRAME_ALLOCATOR.free_pages],r12
    mov rdx,r13
    xor eax,eax
    jmp .return
.skip_metadata:
    mov qword [rsp],1
.next:
    inc rbp
    jmp .range
.no_fit:
    cmp qword [rsp],0
    jne .metadata
.exhausted:
    mov eax,2
    jmp .failure
.metadata:
    mov eax,3
    jmp .failure
.invalid:
    mov eax,1
.failure:
    xor edx,edx
.return:
    add rsp,16
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX allocator, RDX exact live base, R8 exact pages, R9 exact owner.
; EAX=0 success or 1 invalid, with no failure mutation. Single BSP only.
frame_free:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,16
    mov r15,rcx
    mov r12,rdx
    mov r13,r8
    mov r14,r9
    mov rax,FRAME_ALLOCATOR_MAGIC
    cmp [r15+FRAME_ALLOCATOR.ready],rax
    jne .bad
    test rdx,rdx
    jz .bad
    test rdx,0xfff
    jnz .bad
    test r8,r8
    jz .bad
    test r9,r9
    jz .bad
    mov rax,0x800000000
    cmp r8,rax
    ja .bad
    mov r11,r8
    shl r11,12
    add r11,rdx
    jc .bad
    xor ebp,ebp
.find:
    cmp rbp,[r15+FRAME_ALLOCATOR.count]
    jae .bad
    lea rsi,[rbp+rbp*2]
    shl rsi,3
    add rsi,[r15+FRAME_ALLOCATOR.records]
    cmp r12,[rsi]
    je .owned
    inc rbp
    jmp .find
.owned:
    cmp r13,[rsi+8]
    jne .bad
    cmp r14,[rsi+16]
    jne .bad
    mov rbx,[r15+FRAME_ALLOCATOR.ranges]
    mov rdi,[rbx+RANGE_SET.entries]
    mov qword [rsp],-1
    mov qword [rsp+8],-1
    xor r10d,r10d
.neighbors:
    cmp r10,[rbx+RANGE_SET.count]
    jae .merge
    mov r8,r10
    shl r8,4
    add r8,rdi
    cmp r12,[r8+8]
    jne .right
    mov [rsp],r10
.right:
    cmp r11,[r8]
    jne .next
    mov [rsp+8],r10
.next:
    inc r10
    jmp .neighbors
.merge:
    mov rax,[rsp]
    cmp rax,-1
    je .no_left
    shl rax,4
    add rax,rdi
    mov rdx,[rsp+8]
    cmp rdx,-1
    je .left_only
    shl rdx,4
    add rdx,rdi
    mov r8,[rdx+8]
    mov [rax+8],r8
    dec qword [rbx+RANGE_SET.count]
    mov r8,[rbx+RANGE_SET.count]
    shl r8,4
    mov r9,[rdi+r8]
    mov [rdx],r9
    mov r9,[rdi+r8+8]
    mov [rdx+8],r9
    jmp .release
.left_only:
    mov [rax+8],r11
    jmp .release
.no_left:
    mov rdx,[rsp+8]
    cmp rdx,-1
    je .append
    shl rdx,4
    mov [rdi+rdx],r12
    jmp .release
.append:
    mov rax,[rbx+RANGE_SET.count]
    cmp rax,[rbx+RANGE_SET.capacity]
    jae .bad
    shl rax,4
    mov [rdi+rax],r12
    mov [rdi+rax+8],r11
    inc qword [rbx+RANGE_SET.count]
.release:
    dec qword [r15+FRAME_ALLOCATOR.count]
    mov rax,[r15+FRAME_ALLOCATOR.count]
    lea rax,[rax+rax*2]
    shl rax,3
    add rax,[r15+FRAME_ALLOCATOR.records]
    mov r8,[rax]
    mov [rsi],r8
    mov r8,[rax+8]
    mov [rsi+8],r8
    mov r8,[rax+16]
    mov [rsi+16],r8
    add [r15+FRAME_ALLOCATOR.free_pages],r13
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,16
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
