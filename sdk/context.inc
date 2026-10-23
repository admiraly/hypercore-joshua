%ifndef JOSHUA_CONTEXT_INC
%define JOSHUA_CONTEXT_INC
%define CONTEXT_READY_MAGIC 0x315854434153484a
%define CONTEXT_FRAME_SIZE 192
%define CONTEXT_STACK_RESERVE 4096
%define CONTEXT_TICKS 64
struc KERNEL_CONTEXT
 .ready: resq 1
 .state: resq 1                 ; 0 prepared, 1 running, 2 complete
 .current: resq 1
 .ticks: resq 1
 .eois: resq 1
 .restores: resq 1
 .records: resq 1              ; trusted caller-owned array of three records
 .text_start: resq 1
 .text_end: resq 1
 .code_selector: resq 1
 .data_selector: resq 1
endstruc
struc KERNEL_CONTEXT_RECORD
 .stack_low: resq 1
 .stack_high: resq 1
 .frame: resq 1
 .ticks: resq 1
 .progress: resq 1
endstruc
%endif
