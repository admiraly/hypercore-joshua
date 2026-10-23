%ifndef JOSHUA_ELF_INC
%define JOSHUA_ELF_INC
%define ELF_READY_MAGIC 0x31464c4548534f4a
%define ELF_USER_BASE 0x0000400000000000
%define ELF_MAX_BYTES 12288
%define ELF_BACKING_BYTES 24576
struc ELF_PLAN
 .source: resq 1
 .length: resq 1
 .code_offset: resq 1
 .code_size: resq 1
 .data_offset: resq 1
 .data_size: resq 1
 .entry_offset: resq 1
 .ready: resq 1
endstruc
%endif
