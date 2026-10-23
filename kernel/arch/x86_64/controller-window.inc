; RCX inactive owned builder. Reserve PML4 slot257 before whole-chunk proof.
; EAX=0 prepared (all leaves absent), 1 poisoned. No active-root allocation.
controller_window_prepare:
    push rbx
    sub rsp,32
    mov rbx,rcx
    cmp qword [rbx+OWNED_PAGING.ready],0
    jne .bad
    cmp qword [rbx+OWNED_PAGING.failed],0
    jne .bad
    cmp qword [controller_pt],0
    jne .bad
    mov rdx,[rbx+OWNED_PAGING.root]
    test rdx,rdx
    jz .bad
    add rdx,257*8
    cmp qword [rdx],0
    jne .bad
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,rax
    mov rcx,rbx
    call owned_child
    test rax,rax
    jz .bad
    mov rdx,rax
    mov rcx,rbx
    call owned_child
    test rax,rax
    jz .bad
    mov [controller_pt],rax
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+OWNED_PAGING.failed],1
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; RCX aligned physical controller page, EDX slot0..8. Stage only; no MMIO reads.
controller_stage_page:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,32
    mov r13,rcx
    mov r12d,edx
    cmp edx,8
    ja .bad
    cmp rcx,0x100000
    jb .bad
    test ecx,4095
    jnz .bad
    lea r14,[rcx+4096]
    cmp r14,rcx
    jbe .bad
    cmp r14,[owned_paging+OWNED_PAGING.physical_limit]
    ja .bad
    cmp r13,[owned_paging+OWNED_PAGING.image_end]
    jae .framebuffer
    cmp r14,[owned_paging+OWNED_PAGING.image_base]
    ja .bad
.framebuffer:
    cmp r13,[owned_paging+OWNED_PAGING.framebuffer_end]
    jae .descriptors
    cmp r14,[owned_paging+OWNED_PAGING.framebuffer_start]
    ja .bad
.descriptors:
    test r12d,r12d
    jnz .io_authority
    cmp qword [apic_context+APIC_CONTEXT.mode],1
    jne .bad
    cmp r13,[apic_context+APIC_CONTEXT.lapic]
    jne .bad
    cmp r13,[acpi_context+ACPI_CONTEXT.lapic]
    jne .bad
    jmp .scan_map
.io_authority:
    cmp r12,[acpi_context+ACPI_CONTEXT.io_count]
    ja .bad
    mov eax,0xfec00000
    cmp r13,rax
    jb .bad
    mov eax,0xfed00000
    cmp r14,rax
    ja .bad
    lea rsi,[acpi_io_records]
    mov rax,r12
    dec rax
    shl rax,4
    mov edx,[rsi+rax+4]
    cmp r13,rdx
    jne .bad
.scan_map:
    mov rsi,[boot_info+BOOT_INFO.memory_map]
    mov rdi,rsi
    add rdi,[boot_info+BOOT_INFO.memory_map_size]
.find:
    cmp rsi,rdi
    jae .ownership
    cmp r14,[rsi+8]
    jbe .next
    mov rax,[rsi+24]
    shl rax,12
    add rax,[rsi+8]
    cmp r13,rax
    jae .next
    ; Any overlap is checked; gaps never hide a conflicting partial descriptor.
    cmp dword [rsi],0
    je .type_ok
    cmp dword [rsi],11
    jne .bad
.type_ok:
    test qword [rsi+32],0x23000
    jnz .bad
.next:
    add rsi,[boot_info+BOOT_INFO.descriptor_size]
    jmp .find
.ownership:
    mov rsi,[frame_allocator+FRAME_ALLOCATOR.records]
    mov rdi,[frame_allocator+FRAME_ALLOCATOR.count]
.record:
    test rdi,rdi
    jz .unique
    mov rax,[rsi+8]
    shl rax,12
    add rax,[rsi]
    cmp r13,rax
    jae .next_record
    cmp r14,[rsi]
    ja .bad
.next_record:
    add rsi,24
    dec rdi
    jmp .record
.unique:
    lea rsi,[apic_context+APIC_CONTEXT.staged]
    mov edi,9
.compare:
    mov rax,0x000ffffffffff000
    and rax,[rsi]
    cmp rax,r13
    je .bad
    add rsi,8
    dec edi
    jnz .compare
    mov rcx,[owned_paging+OWNED_PAGING.old_root]
    mov rdx,r13
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,r13
    jne .bad
    mov rcx,rax
    mov rdx,r8
    call paging_pat_index
    mov r15d,eax
    lea rcx,[cache_context]
    mov rdx,r13
    mov r8d,r15d
    call cache_controller_uc
    test eax,eax
    jnz .bad
    mov rax,r15
    and eax,3
    shl eax,3
    and r15d,4
    shl r15d,5
    or rax,r15
    mov rdx,PTE_NX|3
    or rax,rdx
    or rax,r13
    lea rsi,[apic_context+APIC_CONTEXT.staged]
    mov [rsi+r12*8],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; Preallocated controller PT only. Stage all pages before publishing any leaf.
; APIC_CONTEXT.mode is1 xAPIC/2 x2APIC. EAX=0 mapped, 1 failure/stop.
controller_mappings_start:
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp,40
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov rax,cr3
    cmp rax,[owned_paging+OWNED_PAGING.root]
    jne .bad
    mov rax,cr4
    test eax,0x20080
    jnz .bad
    mov rdi,[controller_pt]
    test rdi,rdi
    jz .bad
    mov ecx,512
    mov rsi,rdi
.empty:
    cmp qword [rsi],0
    jne .bad
    add rsi,8
    loop .empty
    lea rcx,[cache_context]
    lea rax,[mtrr_snapshot]
    mov [cache_context+CACHE_CONTEXT.raw],rax
    call cache_snapshot
    test eax,eax
    jnz .bad
    mov rax,[cache_context+CACHE_CONTEXT.pat]
    cmp rax,[owned_paging+OWNED_PAGING.pat]
    jne .bad
    cmp qword [apic_context+APIC_CONTEXT.mode],1
    jne .io
    mov rcx,[acpi_context+ACPI_CONTEXT.lapic]
    xor edx,edx
    call controller_stage_page
    test eax,eax
    jnz .bad
.io:
    xor ebx,ebx
    lea rsi,[acpi_io_records]
.io_page:
    cmp rbx,[acpi_context+ACPI_CONTEXT.io_count]
    jae .publish
    mov ecx,[rsi+4]
    lea edx,[rbx+1]
    call controller_stage_page
    test eax,eax
    jnz .bad
    add rsi,16
    inc rbx
    jmp .io_page
.publish:
    lea rsi,[apic_context+APIC_CONTEXT.staged]
    xor ebx,ebx
    xor r12d,r12d
.leaf:
    mov rax,[rsi+rbx*8]
    mov [rdi+rbx*8],rax
    test rax,rax
    jz .next_leaf
    inc r12d
.next_leaf:
    inc ebx
    cmp ebx,9
    jb .leaf
    xor ebx,ebx
.flush:
    cmp qword [rsi+rbx*8],0
    je .next_flush
    mov rax,CONTROLLER_WINDOW_BASE
    mov rdx,rbx
    shl rdx,12
    add rax,rdx
    invlpg [rax]
.next_flush:
    inc ebx
    cmp ebx,9
    jb .flush
    mov [apic_context+APIC_CONTEXT.uc_pages],r12
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
