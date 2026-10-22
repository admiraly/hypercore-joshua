struc PHONEME
 .name:      resd 1
 .f1:        resw 1
 .f2:        resw 1
 .f3:        resw 1
 .b1:        resw 1
 .b2:        resw 1
 .b3:        resw 1
 .g1:        resw 1
 .g2:        resw 1
 .g3:        resw 1
 .av:        resw 1
 .ah:        resw 1
 .af:        resw 1
 .a2:        resw 1
 .a3:        resw 1
 .a4:        resw 1
 .a5:        resw 1
 .ax:        resw 1
 .ab:        resw 1
 .fx:        resw 1
 .bx:        resw 1
 .inherent:  resw 1
 .minimum:   resw 1
 .flags:     resw 1
 .reserved:  resw 7
endstruc

; name; F1 F2 F3; B1 B2 B3 (Hz); glide-target F1 F2 F3 (0 = no glide);
; amplitudes x1000: voicing, aspiration, frication source, parallel A2 A3 A4
; A5 AX and bypass; extra frication resonance Hz and bandwidth; inherent and
; minimum duration ms (Klatt 1979); flags.
; Formants and durations follow Klatt (1979, 1980); amplitudes were tuned
; by analysis of QEMU captures.
%macro phoneme 24
    %strlen phoneme_name_length %1
    db %1
    times 4-phoneme_name_length db 0
    dw %2,%3,%4,%5,%6,%7
    %if %8 == 0
        dw %2,%3,%4
    %else
        dw %8,%9,%10
    %endif
    dw %11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24
    times 7 dw 0
%endmacro

