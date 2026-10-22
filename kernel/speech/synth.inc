; Formant speech synthesizer after Klatt (1979 durations, 1980 synthesizer).
; Input: space-separated ARPAbet-style phonemes; suffix 1 = primary stress,
; 2 = secondary stress. '_' ends a word, ',' is a phrase break, '.' ends a
; sentence. Renders 48 kHz floats with:
;   - KLGLOTT88 glottal flow with tilt, jitter, shimmer and breathiness
;   - cascade nasal zero/pole + five formants for voiced and aspirated sound
;   - parallel formants + bypass shaped frication and bursts
;   - rule-based durations, hat-pattern pitch and locus coarticulation
; Amplitudes and pitch ramp per sample; filters update every 5 ms frame.

%define SPEECH_RATE         48000
%define SPEECH_FRAME        240
%define SPEECH_MAX_SAMPLES  393216
%define SPEECH_MAX_ENTRIES  256
%define SPEECH_COMMA        40
%define SPEECH_PERIOD       70
%define SPEECH_TAIL         40
%define SPEECH_BURST        2
%define SPEECH_ASPIRATION   10
%define SPEECH_AFFRICATION  12
%define SPEECH_E_RENDER     10
%define SPEECH_E_SILENT     11
%define P_F1                0
%define P_F2                1
%define P_F3                2
%define P_B1                3
%define P_FNZ               6
%define P_F0                7
%define P_AV                8
%define P_AH                9
%define P_AF                10
%define P_FX                17
%define P_COUNT             19
%define P_AMPS              9
%define PH_VOWEL            1
%define PH_STOP             2
%define PH_AFFRICATE        4
%define PH_NASAL            8
%define PH_LABIAL           16
%define PH_ALVEOLAR         32
%define PH_VELAR            64
%define PH_VOICED           128
%define PH_ASPIRATE         256
%define RES_A               0
%define RES_B               4
%define RES_C               8
%define RES_Y1              12
%define RES_Y2              16

struc ENTRY
 .phoneme:   resq 1
 .frames:    resd 1
 .stress:    resb 1
 .boundary:  resb 1
 .reserved:  resw 1
 .f0a:       resd 1
 .f0b:       resd 1
 .gain:      resd 1
 .unused:    resd 1
endstruc

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

; Antiresonator (zero pair): history holds inputs. Clobbers xmm1, xmm3.
%macro antiresonate 1
    movss xmm3,xmm0
    mulss xmm0,[%1+RES_A]
    movss xmm1,[%1+RES_B]
    mulss xmm1,[%1+RES_Y1]
    addss xmm0,xmm1
    movss xmm1,[%1+RES_C]
    mulss xmm1,[%1+RES_Y2]
    addss xmm0,xmm1
    movss xmm1,[%1+RES_Y1]
    movss [%1+RES_Y2],xmm1
    movss [%1+RES_Y1],xmm3
%endmacro

; rcx = phoneme string -> eax = 0, SPEECH_E_* or HDA_E_*
speech_say:
    push rbx
    push rsi
    push rdi
    stmxcsr [speech_mxcsr]
    or dword [speech_mxcsr],0x8000
    ldmxcsr [speech_mxcsr]
    call speech_parse
    jc .render_failure
    call speech_timing
    call speech_pitch
    call speech_render
    jc .render_failure
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
.convert:
    movss xmm0,[rsi]
    mulss xmm0,xmm3
    cvtss2si eax,xmm0
    mov [rdi],ax
    mov [rdi+2],ax
    add rsi,4
    add rdi,4
    dec ecx
    jnz .convert
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
    jmp .done
.render_failure:
    mov eax,SPEECH_E_RENDER
.done:
    pop rdi
    pop rsi
    pop rbx
    ret

; rdi = next free entry, r12d = entry count, edx = boundary kind.
; Raises the boundary after the previous phoneme.
speech_mark:
    test r12d,r12d
    jz .done
    cmp qword [rdi-ENTRY_size+ENTRY.phoneme],0
    je .done
    cmp [rdi-ENTRY_size+ENTRY.boundary],dl
    jae .done
    mov [rdi-ENTRY_size+ENTRY.boundary],dl
