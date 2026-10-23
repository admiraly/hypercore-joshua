; Trusted readable source / writable64-byte plan, DF0. EAX0 valid,1 rejected.
; Pointer overflow/alias rejects without stores; malformed image zeros plan.
; No allocation. Preserve all eight integer nonvolatile registers.
elf_validate:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,72
    mov rbx,rcx
    mov rbp,rdx
    mov r12,r8
    test rbx,rbx
    jz .unsafe
    test r12,r12
    jz .unsafe
    mov rax,rbx
    add rax,rbp
    jc .unsafe
    mov rdx,r12
    add rdx,ELF_PLAN_size
    jc .unsafe
    cmp r12,rax
    jae .clear
    cmp rbx,rdx
    jb .unsafe
.clear:
    mov rdi,r12
    xor eax,eax
    mov ecx,ELF_PLAN_size/8
    rep stosq
    cmp rbp,176
    jb .unsafe
    cmp rbp,ELF_MAX_BYTES
    ja .unsafe
    ; ident: ELF64/LSB/current/SysV, version/padding zero.
    mov rax,0x00010102464c457f
    cmp [rbx],rax
    jne .unsafe
    cmp qword [rbx+8],0
    jne .unsafe
    mov rax,0x00000001003e0002
    cmp [rbx+16],rax
    jne .unsafe
    cmp qword [rbx+32],64
    jne .unsafe
    cmp qword [rbx+40],0
    jne .unsafe
    cmp dword [rbx+48],0
    jne .unsafe
    cmp dword [rbx+52],0x00380040
    jne .unsafe
    cmp dword [rbx+56],2
    jne .unsafe
    cmp dword [rbx+60],0
    jne .unsafe
    mov [rsp+ELF_PLAN.source],rbx
    mov [rsp+ELF_PLAN.length],rbp
    lea rsi,[rbx+64]
    mov r13,ELF_USER_BASE
    mov r14d,5
    xor r15d,r15d
.segment:
    cmp dword [rsi],1
    jne .unsafe
    cmp [rsi+4],r14d
    jne .unsafe
    cmp [rsi+16],r13
    jne .unsafe
    cmp qword [rsi+24],0
    jne .unsafe
    cmp qword [rsi+40],4096
    jne .unsafe
    cmp qword [rsi+48],4096
    jne .unsafe
    mov rax,[rsi+8]
    cmp rax,4096
    jb .unsafe
    test eax,4095
    jnz .unsafe
    cmp rax,rbp
    ja .unsafe
    mov rdx,[rsi+32]
    cmp rdx,4096
    ja .unsafe
    mov rcx,rbp
    sub rcx,rax
    cmp rdx,rcx
    ja .unsafe
    test r15d,r15d
    jnz .data
    test rdx,rdx
    jz .unsafe
    mov [rsp+ELF_PLAN.code_offset],rax
    mov [rsp+ELF_PLAN.code_size],rdx
    inc r15d
    inc r14d
    add r13,4096
    add rsi,56
    jmp .segment
.data:
    mov [rsp+ELF_PLAN.data_offset],rax
    mov [rsp+ELF_PLAN.data_size],rdx
    ; Aligned offsets and <=page lengths: nonempty overlap iff offsets equal.
    test rdx,rdx
    jz .entry
    cmp rax,[rsp+ELF_PLAN.code_offset]
    je .unsafe
.entry:
    mov rax,[rbx+24]
    mov rdx,ELF_USER_BASE
    sub rax,rdx
    jc .unsafe
    cmp rax,[rsp+ELF_PLAN.code_size]
    jae .unsafe
    mov [rsp+ELF_PLAN.entry_offset],rax
    mov rsi,rsp
    mov rdi,r12
    mov ecx,7
    rep movsq
    mov rax,ELF_READY_MAGIC
    mov [r12+ELF_PLAN.ready],rax
    xor eax,eax
    jmp .return
.unsafe:
    mov eax,1
.return:
    add rsp,72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX immutable plan/source, RDX exact owned aligned24KiB backing, R8 bytes.
; Revalidate plan/source and all disjoint ranges before first destination write.
; EAX0 copied,1 rejected (destination unchanged); ownership is caller precondition.
elf_copy:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,104
    mov r12,rcx
    mov r13,rdx
    cmp r8,ELF_BACKING_BYTES
    jne .bad
    test r12,r12
    jz .bad
    test r13,r13
    jz .bad
    test r13d,4095
    jnz .bad
    mov r14,r13
    add r14,ELF_BACKING_BYTES
    jc .bad
    mov r15,r12
    add r15,ELF_PLAN_size
    jc .bad
    cmp r13,r15
    jae .plan_separate
    cmp r12,r14
    jb .bad
.plan_separate:
    mov rax,ELF_READY_MAGIC
    cmp [r12+ELF_PLAN.ready],rax
    jne .bad
    mov rbx,[r12+ELF_PLAN.source]
    mov rbp,[r12+ELF_PLAN.length]
    mov rax,rbx
    add rax,rbp
    jc .bad
    ; Source must not alias either the destination or the immutable plan.
    cmp r13,rax
    jae .source_dest_separate
    cmp rbx,r14
    jb .bad
.source_dest_separate:
    cmp r12,rax
    jae .source_plan_separate
    cmp rbx,r15
    jb .bad
.source_plan_separate:
    mov rcx,rbx
    mov rdx,rbp
    lea r8,[rsp+32]
    call elf_validate
    test eax,eax
    jnz .bad
    mov rsi,r12
    lea rdi,[rsp+32]
    mov ecx,ELF_PLAN_size/8
    repe cmpsq
    jne .bad
    ; No further failure edge follows publication of backing bytes.
    mov rdi,r13
    xor eax,eax
    mov ecx,ELF_BACKING_BYTES/8
    rep stosq
    mov rsi,rbx
    add rsi,[r12+ELF_PLAN.code_offset]
    mov rdi,r13
    mov rcx,[r12+ELF_PLAN.code_size]
    rep movsb
    mov rsi,rbx
    add rsi,[r12+ELF_PLAN.data_offset]
    lea rdi,[r13+4096]
    mov rcx,[r12+ELF_PLAN.data_size]
    rep movsb
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,104
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
