; Boot-only ELF adapter: immutable image, fresh context and exact private backing.
; EAX0 stage1,1 poisoned/quarantined before any root/runnable publication.
user_elf_prepare:
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
%if JOSHUA_ELF_TEST
    mov r15,[frame_allocator+FRAME_ALLOCATOR.count]
%endif
    lea rcx,[user_elf_start]
    mov edx,user_elf_end-user_elf_start
    lea r8,[user_elf_plan]
    call elf_validate
    test eax,eax
    jnz .reject
    lea rax,[user_elf_start]
    add rax,[user_elf_plan+ELF_PLAN.code_offset]
    mov [rbx+USER_CONTEXT.payload],rax
    mov rax,[user_elf_plan+ELF_PLAN.code_size]
    mov [rbx+USER_CONTEXT.payload_size],rax
    mov rcx,rbx
    call user_storage_prepare
    test eax,eax
    jnz .bad
    xor ebp,ebp
    mov r12,[rbx+USER_CONTEXT.processes]
.copy:
    mov rcx,[rbx+USER_CONTEXT.builder]
    mov rdx,[r12+USER_PROCESS.backing]
    mov r8d,6
    lea r9d,[rbp+FRAME_OWNER_USER_BASE+1]
    mov r10d,1
    call user_range_owned
    test eax,eax
    jnz .bad
    lea rcx,[user_elf_plan]
    mov rdx,[r12+USER_PROCESS.backing]
    mov r8d,ELF_BACKING_BYTES
    call elf_copy
    test eax,eax
    jnz .bad
    mov rdx,[r12+USER_PROCESS.backing]
    mov rax,0x4a53485500000000
    or rax,rbp
    mov [rdx+4096+8],rax
    add r12,USER_PROCESS_size
    inc ebp
    cmp ebp,USER_PROCESS_COUNT
    jb .copy
    xor eax,eax
    jmp .return
.reject:
%if JOSHUA_ELF_TEST
    lea rcx,[debug_elf_reject]
    call debug_write
    mov rcx,r15
    call debug_hex
    lea rcx,[debug_elf_count_address]
    call debug_write
    lea rcx,[frame_allocator+FRAME_ALLOCATOR.count]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
%endif
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

; Revalidate immutable source and exact plan before granting launch authority.
user_elf_launch_check:
    push rsi
    push rdi
    sub rsp,104
    lea rcx,[user_elf_start]
    mov edx,user_elf_end-user_elf_start
    lea r8,[rsp+32]
    call elf_validate
    test eax,eax
    jnz .return
    lea rsi,[user_elf_plan]
    lea rdi,[rsp+32]
    mov ecx,ELF_PLAN_size/8
    repe cmpsq
    setne al
    movzx eax,al
.return:
    add rsp,104
    pop rdi
    pop rsi
    ret
