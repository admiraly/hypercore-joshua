; Shared at fixed caller RSP. Arguments are trusted source labels, never input.
%macro keyboard_window_activate 4
    pushfq
    pop rax
    test eax,0x200
    jnz %4
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne %4
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne %4
    cmp qword [user_context+USER_CONTEXT.active],0
    jne %4
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    jne %4
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ready],0
    jne %4
    call apic_pending_clear
    test eax,eax
    jnz %4
    lea rax,[keyboard_irq_entry]
    lea rdx,[kernel_idt+0x44*16]
    mov [rdx],ax
    mov byte [rdx+4],0
    shr rax,16
    mov [rdx+6],ax
    shr rax,16
    mov [rdx+8],eax
    mov r12,[keyboard_io_window]
    mov r13,[keyboard_io_selector]
    mov rcx,r12
    mov edx,r13d
    call ioapic_read
    cmp eax,[keyboard_saved_low]
    jne %4
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_read
    cmp eax,[keyboard_saved_high]
    jne %4
    mov rcx,r12
    mov edx,r13d
    mov r8d,0x10044
    call ioapic_write
    and eax,0x1afff             ; writable fields, excluding delivery/remote status
    cmp eax,0x10044
    jne %4
    mov r8d,[keyboard_authority+KEYBOARD_AUTH.bsp_id]
    shl r8d,24
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_write
    and eax,0xff000000
    cmp eax,r8d
    jne %4
    mov rax,cr3
    mov [keyboard_window+KEYBOARD_WINDOW.root],rax
    mov [keyboard_window+KEYBOARD_WINDOW.wait_rsp],rsp
    lea rax,[kernel_stack]
    mov [keyboard_window+KEYBOARD_WINDOW.stack_low],rax
    lea rax,[kernel_stack_top]
    mov [keyboard_window+KEYBOARD_WINDOW.stack_high],rax
    lea rax,[%1]
    mov [keyboard_window+KEYBOARD_WINDOW.wait_rip],rax
    lea rax,[%2]
    mov [keyboard_window+KEYBOARD_WINDOW.drain_rip],rax
    movzx eax,word [kernel_code_selector]
    mov [keyboard_window+KEYBOARD_WINDOW.code_selector],rax
    movzx eax,word [kernel_data_selector]
    mov [keyboard_window+KEYBOARD_WINDOW.data_selector],rax
    mov qword [keyboard_window+KEYBOARD_WINDOW.flags],%3
    lea rax,[image_start+0x1000]
    mov [keyboard_window+KEYBOARD_WINDOW.text_start],rax
    add rax,PE_TEXT_SIZE
    mov [keyboard_window+KEYBOARD_WINDOW.text_end],rax
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],1
    mov rax,KEYBOARD_WINDOW_MAGIC
    mov [keyboard_window+KEYBOARD_WINDOW.ready],rax
    lea rcx,[keyboard_controller]
    call keyboard_controller_enable
    test eax,eax
    jnz %4
    mov rcx,r12
    mov edx,r13d
    mov r8d,0x44
    call ioapic_write
    and eax,0x1afff
    cmp eax,0x44
    jne %4
    call keyboard_collect
    test eax,eax
    jnz %4
%endmacro

%macro keyboard_window_retire 2
    call user_timer_stop
%if JOSHUA_CONSOLE
    call console_timer_check
    test eax,eax
    jnz %2
%endif
    mov rcx,[keyboard_io_window]
    mov edx,[keyboard_io_selector]
    mov r8d,0x10044
    call ioapic_write
    and eax,0x1afff
    cmp eax,0x10044
    jne %2
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.stage],11
    je %%disabled
    lea rcx,[keyboard_controller]
    call keyboard_controller_disable
    test eax,eax
    jnz %2
%%disabled:
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.ready],0
    jne %2
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.failed],0
    jne %2
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.scanning],0
    jne %2
    call keyboard_flush_output
    test eax,eax
    jnz %2
%if JOSHUA_KEYBOARD_TEST = 16 || JOSHUA_KEYBOARD_TEST = 17
    call keyboard_status
    mov [keyboard_fixture+KEYBOARD_FIXTURE.terminal_status],rax
    test eax,1
    jnz %2
%endif
    mov qword [keyboard_window+KEYBOARD_WINDOW.flags],0x202
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],2
%if (JOSHUA_KEYBOARD_TEST >= 4 && JOSHUA_KEYBOARD_TEST <= 7) || JOSHUA_KEYBOARD_TEST = 17
    mov rax,[keyboard_window+KEYBOARD_WINDOW.keyboard_eois]
    mov [keyboard_fixture+KEYBOARD_FIXTURE.drain_keyboard_eois],rax
    mov rax,[keyboard_window+KEYBOARD_WINDOW.timer_eois]
    mov [keyboard_fixture+KEYBOARD_FIXTURE.drain_timer_eois],rax
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],3
%endif
%%drain:
    call apic_pending_clear
    test eax,eax
    jz %%drained
    inc qword [keyboard_window+KEYBOARD_WINDOW.drain_windows]
    cmp qword [keyboard_window+KEYBOARD_WINDOW.drain_windows],1024
    jbe %%drain_open
%if JOSHUA_KEYBOARD_TEST = 7
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],6
%endif
    jmp %2
%%drain_open:
    push 2
    popfq
%if JOSHUA_KEYBOARD_TEST != 7 && JOSHUA_CONSOLE_TEST != 6
    sti
%endif
    nop
%1:
    cli
    jmp %%drain
%%drained:
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],3
%endmacro

%macro keyboard_saturate 1
    cmp qword [%1],-1
    je %%done
    inc qword [%1]
%%done:
%endmacro
