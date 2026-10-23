; Position-independent embedded Assembly, copied into distinct sealed code pages.
; Only initial nonzero GPR is RDI=public user-data VA. No kernel/physical pointers.
user_payload_start:
%macro user_zero 1
    test %1,%1
    jnz user_payload_start.failed
%endmacro
    user_zero rax
    user_zero rcx
    user_zero rdx
    user_zero rbx
    user_zero rbp
    user_zero rsi
    user_zero r8
    user_zero r9
    user_zero r10
    user_zero r11
    user_zero r12
    user_zero r13
    user_zero r14
    user_zero r15
    mov rax,USER_BASE+0x7000
    cmp rsp,rax
    jne user_payload_start.failed
    sub rax,0x6000
    cmp rdi,rax
    jne user_payload_start.failed
    mov ecx,0x31410001
    mov edx,0x31410002
    mov ebx,0x31410003
    mov ebp,0x31410004
    mov esi,0x31410005
    mov r8d,0x31410006
    mov r9d,0x31410007
    mov r10d,0x31410008
    mov r11d,0x31410009
    mov r12d,0x3141000a
    mov r13d,0x3141000b
    mov r14d,0x3141000c
    mov r15d,0x3141000d
    mov rax,-1
    std
    int 0x80
    cmp rax,-38
    jne user_payload_start.failed
%macro user_preserved 2
    cmp %1,%2
    jne user_payload_start.failed
%endmacro
    user_preserved rcx,0x31410001
    user_preserved rdx,0x31410002
    user_preserved rbx,0x31410003
    user_preserved rbp,0x31410004
    user_preserved rsi,0x31410005
    user_preserved r8,0x31410006
    user_preserved r9,0x31410007
    user_preserved r10,0x31410008
    user_preserved r11,0x31410009
    user_preserved r12,0x3141000a
    user_preserved r13,0x3141000b
    user_preserved r14,0x3141000c
    user_preserved r15,0x3141000d
    mov rax,USER_BASE+0x1000
    cmp rdi,rax
    jne user_payload_start.failed
    add rax,0x6000
    cmp rsp,rax
    jne user_payload_start.failed
    pushfq
    pop rax
    test eax,0x400
    jz user_payload_start.failed
    cld
    inc qword [rdi+16]
%if JOSHUA_USER_TEST != 0
    test qword [rdi+8],1        ; processB always executes the independent success path
    jnz user_payload_start.exit
%if JOSHUA_USER_TEST = 18 || JOSHUA_USER_TEST = 20 || JOSHUA_USER_TEST = 21
    jmp user_payload_start.exit
%endif
%if JOSHUA_USER_TEST = 1 || JOSHUA_USER_TEST = 2
    mov rax,[rdi+24]            ; deliberately public fixture probe, not a syscall ptr
%elif JOSHUA_USER_TEST = 3
    mov rax,USER_BASE
%elif JOSHUA_USER_TEST = 4
    mov rax,USER_BASE+0x1000
%elif JOSHUA_USER_TEST = 5
    mov rax,USER_BASE+0x2000
%elif JOSHUA_USER_TEST = 12
    mov rsp,0x0000800000001000
%elif JOSHUA_USER_TEST = 13
    pushfq
    or qword [rsp],0x40000
    popfq
%elif JOSHUA_USER_TEST = 14
    mov ax,[rdi+32]
%elif JOSHUA_USER_TEST = 15
    push qword [rdi+40]
    mov rax,USER_BASE
    push rax
%elif JOSHUA_USER_TEST = 24
    mov rax,USER_BASE
    mov [rdi+56],rax
    mov word [rdi+64],7
%elif JOSHUA_USER_TEST = 16
    mov rsp,USER_BASE+0x2000
%endif
user_payload_fixture:
%if JOSHUA_USER_TEST = 1 || JOSHUA_USER_TEST = 5
    mov rax,[rax]
%elif JOSHUA_USER_TEST = 2 || JOSHUA_USER_TEST = 3
    mov byte [rax],0
%elif JOSHUA_USER_TEST = 4
    call rax
%elif JOSHUA_USER_TEST = 6
    out 0x80,al
%elif JOSHUA_USER_TEST = 7
    cli
%elif JOSHUA_USER_TEST = 8
    pxor xmm0,xmm0
%elif JOSHUA_USER_TEST = 9
    jmp user_payload_fixture
%elif JOSHUA_USER_TEST = 10
    syscall
%elif JOSHUA_USER_TEST = 11
    sysenter
%elif JOSHUA_USER_TEST = 12
    push rax
%elif JOSHUA_USER_TEST = 13
    mov eax,[rdi+1]
%elif JOSHUA_USER_TEST = 14
    mov ds,ax
%elif JOSHUA_USER_TEST = 15
    retfq
%elif JOSHUA_USER_TEST = 16
    int 0x80
%elif JOSHUA_USER_TEST = 24
    call far [rdi+56]
%elif JOSHUA_USER_TEST = 17 || JOSHUA_USER_TEST = 19
    ud2
%endif
    jmp user_payload_start.failed
%endif
user_payload_start.exit:
    xor eax,eax
    xor edi,edi
    int 0x80
    ud2
user_payload_start.failed:
    xor eax,eax
    mov edi,1
    int 0x80
    ud2
user_payload_end:
%if user_payload_end-user_payload_start > 4096
 %error User payload exceeds one private code page
%endif
