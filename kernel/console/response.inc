; RCX command, RDX facts, R8 response. Trusted stable backing-disjoint extents.
; EAX0/1, EDX0. All validation and capacity measurement precedes output stores.
response_build:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,72
    mov r12,rcx
    mov r13,rdx
    mov r14,r8
    test r12,r12
    jz .bad
    test r12,7
    jnz .bad
    mov r9,r12
    add r9,CONSOLE_COMMAND_size-1
    jc .bad
    test r13,r13
    jz .bad
    test r13,7
    jnz .bad
    mov r10,r13
    add r10,CONSOLE_FACTS_size-1
    jc .bad
    test r14,r14
    jz .bad
    test r14,7
    jnz .bad
    mov r11,r14
    add r11,CONSOLE_RESPONSE_size-1
    jc .bad
    cmp r12,r10
    ja .command_facts
    cmp r13,r9
    jbe .bad
.command_facts:
    cmp r12,r11
    ja .command_output
    cmp r14,r9
    jbe .bad
.command_output:
    cmp r13,r11
    ja .facts_output
    cmp r14,r10
    jbe .bad
.facts_output:
    mov rax,COMMAND_MAGIC
    cmp [r12+CONSOLE_COMMAND.ready],rax
    jne .bad
    mov rax,[r12+CONSOLE_COMMAND.command]
    mov rcx,[r12+CONSOLE_COMMAND.status]
    mov rdx,[r12+CONSOLE_COMMAND.json]
    mov r8,[r12+CONSOLE_COMMAND.action]
    test rax,rax
    jz .empty
    cmp rax,8
    jbe .implemented
    test rdx,rdx
    jnz .bad
    test r8,r8
    jnz .bad
    cmp rax,255
    je .unknown
    cmp rax,17
    ja .bad
    cmp rcx,69
    je .facts
    cmp rcx,2
    jne .bad
    jmp .facts
.unknown:
    cmp rcx,127
    je .facts
    cmp rcx,2
    jne .bad
    jmp .facts
.empty:
    or rcx,rdx
    or rcx,r8
    jnz .bad
    jmp .facts
.implemented:
    cmp rcx,2
    je .error
    test rcx,rcx
    jnz .bad
    cmp rdx,1
    ja .bad
    test rdx,rdx
    jz .action
    cmp rax,3
    jb .bad
    cmp rax,6
    ja .bad
.action:
    cmp rax,7
    je .clear
    cmp rax,8
    je .halt
    test r8,r8
    jnz .bad
    jmp .facts
.clear:
    cmp r8,1
    jne .bad
    jmp .facts
.halt:
    cmp r8,2
    jne .bad
    jmp .facts
.error:
    or rdx,r8
    jnz .bad
.facts:
    mov rax,FACTS_MAGIC
    cmp [r13+CONSOLE_FACTS.ready],rax
    jne .bad
    cmp qword [r13+CONSOLE_FACTS.image_bytes],0
    je .bad
    cmp qword [r13+CONSOLE_FACTS.owned_table_pages],0
    je .bad
    mov rax,[r13+CONSOLE_FACTS.enabled_cpus]
    dec rax
    cmp rax,255
    ja .bad
    mov rax,[r13+CONSOLE_FACTS.apic_mode]
    dec rax
    cmp rax,1
    ja .bad
    cmp qword [r13+CONSOLE_FACTS.process_a_state],5
    ja .bad
    cmp qword [r13+CONSOLE_FACTS.process_b_state],5
    ja .bad
    mov rax,[r13+CONSOLE_FACTS.keyboard_eois]
    cmp rax,[r13+CONSOLE_FACTS.keyboard_irqs]
    ja .bad
    xor ebx,ebx ; measuring pass never writes output
    xor r15d,r15d
    call response_format
    test eax,eax
    jnz .bad
    mov [rsp+32],r15
    mov rdi,r14
    mov ecx,CONSOLE_RESPONSE_size/8
    xor eax,eax
    rep stosq
    mov ebx,1
    xor r15d,r15d
    call response_format
    test eax,eax
    jnz .invariant
    cmp r15,[rsp+32]
    jne .invariant
    mov [r14+CONSOLE_RESPONSE.length],r15
    mov rax,RESPONSE_MAGIC
    mov [r14+CONSOLE_RESPONSE.ready],rax
    xor eax,eax
    jmp .return
