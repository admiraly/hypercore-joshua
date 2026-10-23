; Pure caller-owned1568-byte state. BSP/IF0/DF0, mapped extent/exclusivity trusted.
; RCX state -> EAX0/1, EDX0. Explicit init may reset previously used/corrupt storage.
editor_init:
    push rdi
    sub rsp,32
    call editor_address_check
    test eax,eax
    jnz .return
    mov rdi,rcx
    xor eax,eax
    mov ecx,CONSOLE_EDITOR_size/8
    rep stosq
    mov rax,EDITOR_MAGIC
    mov [rdi-CONSOLE_EDITOR_size+CONSOLE_EDITOR.ready],rax
    xor eax,eax
.return:
    xor edx,edx
    add rsp,32
    pop rdi
    ret

; RCX state -> EAX0/1, EDX0. Validate every fixed buffer before event-driven indexing.
editor_check:
    push rbx
    push rbp
    sub rsp,40
    mov rbx,rcx
    call editor_address_check
    test eax,eax
    jnz .return
    mov rax,EDITOR_MAGIC
    cmp [rbx+CONSOLE_EDITOR.ready],rax
    jne .bad
    mov rax,[rbx+CONSOLE_EDITOR.length]
    cmp rax,EDITOR_LINE_CAPACITY
    ja .bad
    cmp [rbx+CONSOLE_EDITOR.cursor],rax
    ja .bad
    mov rax,[rbx+CONSOLE_EDITOR.draft_length]
    cmp rax,EDITOR_LINE_CAPACITY
    ja .bad
    cmp [rbx+CONSOLE_EDITOR.draft_cursor],rax
    ja .bad
    cmp qword [rbx+CONSOLE_EDITOR.submitted_length],EDITOR_LINE_CAPACITY
    ja .bad
    mov rax,[rbx+CONSOLE_EDITOR.history_count]
    cmp rax,EDITOR_HISTORY_CAPACITY
    ja .bad
    cmp qword [rbx+CONSOLE_EDITOR.history_next],EDITOR_HISTORY_CAPACITY
    jae .bad
    cmp [rbx+CONSOLE_EDITOR.history_position],rax
    ja .bad
    cmp eax,EDITOR_HISTORY_CAPACITY
    je .buffers
    cmp [rbx+CONSOLE_EDITOR.history_next],rax
    jne .bad
.buffers:
    lea rdx,[rbx+CONSOLE_EDITOR.line]
    mov r8,[rbx+CONSOLE_EDITOR.length]
    call editor_span_check
    test eax,eax
    jnz .bad
    lea rdx,[rbx+CONSOLE_EDITOR.draft]
    mov r8,[rbx+CONSOLE_EDITOR.draft_length]
    call editor_span_check
    test eax,eax
    jnz .bad
    lea rdx,[rbx+CONSOLE_EDITOR.submitted]
    mov r8,[rbx+CONSOLE_EDITOR.submitted_length]
    call editor_span_check
    test eax,eax
    jnz .bad
    xor ebp,ebp
.history:
    mov r8,[rbx+CONSOLE_EDITOR.history_lengths+rbp*8]
    cmp qword [rbx+CONSOLE_EDITOR.history_count],EDITOR_HISTORY_CAPACITY
    je .occupied
    cmp rbp,[rbx+CONSOLE_EDITOR.history_count]
    jb .occupied
    test r8,r8
    jnz .bad
    jmp .span
.occupied:
    test r8,r8
    jz .bad
.span:
    mov rdx,rbp
    shl rdx,7
    lea rdx,[rbx+CONSOLE_EDITOR.history+rdx]
    call editor_span_check
    test eax,eax
    jnz .bad
    inc ebp
    cmp ebp,EDITOR_HISTORY_CAPACITY
    jb .history
    mov r8,[rbx+CONSOLE_EDITOR.history_position]
    test r8,r8
    jz .ok
    mov rax,[rbx+CONSOLE_EDITOR.history_next]
    sub rax,r8
    and eax,EDITOR_HISTORY_CAPACITY-1
    mov r8,[rbx+CONSOLE_EDITOR.history_lengths+rax*8]
    cmp r8,[rbx+CONSOLE_EDITOR.length]
    jne .bad
    shl rax,7
    lea rdx,[rbx+CONSOLE_EDITOR.history+rax]
    lea r9,[rbx+CONSOLE_EDITOR.line]
    mov ecx,EDITOR_BUFFER_SIZE
