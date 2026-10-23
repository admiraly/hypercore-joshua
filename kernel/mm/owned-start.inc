; RCX builder, RDX address, R8 permission bits (P/RW/NX, supervisor only).
; Verify new identity/permissions and exact inherited cache selector. EDX=index.
owned_check_page:
    push rbx
    push r12
    push r13
    push r14
    sub rsp,40
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    mov rcx,[rbx+OWNED_PAGING.root]
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,r12
    jne .bad
    mov r9,0x8000000000000007
    and r9,rax
    cmp r9,r13
    jne .bad
    mov rcx,rax
    mov rdx,r8
    call paging_pat_index
    mov r14d,eax
    mov rcx,[rbx+OWNED_PAGING.old_root]
    mov rdx,r12
    call paging_lookup
    test rax,rax
    jz .bad
    cmp rdx,r12
    jne .bad
    mov rcx,rax
    mov rdx,r8
    call paging_pat_index
    cmp eax,r14d
    jne .bad
    mov edx,r14d
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; Build, prove and activate the bootstrap address space. No firmware calls.
; EAX=0 success; EAX=1 fatal error, with no attempt to reclaim partial roots.
owned_paging_start:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    lea r12,[owned_paging]
    lea r13,[boot_info]
    mov eax,0x80000001
    cpuid
    bt edx,20
    jnc .bad
    mov ecx,0x277
    rdmsr
    cmp al,6
    jne .bad
    shl rdx,32
    or rax,rdx
    mov [r12+OWNED_PAGING.pat],rax
    mov rax,[paging_inventory+PAGING_INVENTORY.root]
    mov [r12+OWNED_PAGING.old_root],rax
    mov rax,[r13+BOOT_INFO_V1.image_base]
    mov [r12+OWNED_PAGING.image_base],rax
    add rax,[r13+BOOT_INFO_V1.image_size]
    mov [r12+OWNED_PAGING.image_end],rax
    lea rax,[kernel_stack_guard]
    mov [r12+OWNED_PAGING.guard],rax
    mov rdx,[r13+BOOT_INFO.framebuffer]
    mov rax,rdx
    add rax,[r13+BOOT_INFO.framebuffer_size]
    jc .bad
    add rax,4095
    jc .bad
    and rdx,-4096
    and rax,-4096
    mov [r12+OWNED_PAGING.framebuffer_start],rdx
    mov [r12+OWNED_PAGING.framebuffer_end],rax
    cmp rdx,[r12+OWNED_PAGING.image_end]
    jae .separate
    cmp rax,[r12+OWNED_PAGING.image_base]
    ja .bad
.separate:
    mov ecx,[paging_inventory+PAGING_INVENTORY.physical_bits]
    cmp ecx,47
    jbe .limit
    mov ecx,47
.limit:
    mov r8,1
    shl r8,cl
    mov rcx,r12
    lea rdx,[frame_allocator]
    call owned_paging_init
    test eax,eax
    jnz .bad
    mov rsi,[r13+BOOT_INFO.memory_map]
.descriptor:
    mov rax,[r13+BOOT_INFO.memory_map]
    add rax,[r13+BOOT_INFO.memory_map_size]
    cmp rsi,rax
    jae .framebuffer
    mov eax,[rsi]
    dec eax
    cmp eax,6
    jbe .ram
    cmp eax,8
    je .ram
    cmp eax,9
    jne .next_descriptor
.ram:
    test qword [rsi+32],0x2000
    jnz .next_descriptor
    mov rbx,[rsi+8]
    mov rbp,[rsi+24]
    shl rbp,12
    add rbp,rbx
.segment:
    cmp rbx,rbp
    jae .next_descriptor
    test rbx,rbx
    jnz .policy
    mov ebx,4096
    jmp .segment
.policy:
    mov r14,rbp
    mov r15,PTE_NX|1
    cmp dword [rsi],7
    jne .image
    mov rax,[rsi+32]
    test eax,8
    jz .image
    test rax,-16
    jnz .image
    or r15,2
.image:
    cmp rbx,[r12+OWNED_PAGING.image_base]
    jb .before_image
    cmp rbx,[r12+OWNED_PAGING.image_end]
    jae .fb_cut
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,4096
    cmp rbx,rax
    jb .read_only
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_DATA_RVA
    cmp rbx,rax
    jae .data
    test qword [rsi+32],0x4000
    jnz .bad
    mov r15d,1
    jmp .bound
.data:
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_RODATA_RVA
    cmp rbx,rax
    jb .writable
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_BSS_RVA
    cmp rbx,rax
    jb .read_only
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_RELOC_RVA
    cmp rbx,rax
    jae .reloc
.writable:
    test qword [rsi+32],0x21000
    jnz .bad
    mov r15,PTE_NX|3
    mov r8,rax
    lea rcx,[stack_guards]
    mov rdx,rbx
    call stack_guard_boundary
    test r9d,r9d
    jz .bound
    mov rbx,rax
    jmp .segment
.reloc:
    mov rax,[r12+OWNED_PAGING.image_end]
.read_only:
    mov r15,PTE_NX|1
    jmp .bound
.before_image:
    mov rax,[r12+OWNED_PAGING.image_base]
.bound:
    cmp r14,rax
    jbe .fb_cut
    mov r14,rax
