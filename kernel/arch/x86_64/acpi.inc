; Trusted context/map/root; untrusted firmware bytes. No reads outside proved ranges.
; RCX context, RDX physical address, R8 bytes (1..65536) -> EAX=0 readable, 1 rejected.
acpi_readable:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov r12,rcx
    mov r13,rdx
    test rdx,rdx
    jz .bad
    test r8,r8
    jz .bad
    cmp r8,65536
    ja .bad
    mov r14,rdx
    add r14,r8
    jc .bad
    mov rax,[r12+ACPI_CONTEXT.builder]
    cmp r14,[rax+OWNED_PAGING.physical_limit]
    ja .bad
    mov rax,0x0000800000000000
    cmp r14,rax
    ja .bad
    mov r15,[r12+ACPI_CONTEXT.boot]
.cover:
    mov rbx,[r15+BOOT_INFO.memory_map]
    mov rbp,rbx
    add rbp,[r15+BOOT_INFO.memory_map_size]
.find:
    cmp rbx,rbp
    jae .bad
    mov rax,[rbx+8]
    cmp r13,rax
    jb .next
    mov rdi,[rbx+24]
    shl rdi,12
    add rdi,rax
    cmp r13,rdi
    jae .next
    cmp dword [rbx],9
    je .type_ok
    cmp dword [rbx],10
    jne .bad
.type_ok:
    test qword [rbx+32],0x2000
    jnz .bad
    cmp rdi,r14
    cmova rdi,r14
    mov rsi,r13
    and rsi,-4096
.page:
    mov rax,[r12+ACPI_CONTEXT.builder]
    mov rcx,[rax+OWNED_PAGING.root]
    mov rdx,rsi
    call paging_lookup
    test rax,rax
    jz .bad
    test al,4
    jnz .bad
    cmp rdx,rsi
    jne .bad
    add rsi,4096
    cmp rsi,rdi
    jb .page
    mov r13,rdi
    cmp r13,r14
    jb .cover
    xor eax,eax
    jmp .return
.next:
    add rbx,[r15+BOOT_INFO.descriptor_size]
    jmp .find
.bad:
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

; RCX proved byte range, RDX length -> EAX byte sum. Caller supplies bounded length.
acpi_checksum:
    xor eax,eax
.next:
    add al,[rcx]
    inc rcx
    dec rdx
    jnz .next
    ret

; RCX initialized trusted context. EAX=0 ready; 1 unreadable, 2 root/signature,
; 3 checksum, 4 length/record layout, 5 topology, 6 capacity. All failures clear ready/count.
acpi_topology_build:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    mov r12,rcx
    mov qword [r12+ACPI_CONTEXT.ready],0
    mov qword [r12+ACPI_CONTEXT.count],0
    mov qword [r12+ACPI_CONTEXT.enabled],0
    mov qword [r12+ACPI_CONTEXT.xsdt],0
    mov qword [r12+ACPI_CONTEXT.madt],0
    cmp qword [r12+ACPI_CONTEXT.capacity],0
    je .capacity
    cmp qword [r12+ACPI_CONTEXT.capacity],256
    ja .capacity
    cmp qword [r12+ACPI_CONTEXT.records],0
    je .capacity
    mov rax,[r12+ACPI_CONTEXT.bsp_id]
    shr rax,32
    jnz .topology
    mov rax,[r12+ACPI_CONTEXT.boot]
    mov rbx,[rax+BOOT_INFO.acpi_rsdp]
    mov rdx,rbx
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .unreadable
    mov rax,0x2052545020445352
    cmp [rbx],rax
    jne .root
    cmp byte [rbx+15],2
    jb .root
    mov ebp,[rbx+20]
    cmp ebp,36
    jb .layout
    cmp ebp,4096
    ja .layout
    mov rcx,r12
    mov rdx,rbx
    mov r8,rbp
    call acpi_readable
    test eax,eax
    jnz .unreadable
    mov rcx,rbx
    mov edx,20
    call acpi_checksum
    test al,al
    jnz .checksum
    mov rcx,rbx
    mov rdx,rbp
    call acpi_checksum
    test al,al
    jnz .checksum
    mov rbx,[rbx+24]
    mov [r12+ACPI_CONTEXT.xsdt],rbx
    mov rcx,r12
    mov rdx,rbx
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .unreadable
    cmp dword [rbx],0x54445358
    jne .root
    mov ebp,[rbx+4]
    cmp ebp,36
    jb .layout
    cmp ebp,65536
    ja .layout
    mov r13,rbp
    sub r13,36
    test r13b,7
    jnz .layout
    shr r13,3
    cmp r13,256
    ja .capacity
    mov rcx,r12
    mov rdx,rbx
    mov r8,rbp
    call acpi_readable
    test eax,eax
    jnz .unreadable
    mov rcx,rbx
    mov rdx,rbp
    call acpi_checksum
    test al,al
    jnz .checksum
    lea rsi,[rbx+36]
