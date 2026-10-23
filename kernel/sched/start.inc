; Privileged BSP adapter. Maximum nested stack use below saved-frame pointer:
; context_select(112 including return), frame_check(64), config(8) < 256 bytes;
; controller helpers are shallower. 4096 bytes reserved by frame validation.

; Existing irq_save is complete; RSP points at the 192-byte outgoing frame.
context_launch_entry:
    irq_save
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne context_stop
    mov rcx,cr4
    test ecx,0x800000             ; CET conventional stack switching unsupported
    jnz context_stop
    lea rcx,[kernel_context]
    mov rdx,rsp
    xor r8d,r8d
    call context_select
    test eax,eax
    jnz context_stop
    ; Keep the selected frame in a preserved register on the outgoing stack.
    mov rbx,rdx
    mov ecx,0x320
    mov edx,0x40
    call lapic_write
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
    mov rsp,rbx
    irq_restore

context_timer_saved:
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne context_stop
    lea rcx,[kernel_context]
    mov rdx,rsp
    mov r8d,1
    call context_select
    test eax,eax
    jnz context_stop
    mov rbx,rdx
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    jne .eoi
    mov ecx,0x320
    mov edx,0x10040
    call lapic_write
    mov ecx,0x380
    xor edx,edx
    call lapic_write
.eoi:
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [kernel_context+KERNEL_CONTEXT.eois]
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    je .restore
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
.restore:
    mov rsp,rbx
    irq_restore

; Trusted fixed workers never return or allocate. IRQ may hit any instruction,
; including transient PUSHFQ; IRETQ must restore that exact interrupted RSP.
%macro context_worker 4
%1:
%if JOSHUA_CONTEXT_TEST = 3 && %3 = 0
context_worker_guard_instruction:
    mov rax,[context_worker1_guard]
%endif
.loop:
    irq_sentinels check,%2
    cmp rsp,[context_worker_rsp+%3*8]
    jne .bad
    pushfq
    pop qword [rsp]
%if %4
    test qword [rsp],0x400
    jz .bad
%else
    test qword [rsp],0x400
    jnz .bad
%endif
    inc qword [kernel_context_records+(%3+1)*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.progress]
    hlt
    jmp .loop
.bad:
    cli
    cld
    jmp context_stop
%endmacro
context_worker context_worker1,context_worker1_values,0,1
context_worker context_worker2,context_worker2_values,1,0

; EAX0 success/1 fatal. Owned metadata, stacks and immutable templates only.
context_smoke:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov rax,cr4
    test eax,0x800000
    jnz .bad
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.ready],0
    jne .bad
    lea rbx,[kernel_context]
    lea rdi,[kernel_context_records]
    mov [rbx+KERNEL_CONTEXT.records],rdi
    lea rax,[kernel_stack]
    mov [rdi+KERNEL_CONTEXT_RECORD.stack_low],rax
    lea rax,[kernel_stack_top]
    mov [rdi+KERNEL_CONTEXT_RECORD.stack_high],rax
    lea rax,[context_worker1_stack]
    mov [rdi+KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.stack_low],rax
    lea rax,[context_worker1_top]
    mov [rdi+KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.stack_high],rax
    lea rax,[context_worker2_stack]
    mov [rdi+2*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.stack_low],rax
    lea rax,[context_worker2_top]
    mov [rdi+2*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.stack_high],rax
    mov rax,[owned_paging+OWNED_PAGING.image_base]
    add rax,4096
    mov [rbx+KERNEL_CONTEXT.text_start],rax
    mov rax,[owned_paging+OWNED_PAGING.image_base]
    add rax,PE_DATA_RVA
    mov [rbx+KERNEL_CONTEXT.text_end],rax
    movzx eax,word [kernel_code_selector]
    mov [rbx+KERNEL_CONTEXT.code_selector],rax
    movzx eax,word [kernel_data_selector]
    mov [rbx+KERNEL_CONTEXT.data_selector],rax
    mov rax,CONTEXT_READY_MAGIC
    mov [rbx+KERNEL_CONTEXT.ready],rax
    mov rcx,rbx
    call context_config_check
    test eax,eax
    jnz .bad
    mov esi,1