.fb_cut:
    cmp rbx,[r12+OWNED_PAGING.framebuffer_start]
    jb .before_fb
    cmp rbx,[r12+OWNED_PAGING.framebuffer_end]
    jae .map
    mov rbx,[r12+OWNED_PAGING.framebuffer_end]
    jmp .segment
.before_fb:
    cmp r14,[r12+OWNED_PAGING.framebuffer_start]
    jbe .map
    mov r14,[r12+OWNED_PAGING.framebuffer_start]
.map:
    mov rcx,r12
    mov rdx,rbx
    mov r8,r14
    mov r9,r15
    call owned_map_preserved
    test eax,eax
    jnz .bad
    mov rbx,r14
    jmp .segment
.next_descriptor:
    add rsi,[r13+BOOT_INFO.descriptor_size]
    jmp .descriptor
.framebuffer:
    mov rcx,r12
    mov rdx,[r12+OWNED_PAGING.framebuffer_start]
    mov r8,[r12+OWNED_PAGING.framebuffer_end]
    mov r9,PTE_NX|3
    call owned_map_preserved
    test eax,eax
    jnz .bad
    lea rcx,[mapping_context]
    mov rdx,r12
    lea r8,[mapping_records]
    call lifecycle_prepare
    test eax,eax
    jnz .bad
    mov rcx,r12
    call controller_window_prepare
    test eax,eax
    jnz .bad
    ; Prove image boundaries, live stack, metadata and descriptor tables.
    mov rbx,[r12+OWNED_PAGING.image_base]
.verify_image:
    cmp rbx,[r12+OWNED_PAGING.image_end]
    jae .verify_zero
    lea rcx,[stack_guards]
    mov rdx,rbx
    lea r8,[rbx+4096]
    call stack_guard_boundary
    test r9d,r9d
    jnz .verify_absent
    mov r8,PTE_NX|1
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,4096
    cmp rbx,rax
    jb .check_image
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_DATA_RVA
    cmp rbx,rax
    jae .verify_data
    mov r8d,1
    jmp .check_image
.verify_data:
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_RODATA_RVA
    cmp rbx,rax
    jb .verify_writable
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_BSS_RVA
    cmp rbx,rax
    jb .check_image
    mov rax,[r12+OWNED_PAGING.image_base]
    add rax,PE_RELOC_RVA
    cmp rbx,rax
    jae .check_image
.verify_writable:
    mov r8,PTE_NX|3
.check_image:
    mov rcx,r12
    mov rdx,rbx
    call owned_check_page
    test eax,eax
    jnz .bad
    jmp .next_image
.verify_absent:
    mov rcx,[r12+OWNED_PAGING.root]
    mov rdx,rbx
    call paging_lookup
    test rax,rax
    jnz .bad
.next_image:
    add rbx,4096
    jmp .verify_image
.verify_zero:
    mov rcx,[r12+OWNED_PAGING.root]
    xor edx,edx
    call paging_lookup
    test rax,rax
    jnz .bad
    xor edi,edi
    xor r14d,r14d
.verify_record:
    cmp rdi,[frame_allocator+FRAME_ALLOCATOR.count]
    jae .verify_fb
    lea rax,[rdi+rdi*2]
    shl rax,3
    add rax,[frame_allocator+FRAME_ALLOCATOR.records]
    cmp qword [rax+16],FRAME_OWNER_PAGING
    jne .bad
    mov rbx,[rax]
    mov rbp,[rax+8]
    cmp rbp,1
    je .chunk_ok
    cmp rbp,64
    jne .bad
.chunk_ok:
    add r14,rbp
.verify_chunk:
    mov rcx,r12
    mov rdx,rbx
    mov r8,PTE_NX|3
    call owned_check_page
    test eax,eax
    jnz .bad
    ; The data leaf and parent walks must select the same PAT memory type.
    mov ecx,edx
    shl ecx,3
    mov rax,[r12+OWNED_PAGING.pat]
    shr rax,cl
    cmp al,6
    jne .bad
    add rbx,4096
    dec rbp
    jnz .verify_chunk
    inc rdi
    jmp .verify_record
.verify_fb:
    cmp r14,[r12+OWNED_PAGING.table_count]
    jb .bad
    mov rbx,[r12+OWNED_PAGING.framebuffer_start]
.fb_page:
    cmp rbx,[r12+OWNED_PAGING.framebuffer_end]
    jae .activate
    mov rcx,r12
    mov rdx,rbx
    mov r8,PTE_NX|3
    call owned_check_page
    test eax,eax
    jnz .bad
    add rbx,4096
    jmp .fb_page
.activate:
    cmp qword [r12+OWNED_PAGING.failed],0
    jne .bad
    mov qword [r12+OWNED_PAGING.ready],1
    mov ecx,0xc0000080
    rdmsr
    or eax,0x800
    wrmsr
    mov rax,cr0
    or rax,0x10000
    mov cr0,rax
    mov rax,cr4
    test eax,0x20000
    jz .flush
    mov rdx,[r12+OWNED_PAGING.old_root]
    mov cr3,rdx
.flush:
    and rax,-131201
    mov cr4,rax
    mov rax,[r12+OWNED_PAGING.root]
    mov cr3,rax
    mov rax,cr3
    cmp rax,[r12+OWNED_PAGING.root]
    jne .bad
    xor eax,eax
    jmp .return
.bad:
    mov qword [r12+OWNED_PAGING.failed],1
    mov qword [r12+OWNED_PAGING.ready],0
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