.child:
    test r13,r13
    jz .madt
    mov rdi,[rsi]
    mov rcx,r12
    mov rdx,rdi
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .unreadable
    cmp dword [rdi],0x43495041
    jne .next_child
    cmp qword [r12+ACPI_CONTEXT.madt],0
    jne .topology
    mov [r12+ACPI_CONTEXT.madt],rdi
.next_child:
    add rsi,8
    dec r13
    jmp .child
.madt:
    mov rbx,[r12+ACPI_CONTEXT.madt]
    test rbx,rbx
    jz .topology
    ; Header was proved before reading its length.
    mov ebp,[rbx+4]
    cmp ebp,44
    jb .layout
    cmp ebp,65536
    ja .layout
    mov rcx,r12
    mov rdx,rbx
    mov r8,rbp
    call acpi_readable
    test eax,eax
    jnz .unreadable
    mov rcx,rbx
    mov rdx,rbp
    call acpi_checksum
    test al,al
    jnz .checksum
    mov eax,[rbx+36]
    mov [r12+ACPI_CONTEXT.lapic],rax
    mov eax,[rbx+40]
    and eax,1
    mov [r12+ACPI_CONTEXT.pcat],rax
    mov qword [rsp+32],0
    mov qword [rsp+40],0
    lea rsi,[rbx+44]
    add rbp,rbx
.record:
    cmp rsi,rbp
    jae .finish
    mov rax,rbp
    sub rax,rsi
    cmp rax,2
    jb .layout
    movzx r15d,byte [rsi+1]
    cmp r15d,2
    jb .layout
    cmp r15,rax
    ja .layout
    cmp byte [rsi],0
    je .local
    cmp byte [rsi],9
    je .x2
    cmp byte [rsi],5
    je .override
    jmp .next_record
.local:
    cmp r15d,8
    jb .layout
    movzx r13d,byte [rsi+3]
    movzx r14d,byte [rsi+2]
    mov edi,[rsi+4]
    jmp .cpu
.x2:
    cmp r15d,16
    jb .layout
    mov r13d,[rsi+4]
    mov r14d,[rsi+12]
    mov edi,[rsi+8]
.cpu:
    test edi,3
    jz .next_record
    mov rdx,[r12+ACPI_CONTEXT.count]
    cmp rdx,[r12+ACPI_CONTEXT.capacity]
    jae .capacity
    mov rbx,[r12+ACPI_CONTEXT.records]
    xor ecx,ecx
.unique:
    cmp rcx,rdx
    jae .append
    cmp [rbx],r13d
    je .topology
    cmp [rbx+4],r14d
    je .topology
    add rbx,16
    inc rcx
    jmp .unique
.append:
    mov [rbx],r13d
    mov [rbx+4],r14d
    mov [rbx+8],edi
    movzx eax,byte [rsi]
    mov [rbx+12],eax
    inc qword [r12+ACPI_CONTEXT.count]
    test edi,1
    jz .next_record
    inc qword [r12+ACPI_CONTEXT.enabled]
    cmp r13,[r12+ACPI_CONTEXT.bsp_id]
    jne .next_record
    inc qword [rsp+40]
    jmp .next_record
.override:
    cmp r15d,12
    jb .layout
    cmp qword [rsp+32],0
    jne .topology
    inc qword [rsp+32]
    mov rax,[rsi+4]
    mov [r12+ACPI_CONTEXT.lapic],rax
.next_record:
    add rsi,r15
    jmp .record
.finish:
    cmp qword [rsp+40],1
    jne .topology
    mov rax,[r12+ACPI_CONTEXT.lapic]
    test rax,rax
    jz .topology
    test eax,0xfff
    jnz .topology
    add rax,4096
    jc .topology
    mov rdx,[r12+ACPI_CONTEXT.builder]
    cmp rax,[rdx+OWNED_PAGING.physical_limit]
    ja .topology
    mov rdx,0x0000800000000000
    cmp rax,rdx
    ja .topology
    mov rax,ACPI_READY_MAGIC
    mov [r12+ACPI_CONTEXT.ready],rax
    xor eax,eax
    jmp .return
.unreadable:
    mov eax,1
    jmp .failed
.root:
    mov eax,2
    jmp .failed
.checksum:
    mov eax,3
    jmp .failed
.layout:
    mov eax,4
    jmp .failed
.topology:
    mov eax,5
    jmp .failed
.capacity:
    mov eax,6
.failed:
    mov qword [r12+ACPI_CONTEXT.ready],0
    mov qword [r12+ACPI_CONTEXT.count],0
    mov qword [r12+ACPI_CONTEXT.enabled],0
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
