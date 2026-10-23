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
%endif
