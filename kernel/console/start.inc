; Native foreground client. No helper runs with IF1; only exact wait/drain labels do.
console_run:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    pushfq
    pop rax
    test eax,0x600
    jnz .bad
    call console_surface_prepare
    test eax,eax
    jnz .bad
    lea rcx,[console_editor]
    call editor_init
    test eax,eax
    jnz .bad
    lea rcx,[console_grid]
    lea rdx,[console_anchor]
    call grid_init
    test eax,eax
    jnz .bad
    lea rcx,[console_model]
    lea rdx,[console_grid]
    lea r8,[console_anchor]
    call model_init
    test eax,eax
    jnz .bad
    call console_clear_surface
    call console_render
    test eax,eax
    jnz .bad
    call user_timer_stop
    call console_timer_check
    test eax,eax
    jnz .bad
    keyboard_window_activate .wait_close,.drain_close,0x202,.bad
    lea rcx,[debug_console_ready]
    call debug_write
    lea rcx,[console_model]
    call debug_hex
%macro console_address 2
    lea rcx,[%1]
    call debug_write
    lea rcx,[%2]
    call debug_hex
%endmacro
    console_address debug_console_grid,console_grid
    console_address debug_console_editor,console_editor
    console_address debug_console_command,console_command
    console_address debug_console_response,console_response
    console_address debug_console_facts,console_facts
    console_address debug_console_window,keyboard_window
    console_address debug_console_decoder,keyboard_decoder
    console_address debug_console_controller,keyboard_controller
%unmacro console_address 2
    lea rcx,[debug_newline]
    call debug_write
.consume:
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .failed_retire
    lea rcx,[keyboard_decoder]
    call keyboard_pop
    cmp eax,2
    je .wait
    test eax,eax
    jnz .bad
    lea rcx,[console_editor]
    call editor_event
    test eax,eax
    jnz .bad
    cmp edx,2
    je .submit
    cmp edx,1
    je .prompt
    cmp edx,4
    jne .consume
.prompt:
    lea rcx,[console_model]
    lea rdx,[console_grid]
    lea r8,[console_anchor]
    lea r9,[console_editor]
    call model_prompt
    test eax,eax
    jnz .bad
    call console_render
    test eax,eax
    jnz .bad
    jmp .consume
.submit:
    lea rcx,[console_editor]
    lea rdx,[console_command]
    call command_parse
    test eax,eax
    jnz .bad
    call console_facts_capture
    test eax,eax
    jnz .bad
    lea rcx,[console_command]
    lea rdx,[console_facts]
    lea r8,[console_response]
    call response_build
    test eax,eax
    jnz .bad
    lea rax,[console_command]
    mov [rsp+32],rax
    lea rax,[console_response]
    mov [rsp+40],rax
    lea rcx,[console_model]
    lea rdx,[console_grid]
    lea r8,[console_anchor]
    lea r9,[console_editor]
    call model_complete
    test eax,eax
    jnz .bad
    call console_render
    test eax,eax
    jnz .bad
    cmp qword [console_model+CONSOLE_MODEL.action],2
    je .halt
    jmp .consume
.wait:
    irq_sentinels load
    push 2
    popfq
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
    cmp qword [keyboard_return_flags],2
    jne .bad
    jmp .consume
.halt:
    keyboard_window_retire .drain_close,.bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .bad
    call console_finish_screen
    test eax,eax
    jnz .bad
    lea rcx,[debug_console_halted]
    call debug_write
    xor eax,eax
    jmp .return
.failed_retire:
    ; Retire through the same fixed-RSP macro, then failure2 rejects normal success.
    jmp .halt
.bad:
    cli
    cld
    lea rcx,[debug_console_failed]
    call debug_write
    mov eax,1
.return:
    add rsp,56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

console_render:
    lea rcx,[console_grid]
    lea rdx,[console_anchor]
    jmp grid_render

; Source BOOT_INFO is fixed owned image data. grid_init repeats geometry proof.
console_surface_prepare:
    lea r10,[boot_info]
    mov rax,BOOT_MAGIC
    cmp [r10+BOOT_INFO.magic],rax
    jne .bad
    cmp dword [r10+BOOT_INFO.version],1
    jne .bad
    cmp dword [r10+BOOT_INFO.size],BOOT_INFO_V1_size
    jne .bad
    lea rax,[image_start]
    cmp [r10+BOOT_INFO_V1.image_base],rax
    jne .bad
    cmp qword [r10+BOOT_INFO_V1.image_size],PE_IMAGE_SIZE
    jne .bad
    mov r8,[r10+BOOT_INFO.framebuffer]
    mov r9,[r10+BOOT_INFO.framebuffer_size]
    test r8,r8
    jz .bad
    test r9,r9
    jz .bad
    dec r9
    add r9,r8
    jc .bad
    lea rdx,[rax+PE_IMAGE_SIZE-1]
    cmp r8,rdx
    ja .disjoint
    cmp rax,r9
    jbe .bad
