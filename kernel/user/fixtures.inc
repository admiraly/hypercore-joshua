%if JOSHUA_USER_TEST = 24
; Controlled prelaunch authority fixture: install an owned LDT containing a DPL3
; call gate to owned kernel text. No CPL3 runs until machine_prepare clears LDTR.
user_fixture_ldt:
    sub rsp,40
    movzx eax,word [kernel_code_selector]
    cmp eax,24
    jb kernel_fault
    lea r8,[user_test_ldt]
    lea rax,[user_smoke]
    mov [r8],ax
    mov dx,[kernel_code_selector]
    mov [r8+2],dx
    mov word [r8+4],0xec00
    shr rax,16
    mov [r8+6],ax
    shr rax,16
    mov [r8+8],eax
    mov dword [r8+12],0
    mov rax,r8
    and eax,0xffffff
    shl rax,16
    or rax,15
    mov rdx,r8
    shr rdx,24
    and edx,255
    shl rdx,56
    or rax,rdx
    mov rdx,0x0000820000000000
    or rax,rdx
    mov [kernel_gdt+8],rax
    mov rax,r8
    shr rax,32
    mov [kernel_gdt+16],rax
    mov ax,8
    lldt ax
    sldt ax
    cmp ax,8
    jne kernel_fault
    lea rcx,[debug_user_ldt]
    call debug_write
    add rsp,40
    ret
%endif
