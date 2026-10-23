; Embedded position-independent Assembly payload, copied to private sealed pages.
; RDI is the public user data VA. This source page stays kernel RO/NX.
user_payload_start:
    mov rax,-1
    int 0x80
    cmp rax,-38
    jne .failed
    inc qword [rdi+16]
    xor eax,eax
    xor edi,edi
    int 0x80
    ud2
.failed:
    xor eax,eax
    mov edi,1
    int 0x80
    ud2
user_payload_end:
%if user_payload_end-user_payload_start > 4096
 %error User payload exceeds one private code page
%endif
