; Pure command policy through fixed port adapters; caller proves BSP/IF0 and masks
; the selected IOAPIC route before entry. Trusted state/authority storage, DF0.
keyboard_controller_prepare:
    push rbx
    sub rsp,32
    mov rbx,rcx
    cmp qword [rbx+KEYBOARD_CONTROLLER.ready],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.failed],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.stage],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.config],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.scanning],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.discarded],0
    jne .bad
    mov rdx,[rbx+KEYBOARD_CONTROLLER.authority]
    test rdx,rdx
    jz .bad
    mov rax,KEYBOARD_AUTH_MAGIC
    cmp [rdx+KEYBOARD_AUTH.ready],rax
    jne .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],1
    mov ecx,0xad
    call keyboard_write_command
    test eax,eax
    jnz .bad
    mov ecx,0xa7
    call keyboard_write_command
    test eax,eax
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],2
    call keyboard_flush_output
    test eax,eax
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],3
    mov ecx,0x20
    call keyboard_write_command
    test eax,eax
    jnz .bad
    call keyboard_read_reply
    test eax,eax
    jnz .bad
    test edx,0x80
    jnz .bad
    and edx,0x0c
    or edx,0x70
    mov [rbx+KEYBOARD_CONTROLLER.config],rdx
    mov ecx,edx
    call keyboard_write_config
    test eax,eax
    jnz .bad
    mov ecx,0x20
    call keyboard_write_command
    test eax,eax
    jnz .bad
    call keyboard_read_reply
    test eax,eax
    jnz .bad
    cmp rdx,[rbx+KEYBOARD_CONTROLLER.config]
    jne .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],4
    mov ecx,0xab
    call keyboard_write_command
    test eax,eax
    jnz .bad
    call keyboard_read_reply
    test eax,eax
    jnz .bad
    test edx,edx
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],5
    mov ecx,0xae
    call keyboard_write_command
    test eax,eax
    jnz .bad
    mov ecx,0xf5
    mov rdx,rbx
    mov r8d,1
    call keyboard_send_byte
    test eax,eax
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],6
    mov ecx,0xf0
    mov rdx,rbx
    xor r8d,r8d
    call keyboard_send_byte
    test eax,eax
    jnz .bad
    mov ecx,2
    mov rdx,rbx
    xor r8d,r8d
    call keyboard_send_byte
    test eax,eax
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],7
    mov rcx,[rbx+KEYBOARD_CONTROLLER.config]
    and ecx,0x0c
    or ecx,0x61
    mov [rbx+KEYBOARD_CONTROLLER.config],rcx
    call keyboard_write_config
    test eax,eax
    jnz .bad
    mov ecx,0x20
    call keyboard_write_command
    test eax,eax
    jnz .bad
    call keyboard_read_reply
    test eax,eax
    jnz .bad
    cmp rdx,[rbx+KEYBOARD_CONTROLLER.config]
    jne .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],8
    mov rax,KEYBOARD_CONTROLLER_MAGIC
    mov [rbx+KEYBOARD_CONTROLLER.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+KEYBOARD_CONTROLLER.ready],0
    mov qword [rbx+KEYBOARD_CONTROLLER.failed],1
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; Route masked and owned window already published by caller. F4 ACK is the last
; reply read: bytes queued immediately after it remain input, never configuration.
keyboard_controller_enable:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov rax,KEYBOARD_CONTROLLER_MAGIC
    cmp [rbx+KEYBOARD_CONTROLLER.ready],rax
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.failed],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.stage],8
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.scanning],0
    jne .bad
    mov rax,[rbx+KEYBOARD_CONTROLLER.config]
    and rax,-13
    cmp rax,0x61
    jne .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],9
    mov ecx,0xf4
    mov rdx,rbx
    xor r8d,r8d
    call keyboard_send_byte
    test eax,eax
    jnz .bad
    mov qword [rbx+KEYBOARD_CONTROLLER.scanning],1
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],10
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+KEYBOARD_CONTROLLER.ready],0
    mov qword [rbx+KEYBOARD_CONTROLLER.failed],1
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; Caller stops deadline timer/masks IOAPIC before normal IF0 retirement.
; Emergency ISR use instead requires proved active window plus actual ISR44 ownership;
; that caller marks failure, sends EOI once and resumes only the cleanup continuation.
; No readback transaction after F4. Ready is retired; restart requires fresh proof.
keyboard_controller_disable:
    push rbx
    sub rsp,32
    mov rbx,rcx
    mov rax,KEYBOARD_CONTROLLER_MAGIC
    cmp [rbx+KEYBOARD_CONTROLLER.ready],rax
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.failed],0
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.stage],10
    jne .bad
    cmp qword [rbx+KEYBOARD_CONTROLLER.scanning],1
    jne .bad
    mov rax,[rbx+KEYBOARD_CONTROLLER.config]
    and rax,-13
    cmp rax,0x61
    jne .bad
    mov rcx,[rbx+KEYBOARD_CONTROLLER.config]
    and ecx,0xfe
    call keyboard_write_config
    test eax,eax
    jnz .bad
    mov ecx,0xad
    call keyboard_write_command
    test eax,eax
    jnz .bad
    and qword [rbx+KEYBOARD_CONTROLLER.config],0xfe
    or qword [rbx+KEYBOARD_CONTROLLER.config],0x10
    mov qword [rbx+KEYBOARD_CONTROLLER.scanning],0
    mov qword [rbx+KEYBOARD_CONTROLLER.ready],0
    mov qword [rbx+KEYBOARD_CONTROLLER.stage],11
    xor eax,eax
    jmp .return
