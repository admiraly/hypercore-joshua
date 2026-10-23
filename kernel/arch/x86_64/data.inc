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
align 8
candidate_set: dq 0,0,CANDIDATE_CAPACITY
align 8
paging_inventory: dq 0,0,PAGING_TABLE_CAPACITY,0,0,0,0,0,0
paging_cr3: dq 0
align 8
frame_allocator: times FRAME_ALLOCATOR_size db 0
align 8
owned_paging: times OWNED_PAGING_size db 0
align 8
mapping_context: times MAPPING_CONTEXT_size db 0
mapping_records: times MAPPING_RECORD_CAPACITY*MAPPING_RECORD_size db 0
align 8
kernel_tss: times 104 db 0
stack_guards: times STACK_GUARD_COUNT dq 0
ist_tops: times 4 dq 0
kernel_tss_selector: dw 0
align 8
acpi_context: times ACPI_CONTEXT_size db 0
acpi_io_records: times 8*16 db 0
controller_pt: dq 0
cache_context: times CACHE_CONTEXT_size db 0
apic_context: times APIC_CONTEXT_size db 0

%if JOSHUA_CONSOLE_TEST = 3
; Finite fixture trace only; incremented after frame and actual ISR44 ownership.
console_fixture_irqs: dq 0
%endif

%if JOSHUA_CONSOLE_TEST >= 4
console_timer_fixture: times CONSOLE_TIMER_FIXTURE_size db 0
%endif
