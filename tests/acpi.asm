bits 64
default rel
%include 'tests/owned-map.asm'
global host_acpi_build, host_acpi_readable
host_acpi_build:
    sub rsp,40
    mov rcx,rdi
    call acpi_topology_build
    add rsp,40
    ret
host_acpi_readable:
    sub rsp,40
    mov r8,rdx
    mov rdx,rsi
    mov rcx,rdi
    call acpi_readable
    add rsp,40
    ret
%include 'kernel/arch/x86_64/acpi.inc'
; Explicitly check the eight integer nonvolatile registers of the firmware ABI.
global host_acpi_abi
%macro assert_acpi_reg 2
    mov rax,%2
    cmp %1,rax
    jne .abi_bad
%endmacro
%macro assert_acpi_regs 0
    assert_acpi_reg rbx,0x123456789abcde00
    assert_acpi_reg rbp,0x123456789abcde11
    assert_acpi_reg rsi,0x123456789abcde22
    assert_acpi_reg rdi,0x123456789abcde33
    assert_acpi_reg r12,0x123456789abcde44
    assert_acpi_reg r13,0x123456789abcde55
    assert_acpi_reg r14,0x123456789abcde66
    assert_acpi_reg r15,0x123456789abcde77
%endmacro
host_acpi_abi:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,56
    mov [rsp+32],rdi
    mov rbx,0x123456789abcde00
    mov rbp,0x123456789abcde11
    mov rsi,0x123456789abcde22
    mov rdi,0x123456789abcde33
    mov r12,0x123456789abcde44
    mov r13,0x123456789abcde55
    mov r14,0x123456789abcde66
    mov r15,0x123456789abcde77
    mov rcx,[rsp+32]
    call acpi_topology_build
    test eax,eax
    jnz .abi_bad
    assert_acpi_regs
    mov rcx,[rsp+32]
    mov rax,[rcx+ACPI_CONTEXT.boot]
    mov rdx,[rax+BOOT_INFO.acpi_rsdp]
    mov r8d,36
    call acpi_readable
    test eax,eax
    jnz .abi_bad
    assert_acpi_regs
    xor eax,eax
    jmp .abi_return
.abi_bad:
    mov eax,1
.abi_return:
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
