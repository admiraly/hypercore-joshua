; BSP-only bounded test input window. Trusted state and stack were proved at boot.
; Only the explicit KeyboardTest image calls this adapter. No ISR prints/allocates.
keyboard_window_run:
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
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.active],0
    jne .bad
    cmp qword [kernel_context+KERNEL_CONTEXT.state],2
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ready],0
    jne .bad
    call apic_pending_clear
    test eax,eax
    jnz .bad
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
    jne .bad
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_read
    cmp eax,[keyboard_saved_high]
    jne .bad
    mov rcx,r12
    mov edx,r13d
    mov r8d,0x10044
    call ioapic_write
    and eax,0x1afff             ; writable fields, excluding delivery/remote status
    cmp eax,0x10044
    jne .bad
    mov r8d,[keyboard_authority+KEYBOARD_AUTH.bsp_id]
    shl r8d,24
    mov rcx,r12
    lea edx,[r13+1]
    call ioapic_write
    and eax,0xff000000
    cmp eax,r8d
    jne .bad
    mov rax,cr3
    mov [keyboard_window+KEYBOARD_WINDOW.root],rax
    mov [keyboard_window+KEYBOARD_WINDOW.wait_rsp],rsp
    lea rax,[kernel_stack]
    mov [keyboard_window+KEYBOARD_WINDOW.stack_low],rax
    lea rax,[kernel_stack_top]
    mov [keyboard_window+KEYBOARD_WINDOW.stack_high],rax
    lea rax,[keyboard_window_run.wait_close]
    mov [keyboard_window+KEYBOARD_WINDOW.wait_rip],rax
    lea rax,[keyboard_window_run.drain_close]
    mov [keyboard_window+KEYBOARD_WINDOW.drain_rip],rax
    movzx eax,word [kernel_code_selector]
    mov [keyboard_window+KEYBOARD_WINDOW.code_selector],rax
    movzx eax,word [kernel_data_selector]
    mov [keyboard_window+KEYBOARD_WINDOW.data_selector],rax
    mov qword [keyboard_window+KEYBOARD_WINDOW.flags],0x202
%if JOSHUA_KEYBOARD_TEST = 2
    mov qword [keyboard_window+KEYBOARD_WINDOW.flags],0x602
%endif
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
    jnz .bad
    mov rcx,r12
    mov edx,r13d
    mov r8d,0x44
    call ioapic_write
    and eax,0x1afff
    cmp eax,0x44
    jne .bad
    call keyboard_collect
    test eax,eax
    jnz .bad
%if JOSHUA_KEYBOARD_TEST = 16
    lea rdi,[keyboard_decoder+KEYBOARD_DECODER.events]
    mov ecx,64
    mov eax,0x5151
    rep stosw                  ; authored queue setup, not claimed as delivered keys
    mov qword [keyboard_decoder+KEYBOARD_DECODER.count],64
%endif
%if JOSHUA_KEYBOARD_TEST = 17
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.collector_reads],0
    mov rax,[keyboard_window+KEYBOARD_WINDOW.keyboard_irqs]
    mov [keyboard_fixture+KEYBOARD_FIXTURE.keyboard_irqs_base],rax
    mov rax,[keyboard_window+KEYBOARD_WINDOW.keyboard_eois]
    mov [keyboard_fixture+KEYBOARD_FIXTURE.keyboard_eois_base],rax
    mov rax,[keyboard_window+KEYBOARD_WINDOW.timer_eois]
    mov [keyboard_fixture+KEYBOARD_FIXTURE.timer_eois_base],rax
%endif
    lea rcx,[debug_keyboard_window]
    call debug_write
    lea rcx,[keyboard_window]
    call debug_hex
    lea rcx,[debug_keyboard_decoder]
    call debug_write
    lea rcx,[keyboard_decoder]
    call debug_hex
    lea rcx,[debug_keyboard_controller]
    call debug_write
    lea rcx,[keyboard_controller]
    call debug_hex
    lea rcx,[debug_keyboard_fixture]
    call debug_write
    lea rcx,[keyboard_fixture]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
