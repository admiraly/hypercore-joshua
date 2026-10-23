"""Actual captured timer states must pass before mutations are rejected."""
import re
import struct
import sys
from pathlib import Path
from console_timer_oracle import validate_timer_fixture

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('out')
checks=0
for mode in ['UnexpectedTimer','PendingTimer','DrainExhaust']:
    out=root/('console-test-'+mode.lower())
    tag='console-iso-'+('halted' if mode=='PendingTimer' else 'timer-rejected')
    source=(out/(tag+'-source.bin')).read_bytes()
    window=(out/(tag+'-window.bin')).read_bytes()
    lapic=(out/(tag+'-lapic.log')).read_text().replace('\\r\\n','\n').replace('\r','')
    validate_timer_fixture(mode,source,window,lapic)
    checks+=1
    def reject(s=source,w=window,l=lapic):
        global checks
        try:
            validate_timer_fixture(mode,s,w,l)
        except AssertionError:
            checks+=1
        else:
            raise AssertionError('accepted '+mode+' mutation')
    for field,value in [(0,0),(0,3),(1,0),(1,2),(1,0x21),(2,1)]:
        changed=bytearray(source)
        struct.pack_into('<Q',changed,field*8,value)
        reject(s=changed)
    for field in range(3,8):
        changed=bytearray(source)
        value=struct.unpack_from('<Q',changed,field*8)[0]
        struct.pack_into('<Q',changed,field*8,value^1)
        reject(s=changed)
    for field,value in [(0,0),(1,4),(2,0),(10,0x10202),(13,1),(14,0),(16,2),(17,1),(18,4096)]:
        changed=bytearray(window)
        struct.pack_into('<Q',changed,field*8,value)
        reject(w=changed)
    if mode!='DrainExhaust':
        # RF may be hardware-saved; metadata must still be strict202h.
        value=struct.unpack_from('<Q',source,40)[0]
        changed=bytearray(source)
        struct.pack_into('<Q',changed,40,value^0x10000)
        validate_timer_fixture(mode,changed,window,lapic)
        checks+=1
        for bit in range(64):
            if bit==16:continue
            struct.pack_into('<Q',changed,40,value^(1<<bit))
            reject(s=changed)
    for pattern,replacement in [
        (r'(?m)^LVTT\b[^\n]*','LVTT 0xdead'),
        (r'(?m)^ISR[^\n]*','ISR 129'),
        (r'(?m)^IRR[^\n]*','IRR 129'),
        (r'current_count\s*=\s*\d+','current_count=1'),
        (r'initial_count\s*=\s*\d+','initial_count=123'),
        (r'DCR=0x[0-9A-Fa-f]+','DCR=0x0')]:
        changed,count=re.subn(pattern,replacement,lapic)
        assert count==1,'mutation must change an actual field'
        reject(l=changed)
    reject(s=source[:-1])
    reject(w=window[:-1])
print(f'PASS: {checks} captured console timer baseline/mutation checks')