.invariant:
    ud2 ; immutable-input measuring/emission disagreement is never resumed
.bad:
    mov eax,1
.return:
    xor edx,edx
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

; Internal formatter carries R12/R13/R14 inputs, EBX pass and R15 output count.
; It may change RSI/RDI/RBP; nested append preserves them. Never called by an ISR.
%macro response_text 1
    lea rcx,[rel %1]
    mov edx,%1 %+ _end-%1
    call response_append
    test eax,eax
    jnz .return
%endmacro
response_format:
    sub rsp,40
    mov rax,[r12+CONSOLE_COMMAND.status]
    cmp eax,2
    je .usage
    cmp eax,69
    je .unavailable
    cmp eax,127
    je .unknown
    mov rax,[r12+CONSOLE_COMMAND.command]
    test eax,eax
    jz .ok
    cmp eax,1
    je .help
    cmp eax,2
    je .about
    cmp eax,7
    je .clear
    cmp eax,8
    je .halt
    sub eax,3
    imul eax,24
    lea rdi,[rel response_diagnostics]
    add rdi,rax
    lea rbp,[rel console_response_data]
    mov eax,[rdi+16]
    lea rsi,[rbp+rax] ; field descriptors
    mov eax,[rdi+20]
    mov [rsp+32],rax ; remaining fields
    cmp qword [r12+CONSOLE_COMMAND.json],0
    je .human
    response_text response_json_start
    mov ecx,[rdi+8]
    add rcx,rbp
    mov edx,[rdi+12]
    call response_append
    test eax,eax
    jnz .return
    response_text response_json_status
    jmp .field
.human:
    mov ecx,[rdi]
    add rcx,rbp
    mov edx,[rdi+4]
    call response_append
    test eax,eax
    jnz .return
    response_text response_lf
.field:
    cmp qword [r12+CONSOLE_COMMAND.json],0
    je .key
    response_text response_json_key
.key:
    mov ecx,[rsi]
    add rcx,rbp
    mov edx,[rsi+4]
    call response_append
    test eax,eax
    jnz .return
    cmp qword [r12+CONSOLE_COMMAND.json],0
    je .equal
    response_text response_json_colon
    jmp .number
.equal:
    response_text response_equals
.number:
    mov eax,[rsi+8]
    mov rax,[r13+rax]
    call response_number
    test eax,eax
    jnz .return
    cmp qword [r12+CONSOLE_COMMAND.json],0
    jne .next
    response_text response_lf
.next:
    add rsi,12
    dec qword [rsp+32]
    jnz .field
    cmp qword [r12+CONSOLE_COMMAND.json],0
    je .ok
    response_text response_json_end
    jmp .ok
.usage:
    response_text response_usage
    jmp .ok
.unavailable:
    response_text response_unavailable
    jmp .ok
.unknown:
    response_text response_unknown
    jmp .ok
.help:
    response_text response_help
    jmp .ok
.about:
    response_text response_about
    jmp .ok
.clear:
    response_text response_clear
    jmp .ok
.halt:
    response_text response_halt
.ok:
    xor eax,eax
.return:
    add rsp,40
    ret
%unmacro response_text 1

; RAX unsigned value. A maximum20-byte local scratch is filled backwards.
response_number:
    sub rsp,72
    lea r10,[rsp+64]
    xor ecx,ecx
    mov r8d,10
.digit:
    xor edx,edx
    div r8
    add dl,'0'
    dec r10
    mov [r10],dl
    inc ecx
    test rax,rax
    jnz .digit
    mov edx,ecx
    mov rcx,r10
    call response_append
    add rsp,72
    ret

; RCX trusted template/scratch bytes, EDX fixed length. All lengths bounded.
response_append:
    mov eax,1
    mov r10,r15
    add r10,rdx
    jc .return
    cmp r10,RESPONSE_CAPACITY
    ja .return
    test ebx,ebx
    jz .count
    push rsi
    push rdi
    lea rdi,[r14+CONSOLE_RESPONSE.body+r15]
    mov rsi,rcx
    mov ecx,edx
    rep movsb
    pop rdi
    pop rsi
.count:
    mov r15,r10
    xor eax,eax
.return:
    ret
