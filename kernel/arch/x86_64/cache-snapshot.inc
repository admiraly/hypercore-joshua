; RCX initialized context with owned 4096-byte raw storage. EAX=0 ready, 1 stop.
; Raw page: CAP, DEF_TYPE, up to255 base/mask pairs. Metadata/PAT live outside it.
cache_snapshot:
    push rbx
    push rsi
    push r12
    sub rsp,32
    mov r12,rcx
    mov qword [r12+CACHE_CONTEXT.ready],0
    mov rax,cr0
    test eax,0x60000000
    jnz .bad
    mov eax,1
    cpuid
    and edx,0x11000
    cmp edx,0x11000
    jne .bad
    mov eax,0x80000000
    cpuid
    cmp eax,0x80000008
    jb .bad
    mov eax,0x80000008
    cpuid
    movzx eax,al
    cmp eax,32
    jb .bad
    cmp eax,52
    ja .bad
    mov [r12+CACHE_CONTEXT.physical_bits],rax
    mov ecx,0x277
    rdmsr
    shl rdx,32
    or rax,rdx
    mov [r12+CACHE_CONTEXT.pat],rax
    mov rbx,[r12+CACHE_CONTEXT.raw]
    test rbx,rbx
    jz .bad
    mov ecx,0xfe
    rdmsr
    shl rdx,32
    or rax,rdx
    mov [rbx],rax
    movzx esi,al
    mov ecx,0x2ff
    rdmsr
    shl rdx,32
    or rax,rdx
    mov [rbx+8],rax
    add rbx,16
    mov ecx,0x200
.pair:
    test esi,esi
    jz .ready
    rdmsr
    shl rdx,32
    or rax,rdx
    mov [rbx],rax
    inc ecx
    rdmsr
    shl rdx,32
    or rax,rdx
    mov [rbx+8],rax
    inc ecx
    add rbx,16
    dec esi
    jmp .pair
.ready:
    mov rax,CACHE_READY_MAGIC
    mov [r12+CACHE_CONTEXT.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop r12
    pop rsi
    pop rbx
    ret