.done:
    ret

; rdi = entry -> zeroed
%macro clear_entry 0
    mov qword [rdi],0
    mov qword [rdi+8],0
    mov qword [rdi+16],0
    mov qword [rdi+24],0
%endmacro

; rcx = phoneme string -> speech_entries, CF on a bad token or too many entries
speech_parse:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rsi,rcx
    lea rdi,[speech_entries]
    xor r12d,r12d
.token:
    movzx eax,byte [rsi]
    test eax,eax
    jz .finish
    inc rsi
    cmp al,' '
    je .token
    mov edx,1
    cmp al,'_'
    je .word
    mov edx,2
    mov ecx,SPEECH_COMMA
    cmp al,','
    je .pause
    mov edx,3
    mov ecx,SPEECH_PERIOD
    cmp al,'.'
    je .pause
    mov ebx,eax
    mov cl,8
    xor r13d,r13d
.name:
    movzx eax,byte [rsi]
    cmp al,'1'
    je .primary
    cmp al,'2'
    je .secondary
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
.primary:
    mov r13d,1
    inc rsi
    jmp .lookup
.secondary:
    mov r13d,2
    inc rsi
.lookup:
    lea rax,[phoneme_table]
    lea rcx,[phoneme_table_end]
.search:
    cmp rax,rcx
    jae .bad
    cmp [rax+PHONEME.name],ebx
    je .found
    add rax,PHONEME_size
    jmp .search
.found:
    cmp r12d,SPEECH_MAX_ENTRIES-1
    jae .bad
    clear_entry
    mov [rdi+ENTRY.phoneme],rax
    mov [rdi+ENTRY.stress],r13b
    add rdi,ENTRY_size
    inc r12d
    jmp .token
.word:
    call speech_mark
    jmp .token
.pause:
    call speech_mark
    cmp r12d,SPEECH_MAX_ENTRIES-1
    jae .bad
    clear_entry
    mov [rdi+ENTRY.frames],ecx
    mov [rdi+ENTRY.boundary],dl
    add rdi,ENTRY_size
    inc r12d
    jmp .token
.finish:
    mov edx,3
    call speech_mark
    clear_entry
    mov dword [rdi+ENTRY.frames],SPEECH_TAIL
    mov byte [rdi+ENTRY.boundary],3
    inc r12d
    mov [speech_entry_count],r12d
    clc
    jmp .done
.bad:
    stc
.done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; rbx = entry, r13d = index, r12d = count
; -> eax = 1 if a vowel follows within the word, edx = boundary ending the word
speech_word_scan:
    mov rcx,rbx
    mov r8d,r13d
    xor eax,eax
.scan:
    movzx edx,byte [rcx+ENTRY.boundary]
    test edx,edx
    jnz .done
    add rcx,ENTRY_size
    inc r8d
    cmp r8d,r12d
    jae .end
    mov r9,[rcx+ENTRY.phoneme]
    test r9,r9
    jz .scan
    test word [r9+PHONEME.flags],PH_VOWEL
    jz .scan
    mov eax,1
    jmp .scan
.end:
    mov edx,3
.done:
    ret

; Klatt-style multiplicative duration rules -> ENTRY.frames and ENTRY.gain
speech_timing:
    push rbx
    push rsi
    push r12
    push r13
    push r14
    push r15
    lea rbx,[speech_entries]
    mov r12d,[speech_entry_count]
    xor r13d,r13d
.entry:
    cmp r13d,r12d
    jae .done
    mov rsi,[rbx+ENTRY.phoneme]
    test rsi,rsi
    jz .next
    movzx r14d,word [rsi+PHONEME.flags]
    movss xmm6,[float_one]
    movss xmm0,[float_one]
    movss [rbx+ENTRY.gain],xmm0
    call speech_word_scan
    mov r15d,edx
    test r14d,PH_VOWEL
    jz .consonant
    cmp byte [rbx+ENTRY.stress],0
    jne .vowel_position
    mulss xmm6,[duration_unstressed]
    movss xmm0,[gain_unstressed]
    movss [rbx+ENTRY.gain],xmm0
