; RCX boot-info, RDX active owned builder. EAX=0 success, else stage status printed.
; Discovery only: no controller MMIO/MSR/port programming, no STI/AP startup.
acpi_topology_start:
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,32
    mov r13,rcx
    mov r14,rdx
    lea r12,[acpi_context]
    mov qword [r12+ACPI_CONTEXT.ready],0
    mov qword [r12+ACPI_CONTEXT.count],0
    mov qword [r12+ACPI_CONTEXT.enabled],0
    xor eax,eax
    cpuid
    mov r15d,eax
    test eax,eax
    jz .unsupported
    mov eax,1
    cpuid
    test edx,0x200
    jz .unsupported
    mov r8d,ebx
    shr r8d,24
    cmp r15d,0xb
    jb .id
    mov eax,0xb
    xor ecx,ecx
    cpuid
    test ebx,ebx
    jz .id
    mov r8d,edx
.id:
    mov [r12+ACPI_CONTEXT.bsp_id],r8
    mov [r12+ACPI_CONTEXT.boot],r13
    mov [r12+ACPI_CONTEXT.builder],r14
    lea rax,[acpi_cpu_records]
    mov [r12+ACPI_CONTEXT.records],rax
    mov qword [r12+ACPI_CONTEXT.capacity],256
    mov rcx,r12
    call acpi_topology_build
    test eax,eax
    jnz .failure
    lea rcx,[debug_acpi_ready]
    call debug_write
    mov rcx,[r12+ACPI_CONTEXT.bsp_id]
    call debug_hex
    lea rcx,[debug_acpi_cpus]
    call debug_write
    mov rcx,[r12+ACPI_CONTEXT.count]
    call debug_hex
    lea rcx,[debug_acpi_enabled]
    call debug_write
    mov rcx,[r12+ACPI_CONTEXT.enabled]
    call debug_hex
    lea rcx,[debug_acpi_lapic]
    call debug_write
    mov rcx,[r12+ACPI_CONTEXT.lapic]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    xor eax,eax
    jmp .return
.unsupported:
    mov eax,7
.failure:
    mov r15d,eax
    lea rcx,[debug_acpi_failure]
    call debug_write
    mov ecx,r15d
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    mov eax,r15d
.return:
    add rsp,32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
