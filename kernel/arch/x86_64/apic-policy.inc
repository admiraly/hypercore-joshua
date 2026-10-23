; Validated context/mappings, IF0. Pure policy through controller access helpers.
apic_quiesce:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov ecx,0x20
    call lapic_read
    cmp qword [apic_context+APIC_CONTEXT.mode],2
    je .full_id
    shr eax,24
.full_id:
    cmp rax,[acpi_context+ACPI_CONTEXT.bsp_id]
    jne .controller_bad
    mov ecx,0x30
    call lapic_read
    mov edx,eax
    and eax,0xff
    cmp eax,0x10
    jb .controller_bad
    cmp eax,0x1f
    ja .controller_bad
    shr edx,16
    and edx,0xff
    cmp edx,5
    jb .controller_bad
    cmp edx,6
    ja .controller_bad
    mov r15d,edx
    xor ebx,ebx
    lea r13,[acpi_io_records]
.io_preflight:
    cmp rbx,[acpi_context+ACPI_CONTEXT.io_count]
    jae .local_preflight_start
    mov r12,CONTROLLER_WINDOW_BASE
    lea rax,[rbx+1]
    shl rax,12
    add r12,rax
    mov rcx,r12
    xor edx,edx
    call ioapic_read
    shr eax,24
    and eax,15
    cmp eax,[r13]
    jne .controller_bad
    mov rcx,r12
    mov edx,1
    call ioapic_read
    ; Supported PC profile: 82093AA (11h) and integrated PCH (20h).
    mov edx,eax
    and edx,0xff
    cmp edx,0x11
    je .io_version_valid
    cmp edx,0x20
    jne .controller_bad
.io_version_valid:
    shr eax,16
    and eax,0xff
    inc eax
    cmp eax,120
    ja .controller_bad
    mov [r13+12],eax
    mov edi,eax
    mov esi,[r13+8]
    add rdi,rsi
    mov rax,0x100000000
    cmp rdi,rax
    ja .controller_bad
    lea r14,[acpi_io_records]
    xor ebp,ebp
.gsi:
    cmp rbp,rbx
    jae .routes_start
    mov eax,[r14+8]
    mov edx,[r14+12]
    add rdx,rax
    cmp rsi,rdx
    jae .gsi_next
    cmp rdi,rax
    ja .controller_bad
.gsi_next:
    add r14,16
    inc rbp
    jmp .gsi
.routes_start:
    xor ebp,ebp
.routes:
    cmp ebp,[r13+12]
    jae .io_next
    mov rcx,r12
    lea edx,[rbp*2+0x10]
    call ioapic_read
    call apic_route_mask
    test edx,edx
    jnz .routing_bad
    inc ebp
    jmp .routes
.io_next:
    add r13,16
    inc rbx
    jmp .io_preflight
.local_preflight_start:
    lea r14,[apic_lvt_offsets]
    xor ebx,ebx
.local_preflight:
    cmp ebx,r15d
    jae .program
    mov ecx,[r14+rbx*4]
    call lapic_read
    call apic_route_mask
    test edx,edx
    jnz .routing_bad
    inc ebx
    jmp .local_preflight
.program:
    test qword [acpi_context+ACPI_CONTEXT.pcat],1
    jz .program_io_start
    call apic_legacy_mask
    test eax,eax
    jnz .routing_bad
.program_io_start:
    xor ebx,ebx
    lea r13,[acpi_io_records]
.program_io:
    cmp rbx,[acpi_context+ACPI_CONTEXT.io_count]
    jae .program_local_start
    mov r12,CONTROLLER_WINDOW_BASE
    lea rax,[rbx+1]
    shl rax,12
    add r12,rax
    xor ebp,ebp
.mask_route:
    cmp ebp,[r13+12]
    jae .program_io_next
    mov rcx,r12
    lea edx,[rbp*2+0x10]
    call ioapic_read
    mov edi,eax
    call apic_route_mask
    test edx,edx
    jnz .routing_bad
    cmp eax,edi
    je .route_done
    mov r8d,eax
    mov rcx,r12
    lea edx,[rbp*2+0x10]
    call ioapic_write
    test eax,0x10000
    jz .routing_bad
.route_done:
    inc ebp
    jmp .mask_route
.program_io_next:
    add r13,16
    inc rbx
    jmp .program_io
.program_local_start:
    lea r14,[apic_lvt_offsets]
    xor ebx,ebx
.program_local:
    cmp ebx,r15d
    jae .timer_off
    mov ecx,[r14+rbx*4]
    call lapic_read
    mov edi,eax
    call apic_route_mask
    test edx,edx
    jnz .routing_bad
    cmp eax,edi
    je .local_done
    mov edx,eax
    mov ecx,[r14+rbx*4]
    call lapic_write
    mov ecx,[r14+rbx*4]
    call lapic_read
    test eax,0x10000
    jz .routing_bad
.local_done:
    inc ebx
    jmp .program_local
.timer_off:
    mov ecx,0x320
    mov edx,0x10040
    call lapic_write
    mov ecx,0x380
    xor edx,edx
    call lapic_write
    mov ecx,0xf0
    call lapic_read
    and eax,-256
    or eax,0x1ff
    mov edx,eax
    mov ecx,0xf0
    call lapic_write
    mov ecx,0x80
    xor edx,edx
    call lapic_write
    mov ecx,0x3e0
    mov edx,3
    call lapic_write
    call apic_pending_clear
    test eax,eax
    jnz .routing_bad
    mov rax,APIC_READY_MAGIC
    mov [apic_context+APIC_CONTEXT.ready],rax
    xor eax,eax
    jmp .return
.controller_bad:
    mov eax,3
    jmp .return
.routing_bad:
    mov eax,4
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

; EAX=0 no inherited ISR/IRR bits, 1 unsupported pending state. No EOI draining.
apic_pending_clear:
    push rbx
    sub rsp,32
    mov ebx,0x100
.next:
    mov ecx,ebx
    call lapic_read
    test eax,eax
    jnz .bad
    add ebx,0x10
    cmp ebx,0x180
    je .irr
    cmp ebx,0x280
    jb .next
    xor eax,eax
    jmp .return
.irr:
    mov ebx,0x200
    jmp .next
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret
