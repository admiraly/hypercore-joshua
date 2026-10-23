; Fixed two-incarnation privileged adapter. No user pointer or dynamic object API.
%macro user_handle_save 0
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
%endmacro
%macro user_handle_restore 0
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
%endmacro

user_handles_prepare:
    user_handle_save
    cmp qword [user_handles_ready],0
    jne .bad
    mov rax,USER_READY_MAGIC
    cmp [user_context+USER_CONTEXT.ready],rax
    jne .bad
    cmp qword [user_context+USER_CONTEXT.stage],3
    jne .bad
    cmp qword [user_context+USER_CONTEXT.active],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne .bad
    mov rax,cr3
    cmp rax,[user_context+USER_CONTEXT.kernel_root]
    jne .bad
    lea rsi,[user_handle_counters]
    mov ecx,USER_PROCESS_COUNT*USER_HANDLE_COUNTERS_size/8
.zero_counters:
    cmp qword [rsi],0
    jne .bad
    add rsi,8
    loop .zero_counters
    mov r12d,1
    lea r13,[user_handle_tables]
    lea r14,[user_processes]
.prepare:
    cmp qword [r14+USER_PROCESS.state],0
    jne .bad
    lea r15,[r12+FRAME_OWNER_USER_BASE]
    cmp [r14+USER_PROCESS.owner],r15
    jne .bad
    mov rcx,r13
    mov rdx,r15
    mov r8,r12
    xor r9d,r9d
    call handle_init
    test eax,eax
    jnz .bad
    mov rcx,r13
    mov rdx,r15
    mov r8d,1
    mov r9d,HANDLE_READ|HANDLE_DUP
    call handle_grant
    test eax,eax
    jnz .bad
    mov rax,[r14+USER_PROCESS.backing]
    mov [rax+4096+48],rdx
    mov rcx,r13
    mov rdx,r15
    xor r8d,r8d
    xor r9d,r9d
    call handle_check
    test eax,eax
    jnz .bad
    add r13,HANDLE_TABLE_size
    add r14,USER_PROCESS_size
    inc r12d
    cmp r12d,USER_PROCESS_COUNT
    jbe .prepare
    mov rax,USER_HANDLES_READY
    mov [user_handles_ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    user_handle_restore

; RBX exact active process, ECX expected phase/state1 or2.
; Caller saved all nonvolatiles. Return R13 table,R14 counters,R15 owner,RBP tag.
user_handle_bind:
    sub rsp,40
    mov rax,USER_HANDLES_READY
    cmp [user_handles_ready],rax
    jne user_stop
    cmp ecx,1
    jb user_stop
    cmp ecx,2
    ja user_stop
    cmp [user_context+USER_CONTEXT.phase],rcx
    jne user_stop
    cmp [rbx+USER_PROCESS.state],rcx
    jne user_stop
    mov r10,[user_context+USER_CONTEXT.active]
    dec r10
    cmp r10,USER_PROCESS_COUNT
    jae user_stop
    imul rax,r10,USER_PROCESS_size
    lea rdx,[user_processes]
    add rax,rdx
    cmp rbx,rax
    jne user_stop
    lea rbp,[r10+1]
    lea r15,[rbp+FRAME_OWNER_USER_BASE]
    cmp [rbx+USER_PROCESS.owner],r15
    jne user_stop
    mov rax,cr3
    cmp ecx,1
    jne .kernel_root
    cmp rax,[rbx+USER_PROCESS.root]
    jne user_stop
    jmp .root_ok
.kernel_root:
    cmp rax,[user_context+USER_CONTEXT.kernel_root]
    jne user_stop
.root_ok:
    imul rax,r10,HANDLE_TABLE_size
    lea r13,[user_handle_tables]
    add r13,rax
    imul rax,r10,USER_HANDLE_COUNTERS_size
    lea r14,[user_handle_counters]
    add r14,rax
    cmp [r13+HANDLE_TABLE.tag],rbp
    jne user_stop
    mov rcx,r13
    mov rdx,r15
    xor r8d,r8d
    xor r9d,r9d
    call handle_check
    test eax,eax
    jnz user_stop
    lea r10,[r13+HANDLE_TABLE.entries]
    mov r11d,HANDLE_CAPACITY
.objects:
    cmp qword [r10+HANDLE_ENTRY.state],1
    jne .next
    cmp qword [r10+HANDLE_ENTRY.object],1
    jne user_stop
.next:
    add r10,HANDLE_ENTRY_size
    dec r11d
    jnz .objects
    add rsp,40
    ret

; R12 points to the already-validated192-byte saved user frame; RBX active record.
user_handle_gate:
    user_handle_save
    mov ecx,1
    call user_handle_bind
    lea rcx,[r14+USER_HANDLE_COUNTERS.calls]
    call user_handle_increment
    mov rsi,[r12+144]
    mov rcx,r13
    mov rdx,r15
    mov r8,[r12+96]
    mov r9,[r12+104]
    cmp esi,2
    je .duplicate
    test r9,r9
    jnz .invalid
    cmp esi,3
    je .close
    cmp esi,1
    jne user_stop
    mov r9d,HANDLE_READ
    call handle_lookup
    test eax,eax
    jnz .error
    cmp rdx,1
    jne user_stop
    mov rdx,USER_HANDLE_VALUE
    jmp .success
.duplicate:
    call handle_duplicate
    test eax,eax
    jnz .error
    jmp .success
.close:
    call handle_close
    test eax,eax
    jnz .error
.success:
    shl esi,3
    lea rcx,[r14+rsi]
    xor eax,eax
    jmp .publish
.invalid:
    mov eax,1
.error:
    xor edx,edx
    cmp eax,1
    je .request_error
    cmp eax,2
    je .token_error
    cmp eax,3
    je .rights_error
    cmp eax,4
    jne user_stop
    lea rcx,[r14+USER_HANDLE_COUNTERS.full]
    mov rax,-24
    jmp .publish
.request_error:
    lea rcx,[r14+USER_HANDLE_COUNTERS.invalid]
    mov rax,-22
    jmp .publish
.token_error:
    lea rcx,[r14+USER_HANDLE_COUNTERS.bad_token]
    mov rax,-9
    jmp .publish
.rights_error:
    lea rcx,[r14+USER_HANDLE_COUNTERS.denied]
    mov rax,-13
.publish:
    mov [r12+144],rax
    mov [r12+128],rdx
    call user_handle_increment
    user_handle_restore

; Kernel root, active phase/state2, IF0, fixed entry stack before drain windows.
user_handles_revoke:
    user_handle_save
    mov ecx,2
    call user_handle_bind
    cmp qword [r14+USER_HANDLE_COUNTERS.revoked],0
    jne user_stop
    mov rcx,r13
    mov rdx,r15
    xor r8d,r8d
    xor r9d,r9d
    call handle_revoke
    test eax,eax
    jnz user_stop
    mov rcx,r13
    mov rdx,r15
    xor r8d,r8d
    xor r9d,r9d
    call handle_check
    test eax,eax
    jnz user_stop
    lea r10,[r13+HANDLE_TABLE.entries]
    mov r11d,HANDLE_CAPACITY
.empty:
    cmp qword [r10+HANDLE_ENTRY.state],1
    je user_stop
    add r10,HANDLE_ENTRY_size
    dec r11d
    jnz .empty
    mov qword [r14+USER_HANDLE_COUNTERS.revoked],1
    user_handle_restore

user_handle_increment:
    cmp qword [rcx],-1
    je .return
    inc qword [rcx]
.return:
    ret
