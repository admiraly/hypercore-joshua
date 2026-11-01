; Pure response partition; see docs/console-speech-design.md.
; RCX validated-response candidate, RDX disjoint 32776-byte output.
; On failure both buffers remain untouched. Caller proves stable mapped extents.
speech_segment:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,32
    mov rbx,rcx
    mov r12,rdx
    test rbx,rbx
    jz .bad
    test r12,r12
    jz .bad
    test bl,7
    jnz .bad
    test r12b,7
    jnz .bad
    mov rax,rbx
    add rax,CONSOLE_RESPONSE_size
    jc .bad
    mov r13,rax
    mov rax,r12
    add rax,SPEECH_TOKEN_OUTPUT_SIZE
    jc .bad
    cmp rbx,rax
    jae .disjoint
    cmp r12,r13
    jb .bad
.disjoint:
    mov rcx,rbx
    call response_check
    test eax,eax
    jnz .bad
    mov r14,[rbx+CONSOLE_RESPONSE.length]
    mov rdi,r12
    xor eax,eax
    mov ecx,SPEECH_TOKEN_OUTPUT_SIZE/8
    rep stosq
    xor esi,esi
    xor r15d,r15d
.next:
    cmp rsi,r14
    jae .success
    mov r13,rsi
    movzx eax,byte [rbx+CONSOLE_RESPONSE.body+rsi]
    mov edx,eax
    or edx,0x20
    sub edx,'a'
    cmp edx,25
    ja .single
    inc rsi
.word:
    cmp rsi,r14
    jae .word_done
    movzx edx,byte [rbx+CONSOLE_RESPONSE.body+rsi]
    or edx,0x20
    sub edx,'a'
    cmp edx,25
    ja .word_done
    inc rsi
    jmp .word
.word_done:
    mov edx,SPEECH_TOKEN_WORD
    jmp .store
.single:
    inc rsi
    mov edx,SPEECH_TOKEN_SPACE
    cmp eax,10
    je .store
    cmp eax,32
    je .store
    mov edx,SPEECH_TOKEN_DIGIT
    sub eax,'0'
    cmp eax,9
    jbe .store
    mov edx,SPEECH_TOKEN_SYMBOL
.store:
    lea rdi,[r12+16+r15*8]
    mov [rdi],r13w
    mov rax,rsi
    sub rax,r13
    mov [rdi+2],ax
    mov [rdi+4],edx
    inc r15
    jmp .next
.success:
    mov [r12+8],r15
    mov rax,SPEECH_TOKENS_MAGIC
    mov [r12],rax
    xor eax,eax
    xor edx,edx
    jmp .done
.bad:
    mov eax,1
    xor edx,edx
.done:
    add rsp,32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
