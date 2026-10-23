debug_exit: db 'JOSHUA:EBS_OK',13,10,0
debug_halted: db 'JOSHUA:KERNEL_HALTED',13,10,0
debug_fault: db 'JOSHUA:KERNEL_FAULT',13,10,0
screen_title: db 'JOSHUA OS / HYPERCORE',0
screen_subtitle: db 'ASSEMBLY NATIVE BOOT PROTOTYPE',0
screen_exit: db 'UEFI BOOT SERVICES EXITED',0
screen_stack: db 'OWN STACK / GDT / STOP IDT ACTIVE',0
screen_memory: db 'OWNED PAGING / W-X / NX / STACK GUARD',0
screen_disk: db 'NO DISK WRITES / NO INSTALLER',0
screen_limits: db 'NO SHELL / DRIVERS / AI YET',0
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

debug_vector: db 'JOSHUA:FAULT VECTOR=',0
debug_error: db ' ERROR=',0
debug_rip: db ' RIP=',0
debug_cr2: db ' CR2=',0
debug_newline: db 13,10,0
%if JOSHUA_FAULT_TEST || JOSHUA_CONTEXT_TEST = 3 || JOSHUA_USER_TEST = 25
debug_fixture_rip: db 'JOSHUA:FIXTURE_RIP=',0
debug_fixture_target: db 'JOSHUA:FIXTURE_TARGET=',0
%endif
debug_map_valid: db 'JOSHUA:MEMORY_MAP_VALID',13,10,0
debug_candidates: db 'JOSHUA:MEMORY_CANDIDATES_READY COUNT=',0
debug_paging: db 'JOSHUA:PAGING_RESERVED CR3=',0
debug_table_count: db ' TABLES=',0
debug_frames: db 'JOSHUA:FRAME_ALLOCATOR_OK FREE_PAGES=',0
debug_owned: db 'JOSHUA:OWNED_PAGING CR3=',0
debug_lifecycle: db 'JOSHUA:MAPPING_LIFECYCLE_OK',13,10,0
debug_tss: db 'JOSHUA:TSS_READY SELECTOR=',0
debug_ist: db 'JOSHUA:IST INDEX=',0
debug_start: db ' START=',0
debug_end: db ' END=',0
debug_fault_stack: db 'JOSHUA:FAULT_STACK FRAME=',0
debug_old_rsp: db ' OLD_RSP=',0
debug_ist_index: db ' IST=',0
debug_base: db ' BASE=',0
%if JOSHUA_FAULT_TEST
debug_fixture_old_rsp: db 'JOSHUA:FIXTURE_OLD_RSP=',0
%endif
debug_acpi_ready: db 'JOSHUA:ACPI_TOPOLOGY_READY BSP=',0
debug_acpi_cpus: db ' CPUS=',0
debug_acpi_enabled: db ' ENABLED=',0
debug_acpi_lapic: db ' LAPIC=',0
debug_acpi_failure: db 'JOSHUA:ACPI_FAILURE CODE=',0
align 4
apic_lvt_offsets: dd 0x330,0x340,0x350,0x360,0x370,0x2f0
debug_apic_ready: db 'JOSHUA:APIC_READY MODE=',0
debug_uc_pages: db ' UC_PAGES=',0
debug_io_count: db ' IOAPICS=',0
debug_apic_failure: db 'JOSHUA:APIC_FAILURE CODE=',0
align 8
irq_sentinel_values:
%assign irq_value 1
%rep 15
    dq 0x123456789abcde00+irq_value
%assign irq_value irq_value+1
%endrep
debug_timer_ok: db 'JOSHUA:TIMER_OK MODE=',0
debug_timer_ticks: db ' TICKS=',0
debug_timer_eois: db ' EOIS=',0
debug_timer_returns: db ' RETURNS=',0

debug_context_ok: db 'JOSHUA:CONTEXT_SWITCH_OK TICKS=',0
debug_context_progress1: db ' PROGRESS1=',0
debug_context_progress2: db ' PROGRESS2=',0
align 8
context_worker1_values:
%assign context_value 0
%rep 15
 dq 0x123456789abc0000+context_value
%assign context_value context_value+1
%endrep
context_worker2_values:
%assign context_value 0
%rep 15
 dq 0xfedcba9876540000+context_value
%assign context_value context_value+1
%endrep

debug_context_failure: db 'JOSHUA:CONTEXT_FAILURE STATE=',0
debug_context_status: db 'JOSHUA:CONTEXT_STATUS=',0
debug_context_current: db ' CURRENT=',0
debug_context_id: db 'JOSHUA:CONTEXT_INHERITED_ID_SET',13,10,0
debug_context_stack: db 'JOSHUA:CONTEXT_STACK INDEX=',0

debug_user_roots: db 'JOSHUA:USER_ROOTS_PROVED ROOT_A=',0
debug_user_root_b: db ' ROOT_B=',0
debug_user_entry_stack: db 'JOSHUA:USER_ENTRY_STACK INDEX=',0

debug_user_machine: db 'JOSHUA:USER_MACHINE_PREPARED',13,10,0

debug_user_smoke: db 'JOSHUA:USER_PAIR_RETURNED',13,10,0
debug_user_failure: db 'JOSHUA:USER_TRANSITION_FAILURE STEP=',0

debug_user_result: db 'JOSHUA:USER_RESULT INDEX=',0
debug_user_result_root: db ' ROOT=',0
debug_user_result_cs: db ' CS=',0
debug_user_result_state: db ' STATE=',0
debug_user_result_unknown: db ' UNKNOWN=',0
debug_user_result_ticks: db ' TICKS=',0
debug_user_result_eois: db ' EOIS=',0
debug_user_pending: db ' PENDING=',0
debug_user_windows: db ' WINDOWS=',0
debug_user_ldt: db 'JOSHUA:USER_INHERITED_LDT_SET',13,10,0

%if JOSHUA_ELF_TEST
debug_elf_reject: db 'JOSHUA:ELF_REJECT BEFORE=',0
debug_elf_count_address: db ' COUNT_ADDRESS=',0
%endif

debug_keyboard_prepared: db 'JOSHUA:KEYBOARD_PREPARED GSI=',0
debug_keyboard_config: db ' CONFIG=',0
debug_keyboard_failure: db 'JOSHUA:KEYBOARD_FAILURE STAGE=',0
