; Privileged adapter; host fixtures replace these two routines, never runtime.
; RCX active builder -> EAX=0 if BSP serialized assumptions hold, else 4.
lifecycle_platform_check:
    pushfq
    pop rax
    test eax,0x200
    jnz .bad
    mov rax,cr3
    cmp rax,[rcx+OWNED_PAGING.root]
    jne .bad
    mov rax,cr4
    test eax,0x20080
    jnz .bad
    xor eax,eax
    ret
.bad:
    mov eax,4
    ret
; RCX context, RDX arena VA. Aligned PTE stores precede local invalidation.
lifecycle_invalidate:
    invlpg [rdx]
    ret
