model_title: db 'JOSHUA / HYPERCORE'
model_title_end:
model_subtitle: db 'BSP DEVELOPMENT CONSOLE | NO AI EXECUTION'
model_subtitle_end:
model_ready: db 'READY / BSP DEVELOPMENT CONSOLE'
model_ready_end:
model_prefix: db 'JOSHUA> '
model_prefix_end:
model_result_ok: db 'RESULT 0 / OK'
model_result_ok_end:
model_result_usage: db 'RESULT 2 / USAGE'
model_result_usage_end:
model_result_unavailable: db 'RESULT 69 / UNAVAILABLE'
model_result_unavailable_end:
model_result_unknown: db 'RESULT 127 / UNKNOWN'
model_result_unknown_end:
%if model_prefix_end-model_prefix != 8
 %error Prompt prefix width mismatch
%endif
%if model_subtitle_end-model_subtitle > 48 || model_ready_end-model_ready > 48 || model_result_unavailable_end-model_result_unavailable > 48
 %error Console literal exceeds minimum row width
%endif