.synthetic:
    imul rdi,rsi,KERNEL_CONTEXT_RECORD_size
    add rdi,[rbx+KERNEL_CONTEXT.records]
    mov rdx,[rdi+KERNEL_CONTEXT_RECORD.stack_high]
    sub rdx,16
    lea rax,[context_worker_rsp]
    mov [rax+rsi*8-8],rdx
    lea r12,[rdx-CONTEXT_FRAME_SIZE]
    mov [rdi+KERNEL_CONTEXT_RECORD.frame],r12
    mov rdi,r12
    mov ecx,CONTEXT_FRAME_SIZE/8
    xor eax,eax
    rep stosq
    lea r13,[context_worker1_values]
    lea r14,[context_worker1]
    mov r15d,0x602
    cmp esi,1
    je .template
    lea r13,[context_worker2_values]
    lea r14,[context_worker2]
    mov r15d,0x202
.template:
    xor ecx,ecx
.copy:
    mov rax,[r13+rcx*8]
    mov r8d,14
    sub r8,rcx
    mov [r12+r8*8+32],rax
    inc ecx
    cmp ecx,15
    jb .copy
    mov [r12+152],r14
    mov rax,[rbx+KERNEL_CONTEXT.code_selector]
    mov [r12+160],rax
    mov [r12+168],r15
    mov [r12+176],rdx
    mov rax,[rbx+KERNEL_CONTEXT.data_selector]
    mov [r12+184],rax
    mov rcx,rbx
    mov edx,esi
    mov r8,r12
    call context_frame_check
    test eax,eax
    jnz .bad
    inc esi
    cmp esi,3
    jb .synthetic
    ; Install launch gate without changing the owned selector/DPL0 attributes.
    lea rax,[context_launch_entry]
    lea rdx,[kernel_idt+0x42*16]
    mov [rdx],ax
    mov byte [rdx+4],0
    shr rax,16
    mov [rdx+6],ax
    shr rax,16
    mov [rdx+8],eax
%if JOSHUA_CONTEXT_TEST = 1
    pushfq
    pop rax
    or eax,0x200000
    push rax
    popfq
    pushfq
    pop rax
    test eax,0x200000
    jz .bad
    lea rcx,[debug_context_id]
    call debug_write
%elif JOSHUA_CONTEXT_TEST = 2
    ; Corrupt a saved target selector after initial proof; launch must recheck it.
    mov rax,[kernel_context_records+2*KERNEL_CONTEXT_RECORD_size+KERNEL_CONTEXT_RECORD.frame]
    mov qword [rax+160],0
%elif JOSHUA_CONTEXT_TEST = 3
    lea rcx,[debug_fixture_rip]
    call debug_write
    lea rcx,[context_worker_guard_instruction]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    lea rcx,[debug_fixture_target]
    call debug_write
    lea rcx,[context_worker1_guard]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
%endif
    ; Normalize firmware-originated flags, with no intervening flag changes.
    cli
    push 2
    popfq
    int 0x42
    ; Actual resumed bootstrap continuation. Its frame restored IF0.
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    cld
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.current],0
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.ticks],CONTEXT_TICKS
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.eois],CONTEXT_TICKS
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.restores],CONTEXT_TICKS
    jne .bad
    lea rsi,[kernel_context_records+KERNEL_CONTEXT_RECORD_size]
    mov ebp,2
.check_worker:
    cmp qword [rsi+KERNEL_CONTEXT_RECORD.ticks],32
    jne .bad
    cmp qword [rsi+KERNEL_CONTEXT_RECORD.progress],0
    je .bad
    add rsi,KERNEL_CONTEXT_RECORD_size
    dec ebp
    jnz .check_worker
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne .bad
    mov ecx,0x320
    call lapic_read
    test eax,0x10000
    jz .bad
    mov ecx,0x380
    call lapic_read
    test eax,eax
    jnz .bad
    mov ecx,0x390
    call lapic_read
    test eax,eax
    jnz .bad
    call apic_pending_clear
    test eax,eax
    jnz .bad
    xor eax,eax
    jmp .return
.bad:
    cli
    cld
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

; Fail-stop diagnostics only; normal entries perform no logging.
context_stop:
    cli
    cld
    sub rsp,48
    mov [rsp+32],rax
    lea rcx,[debug_context_status]
    call debug_write
    mov rcx,[rsp+32]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    lea rcx,[debug_context_failure]
    call debug_write
    mov rcx,[kernel_context+KERNEL_CONTEXT.state]
    call debug_hex
    lea rcx,[debug_timer_ticks]
    call debug_write
    mov rcx,[kernel_context+KERNEL_CONTEXT.ticks]
    call debug_hex
    lea rcx,[debug_timer_eois]
    call debug_write
    mov rcx,[kernel_context+KERNEL_CONTEXT.eois]
    call debug_hex
    lea rcx,[debug_context_current]
    call debug_write
    mov rcx,[kernel_context+KERNEL_CONTEXT.current]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    jmp kernel_fault
