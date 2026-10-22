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
    lea rax,[kernel_fault]
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
    cmp dword [r12+BOOT_INFO.size],BOOT_INFO_size
    jne kernel_fault
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
