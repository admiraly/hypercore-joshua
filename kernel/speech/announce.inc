; Speaks the boot checkpoint. Speech is optional: every failure is reported
; on serial and screen, and boot continues to the checkpoint halt.
speech_checkpoint:
    push rbx
    sub rsp,32
    call hda_init
    mov ebx,eax
    lea rcx,[debug_speech_unavailable]
    lea rdx,[screen_speech_unavailable]
    test eax,eax
    jnz .report
    lea rcx,[speech_checkpoint_phrase]
    call speech_say
    mov ebx,eax
    lea rcx,[debug_speech_failed]
    lea rdx,[screen_speech_failed]
    test eax,eax
    jnz .report
    lea rcx,[debug_speech_ok]
    lea rdx,[screen_speech_ok]
.report:
    mov [rsp+24],rdx
    mov eax,ebx
    xor edx,edx
    mov r8d,10
    div r8d
    add al,'0'
    add dl,'0'
    mov [debug_speech_code],al
    mov [debug_speech_code+1],dl
    call debug_write
    lea rcx,[debug_speech_status]
    call debug_write
    mov rcx,[rsp+24]
    mov edx,224
    mov r8d,0x00e0e0e0
    call draw_line
    add rsp,32
    pop rbx
    ret
