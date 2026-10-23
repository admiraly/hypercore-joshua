%ifndef JOSHUA_CONSOLE_INC
%define JOSHUA_CONSOLE_INC
%define EDITOR_MAGIC 0x3152444553484f4a
%define EDITOR_LINE_CAPACITY 127
%define EDITOR_BUFFER_SIZE 128
%define EDITOR_HISTORY_CAPACITY 8
struc CONSOLE_EDITOR
 .ready: resq 1
 .length: resq 1
 .cursor: resq 1
 .history_count: resq 1
 .history_next: resq 1
 .history_position: resq 1
 .draft_length: resq 1
 .draft_cursor: resq 1
 .submitted_length: resq 1
 .submissions: resq 1
 .rejected: resq 1
 .cancellations: resq 1
 .line: resb EDITOR_BUFFER_SIZE
 .draft: resb EDITOR_BUFFER_SIZE
 .submitted: resb EDITOR_BUFFER_SIZE
 .history_lengths: resq EDITOR_HISTORY_CAPACITY
 .history: resb EDITOR_HISTORY_CAPACITY*EDITOR_BUFFER_SIZE
endstruc
%if CONSOLE_EDITOR_size != 1568 || CONSOLE_EDITOR.line != 96 || CONSOLE_EDITOR.history != 544
 %error Console editor layout mismatch
%endif
%define GRID_MAGIC 0x3144495247484f4a
%define GRID_MAX_COLUMNS 100
%define GRID_MAX_ROWS 48
struc CONSOLE_SURFACE
 .base: resq 1
 .size: resq 1
 .width: resq 1
 .height: resq 1
 .stride: resq 1
 .format: resq 1
endstruc
struc CONSOLE_GRID
 .ready: resq 1
 .base: resq 1
 .size: resq 1
 .width: resq 1
 .height: resq 1
 .stride: resq 1
 .format: resq 1
 .columns: resq 1
 .rows: resq 1
 .dirty: resq 1
 .cursor_x: resq 1
 .cursor_y: resq 1
 .cursor_enabled: resq 1
 .cells: resb GRID_MAX_COLUMNS*GRID_MAX_ROWS
endstruc
%if CONSOLE_SURFACE_size != 48 || CONSOLE_GRID_size != 4904 || CONSOLE_GRID.cells != 104
 %error Console grid layout mismatch
%endif
%define COMMAND_MAGIC 0x31444e4d43484f4a
struc CONSOLE_COMMAND
 .ready: resq 1
 .command: resq 1
 .status: resq 1
 .json: resq 1
 .action: resq 1
endstruc
%if CONSOLE_COMMAND_size != 40
 %error Console command layout mismatch
%endif
%define FACTS_MAGIC 0x3154434146484f4a
%define RESPONSE_MAGIC 0x3150534552484f4a
%define RESPONSE_CAPACITY 4095
struc CONSOLE_FACTS
 .ready: resq 1
 .image_bytes: resq 1
 .free_pages: resq 1
 .live_allocations: resq 1
 .candidate_ranges: resq 1
 .owned_table_pages: resq 1
 .enabled_cpus: resq 1
 .apic_mode: resq 1
 .timer_eois: resq 1
 .keyboard_irqs: resq 1
 .keyboard_eois: resq 1
 .keyboard_drops: resq 1
 .process_a_state: resq 1
 .process_a_status: resq 1
 .process_b_state: resq 1
 .process_b_status: resq 1
endstruc
struc CONSOLE_RESPONSE
 .ready: resq 1
 .length: resq 1
 .body: resb RESPONSE_CAPACITY+1
endstruc
%if CONSOLE_FACTS_size != 128 || CONSOLE_RESPONSE_size != 4112 || CONSOLE_RESPONSE.body != 16
 %error Console response layout mismatch
%endif
%define MODEL_MAGIC 0x314c444f4d484f4a
struc CONSOLE_MODEL
 .ready: resq 1
 .transcript_row: resq 1
 .transcript_column: resq 1
 .completed: resq 1
 .command: resq 1
 .status: resq 1
 .json: resq 1
 .action: resq 1
endstruc
%if CONSOLE_MODEL_size != 64
 %error Console model layout mismatch
%endif
%endif
