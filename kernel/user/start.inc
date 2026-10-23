; Privileged BSP-only adapters; all metadata/roots/stacks were proved before use.
; Normal entries never log/allocate/call firmware. irq_save supplies192-byte frame.
; Max helper nesting <512 bytes below a saved frame, within16-KiB entry/IST stacks.
%macro user_normalize 0
    push 2
    popfq                       ; IF/DF/AC0, independent of user-controlled flags
    mov ax,[kernel_data_selector]
    mov ds,ax
    mov es,ax
    mov ecx,0xc0000100
    xor eax,eax
    xor edx,edx
    wrmsr
    inc ecx
    wrmsr                       ; FS/GS bases0; no SWAPGS/TLS convention
%endmacro

user_launch_entry:
    irq_save
    mov qword [user_transition_step],2
    user_normalize
    mov r12,rsp
    mov rax,cr3
    cmp rax,[user_context+USER_CONTEXT.kernel_root]
    jne user_stop
    cmp qword [user_context+USER_CONTEXT.active],0
    jne user_stop
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne user_stop
    lea rcx,[kernel_context]
    xor edx,edx
    mov r8,r12
    call context_frame_check
    test eax,eax
    jnz user_stop
    lea rax,[user_smoke.resumed]
    cmp [r12+152],rax
    jne user_stop
    mov rax,[r12+144]            ; trusted launch caller's1-based process index
    dec rax
    cmp rax,USER_PROCESS_COUNT
    jae user_stop
    imul r13,rax,USER_PROCESS_size
    lea rdx,[user_processes]
    add r13,rdx
    cmp qword [r13+USER_PROCESS.state],0
    jne user_stop
    inc rax
    mov [user_context+USER_CONTEXT.active],rax
    mov qword [user_context+USER_CONTEXT.phase],1
    mov qword [r13+USER_PROCESS.state],1
    mov qword [user_transition_step],3
    mov [user_context+USER_CONTEXT.bootstrap_frame],r12
    mov rdi,[r13+USER_PROCESS.kernel_high]
    mov [kernel_tss+4],rdi       ; RSP0
    sub rdi,192
    mov r14,rdi
    xor eax,eax
    mov ecx,192/8
    rep stosq                  ; scrub every saved user GPR and shadow slot
    mov rax,USER_BASE
    mov [r14+152],rax
    add rax,0x1000
    mov [r14+96],rax            ; only public argument: user data VA in RDI
    mov rax,[user_context+USER_CONTEXT.code_selector]
    mov [r14+160],rax
    mov qword [r14+168],0x202
    mov rax,USER_BASE+0x7000
    mov [r14+176],rax
    mov rax,[user_context+USER_CONTEXT.data_selector]
    mov [r14+184],rax
    lea rcx,[user_context]
    mov rdx,r14
    mov r8,[r13+USER_PROCESS.root] ; target identity for synthetic frame proof
    call user_frame_check
    test eax,eax
    jnz user_stop
    mov qword [user_transition_step],4
    mov ecx,0x320
    mov edx,0x40
    call lapic_write
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
    mov rax,[r13+USER_PROCESS.root]
    mov cr3,rax
    mov ax,[user_context+USER_CONTEXT.data_selector]
    mov ds,ax
    mov es,ax
    mov rsp,r14
    irq_restore                ; terminal switch: no kernel pointers in user GPRs

user_gate_entry:
    irq_save
    mov qword [user_transition_step],5
    user_normalize
    mov r12,rsp
    lea rcx,[user_context]
    mov rdx,r12
    mov r8,cr3
    call user_frame_check
    cmp eax,2
    je user_stop
    mov rbx,rdx
    test eax,eax
    jnz user_bad_return
    mov rax,[r12+160]
    mov [rbx+USER_PROCESS.last_cs],rax
    cmp qword [r12+144],0
    je .exit
    inc qword [rbx+USER_PROCESS.unknown_calls]
    mov qword [r12+144],-38
    irq_restore
.exit:
    mov rdx,[r12+96]
    mov ecx,3
    jmp user_terminate

user_bad_return:
    mov ecx,4
    mov rdx,-22
    jmp user_terminate

; Entry from normalized exception stub, before stop-only reporting destroys fields.
; User integer state is discarded. Only the hardware IST frame grants origin.
user_exception_entry:
    user_normalize
    mov r12,rsp
    sub rsp,40
    lea rcx,[user_context]
    mov rdx,r12
    mov r8,cr3
    call user_fault_check
    test eax,eax
    jnz user_stop
    mov rbx,rdx
    mov rax,[r12]
    mov [rbx+USER_PROCESS.fault_vector],rax
    mov rax,[r12+8]
    mov [rbx+USER_PROCESS.fault_error],rax
    mov rax,[r12+16]
    mov [rbx+USER_PROCESS.fault_rip],rax
    mov rax,cr2
    mov [rbx+USER_PROCESS.fault_cr2],rax
    mov rax,[r12+24]
    mov [rbx+USER_PROCESS.last_cs],rax
    mov ecx,4
    xor edx,edx
    jmp user_terminate

