; Speaks the boot checkpoint. Speech is optional: every failure is reported
; on serial and screen, and boot continues to the checkpoint halt.
speech_checkpoint:
    push rbx
    sub rsp,48
    call hda_init
    mov ebx,eax
    lea rcx,[debug_speech_unavailable]
    lea rdx,[screen_speech_unavailable]
    test eax,eax
    jnz .report
%ifdef JOSHUA_PIPER_PCM
    call piper_announce
%else
    lea rcx,[speech_checkpoint_phrase]
    call speech_say
%endif
    mov ebx,eax
    lea rcx,[debug_speech_failed]
    lea rdx,[screen_speech_failed]
    test eax,eax
    jnz .report
    lea rcx,[debug_speech_ok]
    lea rdx,[screen_speech_ok]
.report:
    mov [rsp+32],rdx
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
    mov rcx,[rsp+32]
    mov edx,224
    mov r8d,0x00e0e0e0
    call draw_line
    add rsp,48
    pop rbx
    ret

%ifdef JOSHUA_PIPER_PCM
; Host-rendered, compile-time bounded 48 kHz signed 16-bit stereo PCM.
; Copy into the existing aligned DMA region; no inference executes in the OS.
piper_announce:
    push rsi
    push rdi
    sub rsp,40
    lea rcx,[debug_piper_pcm]
    call debug_write
    cld
    lea rsi,[piper_clip]
    lea rdi,[speech_pcm]
    mov ecx,(piper_clip_end-piper_clip)/8
    rep movsq
    lea rcx,[speech_pcm]
    mov edx,piper_clip_end-piper_clip
    call hda_play
    add rsp,40
    pop rdi
    pop rsi
    ret
%endif
