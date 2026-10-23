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

%ifndef JOSHUA_USER_HANDLE_ADAPTER
%define JOSHUA_USER_HANDLE_ADAPTER
%define USER_HANDLES_READY 0x3152444c4848534a
%define USER_HANDLE_VALUE 0x314741494448534a
%define USER_HANDLE_PROGRESS 0x314b4f4c444e484a
struc USER_HANDLE_COUNTERS
 .calls: resq 1
 .query: resq 1
 .duplicate: resq 1
 .close: resq 1
 .bad_token: resq 1
 .denied: resq 1
 .invalid: resq 1
 .full: resq 1
 .revoked: resq 1
endstruc
%if USER_HANDLE_COUNTERS_size != 72
 %error Handle counter layout drift
%endif
%endif
