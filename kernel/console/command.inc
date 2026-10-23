; RCX checked editor, RDX exclusive result. No hardware or executable user pointers.
; EAX0 parsed (including command errors), EAX1 invalid; EDX0, failure atomic.
command_parse:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp,72
    mov r12,rcx
    mov r13,rdx
    call editor_address_check
    test eax,eax
    jnz .bad
    test r13,r13
    jz .bad
    test r13,7
    jnz .bad
    mov rax,r13
    add rax,CONSOLE_COMMAND_size-1
    jc .bad
    mov rdx,r12
    add rdx,CONSOLE_EDITOR_size-1 ; already checked nonwrapping
    cmp r12,rax
    ja .disjoint
    cmp r13,rdx
    jbe .bad
.disjoint:
    mov rcx,r12
    call editor_check
    test eax,eax
    jnz .bad
    lea rsi,[r12+CONSOLE_EDITOR.submitted]
    mov rbp,[r12+CONSOLE_EDITOR.submitted_length]
    xor ebx,ebx
.leading:
    cmp rbx,rbp
    jae .empty
    cmp byte [rsi+rbx],32
    jne .first
    inc ebx
    jmp .leading
.first:
    lea r14,[rsi+rbx]
    mov r15,rbx
.word:
    cmp rbx,rbp
    jae .lookup
    cmp byte [rsi+rbx],32
    je .lookup
    inc ebx
    jmp .word
.lookup:
    sub r15,rbx
    neg r15
    call command_lookup
    mov r14d,eax
    xor edi,edi
.arguments:
    cmp rbx,rbp
    jae .decide
    cmp byte [rsi+rbx],32
    jne .argument
    inc ebx
    jmp .arguments
.argument:
    inc edi
    lea rax,[rsi+rbx]
    mov [rsp+32],rax
    mov [rsp+40],rbx
.arg_word:
    cmp rbx,rbp
    jae .arg_end
    cmp byte [rsi+rbx],32
    je .arg_end
    inc ebx
    jmp .arg_word
.arg_end:
    mov rax,rbx
    sub rax,[rsp+40]
    mov [rsp+40],rax
    cmp edi,2
    jae .usage ; no need to inspect later text, validated entire editor already
    jmp .arguments
.decide:
    xor ebx,ebx ; JSON
    xor ebp,ebp ; action
    cmp r14d,255
    je .unknown
    test edi,edi
    jnz .option
    cmp r14d,9
    jae .unavailable
    cmp r14d,7
    je .clear
    cmp r14d,8
    je .halt
    xor eax,eax
    jmp .publish
.option:
    cmp r14d,3
    jb .usage
    cmp r14d,6
    ja .usage
    cmp qword [rsp+40],6
    jne .usage
    mov rdx,[rsp+32]
    cmp dword [rdx],0x736a2d2d ; --js
    jne .usage
    cmp word [rdx+4],0x6e6f ; on
    jne .usage
    mov ebx,1
    xor eax,eax
    jmp .publish
.clear:
    mov ebp,1
    xor eax,eax
    jmp .publish
.halt:
    mov ebp,2
    xor eax,eax
    jmp .publish
.unavailable:
    mov eax,69
    jmp .publish
.unknown:
    mov eax,127
    jmp .publish
.usage:
    xor ebx,ebx
    xor ebp,ebp
    mov eax,2
    jmp .publish
.empty:
    xor r14d,r14d
    xor eax,eax
    xor ebx,ebx
    xor ebp,ebp
.publish:
    mov qword [r13+CONSOLE_COMMAND.ready],0
    mov [r13+CONSOLE_COMMAND.command],r14
    mov [r13+CONSOLE_COMMAND.status],rax
    mov [r13+CONSOLE_COMMAND.json],rbx
    mov [r13+CONSOLE_COMMAND.action],rbp
    mov rax,COMMAND_MAGIC
    mov [r13+CONSOLE_COMMAND.ready],rax
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    xor edx,edx
    add rsp,72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret

; R14 token pointer/R15 length -> EAX1..17 or255. Constant data, no dispatch pointers.
command_lookup:
    cmp r15,8
    ja .unknown
    lea r10,[rel console_command_names]
    mov r8d,1
.name:
    xor ecx,ecx
.character:
    cmp rcx,r15
    jae .end
    movzx eax,byte [r14+rcx]
    cmp eax,'A'
    jb .folded
    cmp eax,'Z'
    ja .folded
    add eax,32
.folded:
    cmp al,[r10+rcx]
    jne .next
    inc ecx
    jmp .character
.end:
    cmp byte [r10+rcx],0
    je .found
.next:
    add r10,9
    inc r8d
    cmp r8d,17
    jbe .name
.unknown:
    mov eax,255
    ret
.found:
    mov eax,r8d
    ret
