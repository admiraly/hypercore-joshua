bits 64
default rel
%include 'tests/owned-map.asm'
; Keep the exact route classifier; privileged adapters are renamed and never called.
%define lapic_read target_lapic_read
%define lapic_write target_lapic_write
%define ioapic_read target_ioapic_read
%define ioapic_write target_ioapic_write
%define apic_legacy_mask target_legacy_mask
%include 'kernel/arch/x86_64/apic-access.inc'
%undef lapic_read
%undef lapic_write
%undef ioapic_read
%undef ioapic_write
%undef apic_legacy_mask
global host_apic_policy, host_apic_stage, host_apic_get
host_apic_policy:
    sub rsp,40
    call apic_quiesce
    add rsp,40
    ret
host_apic_stage:
    sub rsp,40
    mov rcx,rdi
    mov edx,esi
    call controller_stage_page
    add rsp,40
    ret
%macro pointer_case 2
    cmp edi,%1
    jne %%next
    lea rax,[%2]
    ret
%%next:
%endmacro
host_apic_get:
    pointer_case 0,apic_context
    pointer_case 1,acpi_context
    pointer_case 2,acpi_io_records
    pointer_case 3,host_lapic
    pointer_case 4,host_io
    pointer_case 5,host_event_count
    pointer_case 6,boot_info
    pointer_case 7,owned_paging
    pointer_case 8,frame_allocator
    pointer_case 9,cache_context
    pointer_case 10,mtrr_snapshot
    pointer_case 11,controller_pt
    pointer_case 12,host_fail_write
    xor eax,eax
    ret
; Event fields: kind, register, value, controller; reads10/11, writes1/2, legacy3.
host_event:
    mov r10,[host_event_count]
    cmp r10,16384
    jae .bad
    shl r10,5
    lea r11,[host_events]
    add r11,r10
    mov [r11],rax
    mov [r11+8],rdx
    mov [r11+16],r8
    mov [r11+24],r9
    inc qword [host_event_count]
    ret
.bad:
    ud2
lapic_read:
    sub rsp,40
    lea rax,[host_lapic]
    mov r8d,[rax+rcx]
    mov edx,ecx
    xor r9d,r9d
    mov eax,10
    call host_event
    mov eax,r8d
    add rsp,40
    ret
lapic_write:
    sub rsp,40
    cmp qword [host_fail_write],0
    jne .log
    lea rax,[host_lapic]
    mov [rax+rcx],edx
.log:
    mov r8d,edx
    mov edx,ecx
    xor r9d,r9d
    mov eax,1
    call host_event
    add rsp,40
    ret
; RCX virtual controller window -> R9 controller index; EDX register unchanged.
host_io_address:
    mov r9,CONTROLLER_WINDOW_BASE
    sub rcx,r9
    shr rcx,12
    dec rcx
    cmp rcx,8
    jae .bad
    mov r9,rcx
    shl rcx,10
    lea rax,[host_io]
    add rax,rcx
    ret
.bad:
    ud2
ioapic_read:
    sub rsp,40
    call host_io_address
    mov r8d,[rax+rdx*4]
    mov eax,11
    call host_event
    mov eax,r8d
    add rsp,40
    ret
ioapic_write:
    sub rsp,40
    call host_io_address
    cmp qword [host_fail_write],0
    jne .log
    mov [rax+rdx*4],r8d
.log:
    mov r8d,[rax+rdx*4]
    mov eax,2
    call host_event
    mov eax,r8d
    add rsp,40
    ret
apic_legacy_mask:
    sub rsp,40
    mov eax,3
    xor edx,edx
    mov r8d,255
    xor r9d,r9d
    call host_event
    xor eax,eax
    add rsp,40
    ret
%include 'kernel/arch/x86_64/apic-policy.inc'
%include 'kernel/arch/x86_64/cache-type.inc'
%include 'kernel/arch/x86_64/cache-snapshot.inc'
%include 'kernel/mm/paging-pat.inc'
%include 'kernel/arch/x86_64/controller-window.inc'
section .rodata
apic_lvt_offsets: dd 0x330,0x340,0x350,0x360,0x370,0x2f0
section .bss
align 8
apic_context: resb APIC_CONTEXT_size
acpi_context: resb ACPI_CONTEXT_size
acpi_io_records: resb 8*16
host_lapic: resd 256
host_io: resd 8*256
host_fail_write: resq 1
host_event_count: resq 1
host_events: resq 16384*4
boot_info: resb BOOT_INFO_V1_size
owned_paging: resb OWNED_PAGING_size
frame_allocator: resb FRAME_ALLOCATOR_size
cache_context: resb CACHE_CONTEXT_size
mtrr_snapshot: resb 4096
controller_pt: resq 1
