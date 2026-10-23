; EAX=0 controller state quiesced/ready; nonzero stage status, IF remains clear.
apic_start:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov qword [apic_context+APIC_CONTEXT.ready],0
    pushfq
    pop rax
    test eax,0x200
    jnz .mode_bad
    mov rax,ACPI_READY_MAGIC
    cmp [acpi_context+ACPI_CONTEXT.ready],rax
    jne .mode_bad
    mov eax,1
    cpuid
    test edx,0x200
    jz .mode_bad
    mov r15d,ecx
    mov ecx,0x1b
    rdmsr
    shl rdx,32
    or rax,rdx
    test eax,0x800
    jz .mode_bad
    test eax,0x100
    jz .mode_bad
%if JOSHUA_TIMER_TEST = 1
    test r15d,0x200000
    jz .mode_bad
    or rax,0x400
    mov rdx,rax
    shr rdx,32
    wrmsr
%endif
    mov qword [apic_context+APIC_CONTEXT.mode],1
    test eax,0x400
    jz .xapic
    test r15d,0x200000
    jz .mode_bad
    mov qword [apic_context+APIC_CONTEXT.mode],2
    jmp .mappings
.xapic:
    mov rdx,0x000ffffffffff000
    and rax,rdx
    cmp rax,[acpi_context+ACPI_CONTEXT.lapic]
    jne .mode_bad
    mov [apic_context+APIC_CONTEXT.lapic],rax
.mappings:
    call controller_mappings_start
    test eax,eax
    jnz .mapping_bad
    cmp qword [apic_context+APIC_CONTEXT.mode],1
    jne .prepared
    mov rax,CONTROLLER_WINDOW_BASE
    mov [apic_context+APIC_CONTEXT.lapic],rax
.prepared:
    call apic_quiesce
    jmp .return
.mode_bad:
    mov eax,1
    jmp .return
.mapping_bad:
    mov eax,2
.return:
    add rsp,40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
