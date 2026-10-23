; Pure screen composition. Trusted stable mapped backing-disjoint records and stack.
; Public signatures and source provenance are frozen in docs/console-design.md.
%macro model_enter 1
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,216
    mov rbp,rsp
    mov r12,rcx
    mov r13,rdx
    mov r14,r8
    mov r15,r9
    mov ebx,%1
%if %1 = 3
    mov rax,[rsp+320]
    mov [rbp+184],rax
    mov rax,[rsp+328]
    mov [rbp+192],rax
%endif
%endmacro
%macro model_leave 0
    xor edx,edx
    add rsp,216
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
model_init:
    model_enter 0
    call model_preflight
    test eax,eax
    jnz .return
    mov rdi,r12
    xor eax,eax
    mov ecx,CONSOLE_MODEL_size/8
    rep stosq
    mov qword [r12+CONSOLE_MODEL.transcript_row],2
    lea rdi,[r13+CONSOLE_GRID.cells]
    mov rsi,[r13+CONSOLE_GRID.rows]
.row:
    mov rcx,[r13+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    add rdi,100
    sub rdi,[r13+CONSOLE_GRID.columns]
    dec rsi
    jnz .row
    lea rcx,[rel model_title]
    mov edx,model_title_end-model_title
    xor r8d,r8d
    call model_row_text
    lea rcx,[rel model_subtitle]
    mov edx,model_subtitle_end-model_subtitle
    mov r8d,1
    call model_row_text
    lea rcx,[rel model_ready]
    mov edx,model_ready_end-model_ready
    mov r8,[r13+CONSOLE_GRID.rows]
    dec r8
    call model_row_text
    call model_prompt_fill
    mov rcx,[r13+CONSOLE_GRID.rows]
    mov eax,1
    shl rax,cl
    dec rax
    mov [r13+CONSOLE_GRID.dirty],rax
    mov rax,MODEL_MAGIC
    mov [r12+CONSOLE_MODEL.ready],rax
    xor eax,eax
.return:
    model_leave
model_check:
    model_enter 1
    call model_preflight
    model_leave
model_prompt:
    model_enter 2
    call model_preflight
    test eax,eax
    jnz .return
    call model_prompt_fill
    xor eax,eax
.return:
    model_leave
model_complete:
    model_enter 3
    call model_preflight
    test eax,eax
    jnz .return
    mov rax,[rbp+184]
    cmp qword [rax+CONSOLE_COMMAND.action],1
    je .clear
    cmp qword [r12+CONSOLE_MODEL.transcript_column],0
    je .echo
    call model_advance
.echo:
    lea rcx,[rel model_prefix]
    mov edx,model_prefix_end-model_prefix
    call model_append
    lea rcx,[r15+CONSOLE_EDITOR.submitted]
    mov rdx,[r15+CONSOLE_EDITOR.submitted_length]
    call model_append
    mov r8d,10
    call model_putchar
    jmp .response
.clear:
    call model_clear_transcript
.response:
    mov rax,[rbp+192]
    lea rcx,[rax+CONSOLE_RESPONSE.body]
    mov rdx,[rax+CONSOLE_RESPONSE.length]
    call model_append
    cmp qword [r12+CONSOLE_MODEL.completed],-1
    je .record
    inc qword [r12+CONSOLE_MODEL.completed]
.record:
    mov rsi,[rbp+184]
    add rsi,CONSOLE_COMMAND.command
    lea rdi,[r12+CONSOLE_MODEL.command]
    mov ecx,4
    rep movsq
    ; Result row is independently authoritative even when transcript scrolls.
    mov rax,[r13+CONSOLE_GRID.rows]
    dec rax
    imul rax,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    mov rcx,[r13+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    lea rcx,[rel model_result_ok]
    mov edx,model_result_ok_end-model_result_ok
    mov rax,[r12+CONSOLE_MODEL.status]
    test eax,eax
    jz .result
    lea rcx,[rel model_result_usage]
    mov edx,model_result_usage_end-model_result_usage
    cmp eax,2
    je .result
    lea rcx,[rel model_result_unavailable]
    mov edx,model_result_unavailable_end-model_result_unavailable
    cmp eax,69
    je .result
    lea rcx,[rel model_result_unknown]
    mov edx,model_result_unknown_end-model_result_unknown
.result:
    mov r8,[r13+CONSOLE_GRID.rows]
    dec r8
    call model_row_text
    call model_prompt_fill
    xor eax,eax
.return:
    model_leave

; Public entry's RBP anchors private scratch across calls. All mutation follows this.
model_preflight:
    sub rsp,40
    mov rcx,r13
    mov rdx,r14
    call grid_check
    test eax,eax
    jnz .return
    xor esi,esi
    mov rax,r12
    mov edx,CONSOLE_MODEL_size
    call model_range_add
    test eax,eax
    jnz .return
    mov rax,r13
    mov edx,CONSOLE_GRID_size
    call model_range_add
    test eax,eax
    jnz .return
    mov rax,r14
    mov edx,CONSOLE_SURFACE_size
    call model_range_add
    test eax,eax
    jnz .return
    mov rax,[r14+CONSOLE_SURFACE.base]
    mov rdx,[r14+CONSOLE_SURFACE.size]
    call model_range_raw
    test eax,eax
    jnz .return
    cmp ebx,2
    jb .records
    mov rax,r15
    mov edx,CONSOLE_EDITOR_size
    call model_range_add
    test eax,eax
    jnz .return
    cmp ebx,3
    jne .records
    mov rax,[rbp+184]
    mov edx,CONSOLE_COMMAND_size
    call model_range_add
    test eax,eax
    jnz .return
    mov rax,[rbp+192]
    mov edx,CONSOLE_RESPONSE_size
    call model_range_add
    test eax,eax
    jnz .return
.records:
    test ebx,ebx
    jz .ok
    mov rax,MODEL_MAGIC
    cmp [r12+CONSOLE_MODEL.ready],rax
    jne .bad
    mov rax,[r12+CONSOLE_MODEL.transcript_row]
    cmp rax,2
    jb .bad
    mov rdx,[r13+CONSOLE_GRID.rows]
    sub rdx,3
    cmp rax,rdx
    ja .bad
    mov rax,[r12+CONSOLE_MODEL.transcript_column]
    cmp rax,[r13+CONSOLE_GRID.columns]
    ja .bad
    mov rax,COMMAND_MAGIC
    mov [rbp+144],rax
    lea rsi,[r12+CONSOLE_MODEL.command]
    lea rdi,[rbp+152]
    mov ecx,4
    rep movsq
    lea rcx,[rbp+144]
    call command_check
    test eax,eax
    jnz .return
    cmp ebx,2
    jb .ok
    mov rcx,r15
    call editor_check
    test eax,eax
    jnz .return
    cmp ebx,3
    jne .ok
    mov rcx,[rbp+184]
    call command_check
    test eax,eax
    jnz .return
    mov rcx,[rbp+192]
    call response_check
    test eax,eax
    jnz .return
    mov rcx,r15
    lea rdx,[rbp+144]
    call command_parse
    test eax,eax
    jnz .return
    lea rsi,[rbp+144]
    mov rdi,[rbp+184]
    mov ecx,5
    repe cmpsq
    jne .bad
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    ret

; Add [RAX,RAX+RDX-1] to private seven-entry table. RSI is the prior entry count.
model_range_add:
    test rax,7
    jnz model_range_raw.bad
model_range_raw:
    test rax,rax
    jz .bad
    test rdx,rdx
    jz .bad
    dec rdx
    add rdx,rax
    jc .bad
    lea r10,[rbp+32]
    xor edi,edi
.loop:
    cmp rdi,rsi
    jae .store
    cmp rax,[r10+8]
    ja .next
    cmp [r10],rdx
    jbe .bad
.next:
    add r10,16
    inc edi
    jmp .loop
.store:
    mov [r10],rax
    mov [r10+8],rdx
    inc esi
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; RCX immutable text, EDX literal length, R8 row. All literals <=48 columns.
model_row_text:
    imul rax,r8,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    mov rsi,rcx
    mov ecx,edx
    rep movsb
    bts [r13+CONSOLE_GRID.dirty],r8
    ret

; Existing cursor metadata was validated even when disabled. Init uses empty input.
model_prompt_fill:
    mov r10,[r13+CONSOLE_GRID.rows]
    sub r10,2
    mov rax,[r13+CONSOLE_GRID.cursor_y]
    bts [r13+CONSOLE_GRID.dirty],rax
    bts [r13+CONSOLE_GRID.dirty],r10
    imul rax,r10,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    mov rcx,[r13+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    imul rax,r10,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    lea rsi,[rel model_prefix]
    mov ecx,8
    rep movsb
    mov qword [r13+CONSOLE_GRID.cursor_x],8
    mov [r13+CONSOLE_GRID.cursor_y],r10
    mov qword [r13+CONSOLE_GRID.cursor_enabled],1
    test ebx,ebx
    jz .return
    mov r8,[r13+CONSOLE_GRID.columns]
    sub r8,8 ; viewport width
    mov r9,[r15+CONSOLE_EDITOR.cursor]
    ; S=max(0,C-(W-1)), using unsigned validated C and positive W.
    xor eax,eax
    cmp r9,r8
    jb .start
    lea rax,[r9+1]
    sub rax,r8
.start:
    mov rdx,r9
    sub rdx,rax
    add rdx,8
    mov [r13+CONSOLE_GRID.cursor_x],rdx
    lea rsi,[r15+CONSOLE_EDITOR.line]
    add rsi,rax
    mov rcx,[r15+CONSOLE_EDITOR.length]
    sub rcx,rax
    cmp rcx,r8
    cmova rcx,r8
    rep movsb
.return:
    ret

model_clear_transcript:
    mov qword [r12+CONSOLE_MODEL.transcript_row],2
    mov qword [r12+CONSOLE_MODEL.transcript_column],0
    mov r8d,2
.row:
    imul rax,r8,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    mov rcx,[r13+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    bts [r13+CONSOLE_GRID.dirty],r8
    inc r8
    mov rax,[r13+CONSOLE_GRID.rows]
    sub rax,2
    cmp r8,rax
    jb .row
    ret

; Advance once, including a full pending-wrap row. Preserves append's RSI/RDI/R8.
model_advance:
    push rsi
    push rdi
    mov qword [r12+CONSOLE_MODEL.transcript_column],0
    mov rax,[r12+CONSOLE_MODEL.transcript_row]
    mov rdx,[r13+CONSOLE_GRID.rows]
    sub rdx,3
    cmp rax,rdx
    je .scroll
    inc qword [r12+CONSOLE_MODEL.transcript_row]
    jmp .return
.scroll:
    lea rdi,[r13+CONSOLE_GRID.cells+200]
    lea rsi,[r13+CONSOLE_GRID.cells+300]
    mov rcx,[r13+CONSOLE_GRID.rows]
    sub rcx,5
    imul rcx,100
    rep movsb
    imul rax,rdx,100
    lea rdi,[r13+CONSOLE_GRID.cells+rax]
    mov rcx,[r13+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    mov rcx,[r13+CONSOLE_GRID.rows]
    sub ecx,2
    mov eax,1
    shl rax,cl
    dec rax
    and rax,~3
    or [r13+CONSOLE_GRID.dirty],rax
.return:
    pop rdi
    pop rsi
    ret

; R8B printable/LF, never from unchecked data.
model_putchar:
    sub rsp,40
    cmp r8b,10
    je .newline
    mov rax,[r12+CONSOLE_MODEL.transcript_column]
    cmp rax,[r13+CONSOLE_GRID.columns]
    jb .write
    call model_advance
.write:
    mov rax,[r12+CONSOLE_MODEL.transcript_row]
    bts [r13+CONSOLE_GRID.dirty],rax
    imul rax,100
    add rax,[r12+CONSOLE_MODEL.transcript_column]
    mov [r13+CONSOLE_GRID.cells+rax],r8b
    inc qword [r12+CONSOLE_MODEL.transcript_column]
    jmp .return
.newline:
    call model_advance
.return:
    add rsp,40
    ret

; RCX source/RDX validated length. Private helpers preserve loop pointers.
model_append:
    sub rsp,40
    mov rsi,rcx
    lea rdi,[rcx+rdx]
.loop:
    cmp rsi,rdi
    jae .return
    movzx r8d,byte [rsi]
    call model_putchar
    inc rsi
    jmp .loop
.return:
    add rsp,40
    ret
%unmacro model_enter 1
%unmacro model_leave 0
