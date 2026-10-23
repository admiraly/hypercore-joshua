; Pure BSP/IF0 policy over caller-owned216-byte storage. DF0, Microsoft x64 ABI.
; Init returns0, no allocation/pointers supplied by input bytes.
keyboard_decoder_init:
    push rdi
    mov rdi,rcx
    xor eax,eax
    mov ecx,KEYBOARD_DECODER_size/8
    rep stosq
    mov rax,KEYBOARD_DECODER_MAGIC
    mov [rdi-KEYBOARD_DECODER_size+KEYBOARD_DECODER.ready],rax
    xor eax,eax
    pop rdi
    ret

; RCX state -> EAX0 valid,1 corrupt. Read-only, no queue-byte access.
keyboard_decoder_check:
    mov rax,KEYBOARD_DECODER_MAGIC
    cmp [rcx+KEYBOARD_DECODER.ready],rax
    jne .bad
    cmp qword [rcx+KEYBOARD_DECODER.modifiers],63
    ja .bad
    cmp qword [rcx+KEYBOARD_DECODER.caps],1
    ja .bad
    cmp qword [rcx+KEYBOARD_DECODER.caps_down],1
    ja .bad
    cmp qword [rcx+KEYBOARD_DECODER.prefix],1
    ja .bad
    cmp qword [rcx+KEYBOARD_DECODER.pause_index],5
    ja .bad
    cmp qword [rcx+KEYBOARD_DECODER.pause_index],0
    je .queue
    cmp qword [rcx+KEYBOARD_DECODER.prefix],0
    jne .bad
.queue:
    mov rax,[rcx+KEYBOARD_DECODER.head]
    cmp rax,KEYBOARD_QUEUE_CAPACITY
    jae .bad
    mov rdx,[rcx+KEYBOARD_DECODER.count]
    cmp rdx,KEYBOARD_QUEUE_CAPACITY
    ja .bad
    add rax,rdx
    and eax,KEYBOARD_QUEUE_CAPACITY-1
    cmp rax,[rcx+KEYBOARD_DECODER.tail]
    jne .bad
    xor eax,eax
    ret
.bad:
    mov eax,1
    ret

; RCX state, EDX raw byte -> EAX0 handled/ignored,1 corrupt metadata/input width.
; Breaks never emit events. Unknown keys are ignored; protocol errors saturate.
keyboard_decode:
    push rbx
    push rbp
    push rsi
    sub rsp,32
    mov rbx,rcx
    mov esi,edx
    call keyboard_decoder_check
    test eax,eax
    jnz .return
    cmp esi,255
    ja .bad
    mov rax,[rbx+KEYBOARD_DECODER.pause_index]
    test rax,rax
    jz .prefix
    lea rdx,[keyboard_pause_suffix]
    cmp sil,[rdx+rax-1]
    jne .resync
    inc rax
    cmp eax,6
    jb .pause_store
    xor eax,eax
.pause_store:
    mov [rbx+KEYBOARD_DECODER.pause_index],rax
    jmp .ok
.prefix:
    cmp esi,0xe1
    je .pause
    cmp esi,0xe0
    je .extended
    cmp esi,0xfa
    je .protocol
    cmp esi,0xfc
    je .protocol
    cmp esi,0xfd
    je .protocol
    cmp esi,0xfe
    je .protocol
    test esi,esi
    jz .protocol
    cmp esi,255
    je .protocol
    mov ebp,esi
    and ebp,0x7f
    cmp qword [rbx+KEYBOARD_DECODER.prefix],0
    jne .extended_key
    cmp ebp,0x2a
    je .left_shift
    cmp ebp,0x36
    je .right_shift
    cmp ebp,0x1d
    je .left_control
    cmp ebp,0x38
    je .left_alt
    cmp ebp,0x3a
    je .caps
    test esi,0x80
    jnz .ok
    lea rax,[keyboard_plain]
    movzx edx,byte [rax+rbp]
    test edx,edx
    jz .ok
    test qword [rbx+KEYBOARD_DECODER.modifiers],48
    jnz .ok
    cmp edx,'a'
    jb .punctuation
    cmp edx,'z'
    ja .punctuation
    test qword [rbx+KEYBOARD_DECODER.modifiers],12
    jnz .control_text
    mov rax,[rbx+KEYBOARD_DECODER.modifiers]
    and eax,3
    setnz al
    movzx eax,al
    xor rax,[rbx+KEYBOARD_DECODER.caps]
    test eax,eax
    jz .emit
    sub edx,32
    jmp .emit
.control_text:
    and edx,31
    jmp .emit
.punctuation:
    test qword [rbx+KEYBOARD_DECODER.modifiers],12
    jnz .ok                    ; Ctrl only has a letter mapping in this profile.
    test qword [rbx+KEYBOARD_DECODER.modifiers],3
    jz .emit
    lea rax,[keyboard_shifted]
    movzx edx,byte [rax+rbp]
    jmp .emit
