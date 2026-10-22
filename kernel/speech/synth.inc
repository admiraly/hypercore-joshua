; Rule-based cascade formant synthesizer (Klatt-style, simplified).
; Input: space-separated ARPAbet-like phonemes; a trailing 1 marks stress.
; Pauses: '_' word gap, ',' phrase break, '.' sentence end (resets pitch).
; Renders 16 kHz float samples, then 48 kHz 16-bit stereo PCM for HDA.

%define SPEECH_RATE         16000
%define SPEECH_FRAME        80
%define SPEECH_MAX_SAMPLES  131072
%define SPEECH_WORD_GAP     6
%define SPEECH_COMMA        30
%define SPEECH_PERIOD       70
%define SPEECH_TAIL         40
%define SPEECH_BURST        3
%define SPEECH_AFFRICATION  10
%define SPEECH_ASPIRATION   8
%define SPEECH_E_RENDER     10
%define SPEECH_E_SILENT     11
%define P_F1                0
%define P_F2                1
%define P_F3                2
%define P_FF                3
%define P_FB                4
%define P_AV                5
%define P_AH                6
%define P_AF                7
%define P_F0                8
%define P_COUNT             9
%define PH_PLOSIVE          1
%define PH_AFFRICATE        2
%define RES_A               0
%define RES_B               4
%define RES_C               8
%define RES_Y1              12
%define RES_Y2              16

; xmm0 = input -> xmm0 = output. Clobbers xmm1.
%macro resonate 1
    mulss xmm0,[%1+RES_A]
    movss xmm1,[%1+RES_B]
    mulss xmm1,[%1+RES_Y1]
    addss xmm0,xmm1
    movss xmm1,[%1+RES_C]
    mulss xmm1,[%1+RES_Y2]
    addss xmm0,xmm1
    movss xmm1,[%1+RES_Y1]
    movss [%1+RES_Y2],xmm1
    movss [%1+RES_Y1],xmm0
%endmacro

; rcx = phoneme string -> eax = 0, SPEECH_E_* or HDA_E_*
speech_say:
    push rbx
    push rsi
    push rdi
    stmxcsr [speech_mxcsr]
    or dword [speech_mxcsr],0x8000
    ldmxcsr [speech_mxcsr]
    call speech_render
    mov eax,SPEECH_E_RENDER
    jc .done
    mov ecx,[speech_samples]
    lea rsi,[speech_wave]
    xorps xmm1,xmm1
    movss xmm2,[abs_mask]
.peak:
    movss xmm0,[rsi]
    andps xmm0,xmm2
    maxss xmm1,xmm0
    add rsi,4
    dec ecx
    jnz .peak
    mov eax,SPEECH_E_SILENT
    comiss xmm1,[peak_floor]
    jb .done
    movss xmm3,[pcm_target]
    divss xmm3,xmm1
    mov ecx,[speech_samples]
    lea rsi,[speech_wave]
    lea rdi,[speech_pcm]
.upsample:
    movss xmm0,[rsi]
    xorps xmm1,xmm1
    cmp ecx,1
    je .last
    movss xmm1,[rsi+4]
.last:
    subss xmm1,xmm0
    mulss xmm1,[one_third]
    mulss xmm0,xmm3
    mulss xmm1,xmm3
    mov edx,3
.frame:
    cvtss2si eax,xmm0
    mov [rdi],ax
    mov [rdi+2],ax
    add rdi,4
    addss xmm0,xmm1
    dec edx
    jnz .frame
    add rsi,4
    dec ecx
    jnz .upsample
    lea rcx,[speech_pcm]
    mov rdx,rdi
    sub rdx,rcx
.pad:
    test edx,255
    jz .play
    mov dword [rdi],0
    add rdi,4
    add edx,4
    jmp .pad
.play:
    call hda_play
.done:
    pop rdi
    pop rsi
    pop rbx
    ret

; rcx = phoneme string -> speech_samples filled, CF on bad token or overflow
speech_render:
    push rbx
    push rsi
    push rdi
    push r12
    mov rsi,rcx
    call synth_reset
