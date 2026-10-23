; Real CPL3 exercise. One checked call body keeps all profiles within one code page.
%macro user_handle_call 2
    push qword %2
    mov eax,%1
    call user_handle_checked
    add rsp,8
    mov rax,USER_BASE+0x7000
    cmp rsp,rax
    jne user_payload_start.failed
%endmacro
%macro user_handle_emit_checker 0
user_handle_checked:
    push rcx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    std
    int 0x80
    cmp rax,[rsp+112]
    jne user_payload_start.failed
    cmp r15,[rsp+0]
    jne user_payload_start.failed
    cmp r14,[rsp+8]
    jne user_payload_start.failed
    cmp r13,[rsp+16]
    jne user_payload_start.failed
    cmp r12,[rsp+24]
    jne user_payload_start.failed
    cmp r11,[rsp+32]
    jne user_payload_start.failed
    cmp r10,[rsp+40]
    jne user_payload_start.failed
    cmp r9,[rsp+48]
    jne user_payload_start.failed
    cmp r8,[rsp+56]
    jne user_payload_start.failed
    cmp rdi,[rsp+64]
    jne user_payload_start.failed
    cmp rsi,[rsp+72]
    jne user_payload_start.failed
    cmp rbp,[rsp+80]
    jne user_payload_start.failed
    cmp rbx,[rsp+88]
    jne user_payload_start.failed
    cmp rcx,[rsp+96]
    jne user_payload_start.failed
    pushfq
    pop rax
    test eax,0x400
    jz user_payload_start.failed
    cld
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rcx
    ret
%endmacro
    mov r15,rdi
    mov rdi,[r15+48]
    xor esi,esi
    user_handle_call 1,0
    mov rax,USER_HANDLE_VALUE
    cmp rdx,rax
    jne user_payload_start.failed
    mov esi,HANDLE_READ
    user_handle_call 2,0
    mov rax,[r15+48]
    inc rax
    cmp rdx,rax
    jne user_payload_start.failed
    mov [r15+72],rdx
    mov rdi,rdx
    xor esi,esi
    user_handle_call 1,0
    mov rax,USER_HANDLE_VALUE
    cmp rdx,rax
    jne user_payload_start.failed
    mov esi,HANDLE_READ
    user_handle_call 2,-13
    test rdx,rdx
    jnz user_payload_start.failed
    mov rdi,[r15+48]
    mov esi,HANDLE_READ|HANDLE_DUP
    user_handle_call 2,0
    mov rax,[r15+48]
    add rax,2
    cmp rdx,rax
    jne user_payload_start.failed
    mov [r15+80],rdx
    xor esi,esi
    user_handle_call 3,0
    test rdx,rdx
    jnz user_payload_start.failed
    user_handle_call 1,-9
    test rdx,rdx
    jnz user_payload_start.failed
    user_handle_call 3,-9
    test rdx,rdx
    jnz user_payload_start.failed
    mov esi,1
    user_handle_call 3,-22
    test rdx,rdx
    jnz user_payload_start.failed
    mov rax,3<<48
    xor rdi,rax
    xor esi,esi
    user_handle_call 1,-9
    test rdx,rdx
    jnz user_payload_start.failed
    mov rdi,[r15+72]
    user_handle_call 3,0
    test rdx,rdx
    jnz user_payload_start.failed
    user_handle_call 1,-9
    test rdx,rdx
    jnz user_payload_start.failed
    mov rdi,[r15+80]
    user_handle_call 1,0
    mov rax,USER_HANDLE_VALUE
    cmp rdx,rax
    jne user_payload_start.failed
%if JOSHUA_USER_TEST = 26
    test qword [r15+8],1
    jnz .handle_complete
    mov qword [r15+96],31
.handle_fill:
    mov esi,HANDLE_READ|HANDLE_DUP
    user_handle_call 2,0
    mov rax,[r15+48]
    mov ecx,31
    sub rcx,[r15+96]
    cmp ecx,2
    jae .handle_new_slot
    add rax,0x100
    add rax,rcx
    jmp .handle_expected
.handle_new_slot:
    inc rcx
    add rax,rcx
.handle_expected:
    cmp rdx,rax
    jne user_payload_start.failed
    dec qword [r15+96]
    jnz .handle_fill
    user_handle_call 2,-24
    test rdx,rdx
    jnz user_payload_start.failed
.handle_complete:
%endif
    mov rax,USER_HANDLE_PROGRESS
    mov [r15+88],rax
    mov rdi,r15
%unmacro user_handle_call 2
