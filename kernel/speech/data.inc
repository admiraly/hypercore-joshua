struc PHONEME
 .name:      resd 1
 .f1:        resw 1
 .f2:        resw 1
 .f3:        resw 1
 .f1b:       resw 1
 .f2b:       resw 1
 .f3b:       resw 1
 .av:        resw 1
 .ah:        resw 1
 .af:        resw 1
 .ff:        resw 1
 .fb:        resw 1
 .duration:  resw 1
 .flags:     resw 1
 .reserved:  resw 1
endstruc

; name, F1 F2 F3, glide-target F1 F2 F3, voicing, aspiration, frication,
; frication centre Hz, frication bandwidth Hz, duration ms, flags
%macro phoneme 14
    %strlen phoneme_name_length %1
    db %1
    times 4-phoneme_name_length db 0
    dw %2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,0
%endmacro

align 4
phoneme_table:
    phoneme 'AA', 730,1090,2440, 730,1090,2440, 100, 0, 0, 2500, 500, 120, 0
    phoneme 'AE', 660,1720,2410, 660,1720,2410, 100, 0, 0, 2500, 500, 110, 0
    phoneme 'AH', 620,1220,2550, 620,1220,2550, 100, 0, 0, 2500, 500,  90, 0
    phoneme 'AO', 570, 840,2410, 570, 840,2410, 100, 0, 0, 2500, 500, 120, 0
    phoneme 'AX', 500,1400,2500, 500,1400,2500,  90, 0, 0, 2500, 500,  60, 0
    phoneme 'EH', 530,1840,2480, 530,1840,2480, 100, 0, 0, 2500, 500, 100, 0
    phoneme 'ER', 490,1350,1690, 490,1350,1690, 100, 0, 0, 2500, 500, 120, 0
    phoneme 'IH', 400,1920,2560, 400,1920,2560, 100, 0, 0, 2500, 500,  80, 0
    phoneme 'IY', 290,2250,2890, 290,2250,2890, 100, 0, 0, 2500, 500, 110, 0
    phoneme 'UH', 450,1030,2380, 450,1030,2380, 100, 0, 0, 2500, 500,  90, 0
    phoneme 'UW', 300, 870,2240, 300, 870,2240, 100, 0, 0, 2500, 500, 110, 0
    phoneme 'AY', 730,1090,2440, 350,2100,2700, 100, 0, 0, 2500, 500, 170, 0
    phoneme 'EY', 530,1840,2480, 300,2200,2800, 100, 0, 0, 2500, 500, 160, 0
    phoneme 'OW', 570, 840,2410, 350, 750,2300, 100, 0, 0, 2500, 500, 160, 0
    phoneme 'AW', 730,1090,2440, 350, 800,2240, 100, 0, 0, 2500, 500, 170, 0
    phoneme 'OY', 570, 840,2410, 350,2100,2700, 100, 0, 0, 2500, 500, 180, 0
    phoneme 'M',  280,1000,2200, 280,1000,2200,  60, 0, 0, 2500, 500,  70, 0
    phoneme 'N',  280,1700,2600, 280,1700,2600,  60, 0, 0, 2500, 500,  65, 0
    phoneme 'NG', 280,2300,2750, 280,2300,2750,  60, 0, 0, 2500, 500,  70, 0
    phoneme 'L',  360,1300,2700, 360,1300,2700,  80, 0, 0, 2500, 500,  60, 0
    phoneme 'R',  420,1300,1600, 420,1300,1600,  85, 0, 0, 2500, 500,  70, 0
    phoneme 'W',  290, 610,2150, 290, 610,2150,  85, 0, 0, 2500, 500,  60, 0
    phoneme 'Y',  260,2070,3020, 260,2070,3020,  85, 0, 0, 2500, 500,  60, 0
    phoneme 'HH', 500,1500,2500, 500,1500,2500,   0,70, 0, 2500, 500,  70, 0
    phoneme 'S',  320,1400,2700, 320,1400,2700,   0, 0,70, 5500,1200, 110, 0
    phoneme 'Z',  320,1400,2700, 320,1400,2700,  40, 0,50, 5500,1200,  90, 0
    phoneme 'SH', 300,1840,2750, 300,1840,2750,   0, 0,70, 2700, 900, 120, 0
    phoneme 'ZH', 300,1840,2750, 300,1840,2750,  40, 0,50, 2700, 900,  80, 0
    phoneme 'F',  340,1100,2080, 340,1100,2080,   0, 0,40, 6500,3000, 100, 0
    phoneme 'V',  340,1100,2080, 340,1100,2080,  40, 0,30, 6500,3000,  70, 0
    phoneme 'TH', 320,1290,2540, 320,1290,2540,   0, 0,35, 6000,2500, 100, 0
    phoneme 'DH', 320,1290,2540, 320,1290,2540,  45, 0,25, 5000,2500,  60, 0
    phoneme 'P',  400,1100,2150, 400,1100,2150,   0,50,50, 1200,1000,  60, PH_PLOSIVE
    phoneme 'B',  400,1100,2150, 400,1100,2150,  25, 0,40, 1200,1000,  50, PH_PLOSIVE
    phoneme 'T',  400,1600,2600, 400,1600,2600,   0,50,65, 4000,1500,  60, PH_PLOSIVE
    phoneme 'D',  400,1600,2600, 400,1600,2600,  25, 0,50, 4000,1500,  50, PH_PLOSIVE
    phoneme 'K',  300,1990,2850, 300,1990,2850,   0,50,60, 2200, 800,  60, PH_PLOSIVE
    phoneme 'G',  300,1990,2850, 300,1990,2850,  25, 0,50, 2200, 800,  50, PH_PLOSIVE
    phoneme 'CH', 300,1840,2750, 300,1840,2750,   0, 0,70, 2700, 900,  50, PH_AFFRICATE
    phoneme 'JH', 300,1840,2750, 300,1840,2750,  25, 0,55, 2700, 900,  45, PH_AFFRICATE
