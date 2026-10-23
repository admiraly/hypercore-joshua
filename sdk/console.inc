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
%endif
