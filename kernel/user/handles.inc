; Pure BSP/IF0 policy. Caller proves stable owned1056-byte backing; see handle-design.md.
; Status EAX0..4; RDX/R8 exact results. All integer nonvolatiles preserved.
handle_init:
    xor eax,eax
    jmp handle_dispatch
handle_check:
    mov eax,1
    jmp handle_dispatch
handle_grant:
    mov eax,2
    jmp handle_dispatch
handle_lookup:
    mov eax,3
    jmp handle_dispatch
handle_duplicate:
    mov eax,4
    jmp handle_dispatch
handle_close:
    mov eax,5
    jmp handle_dispatch
handle_revoke:
    mov eax,6
handle_dispatch:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov ebp,eax
    mov r12,rcx
    mov r13,rdx
    mov r14,r8
    mov r15,r9
    call handle_address
    test eax,eax
    jnz .invalid
    test ebp,ebp
    jz .init
    call handle_validate
    test eax,eax
    jnz .invalid
    cmp ebp,2
    je .grant
    cmp ebp,3
    je .rights_argument
    cmp ebp,4
    je .rights_argument
    test r15,r15
    jnz .invalid
    cmp ebp,5
    je .token
    test r14,r14
    jnz .invalid
    cmp ebp,6
    je .revoke
    jmp .success
.init:
    test r13,r13
    jz .invalid
    test r14,r14
    jz .invalid
    cmp r14,65535
    ja .invalid
    test r15,r15
    jnz .invalid
    mov rsi,r12
    lea rdi,[r12+HANDLE_TABLE_size]
.zero_check:
    cmp qword [rsi],0
    jne .invalid
    add rsi,8
    cmp rsi,rdi
    jb .zero_check
    mov [r12+HANDLE_TABLE.owner],r13
    mov [r12+HANDLE_TABLE.tag],r14
    lea rsi,[r12+HANDLE_TABLE.entries]
.init_entries:
    mov qword [rsi+HANDLE_ENTRY.generation],1
    add rsi,HANDLE_ENTRY_size
    cmp rsi,rdi
    jb .init_entries
    mov rax,HANDLE_MAGIC
    mov [r12+HANDLE_TABLE.ready],rax
    jmp .success
.grant:
    test r14,r14
    jz .invalid
.rights_argument:
    test r15,r15
    jz .invalid
    cmp r15,3
    ja .invalid
    cmp ebp,2
    je .capacity
.token:
    mov rax,r14
    shr rax,48
    cmp rax,[r12+HANDLE_TABLE.tag]
    jne .bad_token
    movzx ecx,r14b
    dec ecx
    cmp ecx,HANDLE_CAPACITY
    jae .bad_token
    shl ecx,5
    lea rsi,[r12+rcx+HANDLE_TABLE.entries]
    cmp qword [rsi+HANDLE_ENTRY.state],1
    jne .bad_token
    mov rax,r14
    shr rax,8
    mov r11,HANDLE_GENERATION_MAX
    and rax,r11
    cmp rax,[rsi+HANDLE_ENTRY.generation]
    jne .bad_token
    cmp ebp,5
    je .close
    mov rax,[rsi+HANDLE_ENTRY.rights]
    cmp ebp,4
    jne .subset
    test eax,HANDLE_DUP
    jz .denied
.subset:
    mov rcx,r15
    not rax
    test rcx,rax
    jnz .denied
    cmp ebp,4
    je .duplicate
    mov rdx,[rsi+HANDLE_ENTRY.object]
    mov r8,[rsi+HANDLE_ENTRY.rights]
    xor eax,eax
    jmp .return
.duplicate:
    mov r14,[rsi+HANDLE_ENTRY.object]
.capacity:
    lea rdi,[r12+HANDLE_TABLE.entries]
    mov ecx,HANDLE_CAPACITY
.find:
    cmp qword [rdi+HANDLE_ENTRY.state],0
    je .publish
    add rdi,HANDLE_ENTRY_size
    dec ecx
    jnz .find
    mov eax,4
    jmp .failure
