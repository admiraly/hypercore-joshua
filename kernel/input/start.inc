; Preparation checkpoint only: no route programming/unmasking or STI. A dedicated
; keyboard IRQ gate/window is still required before the input driver can run.
keyboard_prepare:
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
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne .bad
    cmp qword [owned_paging+OWNED_PAGING.ready],1
    jne .bad
    mov rax,APIC_READY_MAGIC
    cmp [apic_context+APIC_CONTEXT.ready],rax
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    jne .bad
    cmp qword [user_context+USER_CONTEXT.active],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.failed],0
    jne .bad
    mov rax,USER_READY_MAGIC
    cmp [user_context+USER_CONTEXT.ready],rax
    jne .bad
    lea rcx,[acpi_context]
    lea rdx,[keyboard_authority]
    call keyboard_authority_build
    test eax,eax
    jnz .bad
    mov r12,CONTROLLER_WINDOW_BASE
    mov rax,[keyboard_authority+KEYBOARD_AUTH.io_index]
    inc rax
    shl rax,12
    add r12,rax
    mov r13,[keyboard_authority+KEYBOARD_AUTH.io_pin]
    shl r13d,1
    add r13d,0x10
    mov rcx,r12
    mov edx,r13d
    call ioapic_read
    test eax,0x10000
    jz .bad
    mov ebx,eax
    shr eax,8
    and eax,7
    cmp eax,0
    je .masked
    cmp eax,1
    je .masked
    cmp eax,7
    jne .bad
.masked:
    mov [keyboard_io_window],r12
    mov [keyboard_io_selector],r13
    mov [keyboard_saved_low],ebx
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_read
    mov [keyboard_saved_high],eax
    lea rcx,[keyboard_decoder]
    call keyboard_decoder_init
    lea rax,[keyboard_authority]
    mov [keyboard_controller+KEYBOARD_CONTROLLER.authority],rax
    lea rcx,[keyboard_controller]
    call keyboard_controller_prepare
    test eax,eax
    jnz .bad
    ; Recheck mask and exact entry bytes: preparation never owns a live IRQ.
    mov rcx,r12
    mov edx,r13d
    call ioapic_read
    cmp eax,[keyboard_saved_low]
    jne .bad
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_read
    cmp eax,[keyboard_saved_high]
    jne .bad
    lea rcx,[debug_keyboard_prepared]
    call debug_write
    mov rcx,[keyboard_authority+KEYBOARD_AUTH.gsi]
    call debug_hex
    lea rcx,[debug_keyboard_config]
    call debug_write
    mov rcx,[keyboard_controller+KEYBOARD_CONTROLLER.config]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    xor eax,eax
    jmp .return
.bad:
    lea rcx,[debug_keyboard_failure]
    call debug_write
    mov rcx,[keyboard_controller+KEYBOARD_CONTROLLER.stage]
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
