bits 64
default rel
%include 'sdk/boot.inc'
%include 'sdk/context.inc'
%include 'sdk/user.inc'
%include 'sdk/elf.inc'
%include 'sdk/keyboard.inc'
%ifndef JOSHUA_ELF_TEST
 %define JOSHUA_ELF_TEST 0
%endif
%ifndef JOSHUA_QEMU_KVM_TEST
 %define JOSHUA_QEMU_KVM_TEST 0
%endif
%ifndef JOSHUA_USER_TEST
 %define JOSHUA_USER_TEST 0
%endif
%ifndef JOSHUA_CONTEXT_TEST
 %define JOSHUA_CONTEXT_TEST 0
%endif
%ifndef JOSHUA_TIMER_TEST
 %define JOSHUA_TIMER_TEST 0
%endif
%ifndef JOSHUA_FAULT_TEST
 %define JOSHUA_FAULT_TEST 0
%endif

section .headers start=0 vstart=0
image_start:
db 'MZ'
times 0x3c-($-$$) db 0
dd 0x80
times 0x80-($-$$) db 0
db 'PE',0,0
dw 0x8664, 5
dd 0,0,0
dw 0xf0, 0x0022
dw 0x20b
db 0,0
dd PE_TEXT_SIZE,(PE_RODATA_SIZE+0x2000),PE_BSS_SIZE
dd efi_entry,0x1000
dq 0
dd 0x1000,0x200
dw 0,0,0,0,0,0
dd 0,PE_IMAGE_SIZE,0x1000,0
dw 10,0x0140
dq 0x10000,0x1000,0x10000,0x1000
dd 0,16
times 5 dq 0
dd PE_RELOC_RVA,12
times 10 dq 0
db '.text',0,0,0
dd PE_TEXT_SIZE,0x1000,PE_TEXT_SIZE,0x1000,0,0
dw 0,0
dd 0x60000020
db '.data',0,0,0
dd 0x1000,PE_DATA_RVA,0x1000,PE_DATA_RVA,0,0
dw 0,0
dd 0xc0000040
db '.rodata',0
dd PE_RODATA_SIZE,PE_RODATA_RVA,PE_RODATA_SIZE,PE_RODATA_RVA,0,0
dw 0,0
dd 0x40000040
db '.bss',0,0,0,0
dd PE_BSS_SIZE,PE_BSS_RVA,0,0,0,0
dw 0,0
dd 0xc0000080
db '.reloc',0,0
dd 12,PE_RELOC_RVA,0x1000,PE_BSS_RVA,0,0
dw 0,0
dd 0x42000040
times 0x1000-($-$$) db 0

section .text start=0x1000 vstart=0x1000
efi_entry:
    cld
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov [image_handle],rcx
    mov [system_table],rdx
    mov rax,[rdx+ST_BOOT]
    mov [boot_services],rax
    mov rax,[rdx+ST_CON_OUT]
    mov [console_out],rax
    mov rax,[rdx+ST_CON_IN]
    mov [console_in],rax
    call serial_init
    lea rcx,[debug_entry]
    call debug_write
    mov rcx,[console_out]
    mov edx,7
    call [rcx+CON_SET_ATTRIBUTE]
    mov rcx,[console_out]
    call [rcx+CON_CLEAR]
    lea rdx,[banner]
    call print
    xor ecx,ecx
    xor edx,edx
    xor r8d,r8d
    xor r9d,r9d
    mov rax,[boot_services]
    call [rax+BS_SET_WATCHDOG]
    test rax,rax
    jnz .watchdog_error
    call image_info
    test rax,rax
    jnz .image_error
    call cpu_info
    call find_acpi
    call graphics_info
    test rax,rax
    jnz .unsupported
    call show_info
    lea rdx,[autoboot_message]
    call print
    mov ebx,500
