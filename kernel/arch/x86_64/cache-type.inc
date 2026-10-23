; Read-only effective-UC proof for one aligned controller page >=1MiB.
; RCX trusted CACHE_CONTEXT, RDX physical page, R8 inherited PAT index (0..7).
; EAX=0 strict UC, 1 invalid/undefined/non-UC. No register writes/MSR access here.
cache_controller_uc:
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
    mov rax,CACHE_READY_MAGIC
    cmp [r12+CACHE_CONTEXT.ready],rax
    jne .bad
    cmp rdx,0x100000
    jb .bad
    test edx,4095
    jnz .bad
    cmp r8,7
    ja .bad
    mov ecx,r8d
    shl ecx,3
    mov rax,[r12+CACHE_CONTEXT.pat]
    shr rax,cl
    movzx eax,al
    ; PAT WC can override MTRR UC. It is never accepted for controller aliases.
    cmp eax,1
    je .bad
    cmp eax,0
    je .pat_ok
    cmp eax,4
    je .pat_ok
    cmp eax,5
    je .pat_ok
    cmp eax,6
    je .pat_ok
    cmp eax,7
    jne .bad
.pat_ok:
    mov [rsp+32],rax
    mov rcx,[r12+CACHE_CONTEXT.physical_bits]
    cmp rcx,32
    jb .bad
    cmp rcx,52
    ja .bad
    mov r15,1
    shl r15,cl
    mov rax,r13
    add rax,4096
    jc .bad
    cmp rax,r15
    ja .bad
    dec r15
    mov r14,[r12+CACHE_CONTEXT.raw]
    test r14,r14
    jz .bad
    mov rax,[r14+8]
    mov rdx,0xfffffffffffff300
    test rax,rdx
    jnz .bad
    test eax,0x800
    jz .uc
    movzx eax,al
    call .valid_mtrr_type
    test eax,eax
    jnz .bad
    movzx esi,byte [r14]
    lea rdi,[r14+16]
    xor ebp,ebp
.pair:
    test rsi,rsi
    jz .resolve
    mov r8,[rdi+8]
    test r8d,0x800
    jz .next
    ; Reject bits outside physical width and mask reserved bits 0..10.
    mov rax,r15
    and rax,-4096
    or rax,0x800
    not rax
    test r8,rax
    jnz .bad
    and r8,-4096
    mov r9,r15
    xor r9,r8
    inc r9
    lea rax,[r9-1]
    test rax,r9
    jnz .bad
    cmp r9,4096
    jb .bad
    mov r10,[rdi]
    mov rax,r15
    and rax,-4096
    or rax,0xff
    not rax
    test r10,rax
    jnz .bad
    mov r11,r10
    and r11,-4096
    lea rax,[r9-1]
    test r11,rax
    jnz .bad
    movzx eax,r10b
    call .valid_mtrr_type
    test eax,eax
    jnz .bad
    mov rax,r13
    and rax,r8
    cmp rax,r11
    jne .next
    movzx ecx,r10b
    bts rbp,rcx
.next:
    add rdi,16
    dec rsi
    jmp .pair
.resolve:
    test rbp,rbp
    jz .default
    test bpl,1
    jnz .uc
    lea rax,[rbp-1]
    test rax,rbp
    jz .defined
    cmp ebp,0x50
    jne .bad
    ; Only the WT+WB mixture is defined without UC. Its resolved type is WT.
    jmp .defined
.default:
    cmp byte [r14+8],0
    je .uc
.defined:
    cmp qword [rsp+32],0
    jne .bad
.uc:
    xor eax,eax
    jmp .return
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
; EAX raw MTRR type -> EAX=0 supported, 1 invalid. R14 raw snapshot retained.
.valid_mtrr_type:
    test eax,eax
    jz .valid
    cmp eax,1
    jne .not_wc
    test qword [r14],0x400
    jz .invalid
    jmp .valid
.not_wc:
    cmp eax,4
    je .valid
    cmp eax,5
    je .valid
    cmp eax,6
    jne .invalid
.valid:
    xor eax,eax
    ret
.invalid:
    mov eax,1
    ret
