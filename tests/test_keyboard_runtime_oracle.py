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

# Negative fixtures require exact real in-service/pending state, not any failure.
from keyboard_runtime_oracle import validate_stopped_lapic
accept(validate_stopped_lapic,baseline,set(),set(),True)
exhaust=baseline.replace('IRR (none)','IRR 64')
accept(validate_stopped_lapic,exhaust,set(),{64},True)
active=baseline.replace('0x00010040','0x00000040').replace('initial_count = 0','initial_count = 1048576').replace('ISR (none)','ISR 64')
accept(validate_stopped_lapic,active,{64},set(),False)
keyboard=baseline.replace('ISR (none)','ISR 68')
accept(validate_stopped_lapic,keyboard,{68},set(),True)
for value in [baseline,exhaust.replace('IRR 64','IRR 68'),exhaust.replace('IRR 64','IRR 64 68'),exhaust.replace('ISR (none)','ISR 68')]:
    reject(validate_stopped_lapic,value,set(),{64},True)
for old,new in [('ISR 64','ISR (none)'),('ISR 64','ISR 68'),('IRR (none)','IRR 68'),('0x00000040','0x00010040'),('initial_count = 1048576','initial_count = 0'),('current_count = 0','current_count = 1')]:
    reject(validate_stopped_lapic,active.replace(old,new),{64},set(),False)


from keyboard_runtime_oracle import validate_refill_evidence
refill=[0]*18
refill[11]=32;refill[12]=1;refill[13]=1;refill[16]=3;refill[17]=0x1d
accept(validate_refill_evidence,tuple(refill))
for field,value in [(11,16),(12,0),(12,2),(13,0),(15,1),(16,0),(16,1),(16,2),(17,0x1c)]:
    mutated=refill.copy();mutated[field]=value
    reject(validate_refill_evidence,tuple(mutated))

from keyboard_runtime_oracle import validate_output_quiet
accept(validate_output_quiet,'portb[0x0064] = 0x1c\r\n')
accept(validate_output_quiet,'portb[0x0064] = 0x1c\\r\\n')
for value in ['unknown command i /b 0x64','portb[0x0064] = 0x1d','portb[0x0065] = 0x1c',
              'portw[0x0064] = 0x1c','portb[0x0064] = 0x100','portb[0x0064] = 1c',
              'error portb[0x0064] = 0x1c','portb[0x0064] = 0x1c junk','']:
    reject(validate_output_quiet,value)
print(f'PASS: {checks} actual target-oracle LAPIC/queue/refill/port-response mutation checks.')
