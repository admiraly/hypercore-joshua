; Fixed trusted USER_CONTEXT/builder/process metadata, already proved at boot.
; RCX context, R8 current CR3 -> EAX0/RDX active record, EAX2 inconsistent owner.
; This leaf never reads a user frame or user VA. No writes/privileged instructions.
user_event_owner:
    mov rax,USER_READY_MAGIC
    cmp [rcx+USER_CONTEXT.ready],rax
    jne .bad
    cmp qword [rcx+USER_CONTEXT.failed],0
    jne .bad
    cmp qword [rcx+USER_CONTEXT.stage],3
    jne .bad
    cmp qword [rcx+USER_CONTEXT.phase],1
    jne .bad
    mov r11,[rcx+USER_CONTEXT.builder]
    test r11,r11
    jz .bad
    cmp qword [r11+OWNED_PAGING.ready],1
    jne .bad
    cmp qword [r11+OWNED_PAGING.failed],0
    jne .bad
    mov rax,[r11+OWNED_PAGING.root]
    cmp rax,[rcx+USER_CONTEXT.kernel_root]
    jne .bad
    mov r10,[rcx+USER_CONTEXT.active]
    dec r10
    cmp r10,USER_PROCESS_COUNT
    jae .bad
    mov rdx,[rcx+USER_CONTEXT.processes]
    test rdx,rdx
    jz .bad
    imul rax,r10,USER_PROCESS_size
    add rdx,rax
    cmp qword [rdx+USER_PROCESS.state],1
    jne .bad
    lea eax,[r10+FRAME_OWNER_USER_BASE+1]
    cmp [rdx+USER_PROCESS.owner],rax
    jne .bad
    cmp r8,[rdx+USER_PROCESS.root]
    jne .bad
    test r8,r8
    jz .bad
    test r8,4095
    jnz .bad
    xor eax,eax
    ret
.bad:
    xor edx,edx
    mov eax,2
    ret

; RCX context, RDX192-byte IRQ frame on the active process's RSP0 stack, R8 CR3.
; EAX0 resumable,1 invalid user return state (terminate),2 invariant/kernel origin.
; The frame is on a kernel entry stack, NOT at align_down(user RSP)-192.
; Context, records and declared stack allocation are trusted/proved caller storage.
user_frame_check:
    push rbx
    push r12
    push r13
    push r14
    sub rsp,40
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    xor r14d,r14d
    call user_event_owner
    test eax,eax
    jnz .fatal
    mov r14,rdx
    mov rax,[r14+USER_PROCESS.kernel_low]
    test rax,rax
    jz .fatal
    test eax,4095
    jnz .fatal
    mov rdx,[r14+USER_PROCESS.kernel_high]
    test edx,4095
    jnz .fatal
    mov r9,0x0000800000000000
    cmp rdx,r9
    ja .fatal
    cmp rdx,rax
    jbe .fatal
    sub rdx,rax
    cmp rdx,16384
    jne .fatal
    mov rdx,[r14+USER_PROCESS.kernel_high]
    sub rdx,192
    cmp r12,rdx
    jne .fatal
    ; Exact RSP0 frame location, entire readable192-byte range now proved.
    mov rax,[r12+160]
    cmp rax,65535
    ja .fatal
    and eax,3
    cmp eax,3
    jne .fatal
    mov rax,[rbx+USER_CONTEXT.code_selector]
    cmp rax,65535
    ja .fatal
    mov edx,eax
    and edx,7
    cmp edx,3
    jne .fatal
    cmp rax,[r12+160]
    jne .user_bad
    mov rax,[rbx+USER_CONTEXT.data_selector]
    cmp rax,65535
    ja .fatal
    mov edx,eax
    and edx,7
    cmp edx,3
    jne .fatal
    cmp rax,[rbx+USER_CONTEXT.code_selector]
    je .fatal
    cmp rax,[r12+184]
    jne .user_bad
    mov rax,[rbx+USER_CONTEXT.payload_size]
    test rax,rax
    jz .fatal
    cmp rax,4096
    ja .fatal
    mov rdx,USER_BASE
    add rax,rdx
    cmp [r12+152],rdx
    jb .user_bad
    cmp [r12+152],rax
    jae .user_bad
    mov rax,[r12+168]
    test rax,-0x10ed8          ; arithmetic,DF,IF,RF and fixed bit1 only
    jnz .user_bad
    test eax,2
    jz .user_bad
    test eax,0x200
    jz .user_bad
    mov rdx,USER_BASE+0x3000
    cmp [r12+176],rdx
    jb .user_bad
    mov rdx,USER_BASE+0x7000
    cmp [r12+176],rdx
    ja .user_bad
    xor eax,eax
    jmp .return
.user_bad:
    mov eax,1
    jmp .return
.fatal:
    mov eax,2
.return:
    mov rdx,r14
    add rsp,40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; RCX context, RDX normalized56-byte exception frame, R8 current CR3.
; EAX0 contained user synchronous fault;2 retain permanent kernel stop.
; All discarded user RIP/RSP/SS/flags values are deliberately NOT dereferenced
; or required to be resumable. Only hardware CS.RPL3 and ownership prove origin.
user_fault_check:
    push rbx
    push r12
    push r13
    push r14
    sub rsp,40
    mov rbx,rcx
    mov r12,rdx
    mov r13,r8
    xor r14d,r14d
    call user_event_owner
    test eax,eax
    jnz .bad
    mov r14,rdx
    mov rax,[rbx+USER_CONTEXT.fault_top]
    test rax,4095
    jnz .bad
    cmp rax,16384
    jbe .bad
    mov rdx,0x0000800000000000
    cmp rax,rdx
    ja .bad
    sub rax,56
    cmp rax,r12
    jne .bad
    mov rax,[r12+24]
    cmp rax,65535
    ja .bad
    and eax,3
    cmp eax,3
    jne .bad
    mov rcx,[r12]
    cmp rcx,31
    ja .bad
    ; DE,DB,BP,OF,BR,UD,NM,TS,NP,SS,GP,PF,MF,AC,XF.
    mov eax,0x000b7cfb
    bt eax,ecx
    jnc .bad
    xor eax,eax
    jmp .return
.bad:
    mov eax,2
.return:
    mov rdx,r14
    add rsp,40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