%define V PH_VOWEL|PH_VOICED
align 4
phoneme_table:
    phoneme 'AA', 700,1220,2600, 130, 70,160,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 240,100, V
    phoneme 'AE', 620,1660,2430,  70,150,320,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 230, 80, V
    phoneme 'AH', 620,1220,2550,  80, 50,140,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 140, 60, V
    phoneme 'AO', 600, 990,2570,  90,100, 80,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 240,100, V
    phoneme 'AX', 500,1400,2300, 100, 60,110,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 120, 55, V
    phoneme 'EH', 530,1680,2500,  60, 90,200,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 150, 70, V
    phoneme 'ER', 470,1270,1540, 100, 60,110,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 180, 80, V
    phoneme 'IH', 400,1800,2570,  50,100,140,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 135, 40, V
    phoneme 'IY', 310,2020,2960,  45,200,400,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 155, 55, V
    phoneme 'UH', 450,1100,2350,  80,100, 80,   0,   0,   0, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 160, 60, V
    phoneme 'UW', 350,1250,2200,  65,110,140, 320, 950,2200, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 210, 70, V
    phoneme 'AY', 660,1200,2550, 100, 70,200, 400,1880,2500, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 250,150, V
    phoneme 'EY', 480,1720,2520,  70,100,200, 330,2200,2600, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 190,100, V
    phoneme 'OW', 540,1100,2300,  80, 70, 70, 450, 900,2300, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 220, 80, V
    phoneme 'AW', 640,1230,2550,  80, 70,140, 420, 940,2350, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 260,100, V
    phoneme 'OY', 550, 960,2400,  80, 50,130, 360,1820,2450, 1000,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000, 280,150, V
    phoneme 'W',  290, 610,2150,  50, 80, 60,   0,   0,   0,  900,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  80, 60, PH_VOICED
    phoneme 'Y',  260,2070,3020,  40,250,500,   0,   0,   0,  900,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  80, 40, PH_VOICED
    phoneme 'R',  310,1060,1380,  70,100,120,   0,   0,   0,  900,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  80, 30, PH_VOICED
    phoneme 'L',  310,1050,2880,  50,100,280,   0,   0,   0,  900,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  80, 40, PH_VOICED
    phoneme 'M',  480,1270,2130,  40,200,200,   0,   0,   0,  800,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  70, 60, PH_NASAL|PH_LABIAL|PH_VOICED
    phoneme 'N',  480,1340,2470,  40,300,300,   0,   0,   0,  800,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  60, 35, PH_NASAL|PH_ALVEOLAR|PH_VOICED
    phoneme 'NG', 480,2000,2900, 160,150,200,   0,   0,   0,  800,  0,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  95, 60, PH_NASAL|PH_VELAR|PH_VOICED
    phoneme 'HH', 500,1500,2500, 300,150,250,   0,   0,   0,    0,700,   0,  0,  0,  0,  0,   0,  0, 3000,1000,  80, 20, PH_ASPIRATE
    phoneme 'F',  340,1100,2080, 200,120,150,   0,   0,   0,    0,  0,1000,  0,  0,  0,  0,   0,400, 3000,1000, 100, 80, 0
    phoneme 'V',  220,1100,2080,  60, 90,120,   0,   0,   0,  500,  0,1000,  0,  0,  0,  0,   0,250, 3000,1000,  60, 40, PH_VOICED
    phoneme 'TH', 320,1290,2540, 200, 90,200,   0,   0,   0,    0,  0,1000,  0,  0,  0,  0, 250,200, 6000,3000, 100, 60, 0
    phoneme 'DH', 270,1290,2540,  60, 80,170,   0,   0,   0,  500,  0,1000,  0,  0,  0,  0, 150,150, 5000,3000,  50, 30, PH_VOICED
    phoneme 'S',  320,1390,2530, 200, 80,200,   0,   0,   0,    0,  0,1000,  0,  0,  0,200,1000,  0, 6000,1800, 105, 60, 0
    phoneme 'Z',  240,1390,2530,  70, 60,180,   0,   0,   0,  500,  0,1000,  0,  0,  0,150, 700,  0, 6000,1800,  75, 40, PH_VOICED
    phoneme 'SH', 300,1840,2750, 200,100,300,   0,   0,   0,    0,  0,1000,  0,300,300,200,1000,  0, 2900,1000, 105, 80, 0
    phoneme 'ZH', 300,1840,2750,  70, 60,280,   0,   0,   0,  500,  0,1000,  0,  0,200,  0, 700,  0, 2900,1000,  70, 40, PH_VOICED
    phoneme 'P',  400,1100,2150, 300,150,220,   0,   0,   0,    0,600,1000,  0,  0,  0,  0,   0,250, 3000,1000,  70, 40, PH_STOP|PH_LABIAL
    phoneme 'B',  200,1100,2150,  60,110,130,   0,   0,   0,  400,  0,1000,  0,  0,  0,  0,   0,180, 3000,1000,  70, 45, PH_STOP|PH_LABIAL|PH_VOICED
    phoneme 'T',  400,1600,2600, 300,120,250,   0,   0,   0,    0,600,1000,  0,  0,  0,100, 350,  0, 4500,2000,  60, 40, PH_STOP|PH_ALVEOLAR
    phoneme 'D',  200,1600,2600,  60,100,170,   0,   0,   0,  400,  0,1000,  0,  0,  0, 80, 250,  0, 4500,2000,  60, 40, PH_STOP|PH_ALVEOLAR|PH_VOICED
    phoneme 'K',  300,1990,2850, 250,160,330,   0,   0,   0,    0,600,1000,150,300,  0,  0,   0,  0, 3000,1000,  65, 45, PH_STOP|PH_VELAR
    phoneme 'G',  200,1990,2850,  60,150,280,   0,   0,   0,  400,  0,1000,120,220,  0,  0,   0,  0, 3000,1000,  65, 45, PH_STOP|PH_VELAR|PH_VOICED
    phoneme 'CH', 350,1800,2820, 200, 90,300,   0,   0,   0,    0,  0,1000,  0,300,300,  0,1000,  0, 2900,1000,  50, 35, PH_AFFRICATE
    phoneme 'JH', 260,1800,2820,  60, 80,270,   0,   0,   0,  400,  0,1000,  0,  0,200,  0, 700,  0, 2900,1000,  50, 35, PH_AFFRICATE|PH_VOICED
phoneme_table_end:
%undef V

speech_checkpoint_phrase:
    db 'JH AA1 SH UW AX , AA1 N L AY2 N . '
    db 'HH AY1 P ER K AO2 R _ B UW1 T _ CH EH1 K P OY2 N T _ R IY1 CH T .',0

debug_speech_ok: db 'JOSHUA:SPEECH_OK',0
debug_speech_unavailable: db 'JOSHUA:SPEECH_UNAVAILABLE',0
debug_speech_failed: db 'JOSHUA:SPEECH_FAILED',0
debug_speech_status: db ':'
debug_speech_code: db '00',13,10,0
screen_speech_ok: db 'SPEECH: CHECKPOINT ANNOUNCED VIA HDA',0
screen_speech_unavailable: db 'SPEECH: NO HDA OUTPUT PATH',0
screen_speech_failed: db 'SPEECH: HDA PLAYBACK FAILED',0