.vowel_position:
    test eax,eax
    jz .last_syllable
    mulss xmm6,[duration_nonfinal]
    jmp .vowel_context
.last_syllable:
    cmp r15d,2
    jb .vowel_context
    mulss xmm6,[duration_phrase_final]
.vowel_context:
    cmp byte [rbx+ENTRY.boundary],0
    jne .duration
    mov rcx,[rbx+ENTRY_size+ENTRY.phoneme]
    test rcx,rcx
    jz .duration
    test word [rcx+PHONEME.flags],PH_VOWEL|PH_VOICED
    jnz .duration
    mulss xmm6,[duration_before_voiceless]
    jmp .duration
.consonant:
    cmp byte [rbx+ENTRY.boundary],0
    jne .previous
    mov rcx,[rbx+ENTRY_size+ENTRY.phoneme]
    test rcx,rcx
    jz .previous
    test word [rcx+PHONEME.flags],PH_VOWEL
    jz .cluster
.previous:
    test r13d,r13d
    jz .consonant_final
    cmp byte [rbx-ENTRY_size+ENTRY.boundary],0
    jne .consonant_final
    mov rcx,[rbx-ENTRY_size+ENTRY.phoneme]
    test rcx,rcx
    jz .consonant_final
    test word [rcx+PHONEME.flags],PH_VOWEL
    jnz .consonant_final
.cluster:
    mulss xmm6,[duration_cluster]
.consonant_final:
    test eax,eax
    jnz .duration
    cmp r15d,2
    jb .duration
    mulss xmm6,[duration_final_consonant]
.duration:
    movzx eax,word [rsi+PHONEME.inherent]
    movzx ecx,word [rsi+PHONEME.minimum]
    sub eax,ecx
    cvtsi2ss xmm0,eax
    mulss xmm0,xmm6
    cvtsi2ss xmm1,ecx
    addss xmm0,xmm1
    mulss xmm0,[frames_per_ms]
    cvtss2si eax,xmm0
    cmp eax,2
    jge .store
    mov eax,2
.store:
    mov [rbx+ENTRY.frames],eax
.next:
    add rbx,ENTRY_size
    inc r13d
    jmp .entry
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret

; rbx = entry -> eax = frames including stop release phases
entry_frames:
    mov eax,[rbx+ENTRY.frames]
    mov rcx,[rbx+ENTRY.phoneme]
    test rcx,rcx
    jz .done
    movzx edx,word [rcx+PHONEME.flags]
    test edx,PH_AFFRICATE
    jz .stop
    add eax,SPEECH_AFFRICATION
    ret
.stop:
    test edx,PH_STOP
    jz .done
    add eax,SPEECH_BURST
    test edx,PH_VOICED
    jnz .done
    add eax,SPEECH_ASPIRATION
.done:
    ret

; Hat-pattern intonation over a declining baseline, per sentence.
speech_pitch:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    lea rsi,[speech_entries]
    mov r12d,[speech_entry_count]
    xor r13d,r13d
.sentence:
    cmp r13d,r12d
    jae .done
    mov rdi,rsi
    mov r14d,r13d
    xor r15d,r15d
    mov qword [pitch_last_stress],0
.measure:
    cmp r14d,r12d
    jae .measured
    cmp qword [rdi+ENTRY.phoneme],0
    jne .count
    cmp byte [rdi+ENTRY.boundary],3
    je .measured
.count:
    mov rbx,rdi
    call entry_frames
    add r15d,eax
    mov rcx,[rdi+ENTRY.phoneme]
    test rcx,rcx
    jz .advance
    cmp byte [rdi+ENTRY.stress],1
    jne .advance
    test word [rcx+PHONEME.flags],PH_VOWEL
    jz .advance
    mov [pitch_last_stress],rdi
.advance:
    add rdi,ENTRY_size
    inc r14d
    jmp .measure
.measured:
    mov eax,r15d
    test eax,eax
    jnz .slope
    mov eax,1
