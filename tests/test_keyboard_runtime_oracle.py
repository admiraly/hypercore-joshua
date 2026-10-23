"""Mutations invoke the same predicates used against actual QMP state."""
from keyboard_runtime_oracle import validate_lapic, validate_queue_indices

baseline='LVTT 0x00010040 masked one-shot Fixed (vec 64)\nTimer DCR=0x3 (divide by 16) initial_count = 0 current_count = 0\nISR (none)\nIRR (none)\n'
checks=0

def accept(function,*args):
    global checks
    function(*args);checks+=1


def reject(function,*args):
    global checks
    try:function(*args)
    except AssertionError:checks+=1;return
    raise AssertionError('corruption accepted')

accept(validate_lapic,baseline)
accept(validate_lapic,baseline.replace('\n','\\r\\n'))
for old,new in [('0x00010040','0x00000040'),('0x00010040','0x00030040'),('0x00010040','0x00010044'),
                ('initial_count = 0','initial_count = 1'),('current_count = 0','current_count = 1'),
                ('DCR=0x3','DCR=0xb'),('ISR (none)','ISR 64'),('IRR (none)','IRR 68'),
                ('ISR (none)','ISR (none) 64'),('IRR (none)','IRR (none) 68')]:
    reject(validate_lapic,baseline.replace(old,new))
for head in range(64):
    for count in range(65):
        tail=(head+count)%64
        accept(validate_queue_indices,head,tail,count)
        reject(validate_queue_indices,head,(tail+1)%64,count)
for values in [(64,0,0),(0,64,0),(0,0,65),(-1,0,0),(0,-1,0),(0,0,-1)]:reject(validate_queue_indices,*values)
print(f'PASS: {checks} actual-oracle LAPIC retirement/queue-index mutation checks.')
