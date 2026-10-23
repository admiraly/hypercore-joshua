; Internal BSP policy, no privileged instructions. RCX/context and its fixed three-
; record array are trusted readable storage. Stack ranges describe proved RW/NX
; supervisor allocations; arithmetic alone never grants pointer-read authority.
; Returning helpers preserve all eight firmware integer nonvolatile registers.

; RCX trusted context -> EAX 0 valid configuration, 1 invalid. Read-only.
context_config_check:
    mov rax,CONTEXT_READY_MAGIC
    cmp [rcx+KERNEL_CONTEXT.ready],rax
    jne .bad
    mov r10,[rcx+KERNEL_CONTEXT.records]
    test r10,r10
    jz .bad
    test r10,7
    jnz .bad
    mov rax,[rcx+KERNEL_CONTEXT.text_start]
    test rax,rax
    jz .bad
    cmp rax,[rcx+KERNEL_CONTEXT.text_end]
    jae .bad
    mov r11,0x0000800000000000
    cmp [rcx+KERNEL_CONTEXT.text_end],r11
    ja .bad
    mov rax,[rcx+KERNEL_CONTEXT.code_selector]
    test rax,rax
    jz .bad
    test rax,-65536
    jnz .bad
    test eax,7
    jnz .bad
    mov rax,[rcx+KERNEL_CONTEXT.data_selector]
    test rax,rax
    jz .bad
    test rax,-65536
    jnz .bad
    test eax,7
    jnz .bad
    cmp rax,[rcx+KERNEL_CONTEXT.code_selector]
    je .bad
    xor r8d,r8d
.record:
    mov rax,[r10+KERNEL_CONTEXT_RECORD.stack_low]
    test rax,rax
    jz .bad
    test eax,4095
    jnz .bad
    mov rdx,[r10+KERNEL_CONTEXT_RECORD.stack_high]
    test edx,4095
    jnz .bad
    cmp rdx,r11
    ja .bad
    cmp rdx,rax
    jbe .bad
    sub rdx,rax
    cmp rdx,CONTEXT_STACK_RESERVE+CONTEXT_FRAME_SIZE
    jb .bad
    ; Reject every overlap, regardless of record order.
    mov r9,[rcx+KERNEL_CONTEXT.records]
    xor edx,edx
.overlap:
    cmp edx,r8d
    jae .next
    cmp rax,[r9+KERNEL_CONTEXT_RECORD.stack_high]
    jae .advance
    mov rax,[r10+KERNEL_CONTEXT_RECORD.stack_high]
    cmp rax,[r9+KERNEL_CONTEXT_RECORD.stack_low]
    ja .bad
.advance:
    mov rax,[r10+KERNEL_CONTEXT_RECORD.stack_low]
    add r9,KERNEL_CONTEXT_RECORD_size
    inc edx
    jmp .overlap
.next:
    add r10,KERNEL_CONTEXT_RECORD_size
    inc r8d
    cmp r8d,3
    jb .record
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; RCX context, EDX record index, R8 saved-frame pointer -> EAX0/1.
; All pointer/range checks precede frame reads. Config/records caller-owned.
context_frame_check:
    push rbx
    push rsi
    push rdi
    sub rsp,32
    mov rbx,rcx
    mov esi,edx
    mov rdi,r8
    cmp edx,3
    jae .bad
    call context_config_check
    test eax,eax
    jnz .bad
    imul r10,rsi,KERNEL_CONTEXT_RECORD_size
    add r10,[rbx+KERNEL_CONTEXT.records]
    test rdi,15
    jnz .bad
    mov rax,[r10+KERNEL_CONTEXT_RECORD.stack_low]
    add rax,CONTEXT_STACK_RESERVE
    cmp rdi,rax
    jb .bad
    mov rax,[r10+KERNEL_CONTEXT_RECORD.stack_high]
    sub rax,CONTEXT_FRAME_SIZE
    cmp rdi,rax
    ja .bad
    ; The frame is now wholly in a proved stack allocation.
    mov rax,[rdi+152]
    cmp rax,[rbx+KERNEL_CONTEXT.text_start]
    jb .bad
    cmp rax,[rbx+KERNEL_CONTEXT.text_end]
    jae .bad
    mov rax,[rdi+160]
    cmp rax,[rbx+KERNEL_CONTEXT.code_selector]
    jne .bad
    mov rax,[rdi+184]
    cmp rax,[rbx+KERNEL_CONTEXT.data_selector]
    jne .bad
    mov rax,[rdi+168]
    test rax,-0x10ed8             ; arithmetic/DF, IF, bit1 and architectural RF
    jnz .bad
    test eax,2
    jz .bad
    test esi,esi
    jz .bootstrap_flags
    test eax,0x200
    jz .bad
    jmp .rsp