.compare:
    mov al,[rdx]
    cmp al,[r9]
    jne .bad
    inc rdx
    inc r9
    dec ecx
    jnz .compare
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    xor edx,edx
    add rsp,40
    pop rbp
    pop rbx
    ret

%macro editor_increment 1
    cmp qword [rbx+%1],-1
    je %%saturated
    inc qword [rbx+%1]
%%saturated:
%endmacro

; RCX state, EDX16-bit event -> EAX0/1; EDX action0/1/2/3/4 or0 on invalid.
; Complete validation precedes every mutation, including counters. No SIMD/STD.
editor_event:
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
    mov esi,edx
    call editor_check
    test eax,eax
    jnz .bad
    cmp esi,65535
    ja .bad
    mov r12,[rbx+CONSOLE_EDITOR.length]
    mov r13,[rbx+CONSOLE_EDITOR.cursor]
    cmp esi,32
    jb .control
    cmp esi,126
    jbe .insert
.control:
    cmp esi,13
    je .submit
    cmp esi,8
    je .backspace
    cmp esi,3
    je .cancel
    cmp esi,KEY_LEFT
    je .left
    cmp esi,KEY_RIGHT
    je .right
    cmp esi,KEY_HOME
    je .home
    cmp esi,KEY_END
    je .end
    cmp esi,KEY_DELETE
    je .delete
    cmp esi,KEY_UP
    je .up
    cmp esi,KEY_DOWN
    je .down
    jmp .ignored
.insert:
    cmp r12,EDITOR_LINE_CAPACITY
    je .full
    mov r14,r12
.shift_right:
    cmp r14,r13
    jbe .insert_byte
    mov al,[rbx+CONSOLE_EDITOR.line+r14-1]
    mov [rbx+CONSOLE_EDITOR.line+r14],al
    dec r14
    jmp .shift_right
.insert_byte:
    mov [rbx+CONSOLE_EDITOR.line+r13],sil
    inc r12
    inc r13
    mov [rbx+CONSOLE_EDITOR.length],r12
    mov [rbx+CONSOLE_EDITOR.cursor],r13
    mov qword [rbx+CONSOLE_EDITOR.history_position],0
    jmp .changed
.full:
    editor_increment CONSOLE_EDITOR.rejected
    mov edx,3
    jmp .ok
.backspace:
    test r13,r13
    jz .ignored
    dec r13
    jmp .remove
.delete:
    cmp r13,r12
    je .ignored
.remove:
    mov r14,r13
.shift_left:
    mov al,[rbx+CONSOLE_EDITOR.line+r14+1]
    mov [rbx+CONSOLE_EDITOR.line+r14],al
    inc r14
    cmp r14,r12
    jb .shift_left
    dec r12
    mov [rbx+CONSOLE_EDITOR.length],r12
    mov [rbx+CONSOLE_EDITOR.cursor],r13
    mov qword [rbx+CONSOLE_EDITOR.history_position],0
    jmp .changed
.left:
    test r13,r13
    jz .ignored
    dec qword [rbx+CONSOLE_EDITOR.cursor]
    jmp .changed
.right:
    cmp r13,r12
    je .ignored
    inc qword [rbx+CONSOLE_EDITOR.cursor]
    jmp .changed
.home:
    test r13,r13
    jz .ignored
    mov qword [rbx+CONSOLE_EDITOR.cursor],0
    jmp .changed
.end:
    cmp r13,r12
    je .ignored
    mov [rbx+CONSOLE_EDITOR.cursor],r12
    jmp .changed
.up:
    mov r14,[rbx+CONSOLE_EDITOR.history_position]
    cmp r14,[rbx+CONSOLE_EDITOR.history_count]
    je .ignored
    test r14,r14
    jnz .older
    lea rcx,[rbx+CONSOLE_EDITOR.draft]
    lea rdx,[rbx+CONSOLE_EDITOR.line]
    call editor_copy_buffer
    mov [rbx+CONSOLE_EDITOR.draft_length],r12
    mov [rbx+CONSOLE_EDITOR.draft_cursor],r13
.older:
    inc qword [rbx+CONSOLE_EDITOR.history_position]
    jmp .recall
.down:
    cmp qword [rbx+CONSOLE_EDITOR.history_position],0
    je .ignored
    dec qword [rbx+CONSOLE_EDITOR.history_position]
    jnz .recall
    lea rcx,[rbx+CONSOLE_EDITOR.line]
    lea rdx,[rbx+CONSOLE_EDITOR.draft]
    call editor_copy_buffer
    mov rax,[rbx+CONSOLE_EDITOR.draft_length]
    mov [rbx+CONSOLE_EDITOR.length],rax
    mov rax,[rbx+CONSOLE_EDITOR.draft_cursor]
    mov [rbx+CONSOLE_EDITOR.cursor],rax
    jmp .changed
.recall:
    mov r14,[rbx+CONSOLE_EDITOR.history_next]
    sub r14,[rbx+CONSOLE_EDITOR.history_position]
    and r14d,EDITOR_HISTORY_CAPACITY-1
    mov r15,r14
    shl r15,7
    lea rdx,[rbx+CONSOLE_EDITOR.history+r15]
    lea rcx,[rbx+CONSOLE_EDITOR.line]
    call editor_copy_buffer
    mov rax,[rbx+CONSOLE_EDITOR.history_lengths+r14*8]
    mov [rbx+CONSOLE_EDITOR.length],rax
    mov [rbx+CONSOLE_EDITOR.cursor],rax
    jmp .changed
.submit:
    lea rcx,[rbx+CONSOLE_EDITOR.submitted]
    lea rdx,[rbx+CONSOLE_EDITOR.line]
    call editor_copy_buffer
    mov [rbx+CONSOLE_EDITOR.submitted_length],r12
    editor_increment CONSOLE_EDITOR.submissions
    test r12,r12
    jz .submitted
    mov r14,[rbx+CONSOLE_EDITOR.history_next]
    mov r15,r14
    shl r15,7
    lea rcx,[rbx+CONSOLE_EDITOR.history+r15]
    lea rdx,[rbx+CONSOLE_EDITOR.line]
    call editor_copy_buffer
    mov [rbx+CONSOLE_EDITOR.history_lengths+r14*8],r12
    inc r14d
    and r14d,EDITOR_HISTORY_CAPACITY-1
    mov [rbx+CONSOLE_EDITOR.history_next],r14
    cmp qword [rbx+CONSOLE_EDITOR.history_count],EDITOR_HISTORY_CAPACITY
    jae .submitted
    inc qword [rbx+CONSOLE_EDITOR.history_count]
.submitted:
    mov r15d,2
    jmp .reset
.cancel:
    editor_increment CONSOLE_EDITOR.cancellations
    mov r15d,4
.reset:
    lea rcx,[rbx+CONSOLE_EDITOR.line]
    call editor_clear_buffer
    lea rcx,[rbx+CONSOLE_EDITOR.draft]
    call editor_clear_buffer
    mov qword [rbx+CONSOLE_EDITOR.length],0
    mov qword [rbx+CONSOLE_EDITOR.cursor],0
    mov qword [rbx+CONSOLE_EDITOR.history_position],0
    mov qword [rbx+CONSOLE_EDITOR.draft_length],0
    mov qword [rbx+CONSOLE_EDITOR.draft_cursor],0
    mov edx,r15d
    jmp .ok
.changed:
    mov edx,1
    jmp .ok
.ignored:
    xor edx,edx
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
    xor edx,edx
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

; Nonzero/aligned/nonwrapping extent only. Mapping/ownership are caller proofs.
editor_address_check:
    test rcx,rcx
    jz .bad
    test rcx,7
    jnz .bad
    mov rax,rcx
    add rax,CONSOLE_EDITOR_size-1
    jc .bad
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; RDX buffer, R8 length. Fixed128-byte extent; no read before length validation.
editor_span_check:
    cmp r8,EDITOR_LINE_CAPACITY
    ja .bad
    xor r9d,r9d
.text:
    cmp r9,r8
    jae .tail
    movzx eax,byte [rdx+r9]
    sub eax,32
    cmp eax,94
    ja .bad
    inc r9d
    jmp .text
.tail:
    cmp byte [rdx+r9],0
    jne .bad
    inc r9d
    cmp r9d,EDITOR_BUFFER_SIZE
    jb .tail
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; Internal fixed, disjoint128-byte buffers; volatile registers only.
editor_copy_buffer:
    xor r8d,r8d
.copy:
    mov rax,[rdx+r8]
    mov [rcx+r8],rax
    add r8d,8
    cmp r8d,EDITOR_BUFFER_SIZE
    jb .copy
    ret
editor_clear_buffer:
    xor eax,eax
    xor r8d,r8d
.clear:
    mov [rcx+r8],rax
    add r8d,8
    cmp r8d,EDITOR_BUFFER_SIZE
    jb .clear
    ret