.token:
    movzx eax,byte [rsi]
    test eax,eax
    jz .finish
    inc rsi
    cmp al,' '
    je .token
    mov ecx,SPEECH_WORD_GAP
    cmp al,'_'
    je .pause
    mov ecx,SPEECH_COMMA
    cmp al,','
    je .pause
    cmp al,'.'
    jne .phoneme
    mov ecx,SPEECH_PERIOD
    call synth_pause
    jc .done
    movss xmm0,[f0_start]
    movss [f0_base],xmm0
    jmp .token
.pause:
    call synth_pause
    jc .done
    jmp .token
.phoneme:
    mov ebx,eax
    mov cl,8
    xor r12d,r12d
.name:
    movzx eax,byte [rsi]
    cmp al,'1'
    je .stress
    cmp al,'A'
    jb .lookup
    cmp al,'Z'
    ja .lookup
    cmp cl,24
    jae .bad
    shl eax,cl
    or ebx,eax
    add cl,8
    inc rsi
    jmp .name
.stress:
    mov r12d,1
    inc rsi
.lookup:
    lea rdi,[phoneme_table]
    lea rax,[phoneme_table_end]
.search:
    cmp rdi,rax
    jae .bad
    cmp [rdi+PHONEME.name],ebx
    je .found
    add rdi,PHONEME_size
    jmp .search
.found:
    mov rcx,rdi
    mov edx,r12d
    call synth_phoneme
    jc .done
    jmp .token
.bad:
    stc
    jmp .done
.finish:
    mov ecx,SPEECH_TAIL
    call synth_pause
.done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

synth_reset:
    mov dword [speech_samples],0
    lea rcx,[synth_initial]
    lea rdx,[synth_current]
    lea r8,[synth_target]
    xor eax,eax
.copy:
    mov r9d,[rcx+rax*4]
    mov [rdx+rax*4],r9d
    mov [r8+rax*4],r9d
    inc eax
    cmp eax,P_COUNT
    jb .copy
    lea rcx,[synth_state]
    lea rdx,[synth_state_end]
.clear:
    mov dword [rcx],0
    add rcx,4
    cmp rcx,rdx
    jb .clear
    mov dword [noise_seed],0x4a4f5348
    movss xmm0,[f0_start]
    movss [f0_base],xmm0
    lea rcx,[res_glottal]
    xorps xmm0,xmm0
    movss xmm1,[bw_glottal]
    xor r8d,r8d
    jmp res_set

; rcx = resonator, xmm0 = frequency Hz, xmm1 = bandwidth Hz,
; r8d = 0 for unity DC gain (cascade), 1 for unity peak gain (frication)
res_set:
    mov r10,rcx
    mulss xmm0,[two_pi_over_rate]
    mulss xmm0,xmm0
    movss xmm3,xmm1
    lea rcx,[cos_coefficients]
    mov edx,7
    call poly
    movss xmm4,xmm0
    movss xmm0,xmm3
    mulss xmm0,[minus_pi_over_rate]
    lea rcx,[exp_coefficients]
    mov edx,7
    call poly
    movss xmm2,xmm0
    mulss xmm2,xmm0
    movss xmm5,xmm0
    addss xmm5,xmm5
    mulss xmm5,xmm4
    movss [r10+RES_B],xmm5
    xorps xmm1,xmm1
    subss xmm1,xmm2
    movss [r10+RES_C],xmm1
    test r8d,r8d
    jnz .peak
    movss xmm1,[float_one]
    subss xmm1,xmm5
    addss xmm1,xmm2
    movss [r10+RES_A],xmm1
    ret
.peak:
    movss xmm1,xmm4
    mulss xmm1,xmm4
    addss xmm1,xmm1
    subss xmm1,[float_one]
    mulss xmm1,xmm0
    addss xmm1,xmm1
    movss xmm3,[float_one]
    subss xmm3,xmm1
    addss xmm3,xmm2
    sqrtss xmm3,xmm3
    movss xmm1,[float_one]
    subss xmm1,xmm0
    mulss xmm3,xmm1
    movss [r10+RES_A],xmm3
    ret

