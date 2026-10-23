; RCX trusted readable GDT, RDX byte length -> EAX0 accepted/1 unsupported.
; Original copied descriptors only: never grants user authority. No writes/calls.
; Caller owns the whole declared source; pointer arithmetic does not grant access.
user_gdt_audit:
    test rcx,rcx
    jz .bad
    test rcx,7
    jnz .bad
    test rdx,rdx
    jz .bad
    test rdx,7
    jnz .bad
    cmp rdx,65536
    ja .bad
    mov r10,rcx
    mov r11,rcx
    add r11,rdx
    jc .bad
    cmp qword [r10],0            ; selector0 must be an explicit null descriptor
    jne .bad
    add r10,8
.next:
    cmp r10,r11
    jae .good
    cmp qword [r10],0
    je .ordinary_next
    movzx eax,byte [r10+5]
    test eax,0x10
    jz .system
    ; Ordinary segments cannot escalate CPL, but conforming and DPL3 code
    ; would create alternative user CS values. Reject their present authority.
    test eax,0x80
    jz .ordinary_next
    test eax,8
    jz .ordinary_next
    test eax,4
    jnz .bad
    and eax,0x60
    cmp eax,0x60
    je .bad
.ordinary_next:
    add r10,8
    jmp .next
.system:
    ; IA32e LDT/available-TSS/busy-TSS are16-byte descriptors. No call/task
    ; gates or unknown system formats are accepted, including non-present gates.
    mov r8d,eax
    and eax,15
    cmp eax,2
    je .wide
    cmp eax,9
    je .wide
    cmp eax,11
    jne .bad
.wide:
    mov r9,r11
    sub r9,r10
    cmp r9,16
    jb .bad
    lea r9,[r10+16]
    test r8d,0x80
    jz .reserved
    and r8d,0x60
    cmp r8d,0x60
    je .bad
.reserved:
    cmp dword [r10+12],0
    jne .bad
    mov r10,r9
    jmp .next
.good:
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; RCX caller-owned64-KiB GDT, RDX current kernel GDT bytes (before user append).
; EAX0: RDX/R8/R9 user code/data/non-present-data selectors (RPL3), R10 limit.
; EAX1: whole GDT unchanged. Extra non-present data slot grants no authority and
; supports the controlled #NP fixture. Kernel descriptors/selectors stay intact.
user_gdt_prepare:
    push rbx
    push rsi
    sub rsp,40
    mov rbx,rcx
    mov rsi,rdx
    cmp rdx,40                  ; null+owned ring0 code/data/TSS minimum
    jb .bad
    cmp rdx,65512               ; reserve24 bytes in the owned64-KiB allocation
    ja .bad
    call user_gdt_audit
    test eax,eax
    jnz .bad
    mov rax,0x00affb000000ffff
    mov [rbx+rsi],rax
    mov rax,0x00cff3000000ffff
    mov [rbx+rsi+8],rax
    mov rax,0x00cf73000000ffff
    mov [rbx+rsi+16],rax
    mov rdx,rsi
    or edx,3
    lea r8,[rsi+8]
    or r8d,3
    lea r9,[rsi+16]
    or r9d,3
    lea r10,[rsi+23]
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,40
    pop rsi
    pop rbx
    ret

; RCX proved64-KiB owned GDT, RDX copied-prefix bytes, R8 owned TSS base.
; Caller first proves hardware active selectors/IDT use the unchanged owned suffix.
; EAX0 zeroes only the prefix; EAX1 leaves every byte unchanged. No calls/privilege.
user_gdt_retire_prefix:
    test rcx,rcx
    jz .bad
    test rcx,7
    jnz .bad
    cmp rdx,8
    jb .bad
    cmp rdx,65504
    ja .bad
    test rdx,7
    jnz .bad
    mov r10,rcx
    add r10,rdx
    jc .bad
    mov r11,r10
    add r11,32
    jc .bad
    mov rax,0x00af9b000000ffff
    cmp [r10],rax
    jne .bad
    mov rax,0x00cf93000000ffff
    cmp [r10+8],rax
    jne .bad
    ; Exact busy64-bit TSS descriptor: limit103, baseR8, flags/reserved0.
    mov rax,r8
    and eax,0xffffff
    shl rax,16
    or rax,0x67
    mov r11,r8
    shr r11,24
    and r11d,255
    shl r11,56
    or rax,r11
    mov r11,0x00008b0000000000
    or rax,r11
    cmp [r10+16],rax
    jne .bad
    mov rax,r8
    shr rax,32
    cmp [r10+24],rax
    jne .bad
    xor eax,eax
.clear:
    mov [rcx],rax
    add rcx,8
    sub rdx,8
    jnz .clear
    ret
.bad:
    mov eax,1
    ret