.bootstrap_flags:
    test eax,0x10200            ; bootstrap launch requires IF0 and RF0
    jnz .bad
.rsp:
    mov rax,[rdi+176]
    cmp rax,[r10+KERNEL_CONTEXT_RECORD.stack_high]
    ja .bad
    and rax,-16
    sub rax,CONTEXT_FRAME_SIZE
    cmp rax,rdi
    jne .bad
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rdi
    pop rsi
    pop rbx
    ret

; RCX context, RDX incoming frame, R8D reason0 launch/1 timer.
; EAX0/RDX next frame, EAX1/RDX0 failure with all state unchanged.
; EOI is adapter-owned: it must equal completed ticks before this transaction.
context_select:
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
    mov r12,rdx
    mov r13d,r8d
    cmp r8d,1
    ja .bad
    call context_config_check
    test eax,eax
    jnz .bad
    mov r14,[rbx+KERNEL_CONTEXT.records]
    mov r15,[rbx+KERNEL_CONTEXT.current]
    cmp qword [rbx+KERNEL_CONTEXT.state],0
    jne .timer
    test r13d,r13d
    jnz .bad
    test r15,r15
    jnz .bad
    cmp qword [rbx+KERNEL_CONTEXT.ticks],0
    jne .bad
    cmp qword [rbx+KERNEL_CONTEXT.eois],0
    jne .bad
    cmp qword [rbx+KERNEL_CONTEXT.restores],0
    jne .bad
    cmp qword [r14+KERNEL_CONTEXT_RECORD.ticks],0
    jne .bad
    cmp qword [r14+KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.ticks],0
    jne .bad
    cmp qword [r14+2*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.ticks],0
    jne .bad
    mov rcx,rbx
    xor edx,edx
    mov r8,r12
    call context_frame_check
    test eax,eax
    jnz .bad
    mov esi,1
.launch_check:
    mov rcx,rbx
    mov edx,esi
    imul rax,rsi,KERNEL_CONTEXT_RECORD_size
    mov r8,[r14+rax+KERNEL_CONTEXT_RECORD.frame]
    call context_frame_check
    test eax,eax
    jnz .bad
    inc esi
    cmp esi,3
    jb .launch_check
    mov [r14+KERNEL_CONTEXT_RECORD.frame],r12
    mov qword [rbx+KERNEL_CONTEXT.current],1
    mov qword [rbx+KERNEL_CONTEXT.state],1
    mov rdx,[r14+KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.frame]
    xor eax,eax
    jmp .return
.timer:
    cmp qword [rbx+KERNEL_CONTEXT.state],1
    jne .bad
    cmp r13d,1
    jne .bad
    mov rbp,[rbx+KERNEL_CONTEXT.ticks]
    cmp rbp,CONTEXT_TICKS
    jae .bad
    cmp rbp,[rbx+KERNEL_CONTEXT.eois]
    jne .bad
    cmp rbp,[rbx+KERNEL_CONTEXT.restores]
    jne .bad
    mov rax,rbp
    and eax,1
    inc eax
    cmp rax,r15
    jne .bad
    ; Per-worker accounting must match the already completed alternating schedule.
    mov rax,rbp
    inc rax
    shr rax,1
    cmp rax,[r14+KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.ticks]
    jne .bad
    mov rax,rbp
    shr rax,1
    cmp rax,[r14+2*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.ticks]
    jne .bad
    mov rcx,rbx
    mov edx,r15d
    mov r8,r12
    call context_frame_check
    test eax,eax
    jnz .bad
    mov esi,3
    sub esi,r15d
    inc rbp
    cmp rbp,CONTEXT_TICKS
    jb .next_check
    xor esi,esi
.next_check:
    imul rdi,rsi,KERNEL_CONTEXT_RECORD_size
    add rdi,r14
    mov rcx,rbx
    mov edx,esi
    mov r8,[rdi+KERNEL_CONTEXT_RECORD.frame]
    call context_frame_check
    test eax,eax
    jnz .bad
    ; All failures above are read-only. Publish only after both frames are proved.
    imul rax,r15,KERNEL_CONTEXT_RECORD_size
    add rax,r14
    mov [rax+KERNEL_CONTEXT_RECORD.frame],r12
    inc qword [rax+KERNEL_CONTEXT_RECORD.ticks]
    mov [rbx+KERNEL_CONTEXT.ticks],rbp
    inc qword [rbx+KERNEL_CONTEXT.restores]
    mov [rbx+KERNEL_CONTEXT.current],rsi
    test esi,esi
    jnz .selected
    mov qword [rbx+KERNEL_CONTEXT.state],2
.selected:
    mov rdx,[rdi+KERNEL_CONTEXT_RECORD.frame]
    xor eax,eax
    jmp .return
.bad:
    xor edx,edx
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
