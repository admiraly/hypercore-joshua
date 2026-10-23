; RCX validated PAGING_INVENTORY config, RDX physical table-frame address.
; Verify bounds and readable RAM classification before any table dereference.
; Requires retained UEFI x64 identity mappings. EAX=0 valid, 1 invalid.
paging_table_check:
    test rdx,rdx
    jz .bad
    test rdx,0xfff
    jnz .bad
    mov rax,0x0000800000000000
    cmp rdx,rax
    jae .bad
    mov r9,rdx
    add r9,4096
    jc .bad
    mov r11,rcx
    mov ecx,[r11+PAGING_INVENTORY.physical_bits]
    mov r8,1
    shl r8,cl
    mov rcx,r11
    cmp r9,r8
    ja .bad
    mov r10,[rcx+PAGING_INVENTORY.memory_map]
    mov r11,r10
    add r11,[rcx+PAGING_INVENTORY.map_size]
.scan:
    cmp r10,r11
    jae .bad
    cmp rdx,[r10+8]
    jb .next
    mov rax,[r10+24]
    shl rax,12
    add rax,[r10+8]
    cmp r9,rax
    ja .next
    mov eax,[r10]
    dec eax
    cmp eax,6
    jbe .ram
    cmp eax,8
    je .ram
    cmp eax,9
    jne .bad
.ram:
    test qword [r10+32],0x2000
    jnz .bad
    xor eax,eax
    ret
.next:
    add r10,[rcx+PAGING_INVENTORY.descriptor_size]
    jmp .scan
.bad:
    mov eax,1
    ret

; RCX config: validated final map, physical width, flags, owned queue, root.
; Queue entries are {physical frame, level}. Four-level paging only.
; Firmware ABI. EAX=0 success; 1 failure, count cleared. Never changes tables.
paging_inventory_build:
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
    mov qword [r12+PAGING_INVENTORY.count],0
    mov r13,[r12+PAGING_INVENTORY.entries]
    test r13,r13
    jz .bad
    cmp qword [r12+PAGING_INVENTORY.capacity],0
    je .bad
    cmp qword [r12+PAGING_INVENTORY.capacity],PAGING_TABLE_CAPACITY
    ja .bad
    cmp qword [r12+PAGING_INVENTORY.physical_bits],32
    jb .bad
    cmp qword [r12+PAGING_INVENTORY.physical_bits],52
    ja .bad
    test qword [r12+PAGING_INVENTORY.flags],-4
    jnz .bad
    mov rdi,[r12+PAGING_INVENTORY.root]
    mov rdx,rdi
    call paging_table_check
    test eax,eax
    jnz .bad
    mov [r13],rdi
    mov qword [r13+8],4
    mov qword [r12+PAGING_INVENTORY.count],1
    xor r14d,r14d
.table:
    cmp r14,[r12+PAGING_INVENTORY.count]
    jae .done
    mov rax,r14
    shl rax,4
    mov r15,[r13+rax]
    mov rbp,[r13+rax+8]
    xor ebx,ebx
.entry:
    mov rsi,[r15+rbx*8]
    test sil,1
    jz .next
    bt rsi,63
    jnc .address
    test qword [r12+PAGING_INVENTORY.flags],PAGING_FLAG_NXE
    jz .bad
.address:
    mov rdi,0x000ffffffffff000
    and rdi,rsi
    mov ecx,[r12+PAGING_INVENTORY.physical_bits]
    mov rax,1
    shl rax,cl
    cmp rdi,rax
    jae .bad
    cmp ebp,1
    je .next
    test sil,0x80
    jz .child
    cmp ebp,4
    je .bad
    cmp ebp,3
    jne .large_2m
    test qword [r12+PAGING_INVENTORY.flags],PAGING_FLAG_1G
    jz .bad
    test esi,0x3fffe000
    jnz .bad
    jmp .next
.large_2m:
    test esi,0x1fe000
    jnz .bad
    jmp .next
.child:
    xor eax,eax
.find:
    cmp rax,[r12+PAGING_INVENTORY.count]
    jae .append
    mov rdx,rax
    shl rdx,4
    cmp rdi,[r13+rdx]
    jne .find_next
    mov r8,rbp
    dec r8
    cmp r8,[r13+rdx+8]
    jne .bad
    jmp .next
.find_next:
    inc rax
    jmp .find
.append:
    cmp rax,[r12+PAGING_INVENTORY.capacity]
    jae .bad
    mov rcx,r12
    mov rdx,rdi
    call paging_table_check
    test eax,eax
    jnz .bad
    mov rax,[r12+PAGING_INVENTORY.count]
    shl rax,4
    mov [r13+rax],rdi
    mov rdx,rbp
    dec rdx
    mov [r13+rax+8],rdx
    inc qword [r12+PAGING_INVENTORY.count]
.next:
    inc ebx
    cmp ebx,512
    jb .entry
    inc r14
    jmp .table
.bad:
    mov qword [r12+PAGING_INVENTORY.count],0
    mov eax,1
    jmp .return
.done:
    xor eax,eax
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

; RCX successful inventory, RDX owned candidate RANGE_SET.
; EAX=0 success; 1 failure invalidates the candidate count. No table writes.
paging_inventory_reserve:
    push rbx
    push rsi
    push r12
    sub rsp,32
    mov r12,rcx
    mov rsi,rdx
    xor ebx,ebx
.next:
    cmp rbx,[r12+PAGING_INVENTORY.count]
    jae .done
    mov rax,rbx
    shl rax,4
    mov rdx,[r12+PAGING_INVENTORY.entries]
    mov rdx,[rdx+rax]
    lea r8,[rdx+4096]
    mov rcx,rsi
    call range_reserve
    test eax,eax
    jnz .bad
    inc rbx
    jmp .next
.bad:
    mov qword [rsi+RANGE_SET.count],0
    mov eax,1
    jmp .return
.done:
    xor eax,eax
.return:
    add rsp,32
    pop r12
    pop rsi
    pop rbx
    ret
