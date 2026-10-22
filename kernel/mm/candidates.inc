; RCX boot-info v1, EDX physical width. The final map is already validated.
; Require the complete loaded-image allocation to be covered by loader descriptors.
; Firmware ABI; EAX=0 success, 1 invalid. No writes or firmware calls.
memory_image_check:
    push rbx
    push rsi
    mov r10,[rcx+BOOT_INFO_V1.image_base]
    test r10,r10
    jz .bad
    test r10,0xfff
    jnz .bad
    mov r11,[rcx+BOOT_INFO_V1.image_size]
    cmp r11,PE_IMAGE_SIZE
    jb .bad
    test r11,0xfff
    jnz .bad
    add r11,r10
    jc .bad
    mov r8,1
    push rcx
    mov ecx,edx
    shl r8,cl
    pop rcx
    cmp r11,r8
    ja .bad
    mov rbx,[rcx+BOOT_INFO.memory_map]
    mov r9,rbx
    add r9,[rcx+BOOT_INFO.memory_map_size]
.cover:
    mov rsi,rbx
.scan:
    cmp rsi,r9
    jae .bad
    cmp dword [rsi],1
    je .loader
    cmp dword [rsi],2
    jne .next
.loader:
    mov rax,[rsi+8]
    cmp r10,rax
    jb .next
    mov rdx,[rsi+24]
    shl rdx,12
    add rdx,rax
    cmp r10,rdx
    jae .next
    mov r10,rdx
    cmp r10,r11
    jb .cover
    xor eax,eax
    jmp .done
.next:
    add rsi,[rcx+BOOT_INFO.descriptor_size]
    jmp .scan
.bad:
    mov eax,1
.done:
    pop rsi
    pop rbx
    ret

; RCX validated boot-info v1, RDX RANGE_SET, R8D validated physical width.
; Construct candidates only: they must NOT be allocated until active paging
; structures are inventoried and reserved in the following stage.
memory_candidates_build:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp,40
    mov r12,rcx
    mov r13,rdx
    mov r14,1
    mov ecx,r8d
    shl r14,cl
    mov qword [r13+RANGE_SET.count],0
    mov rdi,[r13+RANGE_SET.entries]
    test rdi,rdi
    jz .bad
    mov rsi,[r12+BOOT_INFO.memory_map]
    mov rbx,rsi
    add rbx,[r12+BOOT_INFO.memory_map_size]
.descriptor:
    cmp rsi,rbx
    jae .reserve
    cmp dword [rsi],7
    jne .next
    ; UEFI cache-attribute bits describe supported cache policies. Accept
    ; UC/WC/WT alongside WB, but reject protection/runtime/unknown attributes.
    mov rax,[rsi+32]
    test eax,8
    jz .next
    test rax,-16
    jnz .next
    mov rax,[r13+RANGE_SET.count]
    cmp rax,[r13+RANGE_SET.capacity]
    jae .bad
    shl rax,4
    mov rdx,[rsi+8]
    mov [rdi+rax],rdx
    mov rcx,[rsi+24]
    shl rcx,12
    add rcx,rdx
    mov [rdi+rax+8],rcx
    inc qword [r13+RANGE_SET.count]
.next:
    add rsi,[r12+BOOT_INFO.descriptor_size]
    jmp .descriptor
.reserve:
    mov rcx,r13
    xor edx,edx
    mov r8d,0x1000
    call range_reserve
    test eax,eax
    jnz .bad
    mov rcx,r13
    mov rdx,[r12+BOOT_INFO_V1.image_base]
    mov r8,rdx
    add r8,[r12+BOOT_INFO_V1.image_size]
    call range_reserve
    test eax,eax
    jnz .bad
    mov rcx,r13
    mov rdx,[r12+BOOT_INFO.framebuffer]
    mov r8,rdx
    add r8,[r12+BOOT_INFO.framebuffer_size]
    jc .bad
    cmp r8,r14
    ja .bad
    and rdx,-4096
    add r8,4095
    jc .bad
    and r8,-4096
    call range_reserve
    test eax,eax
    jnz .bad
    xor eax,eax
    jmp .done
.bad:
    mov qword [r13+RANGE_SET.count],0
    mov eax,1
.done:
    add rsp,40
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