%if (JOSHUA_KEYBOARD_TEST >= 4 && JOSHUA_KEYBOARD_TEST <= 7) || JOSHUA_KEYBOARD_TEST = 15 || JOSHUA_KEYBOARD_TEST = 17
    call keyboard_fixture_pending
    test eax,eax
    jnz .bad
%if JOSHUA_KEYBOARD_TEST = 15
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],3
    jmp .wait
%elif JOSHUA_KEYBOARD_TEST = 17
    jmp .wait
%else
    jmp .cleanup
%endif
%endif
    mov ecx,0x320
    mov edx,0x40
    call lapic_write
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
%if JOSHUA_KEYBOARD_TEST = 13
    mov rax,[user_processes+USER_PROCESS.root]
    mov cr3,rax                 ; authored negative: proved private root, wrong owner
%elif JOSHUA_KEYBOARD_TEST = 14
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],3
%endif
.consume:
%if JOSHUA_KEYBOARD_TEST = 16
    cmp qword [keyboard_decoder+KEYBOARD_DECODER.drops],0
    jne .cleanup
    jmp .wait
%endif
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .cleanup
    lea rcx,[keyboard_decoder]
    call keyboard_pop
    cmp eax,2
    je .wait
    test eax,eax
    jnz .bad
    mov rcx,[keyboard_window+KEYBOARD_WINDOW.event_count]
    cmp rcx,64
    jae .bad
    lea rax,[keyboard_window+KEYBOARD_WINDOW.events]
    mov [rax+rcx*2],dx
    inc qword [keyboard_window+KEYBOARD_WINDOW.event_count]
    cmp edx,13
    je .cleanup
    jmp .consume
.wait:
    irq_sentinels load
    push 2
    popfq
%if JOSHUA_KEYBOARD_TEST = 2
    std
%endif
    sti
    hlt
.wait_close:
    cli
    pushfq
    pop qword [keyboard_return_flags]
    cld
    irq_sentinels check
    cmp rsp,[keyboard_window+KEYBOARD_WINDOW.wait_rsp]
    jne .bad
    mov rax,[keyboard_window+KEYBOARD_WINDOW.flags]
    and eax,~0x200
    cmp rax,[keyboard_return_flags]
    jne .bad
    jmp .consume
.cleanup:
    call user_timer_stop
    mov rcx,[keyboard_io_window]
    mov edx,[keyboard_io_selector]
    mov r8d,0x10044
    call ioapic_write
    and eax,0x1afff
    cmp eax,0x10044
    jne .bad
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.stage],11
    je .disabled
    lea rcx,[keyboard_controller]
    call keyboard_controller_disable
    test eax,eax
    jnz .bad
.disabled:
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.ready],0
    jne .bad
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.failed],0
    jne .bad
    cmp qword [keyboard_controller+KEYBOARD_CONTROLLER.scanning],0
    jne .bad
    call keyboard_flush_output
    test eax,eax
    jnz .bad
%if JOSHUA_KEYBOARD_TEST = 16 || JOSHUA_KEYBOARD_TEST = 17
    call keyboard_status
    mov [keyboard_fixture+KEYBOARD_FIXTURE.terminal_status],rax
    test eax,1
    jnz .bad
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
.drain:
    call apic_pending_clear
    test eax,eax
    jz .drained
    inc qword [keyboard_window+KEYBOARD_WINDOW.drain_windows]
    cmp qword [keyboard_window+KEYBOARD_WINDOW.drain_windows],1024
    jbe .drain_open
%if JOSHUA_KEYBOARD_TEST = 7
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],6
%endif
    jmp .bad
.drain_open:
    push 2
    popfq
%if JOSHUA_KEYBOARD_TEST != 7
    sti
%endif
    nop
.drain_close:
    cli
    jmp .drain
.drained:
    mov qword [keyboard_window+KEYBOARD_WINDOW.phase],3
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
%if JOSHUA_KEYBOARD_TEST >= 4 && JOSHUA_KEYBOARD_TEST <= 6
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ticks],0
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.event_count],0
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.drain_windows],0
    je .bad
