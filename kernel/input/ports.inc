; Production fixed i8042 port adapters. Only called after positive ACPI authority
; and masked-route/BSP/IF0 proof; no A20, output-port or reset command is exposed.
keyboard_status:
    xor eax,eax
    in al,0x64
    ret
keyboard_data_read:
    xor eax,eax
    in al,0x60
    ret
keyboard_command_write:
    mov eax,ecx
    out 0x64,al
    ret
keyboard_data_write:
    mov eax,ecx
    out 0x60,al
    ret
