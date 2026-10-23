kernel_entry:
    cli
    cld
    lea rsp,[kernel_stack_top]
    and rsp,-16
    mov r12,rdi
    sgdt [firmware_gdtr]
    movzx ecx,word [firmware_gdtr]
    inc ecx
    cmp ecx,65520
    ja kernel_fault
    test ecx,7
    jnz kernel_fault
    mov r8d,ecx
    mov rsi,[firmware_gdtr+2]
    lea rdi,[kernel_gdt]
    rep movsb
    mov rax,0x00af9b000000ffff
    stosq
    mov rax,0x00cf93000000ffff
    stosq
    mov [kernel_code_selector],r8w
    lea eax,[r8+8]
    mov [kernel_data_selector],ax
    lea eax,[r8+15]
    mov [kernel_gdtr],ax
    lea rax,[kernel_gdt]
    mov [kernel_gdtr+2],rax
    lgdt [kernel_gdtr]
    push r8
    lea rax,[.segments]
    push rax
    retfq
.segments:
    mov ax,[kernel_data_selector]
    mov ds,ax
    mov es,ax
    mov ss,ax
    xor eax,eax
    mov fs,ax
    mov gs,ax
    lea rdi,[kernel_idt]
    lea rax,[exception_stubs]
    mov ecx,256
.gate:
    mov [rdi],ax
    mov dx,[kernel_code_selector]
    mov [rdi+2],dx
    mov byte [rdi+4],0
    mov byte [rdi+5],0x8e
    mov rdx,rax
    shr rdx,16
    mov [rdi+6],dx
    shr rdx,16
    mov [rdi+8],edx
    mov dword [rdi+12],0
    add rdi,16
    add rax,16
    dec ecx
    jnz .gate
    lea rax,[kernel_idt]
    mov [kernel_idtr+2],rax
    lidt [kernel_idtr]
    sub rsp,32
    call serial_configure
    lea rcx,[debug_exit]
    call debug_write
    mov rax,BOOT_MAGIC
    cmp [r12+BOOT_INFO.magic],rax
    jne kernel_fault
    cmp dword [r12+BOOT_INFO.size],BOOT_INFO_V1_size
    jne kernel_fault
%ifdef JOSHUA_FAULT_TEST
    lea rcx,[debug_fixture_rip]
    call debug_write
    lea rcx,[fault_test_instruction]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
%if JOSHUA_FAULT_TEST = 2
    mov eax,0xffff
fault_test_instruction:
    mov ds,ax
%else
fault_test_instruction:
    ud2
%endif
    jmp kernel_fault
%endif
    cmp dword [r12+BOOT_INFO.version],1
    jne kernel_fault
    cmp dword [r12+BOOT_INFO.descriptor_version],1
    jne kernel_fault
    lea rcx,[memory_map]
    cmp [r12+BOOT_INFO.memory_map],rcx
    jne kernel_fault
    mov eax,0x80000000
    cpuid
    cmp eax,0x80000008
    jb kernel_fault
    mov eax,0x80000008
    cpuid
    movzx r9d,al
    mov r15d,r9d
    lea rcx,[memory_map]
    mov rdx,[r12+BOOT_INFO.memory_map_size]
    mov r8,[r12+BOOT_INFO.descriptor_size]
    call memory_map_validate
    test eax,eax
    jnz kernel_fault
    lea rcx,[debug_map_valid]
    call debug_write
    mov rcx,r12
    mov edx,r15d
    call memory_image_check
    test eax,eax
    jnz kernel_fault
    lea rax,[candidate_ranges]
    mov [candidate_set+RANGE_SET.entries],rax
    mov rcx,r12
    lea rdx,[candidate_set]
    mov r8d,r15d
    call memory_candidates_build
    test eax,eax
    jnz kernel_fault
    lea rcx,[debug_candidates]
    call debug_write
    mov rcx,[candidate_set+RANGE_SET.count]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    mov rax,cr0
    bt rax,31
    jnc kernel_fault
    test al,1
    jz kernel_fault
    mov rax,cr4
    test eax,0x1000
    jnz kernel_fault
    test eax,0x20
    jz kernel_fault
    mov ecx,0xc0000080
    rdmsr
    test eax,0x400
    jz kernel_fault
    shr eax,11
    and eax,1
    mov [paging_inventory+PAGING_INVENTORY.flags],rax
    mov eax,0x80000001
    cpuid
    bt edx,26
    jnc .no_1g
    or qword [paging_inventory+PAGING_INVENTORY.flags],PAGING_FLAG_1G
