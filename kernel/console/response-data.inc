; Immutable original labels and relative descriptors; no absolute code pointers.
console_response_data:
response_json_start:
    db 123,34,99,111,109,109,97,110,100,34,58,34
response_json_start_end:
response_json_status:
    db 34,44,34,115,116,97,116,117,115,34,58,48
response_json_status_end:
response_json_key:
    db 44,34
response_json_key_end:
response_json_colon:
    db 34,58
response_json_colon_end:
response_json_end:
    db 125,10
response_json_end_end:
response_equals:
    db 61
response_equals_end:
response_lf:
    db 10
response_lf_end:
response_usage:
    db 85,83,65,71,69,32,69,82,82,79,82,32,40,50,41,10
response_usage_end:
response_unavailable:
    db 85,78,65,86,65,73,76,65,66,76,69,32,40,54,57,41,10
response_unavailable_end:
response_unknown:
    db 85,78,75,78,79,87,78,32,67,79,77,77,65,78,68,32,40,49,50,55,41,10
response_unknown_end:
response_help:
    db 72,69,76,80,32,65,66,79,85,84,32,83,84,65,84,85,83,32,77,69,77,32,80,83,32,68,69,86,73,67,69,83,32,67,76,69,65,82,32,72,65,76,84,10,83,84,65,84,85,83,124,77,69,77,124,80,83,124,68,69,86,73,67,69,83,32,45,45,106,115,111,110,10
response_help_end:
response_about:
    db 74,79,83,72,85,65,32,47,32,72,89,80,69,82,67,79,82,69,10,66,83,80,32,68,69,86,69,76,79,80,77,69,78,84,32,67,79,78,83,79,76,69,10,65,83,83,69,77,66,76,89,32,82,85,78,84,73,77,69,46,32,78,79,32,65,73,32,69,88,69,67,85,84,73,79,78,46,10,81,69,77,85,32,69,86,73,68,69,78,67,69,59,32,80,72,89,83,73,67,65,76,32,81,85,65,76,73,70,73,67,65,84,73,79,78,32,79,80,69,78,46,10
response_about_end:
response_clear:
    db 67,79,78,83,79,76,69,32,67,76,69,65,82,69,68,10
response_clear_end:
response_halt:
    db 72,65,76,84,73,78,71,58,32,81,85,73,69,83,67,73,78,71,32,75,69,89,66,79,65,82,68,10
response_halt_end:
response_status_lower:
    db 115,116,97,116,117,115
response_status_lower_end:
response_status_upper:
    db 83,84,65,84,85,83
response_status_upper_end:
response_mem_lower:
    db 109,101,109
response_mem_lower_end:
response_mem_upper:
    db 77,69,77
response_mem_upper_end:
response_ps_lower:
    db 112,115
response_ps_lower_end:
response_ps_upper:
    db 80,83
response_ps_upper_end:
response_devices_lower:
    db 100,101,118,105,99,101,115
response_devices_lower_end:
response_devices_upper:
    db 68,69,86,73,67,69,83
response_devices_upper_end:
response_key_image_bytes:
    db 105,109,97,103,101,95,98,121,116,101,115
response_key_image_bytes_end:
response_key_free_pages:
    db 102,114,101,101,95,112,97,103,101,115
response_key_free_pages_end:
response_key_live_allocations:
    db 108,105,118,101,95,97,108,108,111,99,97,116,105,111,110,115
response_key_live_allocations_end:
response_key_candidate_ranges:
    db 99,97,110,100,105,100,97,116,101,95,114,97,110,103,101,115
response_key_candidate_ranges_end:
response_key_owned_table_pages:
    db 111,119,110,101,100,95,116,97,98,108,101,95,112,97,103,101,115
response_key_owned_table_pages_end:
response_key_enabled_cpus:
    db 101,110,97,98,108,101,100,95,99,112,117,115
response_key_enabled_cpus_end:
response_key_apic_mode:
    db 97,112,105,99,95,109,111,100,101
response_key_apic_mode_end:
response_key_timer_eois:
    db 116,105,109,101,114,95,101,111,105,115
response_key_timer_eois_end:
response_key_keyboard_irqs:
    db 107,101,121,98,111,97,114,100,95,105,114,113,115
response_key_keyboard_irqs_end:
response_key_keyboard_eois:
    db 107,101,121,98,111,97,114,100,95,101,111,105,115