; Parameter interpolation class: 0 formant-like, 1 pitch, 2 amplitude
param_class: db 0,0,0,0,0,0,0,1,2,2,2,2,2,2,2,2,2,0,0

align 4
; F1 F2 F3 B1 B2 B3 FNZ F0 AV AH AF A2 A3 A4 A5 AX AB FX BX
synth_initial:
    dd 500.0, 1500.0, 2500.0, 60.0, 90.0, 150.0, 270.0, 118.0
    dd 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3000.0, 1000.0
; Per place: F2 locus, F2 vowel coupling, F3 locus, F3 vowel coupling
speech_loci:
    dd  800.0, 0.65, 2200.0, 0.6
    dd 1750.0, 0.45, 2700.0, 0.5
    dd 1900.0, 0.7,  2500.0, 0.6
cos_coefficients:
    dd 2.0876757e-9, -2.7557319e-7, 2.4801587e-5, -1.3888889e-3
    dd 4.1666667e-2, -0.5, 1.0
exp_coefficients:
    dd 1.3888889e-3, 8.3333333e-3, 4.1666667e-2, 0.16666667, 0.5, 1.0, 1.0
two_pi_over_rate: dd 1.3089969e-4
minus_pi_over_rate: dd -6.5449847e-5
inverse_rate: dd 2.0833333e-5
inverse_frame: dd 4.1666668e-3
frames_per_ms: dd 0.175
milli: dd 0.001
float_one: dd 1.0
float_half: dd 0.5
abs_mask: dd 0x7fffffff
peak_floor: dd 1.0e-6
pcm_target: dd 24000.0
noise_scale: dd 2.3283064e-10
noise_smoothing: dd 0.55
jitter_scale: dd 4.656613e-12
shimmer_scale: dd 2.7939677e-11
open_quotient: dd 0.6
inverse_open_quotient: dd 1.6666666
flow_scale: dd 6.75
spectral_tilt: dd 0.55
breathiness: dd 0.04
breath_closed: dd 0.4
aspiration_gain: dd 0.3
parallel_gain: dd 0.22
voice_bar: dd 0.12
stop_f1: dd 250.0
nasal_pole: dd 270.0
nasal_zero: dd 450.0
nasal_bandwidth: dd 100.0
f4_frequency: dd 3500.0
f4_bandwidth: dd 250.0
f5_frequency: dd 4500.0
f5_bandwidth: dd 300.0
f0_top: dd 118.0
f0_bottom: dd 90.0
f0_rise: dd 8.0
f0_peak: dd 26.0
f0_fall: dd 14.0
f0_tail: dd 10.0
f0_secondary: dd 8.0
f0_continuation: dd 16.0
duration_unstressed: dd 0.65
duration_nonfinal: dd 0.85
duration_phrase_final: dd 1.4
duration_before_voiceless: dd 0.8
duration_cluster: dd 0.75
duration_final_consonant: dd 1.2
gain_unstressed: dd 0.8
speech_mxcsr: dd 0
speech_samples: dd 0
speech_entry_count: dd 0
noise_seed: dd 0
f0_now: dd 0
f0_delta: dd 0
jitter: dd 0
shimmer: dd 0
synth_current: times P_COUNT dd 0
synth_goal: times P_COUNT dd 0
synth_start: times P_COUNT dd 0
synth_inverse: times P_COUNT dd 0
amp_step: times P_AMPS dd 0
synth_state:
glottal_phase: dd 0
tilt_state: dd 0
noise_lowpass: dd 0
radiation_prev: dd 0
amp_now: times P_AMPS dd 0
res_nz: times 5 dd 0
res_np: times 5 dd 0
res_c1: times 5 dd 0
res_c2: times 5 dd 0
res_c3: times 5 dd 0
res_c4: times 5 dd 0
res_c5: times 5 dd 0
res_p2: times 5 dd 0
res_p3: times 5 dd 0
res_p4: times 5 dd 0
res_p5: times 5 dd 0
res_px: times 5 dd 0
synth_state_end:

align 8
pitch_last_stress: dq 0
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
