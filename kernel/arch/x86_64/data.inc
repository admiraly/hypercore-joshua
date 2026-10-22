align 8
kernel_gdtr:
    dw 0
    dq 0
firmware_gdtr:
    dw 0
    dq 0
kernel_code_selector: dw 0
kernel_data_selector: dw 0
kernel_idtr:
    dw 4095
    dq 0
frame_stride_qword: dq 0
debug_exit: db 'JOSHUA:EBS_OK',13,10,0
debug_halted: db 'JOSHUA:KERNEL_HALTED',13,10,0
debug_fault: db 'JOSHUA:KERNEL_FAULT',13,10,0
screen_title: db 'JOSHUA OS / HYPERCORE',0
screen_subtitle: db 'ASSEMBLY NATIVE BOOT PROTOTYPE',0
screen_exit: db 'UEFI BOOT SERVICES EXITED',0
screen_stack: db 'OWN STACK / GDT / STOP IDT ACTIVE',0
screen_memory: db 'MEMORY MAP CAPTURED / FIRMWARE PAGING',0
screen_disk: db 'NO DISK WRITES / NO INSTALLER',0
screen_limits: db 'NO SHELL / STORAGE / AI YET',0
screen_stop: db 'BOOT CHECKPOINT REACHED. POWER OFF TO EXIT.',0
font_characters: db 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.-:',0
font_rows:
    db 14,17,17,31,17,17,17
    db 30,17,17,30,17,17,30
    db 14,17,16,16,16,17,14
    db 30,17,17,17,17,17,30
    db 31,16,16,30,16,16,31
    db 31,16,16,30,16,16,16
    db 14,17,16,23,17,17,15
    db 17,17,17,31,17,17,17
    db 14,4,4,4,4,4,14
    db 7,2,2,2,18,18,12
    db 17,18,20,24,20,18,17
    db 16,16,16,16,16,16,31
    db 17,27,21,21,17,17,17
    db 17,25,21,19,17,17,17
    db 14,17,17,17,17,17,14
    db 30,17,17,30,16,16,16
    db 14,17,17,17,21,18,13
    db 30,17,17,30,20,18,17
    db 15,16,16,14,1,1,30
    db 31,4,4,4,4,4,4
    db 17,17,17,17,17,17,14
    db 17,17,17,17,17,10,4
    db 17,17,17,21,21,21,10
    db 17,17,10,4,10,17,17
    db 17,17,10,4,4,4,4
    db 31,1,2,4,8,16,31
    db 14,17,19,21,25,17,14
    db 4,12,4,4,4,4,14
    db 14,17,1,2,4,8,31
    db 30,1,1,14,1,1,30
    db 2,6,10,18,31,2,2
    db 31,16,16,30,1,1,30
    db 14,16,16,30,17,17,14
    db 31,1,2,4,8,8,8
    db 14,17,17,14,17,17,14
    db 14,17,17,15,1,1,14
    db 1,2,2,4,8,8,16
    db 0,0,0,0,0,12,12
    db 0,0,0,31,0,0,0
    db 0,12,12,0,12,12,0
