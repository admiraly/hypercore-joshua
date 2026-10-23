%ifndef JOSHUA_KEYBOARD_INC
%define JOSHUA_KEYBOARD_INC
%define KEYBOARD_DECODER_MAGIC 0x3144424b48534f4a
%define KEYBOARD_QUEUE_CAPACITY 64
%define KEY_LEFT 0x100
%define KEY_RIGHT 0x101
%define KEY_HOME 0x102
%define KEY_END 0x103
%define KEY_DELETE 0x104
%define KEY_UP 0x105
%define KEY_DOWN 0x106
struc KEYBOARD_DECODER
 .ready: resq 1
 .modifiers: resq 1             ; L/R shift bits0/1,control2/3,alt4/5
 .caps: resq 1
 .caps_down: resq 1
 .prefix: resq 1                ; 0 none,1 E0
 .pause_index: resq 1           ; 0 none,1..5 expected Pause suffix byte
 .head: resq 1
 .tail: resq 1
 .count: resq 1
 .drops: resq 1                 ; saturating counters
 .protocol_errors: resq 1
 .events: resw KEYBOARD_QUEUE_CAPACITY
endstruc
%define KEYBOARD_AUTH_MAGIC 0x3141554b48534f4a
struc KEYBOARD_AUTH
 .ready: resq 1
 .fadt: resq 1
 .gsi: resq 1
 .io_index: resq 1
 .io_pin: resq 1
 .bsp_id: resq 1
endstruc
%define KEYBOARD_CONTROLLER_MAGIC 0x3143424b48534f4a
%define KEYBOARD_POLL_BUDGET 100000
struc KEYBOARD_CONTROLLER
 .ready: resq 1
 .failed: resq 1
 .stage: resq 1
 .authority: resq 1
 .config: resq 1
 .scanning: resq 1
 .discarded: resq 1
endstruc
%endif
