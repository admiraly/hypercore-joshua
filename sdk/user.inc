%ifndef JOSHUA_USER_INC
%define JOSHUA_USER_INC
%define USER_READY_MAGIC 0x315253554153484a
%define USER_PROCESS_COUNT 2
%define USER_PML4_SLOT 128
%define USER_BASE 0x0000400000000000
%define FRAME_OWNER_USER_BASE 0x55530000
struc USER_CONTEXT
 .ready: resq 1
 .failed: resq 1
 .stage: resq 1                  ; 0 empty,1 backing prepared,2 roots built,3 proved
 .builder: resq 1
 .processes: resq 1              ; two caller-owned fixed records
 .payload: resq 1                ; trusted immutable embedded Assembly bytes
 .payload_size: resq 1
 .kernel_root: resq 1
 .active: resq 1                 ; 0 none,1/2 process index+1
 .phase: resq 1                  ; 0 inactive,1 running,2 draining
 .bootstrap_frame: resq 1
 .bootstrap_low: resq 1
 .bootstrap_high: resq 1
 .saved_cr0: resq 1
 .code_selector: resq 1
 .data_selector: resq 1
 .np_selector: resq 1
 .fault_top: resq 1             ; proved ordinary IST top for discarded faults
endstruc
struc USER_PROCESS
 .root: resq 1
 .pdpt: resq 1
 .pd: resq 1
 .pt: resq 1
 .backing: resq 1                ; exact6-page record: code,data,four stack pages
 .owner: resq 1
 .state: resq 1                  ; 0 prepared,1 running,2 draining,3 exit,4 fault,5 timeout
 .status: resq 1
 .ticks: resq 1
 .eois: resq 1
 .fault_vector: resq 1
 .fault_error: resq 1
 .fault_rip: resq 1
 .fault_cr2: resq 1
 .kernel_low: resq 1
 .kernel_high: resq 1
 .unknown_calls: resq 1
 .last_cs: resq 1
endstruc
%endif