.slope:
    cvtsi2ss xmm0,eax
    movss xmm7,[f0_top]
    subss xmm7,[f0_bottom]
    divss xmm7,xmm0
    xorps xmm6,xmm6
    mov rbx,rsi
    xor r15d,r15d
.assign:
    cmp rbx,rdi
    jae .sentence_end
    call entry_frames
    cvtsi2ss xmm1,eax
    movss xmm2,xmm1
    mulss xmm2,[float_half]
    addss xmm2,xmm6
    mulss xmm2,xmm7
    movss xmm0,[f0_top]
    subss xmm0,xmm2
    addss xmm6,xmm1
    movss xmm2,xmm6
    mulss xmm2,xmm7
    movss xmm5,[f0_top]
    subss xmm5,xmm2
    mov rcx,[rbx+ENTRY.phoneme]
    test r15d,r15d
    jz .accent
    subss xmm0,[f0_tail]
    subss xmm5,[f0_tail]
    jmp .store
.accent:
    test rcx,rcx
    jz .store
    test word [rcx+PHONEME.flags],PH_VOWEL
    jz .continuation
    movzx eax,byte [rbx+ENTRY.stress]
    cmp eax,1
    jne .secondary
    cmp rbx,[pitch_last_stress]
    jne .rise
    addss xmm0,[f0_peak]
    subss xmm5,[f0_fall]
    mov r15d,1
    jmp .store
.rise:
    addss xmm0,[f0_rise]
    addss xmm5,[f0_peak]
    jmp .continuation
.secondary:
    cmp eax,2
    jne .continuation
    addss xmm0,[f0_secondary]
    addss xmm5,[f0_secondary]
.continuation:
    cmp byte [rbx+ENTRY.boundary],2
    jne .store
    addss xmm5,[f0_continuation]
.store:
    movss [rbx+ENTRY.f0a],xmm0
    movss [rbx+ENTRY.f0b],xmm5
    add rbx,ENTRY_size
    jmp .assign
.sentence_end:
    cmp r14d,r12d
    jae .done
    movss xmm0,[f0_bottom]
    subss xmm0,[f0_tail]
    movss [rdi+ENTRY.f0a],xmm0
    movss [rdi+ENTRY.f0b],xmm0
    add rdi,ENTRY_size
    inc r14d
    mov rsi,rdi
    mov r13d,r14d
    jmp .sentence
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; -> speech_wave / speech_samples, CF on buffer overflow
speech_render:
    push rbx
    push r12
    call synth_reset
    lea rbx,[speech_entries]
    xor r12d,r12d
.entry:
    cmp r12d,[speech_entry_count]
    jae .complete
    call synth_entry
    jc .done
    add rbx,ENTRY_size
    inc r12d
    jmp .entry
.complete:
    clc
.done:
    pop r12
    pop rbx
    ret

synth_reset:
    mov dword [speech_samples],0
    lea rcx,[synth_initial]
    lea rdx,[synth_current]
    xor eax,eax
.copy:
    mov r8d,[rcx+rax*4]
    mov [rdx+rax*4],r8d
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
    mov eax,[synth_initial+P_F0*4]
    mov [f0_now],eax
    mov eax,[float_one]
    mov [jitter],eax
    mov [shimmer],eax
    lea rcx,[res_np]
    movss xmm0,[nasal_pole]
    movss xmm1,[nasal_bandwidth]
    xor r8d,r8d
    call res_set
    lea rcx,[res_c4]
    movss xmm0,[f4_frequency]
    movss xmm1,[f4_bandwidth]
    xor r8d,r8d
    call res_set
    lea rcx,[res_c5]
    movss xmm0,[f5_frequency]
    movss xmm1,[f5_bandwidth]
    xor r8d,r8d
    call res_set
    lea rcx,[res_p4]
    movss xmm0,[f4_frequency]
    movss xmm1,[f4_bandwidth]
    mov r8d,1
    call res_set
    lea rcx,[res_p5]
    movss xmm0,[f5_frequency]
    movss xmm1,[f5_bandwidth]
    mov r8d,1
    jmp res_set