.countdown:
    call read_key
    test rax,rax
    jz .menu
    mov rcx,EFI_NOT_READY
    cmp rax,rcx
    jne .input_error
    mov ecx,10000
    mov rax,[boot_services]
    call [rax+BS_STALL]
    dec ebx
    jnz .countdown
    jmp boot_kernel
.menu:
    lea rdx,[menu_message]
    call print
.wait:
    call read_key
    test rax,rax
    jz .dispatch
    mov rcx,EFI_NOT_READY
    cmp rax,rcx
    jne .input_error
    mov ecx,10000
    mov rax,[boot_services]
    call [rax+BS_STALL]
    jmp .wait
.dispatch:
    movzx eax,word [key_data+2]
    or al,0x20
    cmp al,'b'
    je boot_kernel
    cmp al,'i'
    je .info
    cmp al,'m'
    je .memory
    cmp al,'r'
    je .reboot
    cmp al,'s'
    je .shutdown
    jmp .menu
.info:
    call show_info
    jmp .menu
.memory:
    call get_memory_map
    test rax,rax
    jnz .map_error
    lea rdx,[map_message]
    call print
    mov rcx,[boot_info+BOOT_INFO.memory_map_size]
    call print_hex
    lea rdx,[stride_message]
    call print
    mov rcx,[boot_info+BOOT_INFO.descriptor_size]
    call print_hex
    lea rdx,[newline]
    call print
    jmp .menu
.map_error:
    lea rdx,[map_failure]
    call print
    jmp .menu
.reboot:
    xor ecx,ecx
    jmp .reset
.shutdown:
    mov ecx,2
.reset:
    xor edx,edx
    xor r8d,r8d
    xor r9d,r9d
    mov rax,[system_table]
    mov rax,[rax+ST_RUNTIME]
    call [rax+RT_RESET_SYSTEM]
    jmp stop_cpu
.image_error:
    lea rdx,[image_failure]
    call print
    jmp .return_error
.input_error:
    lea rdx,[input_failure]
    call print
    jmp .return_error
.watchdog_error:
    lea rdx,[watchdog_failure]
    call print
    jmp .return_error
.unsupported:
    lea rdx,[graphics_failure]
    call print
.return_error:
    mov rax,0x8000000000000003
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

image_info:
    sub rsp,40
    mov rcx,[image_handle]
    lea rdx,[loaded_image_guid]
    lea r8,[loaded_image]
    mov rax,[boot_services]
    call [rax+BS_HANDLE_PROTOCOL]
    test rax,rax
    jnz .done
    mov rax,[loaded_image]
    test rax,rax
    jz .bad
    mov rcx,[rax+64]
    lea rdx,[image_start]
    cmp rcx,rdx
    jne .bad
    test rcx,0xfff
    jnz .bad
    mov rdx,[rax+72]
    cmp rdx,PE_IMAGE_SIZE
    jb .bad
    test rdx,0xfff
    jnz .bad
    mov r8,rcx
    add r8,rdx
    jc .bad
    mov [boot_info+BOOT_INFO_V1.image_base],rcx
    mov [boot_info+BOOT_INFO_V1.image_size],rdx
    xor eax,eax
    jmp .done
.bad:
    mov eax,1
.done:
    add rsp,40
    ret

print:
    sub rsp,40
    mov rcx,[console_out]
    call [rcx+CON_OUTPUT]
    add rsp,40
    ret

print_hex:
    sub rsp,40
    lea rdx,[hex_buffer+4]
    mov r8d,16
    lea r9,[hex_digits]
.digit:
    rol rcx,4
    mov eax,ecx
    and eax,15
    movzx eax,byte [r9+rax]
    mov [rdx],ax
    add rdx,2
    dec r8d
    jnz .digit
    lea rdx,[hex_buffer]
    call print
    add rsp,40
    ret

read_key:
    sub rsp,40
    mov rcx,[console_in]
    lea rdx,[key_data]
    call [rcx+CON_READ_KEY]
    add rsp,40
    ret