; xmm0 = x, rcx = coefficients (highest power first), edx = count -> xmm0
poly:
    movss xmm1,[rcx]
.next:
    add rcx,4
    dec edx
    jz .done
    mulss xmm1,xmm0
    addss xmm1,[rcx]
    jmp .next
.done:
    movaps xmm0,xmm1
    ret

; rsi = three formant words -> formant targets
synth_formants:
    lea rdx,[synth_target]
    xor ecx,ecx
.next:
    movzx eax,word [rsi+rcx*2]
    cvtsi2ss xmm0,eax
    movss [rdx+rcx*4],xmm0
    inc ecx
    cmp ecx,3
    jb .next
    ret

; ecx = voicing, edx = aspiration, r8d = frication -> amplitude targets
synth_amplitudes:
    cvtsi2ss xmm0,ecx
    movss [synth_target+P_AV*4],xmm0
    cvtsi2ss xmm0,edx
    movss [synth_target+P_AH*4],xmm0
    cvtsi2ss xmm0,r8d
    movss [synth_target+P_AF*4],xmm0
    ret

; ecx = frames of silence -> CF on overflow
synth_pause:
    push rbx
    mov ebx,ecx
    xor ecx,ecx
    xor edx,edx
    xor r8d,r8d
    call synth_amplitudes
    mov ecx,ebx
    call synth_frames
    pop rbx
    ret

; ecx = frame count -> CF on overflow
synth_frames:
    push rbx
    mov ebx,ecx
.next:
    test ebx,ebx
    jz .complete
    call synth_frame
    jc .done
    dec ebx
    jmp .next
.complete:
    clc
.done:
    pop rbx
    ret

; rcx = phoneme record, edx = 1 when stressed -> CF on overflow
synth_phoneme:
    push rbx
    push rsi
    push r12
    push r13
    push r14
    mov rbx,rcx
    mov r12d,edx
    lea rsi,[rbx+PHONEME.f1]
    call synth_formants
    movzx eax,word [rbx+PHONEME.ff]
    cvtsi2ss xmm0,eax
    movss [synth_target+P_FF*4],xmm0
    movzx eax,word [rbx+PHONEME.fb]
    cvtsi2ss xmm0,eax
    movss [synth_target+P_FB*4],xmm0
    movss xmm0,[f0_base]
    test r12d,r12d
    jz .pitch
    addss xmm0,[f0_stress]
.pitch:
    movss [synth_target+P_F0*4],xmm0
    movzx eax,word [rbx+PHONEME.duration]
    xor edx,edx
    mov ecx,5
    div ecx
    mov r13d,eax
    test r12d,r12d
    jz .length
    lea r13d,[r13+r13*2]
    shr r13d,1
.length:
    test word [rbx+PHONEME.flags],PH_PLOSIVE|PH_AFFRICATE
    jnz .plosive
    movzx ecx,word [rbx+PHONEME.av]
    movzx edx,word [rbx+PHONEME.ah]
    movzx r8d,word [rbx+PHONEME.af]
    call synth_amplitudes
    mov r14d,r13d
    shr r14d,1
.continuant:
    cmp r13d,r14d
    jne .emit
    lea rsi,[rbx+PHONEME.f1b]
    call synth_formants
.emit:
    call synth_frame
    jc .done
    dec r13d
    jnz .continuant
    clc
    jmp .done
.plosive:
    movzx ecx,word [rbx+PHONEME.av]
    xor edx,edx
    xor r8d,r8d
    call synth_amplitudes
    mov ecx,r13d
    call synth_frames
    jc .done
    movzx ecx,word [rbx+PHONEME.av]
    xor edx,edx
    movzx r8d,word [rbx+PHONEME.af]
    call synth_amplitudes
    mov ecx,SPEECH_BURST
    test word [rbx+PHONEME.flags],PH_AFFRICATE
    jz .release
    mov ecx,SPEECH_AFFRICATION