phoneme_table_end:

speech_checkpoint_phrase:
    db 'JH OW1 SH UW AX , AA1 N L AY N . '
    db 'HH AY1 P ER K AO R _ B UW1 T _ CH EH1 K P OY N T _ R IY1 CH T .',0

debug_speech_ok: db 'JOSHUA:SPEECH_OK',0
debug_speech_unavailable: db 'JOSHUA:SPEECH_UNAVAILABLE',0
debug_speech_failed: db 'JOSHUA:SPEECH_FAILED',0
debug_speech_status: db ':'
debug_speech_code: db '00',13,10,0
screen_speech_ok: db 'SPEECH: CHECKPOINT ANNOUNCED VIA HDA',0
screen_speech_unavailable: db 'SPEECH: NO HDA OUTPUT PATH',0
screen_speech_failed: db 'SPEECH: HDA PLAYBACK FAILED',0

align 4
synth_initial:
    dd 500.0, 1500.0, 2500.0, 2500.0, 500.0, 0.0, 0.0, 0.0, 122.0
synth_rate:
    dd 0.35, 0.35, 0.35, 0.5, 0.5, 0.5, 0.5, 0.5, 0.2
cos_coefficients:
    dd 2.0876757e-9, -2.7557319e-7, 2.4801587e-5, -1.3888889e-3
    dd 4.1666667e-2, -0.5, 1.0
exp_coefficients:
    dd 1.3888889e-3, 8.3333333e-3, 4.1666667e-2, 0.16666667, 0.5, 1.0, 1.0
two_pi_over_rate: dd 3.9269908e-4
minus_pi_over_rate: dd -1.9634954e-4
inverse_rate: dd 6.25e-5
noise_scale: dd 4.6566129e-10
float_one: dd 1.0
one_third: dd 0.33333334
abs_mask: dd 0x7fffffff
peak_floor: dd 1.0e-6
pcm_target: dd 22000.0
bw_glottal: dd 100.0
bw_f1: dd 90.0
bw_f2: dd 110.0
bw_f3: dd 170.0
voice_gain: dd 1.0
aspiration_gain: dd 0.001
frication_gain: dd 0.0008
f0_start: dd 122.0
f0_stress: dd 20.0
f0_step: dd 0.08
f0_floor: dd 85.0
speech_mxcsr: dd 0
speech_samples: dd 0
noise_seed: dd 0
f0_base: dd 0
synth_current: times P_COUNT dd 0
synth_target: times P_COUNT dd 0
voice_amp: dd 0
aspiration_amp: dd 0
frication_amp: dd 0
phase_step: dd 0
synth_state:
glottal_phase: dd 0
radiation_prev: dd 0
res_glottal: times 5 dd 0
res_f1: times 5 dd 0
res_f2: times 5 dd 0
res_f3: times 5 dd 0
res_fric: times 5 dd 0
synth_state_end:

align 8
hda_base: dq 0
hda_stream: dq 0
hda_codec: dd 0
hda_rirb_rp: dd 0
hda_afg: dd 0
hda_dac: dd 0
hda_pin: dd 0
hda_pin_caps: dd 0
hda_pin_rank: dd 0
hda_pin_index: dd 0
hda_mixer: dd 0
hda_mixer_index: dd 0
hda_hop_long: dd 0
hda_device_rank: db 2,0,1