.no_1g:
    lea rax,[paging_tables]
    mov [paging_inventory+PAGING_INVENTORY.entries],rax
    lea rax,[memory_map]
    mov [paging_inventory+PAGING_INVENTORY.memory_map],rax
    mov rax,[r12+BOOT_INFO.memory_map_size]
    mov [paging_inventory+PAGING_INVENTORY.map_size],rax
    mov rax,[r12+BOOT_INFO.descriptor_size]
    mov [paging_inventory+PAGING_INVENTORY.descriptor_size],rax
    mov [paging_inventory+PAGING_INVENTORY.physical_bits],r15
    mov rax,cr3
    mov [paging_cr3],rax
    mov rdx,0x000ffffffffff000
    and rdx,rax
    mov [paging_inventory+PAGING_INVENTORY.root],rdx
    lea rcx,[paging_inventory]
    call paging_inventory_build
    test eax,eax
    jnz kernel_fault
    lea rcx,[paging_inventory]
    lea rdx,[candidate_set]
    call paging_inventory_reserve
    test eax,eax
    jnz kernel_fault
    mov rax,cr3
    cmp rax,[paging_cr3]
    jne kernel_fault
    lea rcx,[debug_paging]
    call debug_write
    mov rcx,[paging_cr3]
    call debug_hex
    lea rcx,[debug_table_count]
    call debug_write
    mov rcx,[paging_inventory+PAGING_INVENTORY.count]
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    mov rdi,[r12+BOOT_INFO.framebuffer]
    mov ecx,[r12+BOOT_INFO.stride]
    mov [frame_stride_qword],rcx
    mov eax,[r12+BOOT_INFO.height]
    imul rcx,rax
    mov eax,0x00080808
    rep stosd
    lea rcx,[screen_title]
    mov edx,32
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[screen_subtitle]
    mov edx,64
    mov r8d,0x00b0b0b0
    call draw_line
    lea rcx,[screen_exit]
    mov edx,112
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[screen_stack]
    mov edx,140
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[screen_memory]
    mov edx,168
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[screen_disk]
    mov edx,196
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[screen_limits]
    mov edx,252
    mov r8d,0x00909090
    call draw_line
    lea rcx,[screen_stop]
    mov edx,280
    mov r8d,0x00e0e0e0
    call draw_line
    lea rcx,[debug_halted]
    call debug_write
    jmp stop_cpu

kernel_fault:
    cli
    cld
    lea rsp,[kernel_stack_top]
    and rsp,-16
    sub rsp,32
    lea rcx,[debug_fault]
    call debug_write
    jmp stop_cpu

; Each IDT target has a fixed 16-byte stride. Normalize CPU error-code
; and no-error-code frames to vector/error/RIP/CS/RFLAGS at RSP.
align 16
exception_stubs:
%assign vector 0
%rep 256
%assign stub_start ($-$$)
%if vector != 8 && vector != 10 && vector != 11 && vector != 12 && vector != 13 && vector != 14 && vector != 17 && vector != 21 && vector != 29 && vector != 30
    push qword 0
%endif
    push qword vector
    jmp exception_stop
    times 16-(($-$$)-stub_start) db 0x90
%assign vector vector+1
%endrep

exception_stop:
    cli
    cld
    mov r12,[rsp]
    mov r13,[rsp+8]
    mov r14,[rsp+16]
    mov r15,cr2
    lea rsp,[kernel_stack_top]
    and rsp,-16
    sub rsp,32
    lea rcx,[debug_fault]
    call debug_write
    lea rcx,[debug_vector]
    call debug_write
    mov rcx,r12
    call debug_hex
    lea rcx,[debug_error]
    call debug_write
    mov rcx,r13
    call debug_hex
    lea rcx,[debug_rip]
    call debug_write
    mov rcx,r14
    call debug_hex
    lea rcx,[debug_cr2]
    call debug_write
    mov rcx,r15
    call debug_hex
    lea rcx,[debug_newline]
    call debug_write
    jmp stop_cpu

; RCX unsigned value; firmware ABI, no firmware calls. Fixed 16 digits.
debug_hex:
    sub rsp,56
    mov r10,rcx
    mov r11d,16
    lea rdx,[hex_digits]
    mov byte [rsp+48],0
.digit:
    mov rax,r10
    and eax,15
    mov al,[rdx+rax]
    mov [rsp+r11+31],al
    shr r10,4
    dec r11d
    jnz .digit
    lea rcx,[rsp+32]
    call debug_write
    add rsp,56
    ret

draw_line:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov r12,rcx
    mov r13d,edx
    mov r14d,r8d
    mov r15d,32
    lea rdi,[boot_info]
    lea rsi,[font_characters]
.character:
    movzx eax,byte [r12]
    test al,al
    jz .done
    inc r12
    cmp al,' '
    je .advance
    xor ecx,ecx
.find:
    cmp byte [rsi+rcx],0
    je .advance
    cmp al,[rsi+rcx]
    je .found
    inc ecx
    jmp .find
.found:
    imul ecx,7
    lea rbx,[font_rows]
    add rbx,rcx
    xor r8d,r8d
.row:
    movzx r10d,byte [rbx+r8]
    xor r9d,r9d
.pixel:
    mov ecx,4
    sub ecx,r9d
    bt r10d,ecx
    jnc .next_pixel
    lea eax,[r13+r8*2]
    imul rax,qword [frame_stride_qword]
    lea ecx,[r15+r9*2]
    add rax,rcx
    mov rdx,[rdi+BOOT_INFO.framebuffer]
    lea rdx,[rdx+rax*4]
    mov [rdx],r14d
    mov [rdx+4],r14d
    mov ecx,[rdi+BOOT_INFO.stride]
    lea rdx,[rdx+rcx*4]
    mov [rdx],r14d
    mov [rdx+4],r14d
.next_pixel:
    inc r9d
    cmp r9d,5
    jb .pixel
    inc r8d
    cmp r8d,7
    jb .row
.advance:
    add r15d,12
    mov eax,[rdi+BOOT_INFO.width]
    sub eax,12
    cmp r15d,eax
    jb .character
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