cpu_info:
    push rbx
    push rdi
    sub rsp,40
    mov eax,0x80000000
    cpuid
    cmp eax,0x80000004
    jb .done
    lea rdi,[cpu_brand_ascii]
    mov r8d,0x80000002
.brand:
    mov eax,r8d
    cpuid
    mov [rdi],eax
    mov [rdi+4],ebx
    mov [rdi+8],ecx
    mov [rdi+12],edx
    add rdi,16
    inc r8d
    cmp r8d,0x80000005
    jb .brand
    lea r8,[cpu_brand_ascii]
    lea r9,[cpu_brand]
    mov ecx,48
.wide:
    movzx eax,byte [r8]
    mov [r9],ax
    inc r8
    add r9,2
    dec ecx
    jnz .wide
.done:
    add rsp,40
    pop rdi
    pop rbx
    ret

find_acpi:
    mov rax,[system_table]
    mov r8,[rax+ST_TABLE_COUNT]
    mov r9,[rax+ST_TABLES]
    lea r10,[acpi_guid]
.table:
    test r8,r8
    jz .done
    mov rax,[r10]
    cmp [r9],rax
    jne .next
    mov rax,[r10+8]
    cmp [r9+8],rax
    jne .next
    mov rax,[r9+16]
    mov [boot_info+BOOT_INFO.acpi_rsdp],rax
    ret
.next:
    add r9,24
    dec r8
    jmp .table
.done:
    ret

graphics_info:
    sub rsp,40
    lea rcx,[gop_guid]
    xor edx,edx
    lea r8,[gop]
    mov rax,[boot_services]
    call [rax+BS_LOCATE_PROTOCOL]
    test rax,rax
    jnz .done
    mov rax,[gop]
    test rax,rax
    jz .bad
    mov rax,[rax+GOP_MODE]
    test rax,rax
    jz .bad
    mov r8,[rax+MODE_INFO]
    test r8,r8
    jz .bad
    mov edx,[r8+INFO_FORMAT]
    cmp edx,1
    ja .bad
    mov [boot_info+BOOT_INFO.pixel_format],edx
    mov edx,[r8+INFO_WIDTH]
    cmp edx,640
    jb .bad
    mov [boot_info+BOOT_INFO.width],edx
    mov ecx,[r8+INFO_HEIGHT]
    cmp ecx,400
    jb .bad
    mov [boot_info+BOOT_INFO.height],ecx
    mov r9d,[r8+INFO_STRIDE]
    cmp r9d,edx
    jb .bad
    mov [boot_info+BOOT_INFO.stride],r9d
    mov edx,r9d
    imul rcx,rdx
    mov rdx,0x3fffffffffffffff
    cmp rcx,rdx
    ja .bad
    shl rcx,2
    cmp rcx,[rax+MODE_FB_SIZE]
    ja .bad
    mov rdx,[rax+MODE_FB_BASE]
    test rdx,rdx
    jz .bad
    mov [boot_info+BOOT_INFO.framebuffer],rdx
    add rdx,[rax+MODE_FB_SIZE]
    jc .bad
    mov rdx,[rax+MODE_FB_SIZE]
    mov [boot_info+BOOT_INFO.framebuffer_size],rdx
    lea rcx,[debug_gop]
    call debug_write
    xor eax,eax
    jmp .done
.bad:
    mov eax,1
.done:
    add rsp,40
    ret

show_info:
    sub rsp,40
    lea rdx,[cpu_message]
    call print
    lea rdx,[cpu_brand]
    call print
    lea rdx,[framebuffer_message]
    call print
    mov rcx,[boot_info+BOOT_INFO.framebuffer]
    call print_hex
    lea rdx,[width_message]
    call print
    mov ecx,[boot_info+BOOT_INFO.width]
    call print_hex
    lea rdx,[height_message]
    call print
    mov ecx,[boot_info+BOOT_INFO.height]
    call print_hex
    lea rdx,[acpi_message]
    call print
    mov rcx,[boot_info+BOOT_INFO.acpi_rsdp]
    call print_hex
    lea rdx,[newline]
    call print
    add rsp,40
    ret

get_memory_map:
    sub rsp,56
    mov qword [boot_info+BOOT_INFO.memory_map_size],MAP_CAPACITY
    lea rcx,[boot_info+BOOT_INFO.memory_map_size]
    lea rdx,[memory_map]
    lea r8,[map_key]
    lea r9,[boot_info+BOOT_INFO.descriptor_size]
    lea rax,[boot_info+BOOT_INFO.descriptor_version]
    mov [rsp+32],rax
    mov rax,[boot_services]
    call [rax+BS_GET_MEMORY_MAP]
    test rax,rax
    jnz .done
    cmp qword [boot_info+BOOT_INFO.descriptor_size],40
    jb .bad
    mov rax,[boot_info+BOOT_INFO.memory_map_size]
    test rax,rax
    jz .bad
    cmp rax,MAP_CAPACITY
    ja .bad
    xor edx,edx
    div qword [boot_info+BOOT_INFO.descriptor_size]
    test rdx,rdx
    jnz .bad
    lea rax,[memory_map]
    mov [boot_info+BOOT_INFO.memory_map],rax
    xor eax,eax
    jmp .done
.bad:
    mov rax,EFI_INVALID_PARAMETER
.done:
    add rsp,56
    ret

boot_kernel:
    lea rdx,[boot_message]
    call print
    mov ebx,8
.retry:
    call get_memory_map
    test rax,rax
    jnz .failure
    mov rcx,[image_handle]
    mov rdx,[map_key]
    mov rax,[boot_services]
    call [rax+BS_EXIT_BOOT]
    test rax,rax
    jz .entered
    mov rcx,EFI_INVALID_PARAMETER
    cmp rax,rcx
    jne .failure
    dec ebx
    jnz .retry
.failure:
    lea rcx,[debug_map_failure]
    call debug_write
    jmp stop_cpu
.entered:
    cli
    lea rdi,[boot_info]
    jmp kernel_entry

serial_init:
    push rbx
    mov eax,1
    cpuid
    bt ecx,31
    jnc .done
    mov eax,0x40000000
    cpuid
%if JOSHUA_QEMU_KVM_TEST
    ; Explicit KVM-only alignment fixture. Normal images still recognize TCG only.
    cmp ebx,0x4b4d564b
    jne .tcg_vendor
    cmp ecx,0x564b4d56
    jne .done
    cmp edx,0x0000004d
    je .emulator_uart
.tcg_vendor:
%endif
    cmp ebx,0x54474354
    jne .done
    cmp ecx,0x43544743
    jne .done
    cmp edx,0x47435447
    jne .done
.emulator_uart:
    mov dx,0x3fd
    in al,dx
    cmp al,0xff
    je .done
    mov byte [serial_present],1
.done:
    pop rbx
    ret

serial_configure:
    cmp byte [serial_present],0
    je .done
    mov dx,0x3fb
    mov al,3
    out dx,al
    mov dx,0x3f9
    xor al,al
    out dx,al
    mov dx,0x3fb
    mov al,0x80
    out dx,al
    mov dx,0x3f8
    mov al,1
    out dx,al
    mov dx,0x3f9
    xor al,al
    out dx,al
    mov dx,0x3fb
    mov al,3
    out dx,al
    mov dx,0x3fa
    mov al,0xc7
    out dx,al
    mov dx,0x3fc
    mov al,3
    out dx,al
.done:
    ret

debug_write:
    cmp byte [serial_present],0
    je .done
    mov r8,rcx
.character:
    mov r9b,[r8]
    test r9b,r9b
    jz .done
    mov ecx,65536
    mov dx,0x3fd
.ready:
    in al,dx
    test al,0x20
    jnz .send
    dec ecx
    jnz .ready
    ret
.send:
    mov dx,0x3f8
    mov al,r9b
    out dx,al
    inc r8
    jmp .character
.done:
    ret

stop_cpu:
    cli
.halt:
    hlt
    jmp .halt

%include 'kernel/arch/x86_64/entry.inc'
%include 'kernel/arch/x86_64/descriptors.inc'
%include 'kernel/arch/x86_64/acpi.inc'
%include 'kernel/arch/x86_64/acpi-start.inc'
%include 'kernel/arch/x86_64/controller-window.inc'
%include 'kernel/arch/x86_64/cache-type.inc'
%include 'kernel/arch/x86_64/cache-snapshot.inc'
%include 'kernel/arch/x86_64/apic-access.inc'
%include 'kernel/arch/x86_64/apic-start.inc'
%include 'kernel/arch/x86_64/apic-policy.inc'
%include 'kernel/arch/x86_64/interrupts.inc'
%include 'kernel/sched/context.inc'
%include 'kernel/sched/start.inc'
%include 'kernel/mm/map.inc'
%include 'kernel/mm/ranges.inc'
%include 'kernel/mm/candidates.inc'
%include 'kernel/mm/paging-inventory.inc'
%include 'kernel/mm/frames.inc'
%include 'kernel/mm/frame-check.inc'
%include 'kernel/mm/owned-map.inc'
%include 'kernel/mm/paging-pat.inc'
%include 'kernel/user/descriptors.inc'
%include 'kernel/user/storage.inc'
%include 'kernel/user/elf.inc'
%include 'kernel/user/elf-start.inc'
%include 'kernel/user/map.inc'
%include 'kernel/user/proof.inc'
%include 'kernel/user/machine.inc'
%include 'kernel/user/frame.inc'
%include 'kernel/user/retire.inc'
%include 'kernel/user/start.inc'
%include 'kernel/user/fixtures.inc'
%include 'kernel/input/decoder.inc'
%include 'kernel/input/authority.inc'
%include 'kernel/input/controller.inc'
%include 'kernel/input/ports.inc'
%include 'kernel/input/start.inc'
%include 'kernel/input/frame.inc'
%include 'kernel/mm/owned-start.inc'
%include 'kernel/mm/lifecycle.inc'
%include 'kernel/mm/lifecycle-platform.inc'
%include 'kernel/mm/lifecycle-check.inc'
times PE_TEXT_SIZE-($-$$) db 0

section .data start=PE_DATA_RVA vstart=PE_DATA_RVA
align 8
image_handle: dq 0
system_table: dq 0
boot_services: dq 0
console_out: dq 0
console_in: dq 0
gop: dq 0
map_key: dq 0
serial_present: db 0
key_data: dd 0
hex_buffer: dw '0','x'
times 16 dw '0'
dw 0
cpu_brand_ascii: times 48 db 0
cpu_brand: times 49 dw 0
align 8
boot_info:
    dq BOOT_MAGIC
    dd 1,BOOT_INFO_V1_size
    times BOOT_INFO_V1_size-16 db 0
loaded_image: dq 0
%include 'kernel/arch/x86_64/data.inc'
times 0x1000-($-$$) db 0

section .rodata start=PE_RODATA_RVA vstart=PE_RODATA_RVA
hex_digits: db '0123456789ABCDEF'
loaded_image_guid: dd 0x5b1b31a1
dw 0x9562,0x11d2
db 0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b
gop_guid: dd 0x9042a9de
dw 0x23dc,0x4a38
db 0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a
acpi_guid: dd 0x8868e871
dw 0xe4f1,0x11d3
db 0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81

%macro wide 2
%1: dw __utf16__(%2),0
%endmacro
wide banner, `\r\nJOSHUA OS / HYPERCORE - M0/M1 DEVELOPMENT\r\nAssembly UEFI bootstrap. No installer. No disk writes.\r\n`
wide cpu_message, `CPU: `
wide framebuffer_message, `\r\nFramebuffer: `
wide width_message, `\r\nWidth: `
wide height_message, `  Height: `
wide acpi_message, `\r\nACPI 2.0 RSDP: `
wide newline, `\r\n`
wide autoboot_message, `\r\nBooting HyperCore in 5 seconds. Press any key for diagnostics.\r\n`
wide menu_message, `\r\n[B] Boot  [I] Info  [M] Memory map  [R] Reboot  [S] Shutdown\r\n> `
wide map_message, `Memory map bytes: `
wide stride_message, `  Descriptor bytes: `
wide map_failure, `Memory map unavailable or unsupported.\r\n`
wide boot_message, `\r\nLeaving firmware. HyperCore will display a status screen and halt.\r\n`
wide graphics_failure, `Unsupported framebuffer. Need >=640x400 packed RGB/BGR GOP.\r\n`
wide image_failure, `Loaded image bounds unavailable or unsupported.\r\n`
wide input_failure, `Firmware input failed. Returning to firmware.\r\n`
wide watchdog_failure, `Cannot disable firmware watchdog. Aborting diagnostics.\r\n`
debug_entry: db 'JOSHUA:UEFI_ENTRY',13,10,0
debug_gop: db 'JOSHUA:GOP_OK',13,10,0
debug_map_failure: db 'JOSHUA:BOOT_FAILURE',13,10,0

%include 'kernel/arch/x86_64/rodata.inc'
%include 'kernel/input/keymap.inc'
times 0x1000-($-$$) db 0
%include 'kernel/user/elf-image.inc'
times PE_RODATA_SIZE-($-$$) db 0

section .reloc start=PE_BSS_RVA vstart=PE_RELOC_RVA
dd 0x1000,12
dw 0,0
times 0x1000-($-$$) db 0

section .bss nobits vstart=PE_BSS_RVA
memory_map: resb MAP_CAPACITY
kernel_idt: resb 4096
kernel_stack_guard: resb 4096
kernel_stack: resb 65536
kernel_stack_top:
kernel_gdt: resb 65536

candidate_ranges: resb CANDIDATE_CAPACITY*16
paging_tables: resb PAGING_TABLE_CAPACITY*16
frame_records: resb FRAME_ALLOCATION_CAPACITY*24
double_fault_guard: resb 4096
double_fault_stack: resb 16384
double_fault_top:
nmi_guard: resb 4096
nmi_stack: resb 16384
nmi_top:
machine_check_guard: resb 4096
machine_check_stack: resb 16384
machine_check_top:
exception_guard: resb 4096
exception_stack: resb 16384
exception_top:
acpi_cpu_records: resb 256*16
mtrr_snapshot: resb 4096
context_worker1_guard: resb 4096
context_worker1_stack: resb 16384
context_worker1_top:
context_worker2_guard: resb 4096
context_worker2_stack: resb 16384
context_worker2_top:
context_metadata:
kernel_context: resb KERNEL_CONTEXT_size
kernel_context_records: resb 3*KERNEL_CONTEXT_RECORD_size
context_worker_rsp: resq 2
user_context: resb USER_CONTEXT_size
user_elf_plan: resb ELF_PLAN_size
user_processes: resb USER_PROCESS_COUNT*USER_PROCESS_size
user_transition_step: resq 1
user_terminal_state: resq 1
user_drain_windows: resq 1
user_pending_seen: resq 1
user_test_ldt: resb 16
keyboard_decoder: resb KEYBOARD_DECODER_size
keyboard_authority: resb KEYBOARD_AUTH_size
keyboard_controller: resb KEYBOARD_CONTROLLER_size
keyboard_io_window: resq 1
keyboard_io_selector: resq 1
keyboard_saved_low: resd 1
keyboard_saved_high: resd 1
resb 4096-($-context_metadata)
user_entry1_guard: resb 4096
user_entry1_stack: resb 16384
user_entry1_top:
user_entry2_guard: resb 4096
user_entry2_stack: resb 16384
user_entry2_top:
%if ($-$$) != PE_BSS_SIZE
 %error BSS layout mismatch
%endif