.disjoint:
    mov [console_anchor+CONSOLE_SURFACE.base],r8
    mov rax,[r10+BOOT_INFO.framebuffer_size]
    mov [console_anchor+CONSOLE_SURFACE.size],rax
    mov eax,[r10+BOOT_INFO.width]
    mov [console_anchor+CONSOLE_SURFACE.width],rax
    mov eax,[r10+BOOT_INFO.height]
    mov [console_anchor+CONSOLE_SURFACE.height],rax
    mov eax,[r10+BOOT_INFO.stride]
    mov [console_anchor+CONSOLE_SURFACE.stride],rax
    mov eax,[r10+BOOT_INFO.pixel_format]
    mov [console_anchor+CONSOLE_SURFACE.format],rax
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; Called only after grid_init's surface arithmetic proof, before live input.
console_clear_surface:
    push rdi
    mov rdi,[console_anchor+CONSOLE_SURFACE.base]
    mov r8,[console_anchor+CONSOLE_SURFACE.height]
    mov eax,0x000c0602
    cmp qword [console_anchor+CONSOLE_SURFACE.format],0
    je .row
    mov eax,0x0002060c
.row:
    mov rcx,[console_anchor+CONSOLE_SURFACE.width]
    rep stosd
    mov rdx,[console_anchor+CONSOLE_SURFACE.stride]
    sub rdx,[console_anchor+CONSOLE_SURFACE.width]
    lea rdi,[rdi+rdx*4]
    dec r8
    jnz .row
    pop rdi
    ret

console_timer_check:
    sub rsp,40
    mov ecx,0x320
    call lapic_read
    and eax,0x700ff
    cmp eax,0x10040
    jne .bad
    mov ecx,0x380
    call lapic_read
    test eax,eax
    jnz .bad
    mov ecx,0x390
    call lapic_read
    test eax,eax
    jnz .bad
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    ret

console_facts_capture:
    push rbx
    push rsi
    sub rsp,40
    pushfq
    pop rax
    test eax,0x600
    jnz .bad
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne .bad
    mov rax,BOOT_MAGIC
    cmp [boot_info+BOOT_INFO.magic],rax
    jne .bad
    cmp dword [boot_info+BOOT_INFO.version],1
    jne .bad
    cmp dword [boot_info+BOOT_INFO.size],BOOT_INFO_V1_size
    jne .bad
    lea rax,[image_start]
    cmp [boot_info+BOOT_INFO_V1.image_base],rax
    jne .bad
    cmp qword [boot_info+BOOT_INFO_V1.image_size],PE_IMAGE_SIZE
    jne .bad
    mov rax,FRAME_ALLOCATOR_MAGIC
    cmp [frame_allocator+FRAME_ALLOCATOR.ready],rax
    jne .bad
    lea rax,[candidate_set]
    cmp [frame_allocator+FRAME_ALLOCATOR.ranges],rax
    jne .bad
    lea rax,[frame_records]
    cmp [frame_allocator+FRAME_ALLOCATOR.records],rax
    jne .bad
    cmp qword [frame_allocator+FRAME_ALLOCATOR.capacity],FRAME_ALLOCATION_CAPACITY
    jne .bad
    cmp qword [frame_allocator+FRAME_ALLOCATOR.count],FRAME_ALLOCATION_CAPACITY
    ja .bad
    mov rax,[frame_allocator+FRAME_ALLOCATOR.free_pages]
    cmp rax,[frame_allocator+FRAME_ALLOCATOR.total_pages]
    ja .bad
    lea rax,[candidate_ranges]
    cmp [candidate_set+RANGE_SET.entries],rax
    jne .bad
    cmp qword [candidate_set+RANGE_SET.capacity],CANDIDATE_CAPACITY
    jne .bad
    cmp qword [candidate_set+RANGE_SET.count],CANDIDATE_CAPACITY
    ja .bad
    cmp qword [owned_paging+OWNED_PAGING.ready],1
    jne .bad
    cmp qword [owned_paging+OWNED_PAGING.failed],0
    jne .bad
    mov rax,[owned_paging+OWNED_PAGING.table_count]
    test rax,rax
    jz .bad
    cmp rax,FRAME_ALLOCATION_CAPACITY*64
    ja .bad
    mov rax,ACPI_READY_MAGIC
    cmp [acpi_context+ACPI_CONTEXT.ready],rax
    jne .bad
    cmp qword [acpi_context+ACPI_CONTEXT.count],256
    ja .bad
    mov rax,[acpi_context+ACPI_CONTEXT.enabled]
    test rax,rax
    jz .bad
    cmp rax,[acpi_context+ACPI_CONTEXT.count]
    ja .bad
    mov rax,APIC_READY_MAGIC
    cmp [apic_context+APIC_CONTEXT.ready],rax
    jne .bad
    mov rax,[apic_context+APIC_CONTEXT.mode]
    dec rax
    cmp rax,1
    ja .bad
    mov rax,USER_READY_MAGIC
    cmp [user_context+USER_CONTEXT.ready],rax
    jne .bad
    cmp qword [user_context+USER_CONTEXT.stage],3
    jne .bad
    cmp qword [user_context+USER_CONTEXT.failed],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.active],0
    jne .bad
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne .bad
    xor ebx,ebx
    lea rsi,[user_processes]
