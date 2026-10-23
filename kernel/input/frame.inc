; RCX trusted window, RDX192-byte IRQ frame, R8 currentCR3 -> EAX0/1.
; Read-only. Exact bounds/address proof precedes all proposed frame dereferences.
keyboard_frame_check:
    mov rax,KEYBOARD_WINDOW_MAGIC
    cmp [rcx+KEYBOARD_WINDOW.ready],rax
    jne .bad
    cmp qword [rcx+KEYBOARD_WINDOW.phase],1
    je .phase
    cmp qword [rcx+KEYBOARD_WINDOW.phase],2
    jne .bad
.phase:
    mov rax,[rcx+KEYBOARD_WINDOW.root]
    test rax,rax
    jz .bad
    test rax,4095
    jnz .bad
    cmp r8,rax
    jne .bad
    mov r9,[rcx+KEYBOARD_WINDOW.stack_low]
    test r9,r9
    jz .bad
    test r9,4095
    jnz .bad
    mov r10,[rcx+KEYBOARD_WINDOW.stack_high]
    test r10,4095
    jnz .bad
    cmp r10,r9
    jbe .bad
    mov rax,r10
    sub rax,r9
    cmp rax,8192
    jb .bad
    cmp rax,65536
    ja .bad
    mov r11,[rcx+KEYBOARD_WINDOW.wait_rsp]
    cmp r11,r10
    ja .bad
    and r11,-16
    sub r11,192
    jc .bad
    cmp rdx,r11
    jne .bad
    add r9,4096
    jc .bad
    cmp r11,r9
    jb .bad
    mov rax,r11
    add rax,192
    jc .bad
    cmp rax,r10
    ja .bad
    mov rax,[rcx+KEYBOARD_WINDOW.text_start]
    cmp rax,[rcx+KEYBOARD_WINDOW.text_end]
    jae .bad
    mov r9,[rcx+KEYBOARD_WINDOW.wait_rip]
    cmp qword [rcx+KEYBOARD_WINDOW.phase],1
    je .rip
    mov r9,[rcx+KEYBOARD_WINDOW.drain_rip]
.rip:
    cmp r9,rax
    jb .bad
    cmp r9,[rcx+KEYBOARD_WINDOW.text_end]
    jae .bad
    cmp r9,[rdx+152]
    jne .bad
    mov rax,[rcx+KEYBOARD_WINDOW.wait_rsp]
    cmp [rdx+176],rax
    jne .bad
    mov rax,[rcx+KEYBOARD_WINDOW.code_selector]
    test rax,rax
    jz .bad
    test eax,7
    jnz .bad
    cmp rax,65535
    ja .bad
    cmp [rdx+160],rax
    jne .bad
    mov r9,[rcx+KEYBOARD_WINDOW.data_selector]
    cmp r9,rax
    je .bad
    test r9,r9
    jz .bad
    test r9d,7
    jnz .bad
    cmp r9,65535
    ja .bad
    cmp [rdx+184],r9
    jne .bad
    mov eax,0x202
    cmp qword [rcx+KEYBOARD_WINDOW.phase],2
    je .flags
    mov rax,[rcx+KEYBOARD_WINDOW.flags]
    cmp rax,0x202
    je .flags
    cmp rax,0x602
    jne .bad
.flags:
    cmp [rdx+168],rax
    jne .bad
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret
