; Intel High Definition Audio output driver. Polled: interrupts stay masked.
; Supports one codec path: pin <- DAC, or pin <- mixer/selector <- DAC.

%define HDA_GCAP         0x00
%define HDA_GCTL         0x08
%define HDA_STATESTS     0x0e
%define HDA_CORBLBASE    0x40
%define HDA_CORBUBASE    0x44
%define HDA_CORBWP       0x48
%define HDA_CORBRP       0x4a
%define HDA_CORBCTL      0x4c
%define HDA_CORBSIZE     0x4e
%define HDA_RIRBLBASE    0x50
%define HDA_RIRBUBASE    0x54
%define HDA_RIRBWP       0x58
%define HDA_RINTCNT      0x5a
%define HDA_RIRBCTL      0x5c
%define HDA_RIRBSTS      0x5d
%define HDA_RIRBSIZE     0x5e
%define HDA_SD_CTL       0x00
%define HDA_SD_TAG       0x02
%define HDA_SD_STS       0x03
%define HDA_SD_LPIB      0x04
%define HDA_SD_CBL       0x08
%define HDA_SD_LVI       0x0c
%define HDA_SD_FMT       0x12
%define HDA_SD_BDPL      0x18
%define HDA_SD_BDPU      0x1c
%define HDA_FORMAT       0x0011
%define HDA_POLL         200000
%define HDA_STALL        20000000
%define HDA_E_CONTROLLER 1
%define HDA_E_BAR        2
%define HDA_E_RESET      3
%define HDA_E_CODEC      4
%define HDA_E_ADDRESS    5
%define HDA_E_VERB       6
%define HDA_E_PATH       7
%define HDA_E_STREAM     8
%define HDA_E_PLAYBACK   9

%macro io_delay 0
    out 0x80,al
%endmacro

; -> eax = 0 when an output path is ready, else HDA_E_*
hda_init:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov r15d,HDA_E_CONTROLLER
    mov ecx,0x0403
    call pci_find_class
    cmp eax,-1
    je .done
    mov r12d,eax
    mov r15d,HDA_E_BAR
    mov ecx,r12d
    mov edx,0x10
    call pci_read32
    test al,1
    jnz .done
    mov ebx,eax
    and ebx,-16
    and eax,6
    cmp eax,4
    jne .bar_ready
    mov ecx,r12d
    mov edx,0x14
    call pci_read32
    shl rax,32
    or rbx,rax
.bar_ready:
    test rbx,rbx
    jz .done
    mov ecx,r12d
    mov edx,4
    call pci_read32
    or eax,6
    mov r8d,eax
    mov ecx,r12d
    mov edx,4
    call pci_write16
    mov [hda_base],rbx
    mov r15d,HDA_E_RESET
    mov byte [rbx+HDA_CORBCTL],0
    mov byte [rbx+HDA_RIRBCTL],0
    and dword [rbx+HDA_GCTL],-2
    mov ecx,HDA_POLL
.enter_reset:
    test byte [rbx+HDA_GCTL],1
    jz .in_reset
    io_delay
    dec ecx
    jnz .enter_reset
    jmp .done
.in_reset:
    mov ecx,1000
.hold_reset:
    io_delay
    dec ecx
    jnz .hold_reset
    or dword [rbx+HDA_GCTL],1
    mov ecx,HDA_POLL
.leave_reset:
    test byte [rbx+HDA_GCTL],1
    jnz .running
    io_delay
    dec ecx
    jnz .leave_reset
    jmp .done
.running:
    mov ecx,1000
.settle:
    io_delay
    dec ecx
    jnz .settle
    mov r15d,HDA_E_CODEC
    mov ecx,HDA_POLL
.codec_wait:
    movzx eax,word [rbx+HDA_STATESTS]
    and eax,0x7fff
    jnz .codec_found
    io_delay
    dec ecx
    jnz .codec_wait
    jmp .done
.codec_found:
    bsf eax,eax
    mov [hda_codec],eax
    mov word [rbx+HDA_STATESTS],0x7fff
    mov r15d,HDA_E_ADDRESS
    lea rax,[hda_dma_end]
    shr rax,32
    jz .addressable
    test byte [rbx+HDA_GCAP],1
    jz .done
.addressable:
    mov r15d,HDA_E_RESET
    lea rax,[hda_corb]
    mov [rbx+HDA_CORBLBASE],eax
    shr rax,32
    mov [rbx+HDA_CORBUBASE],eax
    mov byte [rbx+HDA_CORBSIZE],2
    mov word [rbx+HDA_CORBRP],0x8000
    mov ecx,1000
.corb_reset:
    test word [rbx+HDA_CORBRP],0x8000
    jnz .corb_release
    io_delay
    dec ecx
    jnz .corb_reset
.corb_release:
    mov word [rbx+HDA_CORBRP],0
    mov ecx,HDA_POLL
.corb_released:
    test word [rbx+HDA_CORBRP],0x8000
    jz .corb_ready
    io_delay
    dec ecx
    jnz .corb_released
    jmp .done
.corb_ready:
    mov word [rbx+HDA_CORBWP],0
    lea rax,[hda_rirb]
    mov [rbx+HDA_RIRBLBASE],eax
    shr rax,32
    mov [rbx+HDA_RIRBUBASE],eax
    mov byte [rbx+HDA_RIRBSIZE],2
    mov word [rbx+HDA_RIRBWP],0x8000
    mov word [rbx+HDA_RINTCNT],1
    mov dword [hda_rirb_rp],0
    mov byte [rbx+HDA_RIRBSTS],5
    mov byte [rbx+HDA_CORBCTL],2
    ; Response-interrupt status only; INTCTL stays 0, so no interrupt is raised.
    mov byte [rbx+HDA_RIRBCTL],3
    xor ecx,ecx
    mov edx,4
    call hda_param
    jc .verb_failure
    movzx esi,al
    shr eax,16
    movzx edi,al
    mov r15d,HDA_E_PATH
.group:
    test esi,esi
    jz .done
    mov ecx,edi
    mov edx,5
    call hda_param
    jc .verb_failure
    cmp al,1
    je .audio_group
    inc edi
    dec esi
    jmp .group
.audio_group:
    mov [hda_afg],edi
    mov ecx,edi
    shl ecx,20
    or ecx,0x70500
    call hda_verb
    jc .verb_failure
    call hda_find_path
    mov r15d,eax
    test eax,eax
    jnz .done
    call hda_configure
    mov r15d,eax
    test eax,eax
    jnz .done
    mov r15d,HDA_E_STREAM
    movzx eax,word [rbx+HDA_GCAP]
    mov ecx,eax
    shr eax,12
    and eax,15
    jz .done
    shr ecx,8
    and ecx,15
    shl ecx,5
    lea rax,[rbx+rcx+0x80]
    mov [hda_stream],rax
    xor r15d,r15d
    jmp .done
.verb_failure:
    mov r15d,HDA_E_VERB
.done:
    mov eax,r15d
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; ecx = node<<20 | verb -> eax = response, CF set on timeout
hda_verb:
    push rbx
    mov rbx,[hda_base]
    mov eax,[hda_codec]
    shl eax,28
    or ecx,eax
    movzx eax,word [rbx+HDA_CORBWP]
    inc eax
    and eax,255
    lea rdx,[hda_corb]
    mov [rdx+rax*4],ecx
    mov [rbx+HDA_CORBWP],ax
    mov ecx,HDA_POLL
.wait:
    movzx eax,word [rbx+HDA_RIRBWP]
    and eax,255
    cmp eax,[hda_rirb_rp]
    jne .response
    io_delay
    dec ecx
    jnz .wait
    pop rbx
    stc
    ret
.response:
    mov eax,[hda_rirb_rp]
    inc eax
    and eax,255
    mov [hda_rirb_rp],eax
    lea rdx,[hda_rirb]
    mov eax,[rdx+rax*8]
    mov byte [rbx+HDA_RIRBSTS],5
    pop rbx
    clc
    ret

; ecx = node, edx = parameter -> eax = value, CF on timeout
hda_param:
    shl ecx,20
    or ecx,edx
    or ecx,0xf0000
    jmp hda_verb

; ecx = node -> eax = widget type, edx = capabilities, CF on timeout
hda_widget_type:
    mov edx,9
    call hda_param
    jc .done
    mov edx,eax
    shr eax,20
    and eax,15
.done:
    ret

; ecx = node -> eax = connection count, edx = 1 for long-form entries
hda_connections:
    mov edx,0x0e
    call hda_param
    jc .done
    mov edx,eax
    shr edx,7
    and edx,1
    and eax,0x7f
.done:
    ret

; ecx = node, edx = index, r8d = long form -> eax = connected node, CF on timeout
hda_connection:
    push rbx
    mov ebx,edx
    shl ecx,20
    test r8d,r8d
    jnz .long
    and edx,-4
    or ecx,edx
    or ecx,0xf0200
    call hda_verb
    jc .done
    mov ecx,ebx
    and ecx,3
    shl ecx,3
    shr eax,cl
    and eax,0x7f
    jmp .done
.long:
    and edx,-2
    or ecx,edx
    or ecx,0xf0200
    call hda_verb
    jc .done
    mov ecx,ebx
    and ecx,1
    shl ecx,4
    shr eax,cl
    and eax,0x7fff
.done:
    pop rbx
    ret

; ecx = pin -> eax = DAC reached directly or through one mixer/selector, or 0
hda_pin_route:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov r12d,ecx
    call hda_connections
    jc .none
    mov r13d,eax
    mov r14d,edx
    xor esi,esi
.pin_entry:
    cmp esi,r13d
    jae .none
    mov ecx,r12d
    mov edx,esi
    mov r8d,r14d
    call hda_connection
    jc .none
    mov ebx,eax
    mov ecx,eax
    call hda_widget_type
    jc .none
    test eax,eax
    jz .direct
    cmp eax,2
    je .hop
    cmp eax,3
    jne .next_pin_entry
.hop:
    mov ecx,ebx
    call hda_connections
    jc .none
    mov r15d,eax
    mov [hda_hop_long],edx
    xor edi,edi
.hop_entry:
    cmp edi,r15d
    jae .next_pin_entry
    mov ecx,ebx
    mov edx,edi
    mov r8d,[hda_hop_long]
    call hda_connection
    jc .none
    mov [hda_dac],eax
    mov ecx,eax
    call hda_widget_type
    jc .none
    test eax,eax
    jz .through_mixer
    inc edi
    jmp .hop_entry
.through_mixer:
    mov [hda_mixer],ebx
    mov [hda_mixer_index],edi
    mov [hda_pin_index],esi
    mov eax,[hda_dac]
    jmp .done
.next_pin_entry:
    inc esi
    jmp .pin_entry
.direct:
    mov [hda_dac],ebx
    mov dword [hda_mixer],0
    mov [hda_pin_index],esi
    mov eax,ebx
    jmp .done
.none:
    xor eax,eax
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; Prefer speaker, then headphone, then line-out pins. -> eax = 0 or HDA_E_*
hda_find_path:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov ecx,[hda_afg]
    mov edx,4
    call hda_param
    jc .verb_failure
    movzx r13d,al
    shr eax,16
    movzx r12d,al
    xor r14d,r14d
.rank:
    mov esi,r12d
    mov edi,r13d
.widget:
    test edi,edi
    jz .next_rank
    mov ecx,esi
    call hda_widget_type
    jc .verb_failure
    cmp eax,4
    jne .next_widget
    mov ecx,esi
    mov edx,0x0c
    call hda_param
    jc .verb_failure
    test eax,0x10
    jz .next_widget
    mov ebx,eax
    mov ecx,esi
    shl ecx,20
    or ecx,0xf1c00
    call hda_verb
    jc .verb_failure
    mov edx,eax
    shr edx,30
    cmp edx,1
    je .next_widget
    shr eax,20
    and eax,15
    cmp eax,3
    jae .next_widget
    lea rdx,[hda_device_rank]
    movzx eax,byte [rdx+rax]
    cmp eax,r14d
    jne .next_widget
    mov ecx,esi
    call hda_pin_route
    test eax,eax
    jnz .found
.next_widget:
    inc esi
    dec edi
    jmp .widget
.next_rank:
    inc r14d
    cmp r14d,3
    jb .rank
    mov eax,HDA_E_PATH
    jmp .done
.found:
    mov [hda_pin],esi
    mov [hda_pin_caps],ebx
    mov [hda_pin_rank],r14d
    xor eax,eax
    jmp .done
.verb_failure:
    mov eax,HDA_E_VERB
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; ecx = node, edx = verb (12-bit verb<<8 | payload) -> CF on timeout
hda_node_verb:
    shl ecx,20
    or ecx,edx
    jmp hda_verb

; ecx = node, edx = amp-capability parameter, r8d = amp verb payload without gain
hda_amp_on:
    push rbx
    push r12
    push r13
    mov ebx,ecx
    mov r12d,edx
    mov r13d,r8d
    call hda_widget_type
    jc .done
    test edx,1<<3
    mov ecx,ebx
    jnz .own_caps
    mov ecx,[hda_afg]
.own_caps:
    mov edx,r12d
    call hda_param
    jc .done
    and eax,0x7f
    or eax,r13d
    mov ecx,ebx
    shl ecx,20
    or ecx,0x30000
    or ecx,eax
    call hda_verb
.done:
    pop r13
    pop r12
    pop rbx
    ret

; -> eax = 0 or HDA_E_VERB
hda_configure:
    push rbx
    mov ecx,[hda_dac]
    mov edx,0x70500
    call hda_node_verb
    jc .failure
    mov ecx,[hda_pin]
    mov edx,0x70500
    call hda_node_verb
    jc .failure
    mov ebx,[hda_mixer]
    test ebx,ebx
    jz .pin
    mov ecx,ebx
    mov edx,0x70500
    call hda_node_verb
    jc .failure
    mov ecx,ebx
    mov edx,[hda_mixer_index]
    or edx,0x70100
    call hda_node_verb
    jc .failure
    mov ecx,ebx
    mov edx,0x0d
    mov r8d,[hda_mixer_index]
    shl r8d,8
    or r8d,0x7000
    call hda_amp_on
    jc .failure
    mov ecx,ebx
    mov edx,0x12
    mov r8d,0xb000
    call hda_amp_on
    jc .failure
.pin:
    mov ecx,[hda_pin]
    mov edx,[hda_pin_index]
    or edx,0x70100
    call hda_node_verb
    jc .failure
    mov edx,0x70740
    cmp dword [hda_pin_rank],1
    jne .pin_control
    or edx,0x80
.pin_control:
    mov ecx,[hda_pin]
    call hda_node_verb
    jc .failure
    test dword [hda_pin_caps],1<<16
    jz .pin_amp
    mov ecx,[hda_pin]
    mov edx,0x70c02
    call hda_node_verb
    jc .failure
.pin_amp:
    mov ecx,[hda_pin]
    mov edx,0x12
    mov r8d,0xb000
    call hda_amp_on
    jc .failure
    mov ecx,[hda_dac]
    mov edx,0x12
    mov r8d,0xb000
    call hda_amp_on
    jc .failure
    mov ecx,[hda_dac]
    mov edx,0x70610
    call hda_node_verb
    jc .failure
    mov ecx,[hda_dac]
    mov edx,0x20000|HDA_FORMAT
    call hda_node_verb
    jc .failure
    xor eax,eax
    pop rbx
    ret
.failure:
    mov eax,HDA_E_VERB
    pop rbx
    ret

; rcx = 128-byte aligned PCM, edx = bytes (multiple of 256) -> eax = 0 or HDA_E_*
hda_play:
    push rbx
    push rsi
    push rdi
    mov rsi,[hda_stream]
    mov rdi,rcx
    mov ebx,edx
    mov byte [rsi+HDA_SD_CTL],0
    mov ecx,HDA_POLL
.stop:
    test byte [rsi+HDA_SD_CTL],2
    jz .reset
    io_delay
    dec ecx
    jnz .stop
    jmp .failure
.reset:
    mov byte [rsi+HDA_SD_CTL],1
    mov ecx,HDA_POLL
.reset_on:
    test byte [rsi+HDA_SD_CTL],1
    jnz .reset_off
    io_delay
    dec ecx
    jnz .reset_on
    jmp .failure
.reset_off:
    mov byte [rsi+HDA_SD_CTL],0
    mov ecx,HDA_POLL
.reset_clear:
    test byte [rsi+HDA_SD_CTL],1
    jz .program
    io_delay
    dec ecx
    jnz .reset_clear
    jmp .failure
.program:
    lea rax,[hda_bdl]
    mov edx,ebx
    shr edx,1
    mov [rax],rdi
    mov [rax+8],edx
    mov dword [rax+12],0
    lea rcx,[rdi+rdx]
    mov [rax+16],rcx
    mov [rax+24],edx
    mov dword [rax+28],1
    mov [rsi+HDA_SD_BDPL],eax
    shr rax,32
    mov [rsi+HDA_SD_BDPU],eax
    mov [rsi+HDA_SD_CBL],ebx
    mov word [rsi+HDA_SD_LVI],1
    mov word [rsi+HDA_SD_FMT],HDA_FORMAT
    mov byte [rsi+HDA_SD_TAG],0x10
    mov byte [rsi+HDA_SD_STS],0x1c
    mov byte [rsi+HDA_SD_CTL],2
    or r8d,-1
    xor r9d,r9d
.playing:
    test byte [rsi+HDA_SD_STS],4
    jnz .complete
    mov eax,[rsi+HDA_SD_LPIB]
    cmp eax,r8d
    je .stalled
    mov r8d,eax
    xor r9d,r9d
    jmp .playing
.stalled:
    io_delay
    inc r9d
    cmp r9d,HDA_STALL
    jb .playing
    mov byte [rsi+HDA_SD_CTL],0
    jmp .failure
.complete:
    mov byte [rsi+HDA_SD_CTL],0
    mov ecx,HDA_POLL
.drain:
    test byte [rsi+HDA_SD_CTL],2
    jz .stopped
    io_delay
    dec ecx
    jnz .drain
.stopped:
    mov byte [rsi+HDA_SD_STS],0x1c
    xor eax,eax
    jmp .done
.failure:
    mov eax,HDA_E_PLAYBACK
.done:
    pop rdi
    pop rsi
    pop rbx
    ret