; rcx = resonator, xmm0 = frequency Hz, xmm1 = bandwidth Hz,
; r8d = 0 for unity DC gain (cascade), 1 for unity peak gain (parallel).
; Preserves r10 = resonator for callers.
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

; rcx = antiresonator, xmm0 = frequency Hz, xmm1 = bandwidth Hz
anti_set:
    xor r8d,r8d
    call res_set
    movss xmm0,[float_one]
    divss xmm0,[r10+RES_A]
    movss xmm1,[r10+RES_B]
    mulss xmm1,xmm0
    xorps xmm2,xmm2
    subss xmm2,xmm1
    movss [r10+RES_B],xmm2
    movss xmm1,[r10+RES_C]
    mulss xmm1,xmm0
    xorps xmm2,xmm2
    subss xmm2,xmm1
    movss [r10+RES_C],xmm2
    movss [r10+RES_A],xmm0
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

; rcx = phoneme record -> goal F1-F3 and B1-B3
synth_set_formants:
    lea rdx,[synth_goal]
    xor eax,eax
.next:
    movzx r8d,word [rcx+PHONEME.f1+rax*2]
    cvtsi2ss xmm0,r8d
    movss [rdx+rax*4],xmm0
    inc eax
    cmp eax,6
    jb .next
    ret

; rsi = phoneme record, rbx = entry -> goal amplitudes
synth_set_amplitudes:
    lea rdx,[synth_goal+P_AV*4]
    xor eax,eax
.next:
    movzx r8d,word [rsi+PHONEME.av+rax*2]
    cvtsi2ss xmm0,r8d
    mulss xmm0,[milli]
    movss [rdx+rax*4],xmm0
    inc eax
    cmp eax,P_AMPS
    jb .next
    movss xmm0,[rdx]
    mulss xmm0,[rbx+ENTRY.gain]
    movss [rdx],xmm0
    ret

synth_clear_amplitudes:
    lea rdx,[synth_goal+P_AV*4]
    xor eax,eax
.next:
    mov dword [rdx+rax*4],0
    inc eax
    cmp eax,P_AMPS
    jb .next
    ret

; rbx = entry, r12d = index -> rax = nearest vowel record in the phrase
; (following first, then preceding), else the entry's own record
synth_vowel_reference:
    mov rcx,rbx
    mov edx,r12d
.forward:
    add rcx,ENTRY_size
    inc edx
    cmp edx,[speech_entry_count]
    jae .backward_start
    mov rax,[rcx+ENTRY.phoneme]
    test rax,rax
    jz .backward_start
    test word [rax+PHONEME.flags],PH_VOWEL
    jnz .found
    jmp .forward
.backward_start:
    mov rcx,rbx
    mov edx,r12d
.backward:
    test edx,edx
    jz .self
    sub rcx,ENTRY_size
    dec edx
    mov rax,[rcx+ENTRY.phoneme]
    test rax,rax
    jz .self
    test word [rax+PHONEME.flags],PH_VOWEL
    jnz .found
    jmp .backward
.self:
    mov rax,[rbx+ENTRY.phoneme]
.found:
    ret

; r13d = flags, r14 = vowel record -> goal F2/F3 from place loci (F1 for stops)
synth_loci:
    lea rcx,[speech_loci]
    test r13d,PH_LABIAL
    jnz .place
    add rcx,16
    test r13d,PH_ALVEOLAR
    jnz .place
    add rcx,16
.place:
    movzx eax,word [r14+PHONEME.f2]
    cvtsi2ss xmm0,eax
    subss xmm0,[rcx]
    mulss xmm0,[rcx+4]
    addss xmm0,[rcx]
    movss [synth_goal+P_F2*4],xmm0
    movzx eax,word [r14+PHONEME.f3]
    cvtsi2ss xmm0,eax
    subss xmm0,[rcx+8]
    mulss xmm0,[rcx+12]
    addss xmm0,[rcx+8]
    movss [synth_goal+P_F3*4],xmm0
    test r13d,PH_STOP
    jz .done
    movss xmm0,[stop_f1]
    movss [synth_goal+P_F1*4],xmm0
.done:
    ret