response_key_keyboard_eois_end:
response_key_keyboard_drops:
    db 107,101,121,98,111,97,114,100,95,100,114,111,112,115
response_key_keyboard_drops_end:
response_key_process_a_state:
    db 112,114,111,99,101,115,115,95,97,95,115,116,97,116,101
response_key_process_a_state_end:
response_key_process_a_status:
    db 112,114,111,99,101,115,115,95,97,95,115,116,97,116,117,115
response_key_process_a_status_end:
response_key_process_b_state:
    db 112,114,111,99,101,115,115,95,98,95,115,116,97,116,101
response_key_process_b_state_end:
response_key_process_b_status:
    db 112,114,111,99,101,115,115,95,98,95,115,116,97,116,117,115
response_key_process_b_status_end:
response_fields_status:
    dd response_key_image_bytes-console_response_data, response_key_image_bytes_end-response_key_image_bytes, CONSOLE_FACTS.image_bytes
    dd response_key_enabled_cpus-console_response_data, response_key_enabled_cpus_end-response_key_enabled_cpus, CONSOLE_FACTS.enabled_cpus
    dd response_key_apic_mode-console_response_data, response_key_apic_mode_end-response_key_apic_mode, CONSOLE_FACTS.apic_mode
    dd response_key_timer_eois-console_response_data, response_key_timer_eois_end-response_key_timer_eois, CONSOLE_FACTS.timer_eois
response_fields_mem:
    dd response_key_free_pages-console_response_data, response_key_free_pages_end-response_key_free_pages, CONSOLE_FACTS.free_pages
    dd response_key_live_allocations-console_response_data, response_key_live_allocations_end-response_key_live_allocations, CONSOLE_FACTS.live_allocations
    dd response_key_candidate_ranges-console_response_data, response_key_candidate_ranges_end-response_key_candidate_ranges, CONSOLE_FACTS.candidate_ranges
    dd response_key_owned_table_pages-console_response_data, response_key_owned_table_pages_end-response_key_owned_table_pages, CONSOLE_FACTS.owned_table_pages
response_fields_ps:
    dd response_key_process_a_state-console_response_data, response_key_process_a_state_end-response_key_process_a_state, CONSOLE_FACTS.process_a_state
    dd response_key_process_a_status-console_response_data, response_key_process_a_status_end-response_key_process_a_status, CONSOLE_FACTS.process_a_status
    dd response_key_process_b_state-console_response_data, response_key_process_b_state_end-response_key_process_b_state, CONSOLE_FACTS.process_b_state
    dd response_key_process_b_status-console_response_data, response_key_process_b_status_end-response_key_process_b_status, CONSOLE_FACTS.process_b_status
response_fields_devices:
    dd response_key_apic_mode-console_response_data, response_key_apic_mode_end-response_key_apic_mode, CONSOLE_FACTS.apic_mode
    dd response_key_enabled_cpus-console_response_data, response_key_enabled_cpus_end-response_key_enabled_cpus, CONSOLE_FACTS.enabled_cpus
    dd response_key_keyboard_irqs-console_response_data, response_key_keyboard_irqs_end-response_key_keyboard_irqs, CONSOLE_FACTS.keyboard_irqs
    dd response_key_keyboard_eois-console_response_data, response_key_keyboard_eois_end-response_key_keyboard_eois, CONSOLE_FACTS.keyboard_eois
    dd response_key_keyboard_drops-console_response_data, response_key_keyboard_drops_end-response_key_keyboard_drops, CONSOLE_FACTS.keyboard_drops
response_diagnostics:
    dd response_status_upper-console_response_data, response_status_upper_end-response_status_upper
    dd response_status_lower-console_response_data, response_status_lower_end-response_status_lower
    dd response_fields_status-console_response_data, 4
    dd response_mem_upper-console_response_data, response_mem_upper_end-response_mem_upper
    dd response_mem_lower-console_response_data, response_mem_lower_end-response_mem_lower
    dd response_fields_mem-console_response_data, 4
    dd response_ps_upper-console_response_data, response_ps_upper_end-response_ps_upper
    dd response_ps_lower-console_response_data, response_ps_lower_end-response_ps_lower
    dd response_fields_ps-console_response_data, 4
    dd response_devices_upper-console_response_data, response_devices_upper_end-response_devices_upper
    dd response_devices_lower-console_response_data, response_devices_lower_end-response_devices_lower
    dd response_fields_devices-console_response_data, 5
