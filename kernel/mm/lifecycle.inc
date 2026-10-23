; BSP/IF-clear trusted kernel arena. Firmware integer ABI throughout.
; RCX zero context, RDX inactive builder, R8 zero 64-record array.
; EAX=0 prepared, 4 fatal. All table pages retain whole-chunk paging ownership.
lifecycle_prepare:
    push rbx
    push rsi
    sub rsp,40
    mov rbx,rcx
    mov rsi,rdx
    cmp qword [rbx+MAPPING_CONTEXT.ready],0
    jne .bad
    cmp qword [rsi+OWNED_PAGING.failed],0
    jne .bad
    cmp qword [rsi+OWNED_PAGING.ready],0
    jne .bad
    mov [rbx+MAPPING_CONTEXT.builder],rsi
    mov [rbx+MAPPING_CONTEXT.records],r8
    mov rdx,[rsi+OWNED_PAGING.root]
    test rdx,rdx
    jz .bad
    add rdx,256*8
    cmp qword [rdx],0
    jne .bad
    mov rcx,rsi
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,rax
    mov rcx,rsi
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,rax
    mov rcx,rsi
    call owned_child
    test rax,rax
    jz .bad
    mov [rbx+MAPPING_CONTEXT.pt],rax
    mov qword [rbx+MAPPING_CONTEXT.count],0
    mov rax,MAPPING_MAGIC
    mov [rbx+MAPPING_CONTEXT.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+MAPPING_CONTEXT.ready],0
    mov qword [rsi+OWNED_PAGING.failed],1
    mov eax,4
.return:
    add rsp,40
    pop rsi
    pop rbx
    ret

; RCX context -> EAX=0 valid active environment, 4 permanently disabled.
lifecycle_check:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov rax,MAPPING_MAGIC
    cmp [rbx+MAPPING_CONTEXT.ready],rax
    jne .bad
    mov rcx,[rbx+MAPPING_CONTEXT.builder]
    cmp qword [rcx+OWNED_PAGING.failed],0
    jne .bad
    cmp qword [rcx+OWNED_PAGING.ready],1
    jne .bad
    call lifecycle_platform_check
    test eax,eax
    jnz .bad
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+MAPPING_CONTEXT.ready],0
    mov eax,4
.return:
    add rsp,32
    pop rbx
    ret

; RCX builder, RDX physical page -> RAX matching 4K arena PTE or zero.
; Identity leaves must be supervisor RW/NX and non-global. Cache selectors retained.
lifecycle_leaf:
    push rbx
    sub rsp,32
    mov rbx,rdx
    mov rcx,[rcx+OWNED_PAGING.root]
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,rbx
    jne .bad
    mov rdx,rax
    and edx,0x107
    cmp edx,3
    jne .bad
    bt rax,63
    jnc .bad
    mov rdx,rax
    and edx,0x18
    cmp r8,4096
    je .small
    test eax,0x1000
    jz .flags
    or edx,0x80
    jmp .flags
.small:
    and eax,0x80
    or rdx,rax
    ; Only PAT/PWT/PCD survive; discard the raw leaf address and other flags.
    and edx,0x98
.flags:
    mov rax,PTE_NX|3
    or rax,rdx
    or rax,rbx
    jmp .return
.bad:
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret

; RCX context, RDX count (1..512), R8 caller owner (nonzero).
; EAX=0/RDX=VA success; 1 invalid, 2 arena/record/physical exhaustion,
; 3 allocator metadata exhaustion, 4 fatal invariant. RDX=0 on failure.
lifecycle_alloc:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    mov r12,rcx
    mov r13,rdx
    mov r14,r8
    call lifecycle_check
    test eax,eax
    jnz .return_zero
    test r13,r13
    jz .invalid
    cmp r13,512
    ja .invalid
    test r14,r14
    jz .invalid
    mov r15,[r12+MAPPING_CONTEXT.records]
    mov ecx,MAPPING_RECORD_CAPACITY
.record:
    cmp qword [r15+MAPPING_RECORD.state],0
    je .find_run
    add r15,MAPPING_RECORD_size
    dec ecx
    jnz .record
    jmp .exhausted
.find_run:
    xor ebx,ebx
    xor ebp,ebp
    mov rsi,[r12+MAPPING_CONTEXT.pt]
.scan:
    cmp qword [rsi+rbx*8],0
    jne .occupied
    inc rbp
    cmp rbp,r13
    je .found
    jmp .next
.occupied:
    xor ebp,ebp
.next:
    inc ebx
    cmp ebx,512
    jb .scan
    jmp .exhausted
.found:
    inc rbx
    sub rbx,r13
    mov [rsp+32],rbx
    mov rcx,[r12+MAPPING_CONTEXT.builder]
    mov rcx,[rcx+OWNED_PAGING.allocator]
    mov rdx,r13
    mov r8d,FRAME_OWNER_MAPPING
    mov r9d,1
    call frame_alloc
    test eax,eax
    jnz .allocator_failure
    mov [r15+MAPPING_RECORD.physical],rdx
    mov [r15+MAPPING_RECORD.pages],r13
    mov [r15+MAPPING_RECORD.owner],r14
    mov rax,MAPPING_ARENA_BASE
    shl rbx,12
    add rbx,rax
    mov [r15+MAPPING_RECORD.virtual],rbx
    mov qword [r15+MAPPING_RECORD.state],1
    inc qword [r12+MAPPING_CONTEXT.count]
    mov rdi,rdx
    xor ebp,ebp
