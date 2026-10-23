; Normalize a raw leaf PAT index. RCX raw leaf, RDX leaf byte size -> EAX index.
paging_pat_index:
    mov rax,rcx
    shr rax,3
    and eax,3
    mov r8,rcx
    cmp rdx,4096
    jne .large
    shr r8,7
    jmp .bit
.large:
    shr r8,12
.bit:
    and r8d,1
    shl r8d,2
    or eax,r8d
    ret

