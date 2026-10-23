; RCX owned 64-KiB GDT, RDX source, R8 source bytes, R9 owned TSS address.
; Validate BEFORE copying. EAX=0; RDX/R8/R9 code/data/TSS selectors, R10 limit.
; EAX=1 leaves destination untouched. No privileged instructions or calls.
descriptor_gdt_build:
    test r8,r8
    jz .bad
    cmp r8,65504
    ja .bad
    test r8,7
    jnz .bad
    push rdi
    push rsi
    mov rdi,rcx
    mov rsi,rdx
    mov rcx,r8
    rep movsb
    mov rax,0x00af9b000000ffff
    stosq
    mov rax,0x00cf93000000ffff
    stosq
    mov word [rdi],103
    mov [rdi+2],r9w
    mov rax,r9
    shr rax,16
    mov [rdi+4],al
    mov byte [rdi+5],0x89
    mov byte [rdi+6],0
    mov [rdi+7],ah
    shr rax,16
    mov [rdi+8],eax
    mov dword [rdi+12],0
    mov rdx,r8
    lea r9,[r8+16]
    lea r10,[r8+31]
    add r8,8
    xor eax,eax
    pop rsi
    pop rdi
    ret
.bad:
    mov eax,1
    ret

; RCX 104-byte TSS, RDX RSP0, R8 array of four IST tops. Distinct owned storage.
descriptor_tss_init:
    push rdi
    mov r9,rcx
    mov rdi,rcx
    mov ecx,13
    xor eax,eax
    rep stosq
    mov [r9+4],rdx
    mov rax,[r8]
    mov [r9+36],rax
    mov rax,[r8+8]
    mov [r9+44],rax
    mov rax,[r8+16]
    mov [r9+52],rax
    mov rax,[r8+24]
    mov [r9+60],rax
    mov word [r9+102],104
    pop rdi
    ret

; RCX owned IDT, RDX 256 fixed 16-byte stubs, R8W owned code selector.
; DPL0 interrupt gates: DF=IST1, NMI=IST2, MC=IST3, all others IST4.
descriptor_idt_build:
    push rdi
    mov rdi,rcx
    mov rax,rdx
    xor r10d,r10d
.gate:
    mov [rdi],ax
    mov [rdi+2],r8w
    mov byte [rdi+4],4
    cmp r10d,8
    jne .nmi
    mov byte [rdi+4],1
.nmi:
    cmp r10d,2
    jne .mc
    mov byte [rdi+4],2
.mc:
    cmp r10d,18
    jne .offset
    mov byte [rdi+4],3
.offset:
    mov byte [rdi+5],0x8e
    mov rdx,rax
    shr rdx,16
    mov [rdi+6],dx
    shr rdx,16
    mov [rdi+8],edx
    mov dword [rdi+12],0
    add rdi,16
    add rax,16
    inc r10d
    cmp r10d,256
    jb .gate
    pop rdi
    ret

; RCX five guard starts, RDX cursor, R8 end -> RAX next boundary, R9D absent.
; A guard covers [start,start+4096); skip exactly to its end when inside.
stack_guard_boundary:
    mov rax,r8
    xor r9d,r9d
    mov r10d,5
.next:
    mov r11,[rcx]
    cmp rdx,r11
    jb .before
    add r11,4096
    cmp rdx,r11
    jae .advance
    mov rax,r11
    mov r9d,1
    ret
.before:
    cmp rax,r11
    cmova rax,r11
.advance:
    add rcx,8
    dec r10d
    jnz .next
    ret