%elif JOSHUA_KEYBOARD_TEST = 3
    je .bad                    ; explicit missing-input fixture expects deadline
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],1
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ticks],16
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.event_count],0
    jne .bad
%else
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.keyboard_irqs],0
    je .bad
%endif
    lea rcx,[debug_keyboard_done]
    call debug_write
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

; At most32 OBF bytes. EAX1 means still-full/corrupt; caller quiesces or stops.
; Staging before STI and IRQ collection share the actual decoder, never a shortcut.
keyboard_collect:
    push rbx
    push rbp
    sub rsp,40
    mov ebx,32
.next:
    call keyboard_status
    test al,1
    jz .ok
    test ebx,ebx
%if JOSHUA_KEYBOARD_TEST = 17
    jnz .read
    mov [keyboard_fixture+KEYBOARD_FIXTURE.zero_budget_status],rax
    jmp .bad
.read:
%else
    jz .bad
%endif
    mov ebp,eax
    call keyboard_data_read
%if JOSHUA_KEYBOARD_TEST = 17
    inc qword [keyboard_fixture+KEYBOARD_FIXTURE.collector_reads]
%endif
    dec ebx
    test ebp,0xc0
    jnz .error
    test ebp,0x20
    jnz .processed
    mov edx,eax
    lea rcx,[keyboard_decoder]
    call keyboard_decode
    test eax,eax
    jnz .bad
    jmp .processed
.error:
    lea rcx,[keyboard_decoder]
    call keyboard_decoder_resync
    test eax,eax
    jnz .bad
.processed:
%if JOSHUA_KEYBOARD_TEST = 17
    cmp qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],2
    jne .next
    cmp qword [keyboard_fixture+KEYBOARD_FIXTURE.collector_reads],16
    je .refill
    cmp qword [keyboard_fixture+KEYBOARD_FIXTURE.collector_reads],32
    jne .next
.refill:
    call keyboard_fixture_refill
    test eax,eax
    jnz .bad
%endif
    jmp .next
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop rbp
    pop rbx
    ret

keyboard_irq_entry:
    irq_save
%if JOSHUA_KEYBOARD_TEST = 15
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],4
    mov [keyboard_fixture+KEYBOARD_FIXTURE.frame],rsp
%endif
    lea rcx,[keyboard_window]
    mov rdx,rsp
    mov r8,cr3
    call keyboard_frame_check
    test eax,eax
    jnz .frame_reject
    mov ecx,0x120
    call lapic_read
    test eax,0x10
    jz kernel_fault
    inc qword [keyboard_window+KEYBOARD_WINDOW.keyboard_irqs]
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],2
    je .eoi
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .eoi
    call keyboard_collect
    test eax,eax
    jz .eoi
    mov qword [keyboard_window+KEYBOARD_WINDOW.failure],2
    ; Emergency terminal operation: actual ISR44/active ownership has been proved.
    ; IF0 prevents interleaving; no IOAPIC selector access is permitted here.
%if JOSHUA_KEYBOARD_TEST = 17
    inc qword [keyboard_fixture+KEYBOARD_FIXTURE.emergency_disables]
%endif
    lea rcx,[keyboard_controller]
    call keyboard_controller_disable
    test eax,eax
    jnz kernel_fault
.eoi:
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [keyboard_window+KEYBOARD_WINDOW.keyboard_eois]
    irq_restore
.frame_reject:
%if JOSHUA_KEYBOARD_TEST = 15
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],5
%endif
    jmp kernel_fault

keyboard_timer_saved:
%if JOSHUA_KEYBOARD_TEST >= 8 && JOSHUA_KEYBOARD_TEST <= 14
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],4
    mov [keyboard_fixture+KEYBOARD_FIXTURE.frame],rsp
%if JOSHUA_KEYBOARD_TEST = 8
    xor qword [rsp+152],1
