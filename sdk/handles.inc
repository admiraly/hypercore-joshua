%ifndef JOSHUA_HANDLES_INC
%define JOSHUA_HANDLES_INC
%define HANDLE_MAGIC 0x314c444e4848534a
%define HANDLE_CAPACITY 32
%define HANDLE_GENERATION_MAX 0xffffffffff
%define HANDLE_READ 1
%define HANDLE_DUP 2
struc HANDLE_ENTRY
 .state: resq 1
 .generation: resq 1
 .object: resq 1
 .rights: resq 1
endstruc
struc HANDLE_TABLE
 .ready: resq 1
 .owner: resq 1
 .tag: resq 1
 .reserved: resq 1
 .entries: resb HANDLE_CAPACITY*HANDLE_ENTRY_size
endstruc
%if HANDLE_ENTRY_size != 32 || HANDLE_TABLE_size != 1056
 %error 'Handle layout drift'
%endif
%endif
