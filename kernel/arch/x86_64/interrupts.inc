; Returning IRQ protocol: hardware frame includes old SS:RSP/RFLAGS; no SIMD live.
%macro irq_save 0
    push rax
    push rcx
    push rdx
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
    cld
    sub rsp,32
%endmacro
%macro irq_restore 0
    add rsp,32
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
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

apic_timer_entry:
    irq_save
%if JOSHUA_KEYBOARD_TEST != 0 || JOSHUA_CONSOLE
    cmp qword [keyboard_window+KEYBOARD_WINDOW.phase],0
    jne keyboard_timer_saved
%endif
    cmp qword [user_context+USER_CONTEXT.phase],0
    jne user_timer_saved
    cmp qword [kernel_context+KERNEL_CONTEXT.state],1
    je context_timer_saved
    mov rax,APIC_READY_MAGIC
    cmp [apic_context+APIC_CONTEXT.ready],rax
    jne kernel_fault
    inc qword [apic_context+APIC_CONTEXT.ticks]
    cmp qword [apic_context+APIC_CONTEXT.ticks],16
    jb .eoi
    mov ecx,0x320
    mov edx,0x10040
    call lapic_write
    mov ecx,0x380
    xor edx,edx
    call lapic_write
.eoi:
%if JOSHUA_TIMER_TEST != 2
    mov ecx,0xb0
    xor edx,edx
    call lapic_write
    inc qword [apic_context+APIC_CONTEXT.eois]
%endif
    cmp qword [apic_context+APIC_CONTEXT.ticks],16
    jae .return
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
.return:
    inc qword [apic_context+APIC_CONTEXT.returns]
    irq_restore
apic_test_entry:
    irq_save
    inc qword [apic_context+APIC_CONTEXT.selftest]
    irq_restore
apic_spurious_entry:
    ; A genuine spurious vector has no ISR ownership and must not receive EOI.
    iretq

; IF-clear caller only. Existing selector/type stay DPL0, just replace targets/IST.
apic_install_gates:
    sub rsp,40
    lea rax,[apic_timer_entry]
    lea rdx,[kernel_idt+0x40*16]
    call .gate
    lea rax,[apic_test_entry]
    lea rdx,[kernel_idt+0x41*16]
    call .gate
    lea rax,[apic_spurious_entry]
    lea rdx,[kernel_idt+0xff*16]
    call .gate
    add rsp,40
    ret
.gate:
    mov [rdx],ax
    mov byte [rdx+4],0
    shr rax,16
    mov [rdx+6],ax
    shr rax,16
    mov [rdx+8],eax
    ret

%macro irq_sentinels 1-2 irq_sentinel_values
%assign irq_offset 0
%rep 15
%if irq_offset = 0
 %define irq_reg rax
%elif irq_offset = 8
 %define irq_reg rcx
%elif irq_offset = 16
 %define irq_reg rdx
%elif irq_offset = 24
 %define irq_reg rbx
%elif irq_offset = 32
 %define irq_reg rbp
%elif irq_offset = 40
 %define irq_reg rsi
%elif irq_offset = 48
 %define irq_reg rdi
%elif irq_offset = 56
 %define irq_reg r8
%elif irq_offset = 64
 %define irq_reg r9
%elif irq_offset = 72
 %define irq_reg r10
%elif irq_offset = 80
 %define irq_reg r11
%elif irq_offset = 88
 %define irq_reg r12
%elif irq_offset = 96
 %define irq_reg r13
%elif irq_offset = 104
 %define irq_reg r14
%else
 %define irq_reg r15
%endif
%ifidni %1,load
    mov irq_reg,[%2+irq_offset]
%else
    cmp irq_reg,[%2+irq_offset]
    jne .bad
%endif
%assign irq_offset irq_offset+8
%endrep
%endmacro

; Full GPR/RSP/arithmetic+DF return test followed by sixteen real timer returns.
; EAX=0 complete with IF0, 1 stop. Only STI window is the HLT loop below.
apic_timer_smoke:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    call apic_install_gates
    mov [apic_context+APIC_CONTEXT.wait_rsp],rsp
    irq_sentinels load
    std
    stc
    pushfq
    pop qword [rsp+32]
    int 0x41
    pushfq
    pop qword [rsp+40]
    cld
    irq_sentinels check
    cmp rsp,[apic_context+APIC_CONTEXT.wait_rsp]
    jne .bad
    mov rax,[rsp+32]
    xor rax,[rsp+40]
    test eax,0xcd5
    jnz .bad
    cmp qword [apic_context+APIC_CONTEXT.selftest],1
    jne .bad
    mov ecx,0x320
    mov edx,0x40
    call lapic_write
    mov ecx,0x380
    mov edx,0x100000
    call lapic_write
    mov [apic_context+APIC_CONTEXT.wait_rsp],rsp
    irq_sentinels load
.wait:
    cmp qword [apic_context+APIC_CONTEXT.ticks],16
    jae .returned
    pushfq
    pop qword [rsp+32]
    sti
    hlt
    pushfq
    pop qword [rsp+40]
    cli
    test qword [rsp+40],0x200
    jz .bad
    jmp .wait
.returned:
    irq_sentinels check
    cmp rsp,[apic_context+APIC_CONTEXT.wait_rsp]
    jne .bad
    mov rax,[rsp+32]
    xor rax,[rsp+40]
    test eax,0xcd5
    jnz .bad
    cmp qword [apic_context+APIC_CONTEXT.ticks],16
    jne .bad
    cmp qword [apic_context+APIC_CONTEXT.eois],16
    jne .bad
    cmp qword [apic_context+APIC_CONTEXT.returns],16
    jne .bad
    mov ecx,0x320
    call lapic_read
    test eax,0x10000
    jz .bad
    mov ecx,0x380
    call lapic_read
    test eax,eax
    jnz .bad
    mov ecx,0x390
    call lapic_read
    test eax,eax
    jnz .bad
    call apic_pending_clear
    test eax,eax
    jnz .bad
    xor eax,eax
    jmp .return
.bad:
    cli
    cld
    mov eax,1
.return:
    add rsp,56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
