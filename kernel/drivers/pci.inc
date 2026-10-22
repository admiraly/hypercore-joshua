; PCI configuration mechanism #1 (ports 0xCF8/0xCFC).
; A device address is bus<<16 | device<<11 | function<<8.

; ecx = device address, edx = register -> eax = dword
pci_read32:
    mov eax,ecx
    or eax,0x80000000
    and edx,0xfc
    or eax,edx
    mov dx,0xcf8
    out dx,eax
    mov dx,0xcfc
    in eax,dx
    ret

; ecx = device address, edx = even register, r8w = value
pci_write16:
    mov eax,ecx
    or eax,0x80000000
    mov r9d,edx
    and edx,0xfc
    or eax,edx
    mov dx,0xcf8
    out dx,eax
    mov edx,r9d
    and edx,2
    add edx,0xcfc
    mov eax,r8d
    out dx,ax
    ret

; ecx = class<<8 | subclass -> eax = first matching device address, or -1
pci_find_class:
    push rbx
    push r12
    mov r12d,ecx
    xor ebx,ebx
.probe:
    mov ecx,ebx
    xor edx,edx
    call pci_read32
    cmp ax,0xffff
    je .absent
    mov ecx,ebx
    mov edx,8
    call pci_read32
    shr eax,16
    cmp eax,r12d
    je .found
    test ebx,0x700
    jnz .next
    mov ecx,ebx
    mov edx,0x0c
    call pci_read32
    test eax,0x00800000
    jnz .next
    or ebx,0x700
    jmp .next
.absent:
    test ebx,0x700
    jnz .next
    or ebx,0x700
.next:
    add ebx,0x100
    cmp ebx,0x1000000
    jb .probe
    mov eax,-1
    jmp .done
.found:
    mov eax,ebx
.done:
    pop r12
    pop rbx
    ret