; rbx = entry, r12d = index -> CF on overflow
synth_entry:
    push rsi
    push rdi
    push r13
    push r14
    push r15
    mov rsi,[rbx+ENTRY.phoneme]
    lea rcx,[synth_current]
    lea rdi,[synth_goal]
    xor eax,eax
.copy:
    mov edx,[rcx+rax*4]
    mov [rdi+rax*4],edx
    inc eax
    cmp eax,P_COUNT
    jb .copy
    call synth_clear_amplitudes
    mov eax,[rbx+ENTRY.f0a]
    mov [rdi+P_F0*4],eax
    test rsi,rsi
    jnz .phoneme
    mov rcx,rdi
    mov edx,[rbx+ENTRY.frames]
    mov r8d,8
    mov r9d,2
    call synth_segment
    jmp .done
.phoneme:
    movzx r13d,word [rsi+PHONEME.flags]
    call synth_vowel_reference
    mov r14,rax
    mov rcx,rsi
    test r13d,PH_ASPIRATE
    jz .formants
    mov rcx,r14
.formants:
    call synth_set_formants
    test r13d,PH_STOP|PH_NASAL
    jz .zero
    call synth_loci
.zero:
    mov eax,[nasal_pole]
    test r13d,PH_NASAL
    jz .nasal_zero
    mov eax,[nasal_zero]
.nasal_zero:
    mov [rdi+P_FNZ*4],eax
    movzx eax,word [rsi+PHONEME.fx]
    cvtsi2ss xmm0,eax
    movss [rdi+P_FX*4],xmm0
    movzx eax,word [rsi+PHONEME.bx]
    cvtsi2ss xmm0,eax
    movss [rdi+P_FX*4+4],xmm0
    test r13d,PH_STOP|PH_AFFRICATE
    jnz .obstruent
    call synth_set_amplitudes
    mov eax,[rbx+ENTRY.frames]
    mov r15d,eax
    shr eax,1
    sub r15d,eax
    mov edx,eax
    mov rcx,rdi
    mov r8d,8
    mov r9d,2
    call synth_segment
    jc .done
    test r13d,PH_VOWEL
    jz .second_half
    lea rcx,[rsi+PHONEME.g1]
    lea rdx,[synth_goal]
    xor eax,eax
.glide:
    movzx r8d,word [rcx+rax*2]
    cvtsi2ss xmm0,r8d
    movss [rdx+rax*4],xmm0
    inc eax
    cmp eax,3
    jb .glide
.second_half:
    mov eax,[rbx+ENTRY.f0b]
    mov [rdi+P_F0*4],eax
    mov rcx,rdi
    mov edx,r15d
    mov r8d,8
    mov r9d,2
    call synth_segment
    jmp .done
.obstruent:
    test r13d,PH_VOICED
    jz .closure
    mov eax,[voice_bar]
    mov [rdi+P_AV*4],eax
.closure:
    mov rcx,rdi
    mov edx,[rbx+ENTRY.frames]
    mov r8d,4
    mov r9d,1
    call synth_segment
    jc .done
    call synth_set_amplitudes
    mov dword [rdi+P_AH*4],0
    mov edx,SPEECH_BURST
    test r13d,PH_AFFRICATE
    jz .release
    mov edx,SPEECH_AFFRICATION
.release:
    mov rcx,rdi
    mov r8d,2
    mov r9d,1
    call synth_segment
    jc .done
    test r13d,PH_AFFRICATE|PH_VOICED
    jnz .done
    call synth_clear_amplitudes
    movzx eax,word [rsi+PHONEME.ah]
    cvtsi2ss xmm0,eax
    mulss xmm0,[milli]
    movss [rdi+P_AH*4],xmm0
    mov rcx,r14
    call synth_set_formants
    mov eax,[rbx+ENTRY.f0b]
    mov [rdi+P_F0*4],eax
    mov rcx,rdi
    mov edx,SPEECH_ASPIRATION
    mov r8d,8
    mov r9d,1
    call synth_segment
.done:
    pop r15
    pop r14
    pop r13
    pop rdi
    pop rsi
    ret