.publish:
    mov [rdi+HANDLE_ENTRY.object],r14
    mov [rdi+HANDLE_ENTRY.rights],r15
    mov qword [rdi+HANDLE_ENTRY.state],1
    mov rdx,[rdi+HANDLE_ENTRY.generation]
    shl rdx,8
    mov rax,rdi
    sub rax,r12
    sub rax,HANDLE_TABLE.entries
    shr rax,5
    inc eax
    or rdx,rax
    mov rax,[r12+HANDLE_TABLE.tag]
    shl rax,48
    or rdx,rax
    xor r8d,r8d
    xor eax,eax
    jmp .return
.close:
    call handle_close_slot
    jmp .success
.revoke:
    lea rsi,[r12+HANDLE_TABLE.entries]
    mov ebx,HANDLE_CAPACITY
.revoke_loop:
    cmp qword [rsi+HANDLE_ENTRY.state],1
    jne .revoke_next
    call handle_close_slot
.revoke_next:
    add rsi,HANDLE_ENTRY_size
    dec ebx
    jnz .revoke_loop
.success:
    xor eax,eax
    jmp .failure
.invalid:
    mov eax,1
    jmp .failure
.bad_token:
    mov eax,2
    jmp .failure
.denied:
    mov eax,3
.failure:
    xor edx,edx
    xor r8d,r8d
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

handle_address:
    mov eax,1
    test r12,r12
    jz .return
    test r12,7
    jnz .return
    mov rax,r12
    add rax,HANDLE_TABLE_size ; exclusive end must remain representable
    jc .bad
    xor eax,eax
.return:
    ret
.bad:
    mov eax,1
    ret

; Address prechecked; R12 table, R13 expected owner. No table stores.
handle_validate:
    mov eax,1
    test r13,r13
    jz .return
    cmp [r12+HANDLE_TABLE.owner],r13
    jne .return
    mov rax,HANDLE_MAGIC
    cmp [r12+HANDLE_TABLE.ready],rax
    jne .bad
    cmp qword [r12+HANDLE_TABLE.reserved],0
    jne .bad
    cmp qword [r12+HANDLE_TABLE.tag],1
    jb .bad
    cmp qword [r12+HANDLE_TABLE.tag],65535
    ja .bad
    mov r11,HANDLE_GENERATION_MAX
    lea rsi,[r12+HANDLE_TABLE.entries]
    mov ecx,HANDLE_CAPACITY
.entry:
    cmp qword [rsi+HANDLE_ENTRY.generation],1
    jb .bad
    cmp [rsi+HANDLE_ENTRY.generation],r11
    ja .bad
    mov rax,[rsi+HANDLE_ENTRY.state]
    cmp rax,1
    je .live
    cmp rax,2
    ja .bad
    test eax,eax
    jz .empty
    cmp [rsi+HANDLE_ENTRY.generation],r11
    jne .bad
.empty:
    cmp qword [rsi+HANDLE_ENTRY.object],0
    jne .bad
    cmp qword [rsi+HANDLE_ENTRY.rights],0
    jne .bad
    jmp .next
.live:
    cmp qword [rsi+HANDLE_ENTRY.object],0
    je .bad
    cmp qword [rsi+HANDLE_ENTRY.rights],1
    jb .bad
    cmp qword [rsi+HANDLE_ENTRY.rights],3
    ja .bad
.next:
    add rsi,HANDLE_ENTRY_size
    dec ecx
    jnz .entry
    xor eax,eax
.return:
    ret
.bad:
    mov eax,1
    ret

; Prevalidated live entry. Leaf, no failure or calls.
handle_close_slot:
    mov qword [rsi+HANDLE_ENTRY.object],0
    mov qword [rsi+HANDLE_ENTRY.rights],0
    mov r11,HANDLE_GENERATION_MAX
    cmp [rsi+HANDLE_ENTRY.generation],r11
    je .retire
    inc qword [rsi+HANDLE_ENTRY.generation]
    mov qword [rsi+HANDLE_ENTRY.state],0
    ret
.retire:
    mov qword [rsi+HANDLE_ENTRY.state],2
    ret