%elif JOSHUA_KEYBOARD_TEST = 9
    xor qword [rsp+184],8
%elif JOSHUA_KEYBOARD_TEST = 10
    xor qword [rsp+168],0x40000
%elif JOSHUA_KEYBOARD_TEST = 11
    add qword [rsp+176],8
%endif
%endif
    lea rcx,[keyboard_window]
    mov rdx,rsp
%if JOSHUA_KEYBOARD_TEST = 12
    add rdx,16
%endif
    mov r8,cr3
    call keyboard_frame_check
    test eax,eax
    jnz .frame_reject
    mov ecx,0x120
    call lapic_read
    test eax,1
    jz kernel_fault
%if JOSHUA_KEYBOARD_TEST = 17
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],2
    jne .no_failed_timer
    inc qword [keyboard_fixture+KEYBOARD_FIXTURE.failed_timer_eois]
.no_failed_timer:
%endif
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],2
    je .eoi
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .eoi
    inc qword [keyboard_window+KEYBOARD_WINDOW.ticks]
%if JOSHUA_KEYBOARD_TEST = 3
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ticks],16
%else
    cmp qword [keyboard_window+KEYBOARD_WINDOW.ticks],512
%endif
    jb .eoi
    mov qword [keyboard_window+KEYBOARD_WINDOW.failure],1
    call user_timer_stop
.eoi:
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [keyboard_window+KEYBOARD_WINDOW.timer_eois]
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],1
    jne .return
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .return
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
.return:
    irq_restore

.frame_reject:
%if JOSHUA_KEYBOARD_TEST >= 8 && JOSHUA_KEYBOARD_TEST <= 14
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],5
%endif
    jmp kernel_fault

; IF0 target fixture: actual IRR proof before normal source retirement. No fake INT.
%if (JOSHUA_KEYBOARD_TEST >= 4 && JOSHUA_KEYBOARD_TEST <= 7) || JOSHUA_KEYBOARD_TEST = 15 || JOSHUA_KEYBOARD_TEST = 17
keyboard_fixture_pending:
    push rbx
    sub rsp,32
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],1
%if JOSHUA_KEYBOARD_TEST = 4 || JOSHUA_KEYBOARD_TEST = 7
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.expected],1
%elif JOSHUA_KEYBOARD_TEST = 5 || JOSHUA_KEYBOARD_TEST = 15
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.expected],0x10
%else
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.expected],0x11
%endif
%if JOSHUA_KEYBOARD_TEST != 5 && JOSHUA_KEYBOARD_TEST != 15
    mov ecx,0x320
    mov edx,0x40
    call lapic_write
    mov ecx,0x380
    mov edx,1
    call lapic_write
%endif
    mov ebx,0x1000000
.poll:
    mov ecx,0x220
    call lapic_read
    and eax,0x11
    cmp rax,[keyboard_fixture+KEYBOARD_FIXTURE.expected]
    je .proved
    pause
    dec ebx
    jnz .poll
    mov eax,1
    jmp .return
.proved:
    mov [keyboard_fixture+KEYBOARD_FIXTURE.pending],rax
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.stage],2
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
%endif


; Explicit controlled-producer timing only. Never present in normal ISR images.
; Data budget remains32; host must replenish actual PS2 data after reads16/32.
%if JOSHUA_KEYBOARD_TEST = 17
keyboard_fixture_refill:
    push rbx
    sub rsp,32
    mov ebx,0x1000000
.poll:
    call keyboard_status
    test eax,1
    jnz .ready
    pause
    dec ebx
    jnz .poll
    mov qword [keyboard_fixture+KEYBOARD_FIXTURE.refill_timeout],1
    mov eax,1
    jmp .return
.ready:
    cmp qword [keyboard_fixture+KEYBOARD_FIXTURE.collector_reads],16
    jne .second
    or qword [keyboard_fixture+KEYBOARD_FIXTURE.refill_acks],1
    jmp .acked
.second:
    or qword [keyboard_fixture+KEYBOARD_FIXTURE.refill_acks],2
.acked:
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
%endif