; rcx = goal vector, edx = frames, r8d = formant transition frames,
; r9d = amplitude transition frames. Pitch glides across the whole segment.
; -> CF on overflow
synth_segment:
    push rbx
    push rsi
    push rdi
    push r12
    mov rsi,rcx
    mov r12d,edx
    test edx,edx
    jz .complete
    cvtsi2ss xmm0,r8d
    movss xmm3,[float_one]
    divss xmm3,xmm0
    cvtsi2ss xmm0,r9d
    movss xmm4,[float_one]
    divss xmm4,xmm0
    cvtsi2ss xmm0,edx
    movss xmm5,[float_one]
    divss xmm5,xmm0
    lea rdi,[synth_inverse]
    lea rcx,[param_class]
    lea rdx,[synth_current]
    lea r8,[synth_start]
    xor eax,eax
.class:
    mov r9d,[rdx+rax*4]
    mov [r8+rax*4],r9d
    movzx r9d,byte [rcx+rax]
    movss xmm0,xmm3
    cmp r9d,1
    jb .set
    movss xmm0,xmm5
    je .set
    movss xmm0,xmm4
.set:
    movss [rdi+rax*4],xmm0
    inc eax
    cmp eax,P_COUNT
    jb .class
    mov ebx,1
.frame:
    cvtsi2ss xmm2,ebx
    lea rcx,[synth_current]
    lea rdx,[synth_start]
    lea r8,[synth_inverse]
    xor eax,eax
.param:
    movss xmm0,xmm2
    mulss xmm0,[r8+rax*4]
    minss xmm0,[float_one]
    movss xmm1,[rsi+rax*4]
    subss xmm1,[rdx+rax*4]
    mulss xmm1,xmm0
    addss xmm1,[rdx+rax*4]
    movss [rcx+rax*4],xmm1
    inc eax
    cmp eax,P_COUNT
    jb .param
    call synth_frame
    jc .done
    inc ebx
    cmp ebx,r12d
    jbe .frame
.complete:
    clc
.done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; Renders one 5 ms frame from synth_current. CF when the buffer is full.
synth_frame:
    push rdi
    mov eax,[speech_samples]
    add eax,SPEECH_FRAME
    cmp eax,SPEECH_MAX_SAMPLES
    ja .full
    lea rcx,[res_c1]
    movss xmm0,[synth_current+P_F1*4]
    movss xmm1,[synth_current+P_B1*4]
    xor r8d,r8d
    call res_set
    lea rcx,[res_c2]
    movss xmm0,[synth_current+P_F2*4]
    movss xmm1,[synth_current+P_B1*4+4]
    xor r8d,r8d
    call res_set
    lea rcx,[res_c3]
    movss xmm0,[synth_current+P_F3*4]
    movss xmm1,[synth_current+P_B1*4+8]
    xor r8d,r8d
    call res_set
    lea rcx,[res_nz]
    movss xmm0,[synth_current+P_FNZ*4]
    movss xmm1,[nasal_bandwidth]
    call anti_set
    lea rcx,[res_p2]
    movss xmm0,[synth_current+P_F2*4]
    movss xmm1,[synth_current+P_B1*4+4]
    mov r8d,1
    call res_set
    lea rcx,[res_p3]
    movss xmm0,[synth_current+P_F3*4]
    movss xmm1,[synth_current+P_B1*4+8]
    mov r8d,1
    call res_set
    lea rcx,[res_px]
    movss xmm0,[synth_current+P_FX*4]
    movss xmm1,[synth_current+P_FX*4+4]
    mov r8d,1
    call res_set
    lea rcx,[synth_current+P_AV*4]
    lea rdx,[amp_now]
    lea r8,[amp_step]
    xor eax,eax
.ramp:
    movss xmm0,[rcx+rax*4]
    subss xmm0,[rdx+rax*4]
    mulss xmm0,[inverse_frame]
    movss [r8+rax*4],xmm0
    inc eax
    cmp eax,P_AMPS
    jb .ramp
    movss xmm0,[synth_current+P_F0*4]
    subss xmm0,[f0_now]
    mulss xmm0,[inverse_frame]
    movss [f0_delta],xmm0
    mov eax,[speech_samples]
    lea rdi,[speech_wave]
    lea rdi,[rdi+rax*4]
    add eax,SPEECH_FRAME
    mov [speech_samples],eax
    mov ecx,SPEECH_FRAME
