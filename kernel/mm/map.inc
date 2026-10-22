; Validate only; no allocation, sorting, writes or reclamation.
; RCX map, RDX byte length, R8 descriptor stride, R9 physical address bits.
; Caller owns/readably maps the entire buffer. EFI descriptor version 1.
; Firmware ABI, EAX=0 valid, EAX=1 invalid; all nonvolatile registers preserved.
memory_map_validate:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    test rcx,rcx
    jz .invalid
    test rdx,rdx
    jz .invalid
    cmp rdx,MAP_CAPACITY
    ja .invalid
    cmp r8,40
    jb .invalid
    test r8,7
    jnz .invalid
    cmp r9d,32
    jb .invalid
    cmp r9d,52
    ja .invalid
    mov r12,rcx
    mov r13,r8
    mov rax,rcx
    add rax,rdx
    jc .invalid
    mov rax,rdx
    xor edx,edx
    div r8
    test rdx,rdx
    jnz .invalid
    test rax,rax
    jz .invalid
    mov r14,rax
    mov ecx,r9d
    mov r15,1
    shl r15,cl
    xor esi,esi
.next:
    mov rdi,[r12+8]
    test rdi,0xfff
    jnz .invalid
    mov rbx,[r12+24]
    test rbx,rbx
    jz .invalid
    mov rax,r15
    shr rax,12
    cmp rbx,rax
    ja .invalid
    shl rbx,12
    add rbx,rdi
    jc .invalid
    cmp rbx,r15
    ja .invalid
    ; Every previous descriptor is validated already. Reject any intersection,
    ; independent of descriptor type or ordering. Adjacency is legal.
    mov rbp,r12
    mov r10,rsi
.previous:
    test r10,r10
    jz .accepted
    sub rbp,r13
    mov rax,[rbp+8]
    mov rdx,[rbp+24]
    shl rdx,12
    add rdx,rax
    cmp rdi,rdx
    jae .disjoint
    cmp rax,rbx
    jb .invalid
.disjoint:
    dec r10
    jmp .previous
.accepted:
    inc rsi
    add r12,r13
    cmp rsi,r14
    jb .next
    xor eax,eax
    jmp .done
.invalid:
    mov eax,1
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
