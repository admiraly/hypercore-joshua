; IF-clear, trusted/proved USER_CONTEXT only. One-time preflight before any CPL3.
; RCX context -> EAX0 complete/1 fatal; failures are stop-only, never rolled back.
; No firmware calls, no inference/FPU, all integer nonvolatile registers preserved.
user_machine_prepare:
    push rbx
    push rsi
    push rdi
    sub rsp,32
    mov rdi,rcx
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov rax,USER_READY_MAGIC
    cmp [rdi+USER_CONTEXT.ready],rax
    jne .bad
    cmp qword [rdi+USER_CONTEXT.failed],0
    jne .bad
    cmp qword [rdi+USER_CONTEXT.stage],3
    jne .bad
    cmp qword [rdi+USER_CONTEXT.active],0
    jne .bad
    cmp qword [rdi+USER_CONTEXT.phase],0
    jne .bad
    mov rax,cr3
    cmp rax,[rdi+USER_CONTEXT.kernel_root]
    jne .bad
    mov rax,cr4
    test eax,0x00c01000          ; CET, PKE, LA57 require unsupported context state
    jnz .bad
    mov rsi,cr0
    ; No inherited fast system-call target may remain callable by ring3.
    mov ecx,0xc0000080          ; IA32_EFER; preserve LME/LMA/NXE and other bits
    rdmsr
    shl rdx,32
    or rax,rdx
    and rax,-2                 ; clear SCE only
    mov rbx,rax
    mov rdx,rax
    shr rdx,32
    wrmsr
    rdmsr
    shl rdx,32
    or rax,rdx
    cmp rax,rbx
    jne .bad
    mov eax,1
    cpuid
    bt edx,11                  ; SEP: only access SYSENTER MSRs when advertised
    jnc .segments
    mov ecx,0x174              ; IA32_SYSENTER_CS=0 rejects Intel SYSENTER entry
    xor eax,eax
    xor edx,edx
    wrmsr
    rdmsr
    or eax,edx
    jnz .bad
    mov ecx,0x175
    xor eax,eax
    xor edx,edx
    wrmsr
    rdmsr
    or eax,edx
    jnz .bad
    mov ecx,0x176
    xor eax,eax
    xor edx,edx
    wrmsr
    rdmsr
    or eax,edx
    jnz .bad
.segments:
    xor eax,eax
    lldt ax
    sldt ax
    test ax,ax
    jnz .bad
    mov ecx,0xc0000100          ; no TLS ABI: FS_BASE and GS_BASE are both zero
    xor eax,eax
    xor edx,edx
    wrmsr
    rdmsr
    or eax,edx
    jnz .bad
    mov ecx,0xc0000101
    xor eax,eax
    xor edx,edx
    wrmsr
    rdmsr
    or eax,edx
    jnz .bad
    mov [rdi+USER_CONTEXT.saved_cr0],rsi
    mov rax,rsi
    or rax,8                   ; TS: unsupported x87/SIMD triggers contained #NM
    mov cr0,rax
    mov rdx,cr0
    cmp rax,rdx
    jne .bad
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rdi
    pop rsi
    pop rbx
    ret