.bad:
    mov qword [rbx+KEYBOARD_CONTROLLER.ready],0
    mov qword [rbx+KEYBOARD_CONTROLLER.failed],1
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; Each individual status wait is bounded; helpers preserve all nonvolatiles.
keyboard_wait_input:
    push rbx
    sub rsp,32
    mov ebx,KEYBOARD_POLL_BUDGET
.next:
    call keyboard_status
    test al,2
    jz .ok
    pause
    dec ebx
    jnz .next
    mov eax,1
    jmp .return
.ok:
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
keyboard_write_command:
    push rbx
    sub rsp,32
    mov ebx,ecx
    call keyboard_wait_input
    test eax,eax
    jnz .return
    mov ecx,ebx
    call keyboard_command_write
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
keyboard_write_data:
    push rbx
    sub rsp,32
    mov ebx,ecx
    call keyboard_wait_input
    test eax,eax
    jnz .return
    mov ecx,ebx
    call keyboard_data_write
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
keyboard_write_config:
    push rbx
    sub rsp,32
    mov ebx,ecx
    mov ecx,0x60
    call keyboard_write_command
    test eax,eax
    jnz .return
    mov ecx,ebx
    call keyboard_write_data
.return:
    add rsp,32
    pop rbx
    ret

; EAX0/EDX byte or EAX1 rejection. Never consumes non-OBF data.
keyboard_read_reply:
    push rbx
    sub rsp,32
    mov ebx,KEYBOARD_POLL_BUDGET
.next:
    call keyboard_status
    test al,0xe0
    jnz .bad
    test al,1
    jnz .read
    pause
    dec ebx
    jnz .next
.bad:
    mov eax,1
    xor edx,edx
    jmp .return
.read:
    call keyboard_data_read
    mov edx,eax
    xor eax,eax
.return:
    add rsp,32
    pop rbx
    ret
keyboard_flush_output:
    push rbx
    sub rsp,32
    mov ebx,32
.next:
    call keyboard_status
    test al,1
    jz .ok
    test ebx,ebx
    jz .bad
    call keyboard_data_read
    dec ebx
    jmp .next
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rbx
    ret

; ECX byte, RDX trusted controller, R8D1 permits <=32 pre-F5 inherited bytes.
; At most three FE retransmissions (four writes). No scanning data after F4 ACK read.
keyboard_send_byte:
    push rbx
    push rbp
    push r12
    push r13
    sub rsp,40
    mov ebx,ecx
    mov r12,rdx
    mov r13d,r8d
    xor ebp,ebp
.send:
    mov ecx,ebx
    call keyboard_write_data
    test eax,eax
    jnz .return
.reply:
    call keyboard_read_reply
    test eax,eax
    jnz .return
    cmp edx,0xfa
    je .ok
    cmp edx,0xfe
    je .resend
    cmp edx,0xfc
    je .bad
    cmp edx,0xfd
    je .bad
    test r13d,r13d
    jz .bad
    cmp qword [r12+KEYBOARD_CONTROLLER.discarded],32
    jae .bad
    inc qword [r12+KEYBOARD_CONTROLLER.discarded]
    jmp .reply
.resend:
    inc ebp
    cmp ebp,3
    jbe .send
.bad:
    mov eax,1
    jmp .return
.ok:
    xor eax,eax
.return:
    add rsp,40
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
