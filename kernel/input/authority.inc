; Firmware-only authority discovery. Caller owns a trusted ACPI_CONTEXT and
; disjoint writable KEYBOARD_AUTH; retained tables are immutable during the call.
; RCX context, RDX output -> EAX0 valid,1 rejected/zeroed output. No hardware I/O.
keyboard_authority_build:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,232
    mov r12,rcx
    mov rbx,rdx
    test rdx,rdx
    jz .bad_pointer
    mov rax,rdx
    add rax,KEYBOARD_AUTH_size
    jc .bad_pointer
    mov rdi,rdx
    xor eax,eax
    mov ecx,KEYBOARD_AUTH_size/8
    rep stosq
    test r12,r12
    jz .bad
    mov rax,ACPI_READY_MAGIC
    cmp [r12+ACPI_CONTEXT.ready],rax
    jne .bad
    cmp qword [r12+ACPI_CONTEXT.bsp_id],255
    ja .bad
    mov rcx,r12
    mov rdx,[r12+ACPI_CONTEXT.xsdt]
    mov r8d,0x54445358
    mov r9d,36
    call keyboard_table_check
    test eax,eax
    jnz .bad
    sub edx,36
    test edx,7
    jnz .bad
    shr edx,3
    cmp edx,256
    ja .bad
    mov ebp,edx
    mov rsi,[r12+ACPI_CONTEXT.xsdt]
    add rsi,36
    xor r13d,r13d
.child:
    test ebp,ebp
    jz .fadt
    mov rdi,[rsi]
    mov rcx,r12
    mov rdx,rdi
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .bad
    cmp dword [rdi],0x50434146
    jne .next_child
    test r13,r13
    jnz .bad
    mov r13,rdi
.next_child:
    add rsi,8
    dec ebp
    jmp .child
.fadt:
    mov rcx,r12
    mov rdx,r13
    mov r8d,0x50434146
    mov r9d,244
    call keyboard_table_check
    test eax,eax
    jnz .bad
    movzx eax,byte [r13+8]
    cmp eax,3
    jb .bad
    cmp eax,4
    jbe .boot_flags
    cmp edx,268
    jb .bad
    cmp eax,5
    je .boot_flags
    cmp edx,276
    jb .bad
.boot_flags:
    movzx eax,word [r13+109]
    test eax,0xffc0
    jnz .bad
    test eax,2
    jz .bad
    ; Default ISA GSI mapping. Overrides replace one source only, never merge.
    xor eax,eax
.default_gsi:
    mov [rsp+32+rax*8],rax
    inc eax
    cmp eax,16
    jb .default_gsi
    xor r14d,r14d
    mov rcx,r12
    mov rdx,[r12+ACPI_CONTEXT.madt]
    mov r8d,0x43495041
    mov r9d,44
    call keyboard_table_check
    test eax,eax
    jnz .bad
    mov rsi,[r12+ACPI_CONTEXT.madt]
    mov r15,rsi
    add r15,rdx
    add rsi,44
.record:
    cmp rsi,r15
    je .aliases
    mov rax,r15
    sub rax,rsi
    cmp rax,2
    jb .bad
    movzx ebp,byte [rsi+1]
    cmp ebp,2
    jb .bad
    cmp rbp,rax
    ja .bad
    cmp byte [rsi],2
    jne .next_record
    cmp ebp,10
    jb .bad
    cmp byte [rsi+2],0
    jne .bad
    movzx eax,byte [rsi+3]
    cmp eax,15
    ja .bad
    bt r14,rax
    jc .bad
    bts r14,rax
    movzx edx,word [rsi+8]
    test edx,0xfff0
    jnz .bad
    mov ecx,edx
    and ecx,3
    cmp ecx,2
    je .bad
    mov ecx,edx
    shr ecx,2
    cmp ecx,2
    je .bad
    cmp eax,1
    jne .store_iso
    test edx,0xa              ; IRQ1 accepts conforming/high and conforming/edge.
    jnz .bad
.store_iso:
    mov edx,[rsi+4]
    mov [rsp+32+rax*8],rdx
.next_record:
    add rsi,rbp
    jmp .record
.aliases:
    mov r14,[rsp+40]          ; selected IRQ1 GSI
    xor eax,eax
.alias:
    cmp eax,1
    je .next_alias
    cmp r14,[rsp+32+rax*8]
    je .bad
.next_alias:
    inc eax
    cmp eax,16
    jb .alias
    mov rbp,[r12+ACPI_CONTEXT.io_count]
    test rbp,rbp
    jz .bad
    cmp rbp,8
    ja .bad
    cmp rbp,[r12+ACPI_CONTEXT.io_capacity]
    ja .bad
    cmp qword [r12+ACPI_CONTEXT.io_capacity],8
    ja .bad
    mov rsi,[r12+ACPI_CONTEXT.io_records]
    test rsi,rsi
    jz .bad
    test rsi,3
    jnz .bad
    mov qword [rsp+160],-1
    xor r15d,r15d
.io:
    mov eax,[rsi+12]
    test eax,eax
    jz .bad
    cmp eax,120
    ja .bad
    mov edx,[rsi+8]
    add rax,rdx
    mov rcx,0x100000000
    cmp rax,rcx
    ja .bad
    cmp r14,rdx
    jb .io_next
    cmp r14,rax
    jae .io_next
    cmp qword [rsp+160],-1
    jne .bad
    mov [rsp+160],r15
    mov rax,r14
    sub rax,rdx
    mov [rsp+168],rax
.io_next:
    add rsi,16
    inc r15
    cmp r15,rbp
    jb .io
    cmp qword [rsp+160],-1
    je .bad
    mov [rbx+KEYBOARD_AUTH.fadt],r13
    mov [rbx+KEYBOARD_AUTH.gsi],r14
    mov rax,[rsp+160]
    mov [rbx+KEYBOARD_AUTH.io_index],rax
    mov rax,[rsp+168]
    mov [rbx+KEYBOARD_AUTH.io_pin],rax
    mov rax,[r12+ACPI_CONTEXT.bsp_id]
    mov [rbx+KEYBOARD_AUTH.bsp_id],rax
    mov rax,KEYBOARD_AUTH_MAGIC
    mov [rbx+KEYBOARD_AUTH.ready],rax
    xor eax,eax
    jmp .return
.bad:
.bad_pointer:
    mov eax,1
.return:
    add rsp,232
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX trusted ACPI context, RDX candidate table, R8D signature, R9D min length.
; EAX0/EDX complete checked length; EAX1/EDX0 invalid. Predereference coverage.
keyboard_table_check:
    push rbx
    push r12
    push r13
    push r14
    sub rsp,40
    mov rbx,rcx
    mov r12,rdx
    mov r13d,r8d
    mov r14d,r9d
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .bad
    cmp [r12],r13d
    jne .bad
    mov r8d,[r12+4]
    cmp r8d,r14d
    jb .bad
    cmp r8d,65536
    ja .bad
    mov rcx,rbx
    mov rdx,r12
    call acpi_readable
    test eax,eax
    jnz .bad
    mov rcx,r12
    mov edx,[r12+4]
    call acpi_checksum
    test al,al
    jnz .bad
    mov edx,[r12+4]
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
    xor edx,edx
.return:
    add rsp,40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
