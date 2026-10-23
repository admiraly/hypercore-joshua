debug_console_ready: db 'JOSHUA:CONSOLE_READY MODEL=',0
debug_console_grid: db ' GRID=',0
debug_console_editor: db ' EDITOR=',0
debug_console_command: db ' COMMAND=',0
debug_console_response: db ' RESPONSE=',0
debug_console_facts: db ' FACTS=',0
debug_console_window: db ' WINDOW=',0
debug_console_decoder: db ' DECODER=',0
debug_console_controller: db ' CONTROLLER=',0
debug_console_halted: db 'JOSHUA:CONSOLE_HALTED',13,10,0
debug_console_failed: db 'JOSHUA:CONSOLE_FAILURE',13,10,0
console_halted_text: db 'HALTED / INTERRUPTS RETIRED'
console_halted_text_end:
%if console_halted_text_end-console_halted_text > 48
 %error Halt label exceeds console row
%endif

%if JOSHUA_CONSOLE_TEST != 0
debug_console_fixture: db 'JOSHUA:CONSOLE_FIXTURE ID=',0
%if JOSHUA_CONSOLE_TEST = 2 || JOSHUA_CONSOLE_TEST = 3
debug_console_fixture_source: db ' SOURCE=',0
%endif
%endif
