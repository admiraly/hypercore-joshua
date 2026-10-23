; Fixed nine-byte zero-padded names in command-ID order. No function pointers.
console_command_names:
%macro console_name 1
 %%start: db %1,0
 times 9-($-%%start) db 0
%endmacro
    console_name 'help'
    console_name 'about'
    console_name 'status'
    console_name 'mem'
    console_name 'ps'
    console_name 'devices'
    console_name 'clear'
    console_name 'halt'
    console_name 'files'
    console_name 'net'
    console_name 'model'
    console_name 'agents'
    console_name 'grant'
    console_name 'revoke'
    console_name 'bench'
    console_name 'logs'
    console_name 'shutdown'
%unmacro console_name 1
%if $-console_command_names != 17*9
 %error Command name layout mismatch
%endif