user_timer_saved:
    user_normalize
    cmp qword [user_context+USER_CONTEXT.phase],2
    je user_drain_timer_saved
    mov r12,rsp
    lea rcx,[user_context]
    mov rdx,r12
    mov r8,cr3
    call user_frame_check
    cmp eax,2
    je user_stop
    mov rbx,rdx
    test eax,eax
    jnz .invalid
    inc qword [rbx+USER_PROCESS.ticks]
    cmp qword [rbx+USER_PROCESS.ticks],8
    jae .timeout
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [rbx+USER_PROCESS.eois]
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
    irq_restore
.invalid:
    mov esi,4
    jmp .terminal
.timeout:
    mov esi,5
.terminal:
    ; Stop/mask before final EOI; the terminating handler owns this ISR delivery.
    call user_timer_stop
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [rbx+USER_PROCESS.eois]
    mov ecx,esi
    xor edx,edx
    jmp user_terminate

; RBX trusted active record; ECX terminal state3/4/5, RDX status. No return.
user_terminate:
    mov qword [user_transition_step],6
    mov [user_terminal_state],rcx
    mov [rbx+USER_PROCESS.status],rdx
    mov qword [rbx+USER_PROCESS.state],2
    mov qword [user_context+USER_CONTEXT.phase],2
    mov rax,[user_context+USER_CONTEXT.kernel_root]
    mov cr3,rax
    call user_timer_stop
    mov ax,[kernel_data_selector]
    mov ss,ax
    mov rsp,[rbx+USER_PROCESS.kernel_high]
    sub rsp,64                 ;32-byte caller shadow space plus private headroom
    jmp user_drain_continuation

user_timer_stop:
    sub rsp,40
    mov ecx,0x320
    mov edx,0x10040
    call lapic_write
    mov ecx,0x380
    xor edx,edx
    call lapic_write
    add rsp,40
    ret

; IF0, kernel CR3, proved fixed entry stack; no user state can be resumed.
user_drain_continuation:
    mov qword [user_transition_step],7
    mov qword [user_drain_windows],0
.window:
    call apic_pending_clear
    test eax,eax
    jz user_drain_finish
    cmp qword [user_drain_windows],1024
    jae user_stop
    inc qword [user_drain_windows]
    sti
    nop
user_drain_close:
    cli
    jmp user_drain_continuation.window
user_drain_finish:
    mov qword [user_transition_step],8
    mov r12,[user_context+USER_CONTEXT.bootstrap_frame]
    lea rcx,[kernel_context]
    xor edx,edx
    mov r8,r12
    call context_frame_check
    test eax,eax
    jnz user_stop
    lea rax,[user_smoke.resumed]
    cmp [r12+152],rax
    jne user_stop
    mov rax,[user_terminal_state]
    mov [rbx+USER_PROCESS.state],rax
    lea rax,[kernel_stack_top]
    mov [kernel_tss+4],rax
    mov qword [user_context+USER_CONTEXT.active],0
    mov qword [user_context+USER_CONTEXT.phase],0
    mov rsp,r12
    irq_restore

; Only this exact RIP/stack/state/root may receive an owned timer at CPL0.
user_drain_timer_saved:
    mov rax,cr3
    cmp rax,[user_context+USER_CONTEXT.kernel_root]
    jne user_stop
    mov rax,[user_context+USER_CONTEXT.active]
    dec rax
    cmp rax,USER_PROCESS_COUNT
    jae user_stop
    imul rbx,rax,USER_PROCESS_size
    lea rax,[user_processes]
    add rbx,rax
    cmp qword [rbx+USER_PROCESS.state],2
    jne user_stop
    mov rax,[rbx+USER_PROCESS.kernel_high]
    sub rax,256
    cmp rsp,rax
    jne user_stop              ; entire frame lies on the proved entry stack
    lea rax,[user_drain_close]
    cmp [rsp+152],rax
    jne user_stop
    movzx eax,word [kernel_code_selector]
    cmp [rsp+160],rax
    jne user_stop
    movzx eax,word [kernel_data_selector]
    cmp [rsp+184],rax
    jne user_stop
    mov rax,[rbx+USER_PROCESS.kernel_high]
    sub rax,64
    cmp [rsp+176],rax
    jne user_stop
    mov rax,[rsp+168]
    test rax,-0x10ad8          ; arithmetic/IF/RF/fixed bit1; no DF/AC
    jnz user_stop
    and eax,0x202
    cmp eax,0x202
    jne user_stop
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [rbx+USER_PROCESS.eois]
    irq_restore

user_stop:
    cli
    mov rax,[owned_paging+OWNED_PAGING.root]
    mov cr3,rax
    lea rsp,[kernel_stack_top]
    sub rsp,32
    lea rcx,[debug_user_failure]
    call debug_write
    mov rcx,[user_transition_step]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    jmp kernel_fault