.preflight:
    mov rcx,[r12+MAPPING_CONTEXT.builder]
    mov rdx,rdi
    call lifecycle_leaf
    test rax,rax
    jz .fatal
    add rdi,4096
    inc rbp
    cmp rbp,r13
    jb .preflight
    mov rdi,[r15+MAPPING_RECORD.physical]
    mov rsi,[rsp+32]
    shl rsi,3
    add rsi,[r12+MAPPING_CONTEXT.pt]
    xor ebp,ebp
.publish:
    mov rcx,[r12+MAPPING_CONTEXT.builder]
    mov rdx,rdi
    call lifecycle_leaf
    test rax,rax
    jz .fatal
    mov [rsi+rbp*8],rax
    add rdi,4096
    inc rbp
    cmp rbp,r13
    jb .publish
    xor ebp,ebp
.invalidate:
    mov rcx,r12
    mov rdx,rbx
    call lifecycle_invalidate
    add rbx,4096
    inc rbp
    cmp rbp,r13
    jb .invalidate
    mov qword [r15+MAPPING_RECORD.state],2
    mov rdx,[r15+MAPPING_RECORD.virtual]
    xor eax,eax
    jmp .return
.fatal:
    mov qword [r15+MAPPING_RECORD.state],3
    mov qword [r12+MAPPING_CONTEXT.ready],0
    mov eax,4
    jmp .return_zero
.allocator_failure:
    cmp eax,1
    jne .return_zero
    mov qword [r12+MAPPING_CONTEXT.ready],0
    mov eax,4
    jmp .return_zero
.invalid:
    mov eax,1
    jmp .return_zero
.exhausted:
    mov eax,2
.return_zero:
    xor edx,edx
.return:
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

; RCX context, RDX exact VA, R8 exact pages, R9 exact caller owner.
; EAX=0 released, 1 rejected unchanged, 4 fatal/quarantined. No frame free
; occurs until ALL leaves are clear and ALL local invalidations have completed.
lifecycle_free:
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
    call lifecycle_check
    test eax,eax
    jnz .return
    mov rax,MAPPING_ARENA_BASE
    cmp r13,rax
    jb .invalid
    sub rax,r13
    neg rax
    cmp rax,512*4096
    jae .invalid
    test eax,0xfff
    jnz .invalid
    test r14,r14
    jz .invalid
    cmp r14,512
    ja .invalid
    test r15,r15
    jz .invalid
    mov rbx,[r12+MAPPING_CONTEXT.records]
    mov ecx,MAPPING_RECORD_CAPACITY
.find:
    cmp qword [rbx+MAPPING_RECORD.state],2
    jne .next
    cmp [rbx+MAPPING_RECORD.virtual],r13
    je .match
.next:
    add rbx,MAPPING_RECORD_size
    dec ecx
    jnz .find
    jmp .invalid
.match:
    cmp [rbx+MAPPING_RECORD.pages],r14
    jne .invalid
    cmp [rbx+MAPPING_RECORD.owner],r15
    jne .invalid
    mov rax,MAPPING_ARENA_BASE
    mov rsi,r13
    sub rsi,rax
    shr rsi,9
    add rsi,[r12+MAPPING_CONTEXT.pt]
    mov rdi,[rbx+MAPPING_RECORD.physical]
    xor ebp,ebp
.preflight:
    mov rcx,[r12+MAPPING_CONTEXT.builder]
    mov rdx,rdi
    call lifecycle_leaf
    test rax,rax
    jz .fatal
    mov rdx,[rsi+rbp*8]
    and rdx,-97
    cmp rax,rdx
    jne .fatal
    add rdi,4096
    inc rbp
    cmp rbp,r14
    jb .preflight
    mov qword [rbx+MAPPING_RECORD.state],4
    xor ebp,ebp
.clear:
    mov qword [rsi+rbp*8],0
    inc rbp
    cmp rbp,r14
    jb .clear
    xor ebp,ebp
.invalidate:
    mov rcx,r12
    mov rdx,r13
    call lifecycle_invalidate
    add r13,4096
    inc rbp
    cmp rbp,r14
    jb .invalidate
    mov rcx,[r12+MAPPING_CONTEXT.builder]
    mov rcx,[rcx+OWNED_PAGING.allocator]
    mov rdx,[rbx+MAPPING_RECORD.physical]
    mov r8,r14
    mov r9d,FRAME_OWNER_MAPPING
    call frame_free
    test eax,eax
    jnz .fatal
    mov rdi,rbx
    xor eax,eax
    mov ecx,MAPPING_RECORD_size/8
    rep stosq
    dec qword [r12+MAPPING_CONTEXT.count]
    xor eax,eax
    jmp .return
.fatal:
    mov qword [rbx+MAPPING_RECORD.state],3
    mov qword [r12+MAPPING_CONTEXT.ready],0
    mov eax,4
    jmp .return
.invalid:
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
