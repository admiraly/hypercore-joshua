; RCX points to RANGE_SET, RDX reservation start, R8 exclusive end.
; Caller owns validated, disjoint page-aligned entries; no allocator consumes them.
; EAX=0 success, 1 invalid bounds/capacity. Failure invalidates the entire set:
; earlier entries may already be changed. All nonvolatile registers preserved.
range_reserve:
    test rdx,0xfff
    jnz .invalid
    test r8,0xfff
    jnz .invalid
    cmp rdx,r8
    jae .invalid
    mov r9,[rcx+RANGE_SET.entries]
    test r9,r9
    jz .invalid
    mov r10,[rcx+RANGE_SET.count]
    cmp r10,[rcx+RANGE_SET.capacity]
    ja .invalid
    xor r11d,r11d
.next:
    cmp r11,r10
    jae .done
    mov rax,r11
    shl rax,4
    add rax,r9
    cmp rdx,[rax+8]
    jae .advance
    cmp r8,[rax]
    jbe .advance
    cmp rdx,[rax]
    ja .tail
    cmp r8,[rax+8]
    jb .trim_front
    ; Delete by moving the last entry; recheck its intersection at this index.
    dec r10
    push rdx
    mov rdx,r10
    shl rdx,4
    push r8
    mov r8,[r9+rdx]
    mov [rax],r8
    mov r8,[r9+rdx+8]
    mov [rax+8],r8
    pop r8
    pop rdx
    jmp .next
.trim_front:
    mov [rax],r8
    jmp .advance
.tail:
    cmp r8,[rax+8]
    jae .trim_back
    cmp r10,[rcx+RANGE_SET.capacity]
    jae .invalid
    ; Split into [oldstart,reservationstart) and [reservationend,oldend).
    push rdx
    mov rdx,r10
    shl rdx,4
    mov [r9+rdx],r8
    push r8
    mov r8,[rax+8]
    mov [r9+rdx+8],r8
    pop r8
    pop rdx
    inc r10
.trim_back:
    mov [rax+8],rdx
.advance:
    inc r11
    jmp .next
.done:
    mov [rcx+RANGE_SET.count],r10
    xor eax,eax
    ret
.invalid:
    mov eax,1
    ret
