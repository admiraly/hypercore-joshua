; Authored static ELF64 profile. ELF offsets are relative to user_elf_start.
user_elf_start:
%if JOSHUA_ELF_TEST = 1
    db 0,'ELF',2,1,1,0
%else
    db 0x7f,'ELF',2,1,1,0
%endif
    dq 0
    dw 2,62
    dd 1
%if JOSHUA_ELF_TEST = 2
    dq USER_BASE+4096,64,0
%else
    dq USER_BASE+user_payload_start-user_code_start,64,0
%endif
    dd 0
    dw 64,56,2,0,0,0
    dd 1,5
    dq 4096,USER_BASE,0,user_payload_end-user_code_start,4096,4096
    dd 1,6
    dq 8192,USER_BASE+4096,0,user_elf_end-user_elf_data,4096,4096
    times 4096-($-user_elf_start) db 0
user_code_start:
    times 16 db 0xcc        ; wrong entry at page start must fault, never fall through
%include 'kernel/user/payload.inc'
    times 8192-($-user_elf_start) db 0
user_elf_data:
    times 128 db 0
    dq 0x31464c4548534f4a,0x455441445f464c45
user_elf_end:
