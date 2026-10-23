; Validated APIC_CONTEXT.mode:1 xAPIC,2 x2APIC. Only supported 32-bit registers.
; ECX byte offset -> EAX value. Nonvolatile registers preserved.
lapic_read:
    cmp qword [apic_context+APIC_CONTEXT.mode],2
    je .msr
    mov rax,[apic_context+APIC_CONTEXT.lapic]
    mov eax,[rax+rcx]
    ret
.msr:
    shr ecx,4
    add ecx,0x800
    rdmsr
    ret
; ECX byte offset, EDX value. No ICR/IPI/DFR/LDR access by callers.
lapic_write:
    cmp qword [apic_context+APIC_CONTEXT.mode],2
    je .msr
    mov rax,[apic_context+APIC_CONTEXT.lapic]
    mov [rax+rcx],edx
    mov eax,[rax+0x20]
    ret
.msr:
    mov eax,edx
    xor edx,edx
    shr ecx,4
    add ecx,0x800
    mfence
    wrmsr
    ret
; RCX window VA, EDX IOREGSEL index -> EAX IOREGWIN. BSP/IF0 only.
ioapic_read:
    mov [rcx],edx
    mov eax,[rcx+0x10]
    ret
; RCX window VA, EDX selector, R8D value. Readback flushes selector/data writes.
ioapic_write:
    mov [rcx],edx
    mov [rcx+0x10],r8d
    mov eax,[rcx+0x10]
    ret

; EAX route/LVT value -> EAX masked value, EDX0 supported /1 unsupported.
; SMI/NMI assignments retained. Unsupported active INIT/reserved routes reject.
apic_route_mask:
    mov r9d,eax
    shr r9d,8
    and r9d,7
    xor edx,edx
    cmp r9d,2
    je .return
    cmp r9d,4
    je .return
    cmp r9d,0
    je .mask
    cmp r9d,1
    je .mask
    cmp r9d,7
    je .mask
    test eax,0x10000
    jnz .return
    mov edx,1
    ret
.mask:
    or eax,0x10000
.return:
    ret

; Only called when MADT PCAT_COMPAT authorizes legacy PIC mask ports. IF0.
apic_legacy_mask:
    mov al,0xff
    out 0x21,al
    out 0xa1,al
    in al,0x21
    cmp al,0xff
    jne .bad
    in al,0xa1
    cmp al,0xff
    jne .bad
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret
