; Checked grid/renderer. Trusted disjoint mapped extents; BSP/IF0/DF0.
; Every public helper returns EAX0/1, EDX0 and preserves integer nonvolatiles.
%macro grid_enter 0
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
%endmacro
%macro grid_leave 0
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
%endmacro

; Internal geometry/range proof, returns R8 columns/R9 rows on success.
grid_preflight:
    grid_enter
    test r12,r12
    jz .bad
    test r12,7
    jnz .bad
    mov rax,r12
    add rax,CONSOLE_GRID_size-1
    jc .bad
    test r13,r13
    jz .bad
    test r13,7
    jnz .bad
    mov rbx,r13
    add rbx,CONSOLE_SURFACE_size-1
    jc .bad
    ; Inclusive grid/anchor intervals must not intersect.
    cmp r12,rbx
    ja .records_disjoint
    cmp r13,rax
    jbe .bad
.records_disjoint:
    mov rsi,[r13+CONSOLE_SURFACE.base]
    test rsi,rsi
    jz .bad
    mov rdi,[r13+CONSOLE_SURFACE.size]
    test rdi,rdi
    jz .bad
    dec rdi
    add rdi,rsi
    jc .bad
    cmp rsi,rax
    ja .grid_disjoint
    cmp r12,rdi
    jbe .bad
.grid_disjoint:
    cmp rsi,rbx
    ja .anchor_disjoint
    cmp r13,rdi
    jbe .bad
.anchor_disjoint:
    cmp qword [r13+CONSOLE_SURFACE.format],1
    ja .bad
    mov rax,[r13+CONSOLE_SURFACE.width]
    cmp rax,32+48*12
    jb .bad
    mov rbp,[r13+CONSOLE_SURFACE.height]
    cmp rbp,32+20*16
    jb .bad
    mov rbx,[r13+CONSOLE_SURFACE.stride]
    cmp rbx,rax
    jb .bad
    ; Full visible scanlines including stride padding must fit capability.
    mov rax,rbx
    mul rbp
    test rdx,rdx
    jnz .bad
    mov ecx,4
    mul rcx
    test rdx,rdx
    jnz .bad
    cmp rax,[r13+CONSOLE_SURFACE.size]
    ja .bad
    mov rax,[r13+CONSOLE_SURFACE.width]
    sub rax,32
    xor edx,edx
    mov ecx,12
    div rcx
    mov r8d,100
    cmp rax,r8
    cmovb r8,rax
    sub rbp,32
    shr rbp,4
    mov r9d,48
    cmp rbp,r9
    cmovb r9,rbp
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    grid_leave

grid_init:
    grid_enter
    call grid_preflight
    test eax,eax
    jnz .return
    mov r14,r8
    mov r15,r9
    mov rdi,r12
    xor eax,eax
    mov ecx,CONSOLE_GRID_size/8
    rep stosq
    lea rdi,[r12+CONSOLE_GRID.base]
    mov rsi,r13
    mov ecx,CONSOLE_SURFACE_size/8
    rep movsq
    mov [r12+CONSOLE_GRID.columns],r14
    mov [r12+CONSOLE_GRID.rows],r15
    mov ecx,r15d
    mov eax,1
    shl rax,cl
    dec rax
    mov [r12+CONSOLE_GRID.dirty],rax
    lea rdi,[r12+CONSOLE_GRID.cells]
    mov ebx,r15d
.row:
    mov ecx,r14d
    mov al,32
    rep stosb
    add rdi,100
    sub rdi,r14
    dec ebx
    jnz .row
    mov rax,GRID_MAGIC
    mov [r12+CONSOLE_GRID.ready],rax
    xor eax,eax
.return:
    grid_leave

grid_check:
    grid_enter
    call grid_preflight
    test eax,eax
    jnz .return
    mov rax,GRID_MAGIC
    cmp [r12+CONSOLE_GRID.ready],rax
    jne .bad
    xor ebx,ebx
.surface:
    mov rax,[r13+rbx]
    cmp [r12+CONSOLE_GRID.base+rbx],rax
    jne .bad
    add ebx,8
    cmp ebx,CONSOLE_SURFACE_size
    jb .surface
    cmp [r12+CONSOLE_GRID.columns],r8
    jne .bad
    cmp [r12+CONSOLE_GRID.rows],r9
    jne .bad
    mov ecx,r9d
    mov rax,[r12+CONSOLE_GRID.dirty]
    shr rax,cl
    jnz .bad
    cmp qword [r12+CONSOLE_GRID.cursor_enabled],1
    ja .bad
    cmp [r12+CONSOLE_GRID.cursor_x],r8
    jae .bad
    cmp [r12+CONSOLE_GRID.cursor_y],r9
    jae .bad
    lea rsi,[r12+CONSOLE_GRID.cells]
    xor ebx,ebx
.row:
    xor ebp,ebp
.cell:
    movzx eax,byte [rsi]
    cmp rbx,r9
    jae .inactive
    cmp rbp,r8
    jae .inactive
    sub eax,32
    cmp eax,94
    ja .bad
    jmp .next
.inactive:
    test eax,eax
    jnz .bad
.next:
    inc rsi
    inc ebp
    cmp ebp,100
    jb .cell
    inc ebx
    cmp ebx,48
    jb .row
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    grid_leave

; Shared mutation body. R14 index, R15 value, EBX=0 cell /1 cursor.
grid_cell:
    grid_enter
    xor ebx,ebx
    jmp grid_mutate
grid_cursor:
    grid_enter
    mov ebx,1
grid_mutate:
    mov r14,r8
    mov r15,r9
    call grid_check
    test eax,eax
    jnz .return
    cmp r14,4799
    ja .bad
    mov rax,r14
    xor edx,edx
    mov ecx,100
    div rcx
    cmp rax,[r12+CONSOLE_GRID.rows]
    jae .bad
    cmp rdx,[r12+CONSOLE_GRID.columns]
    jae .bad
    test ebx,ebx
    jnz .cursor
    cmp r15,32
    jb .bad
    cmp r15,126
    ja .bad
    cmp byte [r12+CONSOLE_GRID.cells+r14],r15b
    je .ok
    mov [r12+CONSOLE_GRID.cells+r14],r15b
    bts [r12+CONSOLE_GRID.dirty],rax
    jmp .ok
.cursor:
    cmp r15,1
    ja .bad
    cmp [r12+CONSOLE_GRID.cursor_x],rdx
    jne .cursor_change
    cmp [r12+CONSOLE_GRID.cursor_y],rax
    jne .cursor_change
    cmp [r12+CONSOLE_GRID.cursor_enabled],r15
    je .ok
.cursor_change:
    mov rcx,[r12+CONSOLE_GRID.cursor_y]
    bts [r12+CONSOLE_GRID.dirty],rcx
    bts [r12+CONSOLE_GRID.dirty],rax
    mov [r12+CONSOLE_GRID.cursor_x],rdx
    mov [r12+CONSOLE_GRID.cursor_y],rax
    mov [r12+CONSOLE_GRID.cursor_enabled],r15
.ok:
    xor eax,eax
    jmp .return
.bad:
    mov eax,1
.return:
    grid_leave

; Each bounded cell paints its 12x16 rectangle. Full surface proof precedes stores.
grid_render:
    grid_enter
    call grid_check
    test eax,eax
    jnz .return
    mov dword [rsp+32],0x000c0602 ; RGB format0: R is low byte
    mov dword [rsp+36],0x00ffdcaa
    cmp qword [r12+CONSOLE_GRID.format],0
    je .colors
    mov dword [rsp+32],0x0002060c
    mov dword [rsp+36],0x00aadcff
.colors:
    lea rax,[rel console_font]
    mov [rsp+40],rax
    xor ebx,ebx
.row:
    bt [r12+CONSOLE_GRID.dirty],rbx
    jnc .next_row
    xor ebp,ebp
.cell:
    imul rax,rbx,100
    add rax,rbp
    movzx eax,byte [r12+CONSOLE_GRID.cells+rax]
    sub eax,32
    imul eax,7
    add rax,[rsp+40]
    mov [rsp+48],rax
    xor esi,esi
.scanline:
    ; Row offset cannot overflow: preflight proved stride*height*4 fits size.
    mov rax,rbx
    shl rax,4
    add rax,16
    add rax,rsi
    imul rax,[r12+CONSOLE_GRID.stride]
    imul rdx,rbp,12
    add rax,rdx
    add rax,16
    lea r14,[rax*4]
    add r14,[r12+CONSOLE_GRID.base]
    xor r15d,r15d
    cmp esi,14
    jae .pixels
    mov rax,rsi
    shr eax,1
    mov rdx,[rsp+48]
    movzx r15d,byte [rdx+rax]
.pixels:
    xor edi,edi
.pixel:
    mov eax,[rsp+32]
    cmp esi,14
    jae .cursor
    cmp edi,10
    jae .store
    mov ecx,edi
    shr ecx,1
    mov edx,4
    sub edx,ecx
    bt r15d,edx
    jnc .store
    mov eax,[rsp+36]
    jmp .store
.cursor:
    cmp qword [r12+CONSOLE_GRID.cursor_enabled],1
    jne .store
    cmp [r12+CONSOLE_GRID.cursor_x],rbp
    jne .store
    cmp [r12+CONSOLE_GRID.cursor_y],rbx
    jne .store
    mov eax,[rsp+36]
.store:
    mov [r14+rdi*4],eax
    inc edi
    cmp edi,12
    jb .pixel
    inc esi
    cmp esi,16
    jb .scanline
    inc ebp
    cmp rbp,[r12+CONSOLE_GRID.columns]
    jb .cell
.next_row:
    inc ebx
    cmp rbx,[r12+CONSOLE_GRID.rows]
    jb .row
    mov qword [r12+CONSOLE_GRID.dirty],0
    xor eax,eax
.return:
    grid_leave
%unmacro grid_enter 0
%unmacro grid_leave 0