.extended_key:
    mov qword [rbx+KEYBOARD_DECODER.prefix],0
    cmp ebp,0x1d
    je .right_control
    cmp ebp,0x38
    je .right_alt
    test esi,0x80
    jnz .ok
    test qword [rbx+KEYBOARD_DECODER.modifiers],60
    jnz .ok
    cmp ebp,0x1c
    je .enter
    cmp ebp,0x35
    je .slash
    mov edx,KEY_LEFT
    cmp ebp,0x4b
    je .emit
    mov edx,KEY_RIGHT
    cmp ebp,0x4d
    je .emit
    mov edx,KEY_HOME
    cmp ebp,0x47
    je .emit
    mov edx,KEY_END
    cmp ebp,0x4f
    je .emit
    mov edx,KEY_DELETE
    cmp ebp,0x53
    je .emit
    mov edx,KEY_UP
    cmp ebp,0x48
    je .emit
    mov edx,KEY_DOWN
    cmp ebp,0x50
    je .emit
    jmp .ok                    ; PrintScreen's fake shifts never change state.
.enter:
    mov edx,13
    jmp .emit
.slash:
    mov edx,'/'
    jmp .emit
.left_shift:
    mov edx,1
    jmp .modifier
.right_shift:
    mov edx,2
    jmp .modifier
.left_control:
    mov edx,4
    jmp .modifier
.right_control:
    mov edx,8
    jmp .modifier
.left_alt:
    mov edx,16
    jmp .modifier
.right_alt:
    mov edx,32
.modifier:
    test esi,0x80
    jnz .release
    or [rbx+KEYBOARD_DECODER.modifiers],rdx
    jmp .ok
.release:
    not rdx
    and [rbx+KEYBOARD_DECODER.modifiers],rdx
    jmp .ok
.caps:
    test esi,0x80
    jnz .caps_release
    cmp qword [rbx+KEYBOARD_DECODER.caps_down],0
    jne .ok
    xor qword [rbx+KEYBOARD_DECODER.caps],1
    mov qword [rbx+KEYBOARD_DECODER.caps_down],1
    jmp .ok
.caps_release:
    mov qword [rbx+KEYBOARD_DECODER.caps_down],0
    jmp .ok
.pause:
    cmp qword [rbx+KEYBOARD_DECODER.prefix],0
    jne .resync
    mov qword [rbx+KEYBOARD_DECODER.pause_index],1
    jmp .ok
.extended:
    cmp qword [rbx+KEYBOARD_DECODER.prefix],0
    jne .resync
    mov qword [rbx+KEYBOARD_DECODER.prefix],1
    jmp .ok
.resync:
    mov qword [rbx+KEYBOARD_DECODER.modifiers],0
    mov qword [rbx+KEYBOARD_DECODER.caps_down],0
.protocol:
    mov qword [rbx+KEYBOARD_DECODER.prefix],0
    mov qword [rbx+KEYBOARD_DECODER.pause_index],0
    cmp qword [rbx+KEYBOARD_DECODER.protocol_errors],-1
    je .ok
    inc qword [rbx+KEYBOARD_DECODER.protocol_errors]
    jmp .ok
.emit:
    test edx,edx
    jz .ok
    cmp qword [rbx+KEYBOARD_DECODER.count],KEYBOARD_QUEUE_CAPACITY
    jae .overflow
    mov rcx,[rbx+KEYBOARD_DECODER.tail]
    mov [rbx+KEYBOARD_DECODER.events+rcx*2],dx
    inc ecx
    and ecx,KEYBOARD_QUEUE_CAPACITY-1
    mov [rbx+KEYBOARD_DECODER.tail],rcx
    inc qword [rbx+KEYBOARD_DECODER.count]
    jmp .ok
.overflow:
    cmp qword [rbx+KEYBOARD_DECODER.drops],-1
    je .ok
    inc qword [rbx+KEYBOARD_DECODER.drops]
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    add rsp,32
    pop rsi
    pop rbp
    pop rbx
    ret

; RCX state -> EAX0/EDX event, EAX2 empty, EAX1 corrupt. No output pointer.
keyboard_pop:
    push rbx
    sub rsp,32
    mov rbx,rcx
    call keyboard_decoder_check
    test eax,eax
    jnz .bad
    cmp qword [rbx+KEYBOARD_DECODER.count],0
    je .empty
    mov rcx,[rbx+KEYBOARD_DECODER.head]
    movzx edx,word [rbx+KEYBOARD_DECODER.events+rcx*2]
    inc ecx
    and ecx,KEYBOARD_QUEUE_CAPACITY-1
    mov [rbx+KEYBOARD_DECODER.head],rcx
    dec qword [rbx+KEYBOARD_DECODER.count]
    xor eax,eax
    jmp .return
.empty:
    mov eax,2
.bad:
    xor edx,edx
.return:
    add rsp,32
    pop rbx
    ret


; RCX valid state -> EAX0 reset transient/modifier state after transport corruption.
; Preserve queued events and caps setting; corrupt metadata is left unchanged.
keyboard_decoder_resync:
    push rbx
    sub rsp,32
    mov rbx,rcx
    call keyboard_decoder_check
    test eax,eax
    jnz .return
    mov qword [rbx+KEYBOARD_DECODER.modifiers],0
    mov qword [rbx+KEYBOARD_DECODER.caps_down],0
    mov qword [rbx+KEYBOARD_DECODER.prefix],0
    mov qword [rbx+KEYBOARD_DECODER.pause_index],0
    cmp qword [rbx+KEYBOARD_DECODER.protocol_errors],-1
    je .return
    inc qword [rbx+KEYBOARD_DECODER.protocol_errors]
.return:
    add rsp,32
    pop rbx
    ret