.sample:
    lea rdx,[amp_now]
    lea r8,[amp_step]
    xor eax,eax
.advance:
    movss xmm0,[rdx+rax*4]
    addss xmm0,[r8+rax*4]
    movss [rdx+rax*4],xmm0
    inc eax
    cmp eax,P_AMPS
    jb .advance
    movss xmm0,[f0_now]
    addss xmm0,[f0_delta]
    movss [f0_now],xmm0
    imul eax,[noise_seed],1664525
    add eax,1013904223
    cvtsi2ss xmm8,eax
    imul eax,eax,1664525
    add eax,1013904223
    mov [noise_seed],eax
    cvtsi2ss xmm9,eax
    movss xmm2,xmm8
    addss xmm2,xmm9
    mulss xmm2,[noise_scale]
    movss xmm0,[noise_lowpass]
    subss xmm2,xmm0
    mulss xmm2,[noise_smoothing]
    addss xmm2,xmm0
    movss [noise_lowpass],xmm2
    movss xmm0,[f0_now]
    mulss xmm0,[inverse_rate]
    mulss xmm0,[jitter]
    addss xmm0,[glottal_phase]
    comiss xmm0,[float_one]
    jb .in_period
    subss xmm0,[float_one]
    movss xmm1,xmm8
    mulss xmm1,[jitter_scale]
    addss xmm1,[float_one]
    movss [jitter],xmm1
    movss xmm1,xmm9
    mulss xmm1,[shimmer_scale]
    addss xmm1,[float_one]
    movss [shimmer],xmm1
.in_period:
    movss [glottal_phase],xmm0
    xorps xmm3,xmm3
    movss xmm4,[breath_closed]
    comiss xmm0,[open_quotient]
    jae .closed
    mulss xmm0,[inverse_open_quotient]
    movss xmm3,[float_one]
    subss xmm3,xmm0
    mulss xmm3,xmm0
    mulss xmm3,xmm0
    mulss xmm3,[flow_scale]
    movss xmm4,[float_one]
.closed:
    mulss xmm3,[amp_now]
    mulss xmm3,[shimmer]
    movss xmm0,[tilt_state]
    subss xmm0,xmm3
    mulss xmm0,[spectral_tilt]
    addss xmm0,xmm3
    movss [tilt_state],xmm0
    movss xmm1,[amp_now]
    mulss xmm1,[breathiness]
    addss xmm1,[amp_now+4]
    mulss xmm1,xmm4
    mulss xmm1,xmm2
    mulss xmm1,[aspiration_gain]
    addss xmm0,xmm1
    antiresonate res_nz
    resonate res_np
    resonate res_c1
    resonate res_c2
    resonate res_c3
    resonate res_c4
    resonate res_c5
    movss xmm5,xmm0
    movss xmm6,xmm2
    mulss xmm6,[amp_now+8]
    movss xmm0,xmm6
    resonate res_p2
    mulss xmm0,[amp_now+12]
    movss xmm7,xmm0
    movss xmm0,xmm6
    resonate res_p3
    mulss xmm0,[amp_now+16]
    subss xmm7,xmm0
    movss xmm0,xmm6
    resonate res_p4
    mulss xmm0,[amp_now+20]
    addss xmm7,xmm0
    movss xmm0,xmm6
    resonate res_p5
    mulss xmm0,[amp_now+24]
    subss xmm7,xmm0
    movss xmm0,xmm6
    resonate res_px
    mulss xmm0,[amp_now+28]
    addss xmm7,xmm0
    movss xmm0,xmm6
    mulss xmm0,[amp_now+32]
    addss xmm7,xmm0
    mulss xmm7,[parallel_gain]
    addss xmm5,xmm7
    movss xmm0,xmm5
    subss xmm0,[radiation_prev]
    movss [radiation_prev],xmm5
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