.release:
    call synth_frames
    jc .done
    movzx edx,word [rbx+PHONEME.ah]
    test edx,edx
    jz .done
    xor ecx,ecx
    xor r8d,r8d
    call synth_amplitudes
    mov ecx,SPEECH_ASPIRATION
    call synth_frames
.done:
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret

; Renders one 5 ms frame toward the current targets. CF when the buffer is full.
synth_frame:
    push rdi
    mov eax,[speech_samples]
    add eax,SPEECH_FRAME
    cmp eax,SPEECH_MAX_SAMPLES
    ja .full
    movss xmm0,[f0_base]
    subss xmm0,[f0_step]
    maxss xmm0,[f0_floor]
    movss [f0_base],xmm0
    lea rcx,[synth_current]
    lea rdx,[synth_target]
    lea r8,[synth_rate]
    mov r9d,P_COUNT
.smooth:
    movss xmm0,[rdx]
    subss xmm0,[rcx]
    mulss xmm0,[r8]
    addss xmm0,[rcx]
    movss [rcx],xmm0
    add rcx,4
    add rdx,4
    add r8,4
    dec r9d
    jnz .smooth
    lea rcx,[res_f1]
    movss xmm0,[synth_current+P_F1*4]
    movss xmm1,[bw_f1]
    xor r8d,r8d
    call res_set
    lea rcx,[res_f2]
    movss xmm0,[synth_current+P_F2*4]
    movss xmm1,[bw_f2]
    xor r8d,r8d
    call res_set
    lea rcx,[res_f3]
    movss xmm0,[synth_current+P_F3*4]
    movss xmm1,[bw_f3]
    xor r8d,r8d
    call res_set
    lea rcx,[res_fric]
    movss xmm0,[synth_current+P_FF*4]
    movss xmm1,[synth_current+P_FB*4]
    mov r8d,1
    call res_set
    movss xmm0,[synth_current+P_AV*4]
    mulss xmm0,[voice_gain]
    movss [voice_amp],xmm0
    movss xmm0,[synth_current+P_AH*4]
    mulss xmm0,[aspiration_gain]
    movss [aspiration_amp],xmm0
    movss xmm0,[synth_current+P_AF*4]
    mulss xmm0,[frication_gain]
    movss [frication_amp],xmm0
    movss xmm0,[synth_current+P_F0*4]
    mulss xmm0,[inverse_rate]
    movss [phase_step],xmm0
    mov eax,[speech_samples]
    lea rdi,[speech_wave]
    lea rdi,[rdi+rax*4]
    add eax,SPEECH_FRAME
    mov [speech_samples],eax
    mov ecx,SPEECH_FRAME
.sample:
    imul eax,[noise_seed],1664525
    add eax,1013904223
    mov [noise_seed],eax
    cvtsi2ss xmm2,eax
    mulss xmm2,[noise_scale]
    movss xmm0,[glottal_phase]
    addss xmm0,[phase_step]
    xorps xmm3,xmm3
    comiss xmm0,[float_one]
    jb .no_pulse
    subss xmm0,[float_one]
    movss xmm3,[voice_amp]
.no_pulse:
    movss [glottal_phase],xmm0
    movaps xmm0,xmm3
    resonate res_glottal
    movss xmm1,xmm2
    mulss xmm1,[aspiration_amp]
    addss xmm0,xmm1
    resonate res_f1
    resonate res_f2
    resonate res_f3
    movss xmm4,xmm0
    movss xmm0,xmm2
    mulss xmm0,[frication_amp]
    resonate res_fric
    addss xmm0,xmm4
    movss xmm1,xmm0
    subss xmm0,[radiation_prev]
    movss [radiation_prev],xmm1
    movss [rdi],xmm0
    add rdi,4
    dec ecx
    jnz .sample
    pop rdi
    clc
    ret
.full:
    pop rdi
    stc
    ret