; Normal kernel call; two single-use processes, then actual bootstrap continuation.
user_smoke:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov qword [user_transition_step],1
    call user_descriptor_retire
    test eax,eax
    jnz .bad
    lea rcx,[kernel_gdt]
    movzx edx,word [kernel_gdtr]
    inc edx
    call user_gdt_prepare
    test eax,eax
    jnz .bad
    mov [user_context+USER_CONTEXT.code_selector],rdx
    mov [user_context+USER_CONTEXT.data_selector],r8
    mov [user_context+USER_CONTEXT.np_selector],r9
    mov [kernel_gdtr],r10w
    lgdt [kernel_gdtr]
    lea rax,[exception_top]
    mov [user_context+USER_CONTEXT.fault_top],rax
    lea rax,[kernel_stack]
    mov [user_context+USER_CONTEXT.bootstrap_low],rax
    lea rax,[kernel_stack_top]
    mov [user_context+USER_CONTEXT.bootstrap_high],rax
    lea rax,[user_launch_entry]
    lea rdx,[kernel_idt+0x43*16]
    mov r8b,0x8e
    call .gate
    lea rax,[user_gate_entry]
    lea rdx,[kernel_idt+0x80*16]
    mov r8b,0xee
    call .gate
%if JOSHUA_USER_TEST = 13
    mov rax,cr0
    or rax,0x40000
    mov cr0,rax                 ; controlled #AC campaign; preserve other CR0 bits
%endif
    mov rax,[user_processes+USER_PROCESS.backing]
    lea rdx,[user_smoke]
    mov [rax+4096+24],rdx        ; explicit supervisor fixture probe address
    mov rdx,[user_context+USER_CONTEXT.np_selector]
    mov [rax+4096+32],rdx
    movzx edx,word [kernel_code_selector]
    mov [rax+4096+40],rdx
    mov r12d,1
.run:
    mov eax,r12d
    cli
    push 2
    popfq
    int 0x43
.resumed:
    mov qword [user_transition_step],9
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov rax,cr3
    cmp rax,[user_context+USER_CONTEXT.kernel_root]
    jne .bad
    imul rbx,r12,USER_PROCESS_size
    lea rax,[user_processes-USER_PROCESS_size]
    add rbx,rax
    lea rcx,[debug_user_result]
    call debug_write
    mov rcx,r12
    call debug_hex
%macro user_result_field 2
    lea rcx,[%1]
    call debug_write
    mov rcx,[rbx+%2]
    call debug_hex
%endmacro
    user_result_field debug_user_result_root,USER_PROCESS.root
    user_result_field debug_user_result_cs,USER_PROCESS.last_cs
    user_result_field debug_user_result_state,USER_PROCESS.state
    user_result_field debug_user_result_unknown,USER_PROCESS.unknown_calls
    user_result_field debug_user_result_ticks,USER_PROCESS.ticks
    user_result_field debug_user_result_eois,USER_PROCESS.eois
    user_result_field debug_vector,USER_PROCESS.fault_vector
    user_result_field debug_error,USER_PROCESS.fault_error
    user_result_field debug_rip,USER_PROCESS.fault_rip
    user_result_field debug_cr2,USER_PROCESS.fault_cr2
    lea rcx,[debug_newline]
    call debug_write
    mov ecx,3
%if JOSHUA_USER_TEST != 0
    cmp r12d,1
    jne .expected_state
%if JOSHUA_USER_TEST = 9
    mov ecx,5
%else
    mov ecx,4
%endif
.expected_state:
%endif
    cmp [rbx+USER_PROCESS.state],rcx
    jne .bad
    xor edx,edx
%if JOSHUA_USER_TEST = 16
    cmp r12d,1
    jne .expected_status
    mov rdx,-22
.expected_status:
%endif
    cmp [rbx+USER_PROCESS.status],rdx
    jne .bad
    cmp qword [rbx+USER_PROCESS.unknown_calls],1
    jne .bad
    mov rax,[user_context+USER_CONTEXT.code_selector]
    cmp [rbx+USER_PROCESS.last_cs],rax
    jne .bad
    mov rax,[rbx+USER_PROCESS.backing]
    cmp qword [rax+4096+16],1
    jne .bad
    mov rdx,0x4a53485500000000
    lea rcx,[r12-1]
    or rdx,rcx
    cmp [rax+4096+8],rdx
    jne .bad
    inc r12d
    cmp r12d,USER_PROCESS_COUNT
    jbe .run
%if JOSHUA_USER_TEST = 13
    mov rax,cr0
    and rax,~0x40000
    mov rdx,[user_context+USER_CONTEXT.saved_cr0]
    and edx,0x40000
    or rax,rdx
    mov cr0,rax
%endif
    xor eax,eax
    jmp .return
.bad:
    lea rcx,[debug_user_failure]
    call debug_write
    mov rcx,[user_transition_step]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
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
.gate:
    mov [rdx],ax
    mov byte [rdx+4],0
    mov [rdx+5],r8b
    shr rax,16
    mov [rdx+6],ax
    shr rax,16
    mov [rdx+8],eax
    ret
