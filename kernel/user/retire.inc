; BSP-only after owned segment reload/LTR/LIDT and LLDT0, before user gate changes.
; Prove live authority first; no writes until the final bounded prefix helper.
user_descriptor_retire:
    push rbx
    push rsi
    push rdi
    sub rsp,64
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov ax,cs
    cmp ax,[kernel_code_selector]
    jne .bad
    mov ax,ss
    cmp ax,[kernel_data_selector]
    jne .bad
    mov ax,ds
    cmp ax,[kernel_data_selector]
    jne .bad
    mov ax,es
    cmp ax,[kernel_data_selector]
    jne .bad
    mov ax,fs
    test ax,ax
    jnz .bad
    mov ax,gs
    test ax,ax
    jnz .bad
    sldt ax
    test ax,ax
    jnz .bad
    str ax
    cmp ax,[kernel_tss_selector]
    jne .bad
    movzx r9d,word [kernel_code_selector]
    cmp r9d,8
    jb .bad
    cmp r9d,65504
    ja .bad
    test r9d,7
    jnz .bad
    lea eax,[r9+8]
    cmp ax,[kernel_data_selector]
    jne .bad
    lea eax,[r9+16]
    cmp ax,[kernel_tss_selector]
    jne .bad
    sgdt [rsp+32]
    lea rax,[kernel_gdt]
    cmp [rsp+34],rax
    jne .bad
    lea eax,[r9+31]
    cmp ax,[rsp+32]
    jne .bad
    sidt [rsp+48]
    cmp word [rsp+48],4095
    jne .bad
    lea rsi,[kernel_idt]
    cmp [rsp+50],rsi
    jne .bad
    xor ebx,ebx
.gate:
    cmp [rsi+2],r9w
    jne .bad
    cmp byte [rsi+5],0x8e
    jne .bad
    cmp dword [rsi+12],0
    jne .bad
    mov r10d,4
    cmp ebx,8
    jne .nmi
    mov r10d,1
.nmi:
    cmp ebx,2
    jne .mc
    mov r10d,2
.mc:
    cmp ebx,18
    jne .target
    mov r10d,3
.target:
    lea rdi,[exception_stubs]
    mov eax,ebx
    shl eax,4
    add rdi,rax
    cmp ebx,0x40
    jne .test_gate
    lea rdi,[apic_timer_entry]
    xor r10d,r10d
.test_gate:
    cmp ebx,0x41
    jne .launch_gate
    lea rdi,[apic_test_entry]
    xor r10d,r10d
.launch_gate:
    cmp ebx,0x42
    jne .spurious
    lea rdi,[context_launch_entry]
    xor r10d,r10d
.spurious:
    cmp ebx,0xff
    jne .compare
    lea rdi,[apic_spurious_entry]
    xor r10d,r10d
.compare:
    cmp [rsi+4],r10b
    jne .bad
    movzx eax,word [rsi]
    movzx edx,word [rsi+6]
    shl rdx,16
    or rax,rdx
    mov edx,[rsi+8]
    shl rdx,32
    or rax,rdx
    cmp rax,rdi
    jne .bad
    add rsi,16
    inc ebx
    cmp ebx,256
    jb .gate
    lea rcx,[kernel_gdt]
    movzx edx,word [kernel_code_selector]
    lea r8,[kernel_tss]
    call user_gdt_retire_prefix
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,64
    pop rdi
    pop rsi
    pop rbx
    ret