.process:
    mov rax,[rsi+USER_PROCESS.state]
    sub rax,3
    cmp rax,2
    ja .bad
    add rsi,USER_PROCESS_size
    inc ebx
    cmp ebx,USER_PROCESS_COUNT
    jb .process
    mov rax,KEYBOARD_WINDOW_MAGIC
    cmp [keyboard_window+KEYBOARD_WINDOW.ready],rax
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],1
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .bad
    mov rax,[keyboard_window+KEYBOARD_WINDOW.keyboard_irqs]
    cmp rax,[keyboard_window+KEYBOARD_WINDOW.keyboard_eois]
    jne .bad
    ; Read-only metadata validation; no event is consumed during snapshot capture.
    lea rcx,[keyboard_decoder]
    call keyboard_decoder_check
    test eax,eax
    jnz .bad
    mov qword [console_facts+CONSOLE_FACTS.ready],0
%macro fact_copy 2
    mov rax,[%2]
    mov [console_facts+CONSOLE_FACTS.%1],rax
%endmacro
    fact_copy image_bytes,boot_info+BOOT_INFO_V1.image_size
    fact_copy free_pages,frame_allocator+FRAME_ALLOCATOR.free_pages
    fact_copy live_allocations,frame_allocator+FRAME_ALLOCATOR.count
    fact_copy candidate_ranges,candidate_set+RANGE_SET.count
    fact_copy owned_table_pages,owned_paging+OWNED_PAGING.table_count
    fact_copy enabled_cpus,acpi_context+ACPI_CONTEXT.enabled
    fact_copy apic_mode,apic_context+APIC_CONTEXT.mode
    fact_copy timer_eois,apic_context+APIC_CONTEXT.eois
    fact_copy keyboard_irqs,keyboard_window+KEYBOARD_WINDOW.keyboard_irqs
    fact_copy keyboard_eois,keyboard_window+KEYBOARD_WINDOW.keyboard_eois
    fact_copy keyboard_drops,keyboard_decoder+KEYBOARD_DECODER.drops
    fact_copy process_a_state,user_processes+USER_PROCESS.state
    fact_copy process_a_status,user_processes+USER_PROCESS.status
    fact_copy process_b_state,user_processes+USER_PROCESS_size+USER_PROCESS.state
    fact_copy process_b_status,user_processes+USER_PROCESS_size+USER_PROCESS.status
%unmacro fact_copy 2
    mov rax,FACTS_MAGIC
    mov [console_facts+CONSOLE_FACTS.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop rsi
    pop rbx
    ret

console_finish_screen:
    push rdi
    sub rsp,32
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],3
    jne .bad
    cmp qword [keyboard_window+KEYBOARD_WINDOW.failure],0
    jne .bad
    cmp qword [console_model+CONSOLE_MODEL.action],2
    jne .bad
    call console_timer_check
    test eax,eax
    jnz .bad
    call apic_pending_clear
    test eax,eax
    jnz .bad
    call keyboard_status
    test eax,1
    jnz .bad
    lea rcx,[console_model]
    lea rdx,[console_grid]
    lea r8,[console_anchor]
    call model_check
    test eax,eax
    jnz .bad
    mov qword [console_grid+CONSOLE_GRID.cursor_enabled],0
    mov rax,[console_grid+CONSOLE_GRID.cursor_y]
    bts [console_grid+CONSOLE_GRID.dirty],rax
    mov r8,[console_grid+CONSOLE_GRID.rows]
    dec r8
    bts [console_grid+CONSOLE_GRID.dirty],r8
    imul rax,r8,100
    lea rdi,[console_grid+CONSOLE_GRID.cells]
    add rdi,rax
    mov rcx,[console_grid+CONSOLE_GRID.columns]
    mov al,32
    rep stosb
    imul rax,r8,100
    lea rdi,[console_grid+CONSOLE_GRID.cells]
    add rdi,rax
    lea rdx,[console_halted_text]
    xor ecx,ecx
.copy:
    mov al,[rdx+rcx]
    mov [rdi+rcx],al
    inc ecx
    cmp ecx,console_halted_text_end-console_halted_text
    jb .copy
    call console_render
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rdi
    ret
