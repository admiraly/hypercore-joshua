"""Host oracle for real CPL3 results, exact fault identity, and bounded watchdog."""
import argparse
import json
from pathlib import Path
import re
import struct

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output',type=Path)
parser.add_argument('--boot-media',choices=['iso','fat'],default='iso')
parser.add_argument('--expected-stop',choices=['None','DrainExhaust','KernelTimer','InheritedCode','LiveSelector'],default='None')
args=parser.parse_args()
manifest=json.loads((args.output/'manifest.json').read_text())
mode=manifest['userTest']
if mode=='AlignmentKvm':mode='Alignment'
if mode=='StackKvm':mode='Stack'
intel_sysenter=mode=='SysenterKvm'
if mode=='SysenterKvm':mode='Sysenter'
if mode=='SyscallKvm':mode='Syscall'
log=(args.output/f'qemu-{args.boot_media}-serial.log').read_text()
listing=(args.output/'boot.lst').read_text().splitlines()
image=(args.output/'BOOTX64.EFI').read_bytes()
BASE=0x400000000000

def symbol(name):
    for index,line in enumerate(listing):
        if re.search(r'(?<![\w.])'+re.escape(name)+r':',line):
            for candidate in listing[index:]:
                match=re.match(r'\s*\d+\s+([0-9A-F]{8})\s',candidate)
                if match:return int(match[1],16)
    raise AssertionError(f'Missing listing symbol {name}')

pe=struct.unpack_from('<I',image,0x3c)[0]
section_table=pe+24+struct.unpack_from('<H',image,pe+20)[0]
sections={}
for i in range(struct.unpack_from('<H',image,pe+6)[0]):
    offset=section_table+i*40
    name=image[offset:offset+8].rstrip(b'\0').decode()
    sections[name]=struct.unpack_from('<I',image,offset+12)[0]
tss_base=int(re.search(r'JOSHUA:TSS_READY SELECTOR=[0-9A-F]+ BASE=([0-9A-F]+)',log)[1],16)
image_base=tss_base-sections['.data']-symbol('kernel_tss')
probe=image_base+sections['.text']+symbol('user_smoke')
roots=[int(x,16) for x in re.search(r'USER_ROOTS_PROVED ROOT_A=([0-9A-F]+) ROOT_B=([0-9A-F]+)',log).groups()]
kernel_root=int(re.search(r'JOSHUA:OWNED_PAGING CR3=([0-9A-F]+)',log)[1],16)
assert len(set(roots+[kernel_root]))==3
rows=re.findall(r'JOSHUA:USER_RESULT [^\r\n]+',log)
if args.expected_stop!='None':
    assert mode==args.expected_stop, 'Golden stop expectation differs from built fixture'
    assert not rows and 'JOSHUA:USER_PAIR_RETURNED' not in log and 'JOSHUA:KERNEL_HALTED' not in log
    if mode in ['InheritedCode','LiveSelector']:
        assert 'JOSHUA:USER_TRANSITION_FAILURE STEP=0000000000000001' in log
        assert log.index('JOSHUA:USER_MACHINE_PREPARED')<log.index('JOSHUA:KERNEL_FAULT')
        print(f'PASS: {mode} rejects descriptor authority before any CPL3 launch.')
        raise SystemExit(0)
    fatal=re.search(r'JOSHUA:USER_TRANSITION_FAILURE STEP=([0-9A-F]+) PENDING=([0-9A-F]+) WINDOWS=([0-9A-F]+) EOIS=([0-9A-F]+) TICKS=([0-9A-F]+)',log)
    assert fatal is not None
    step,pending,windows,eois,ticks=[int(x,16) for x in fatal.groups()]
    assert step==7 and pending==1 and eois==ticks
    assert windows==(1024 if mode=='DrainExhaust' else 0)
    assert log.index(fatal[0])<log.index('JOSHUA:KERNEL_FAULT')
    print(f'PASS: {mode} stops before pending-timer EOI, terminal success or processB.')
    raise SystemExit(0)
assert len(rows)==2
results=[{k:int(v,16) for k,v in re.findall(r'([A-Z0-9_]+)=([0-9A-F]{16})',row)} for row in rows]
for index,result in enumerate(results,1):
    assert result['INDEX']==index
    assert result['ROOT']==roots[index-1]
    assert result['CS']&7==3
    assert result['CS']==results[0]['CS']
    assert result['UNKNOWN']==1
    assert result['TICKS']<=8 and result['EOIS']>=result['TICKS']
a,b=results
assert b['STATE']==3 and b['VECTOR']==0 and b['ERROR']==0 and b['RIP']==0
assert b['TICKS']<8 and b['EOIS']<=b['TICKS']+1
if mode in ['None','PendingExit']:
    assert a['STATE']==3 and a['VECTOR']==0 and a['RIP']==0
elif mode=='Spin':
    assert (a['STATE'],a['TICKS'],a['EOIS'])==(5,8,8)
    assert a['VECTOR']==0 and a['RIP']==0
elif mode=='BadReturn':
    assert a['STATE']==4 and a['VECTOR']==0 and a['RIP']==0
else:
    expected={
        'KernelRead':(14,5,probe),'KernelWrite':(14,7,probe),
        'CodeWrite':(14,7,BASE),'Nx':(14,21,BASE+0x1000),
        'Guard':(14,4,BASE+0x2000),'Io':(13,0,None),'Cli':(13,0,None),
        'Fpu':(7,0,None),'Syscall':(6,0,None),'Sysenter':(6,0,None),
        'Stack':(12,0,None),'Alignment':(17,0,None),
        'Np':(11,(a['CS']+16)&~3,None),
        'FarTransfer':(13,(a['CS']-32)&~3,None),'Ud2':(6,0,None),'PendingFault':(6,0,None),'InheritedLdt':(13,4,None),
    }
    vector,error,cr2=expected[mode]
    if intel_sysenter:vector=13  # explicit Intel-host KVM profile, null SYSENTER_CS
    rip=BASE+symbol('user_payload_fixture')-symbol('user_code_start')
    if mode=='Nx':rip=BASE+0x1000
    assert a['STATE']==4
    assert (a['VECTOR'],a['ERROR'],a['RIP'])==(vector,error,rip),(mode,a,(vector,error,rip))
    if cr2 is not None:assert a['CR2']==cr2,(mode,a['CR2'],cr2)
if mode in ['PendingExit','PendingFault']:
    assert a['PENDING']==1 and 1<=a['WINDOWS']<=1024 and a['EOIS']==a['TICKS']+1
    assert b['PENDING']==0
if mode=='InheritedLdt':assert log.index('JOSHUA:USER_INHERITED_LDT_SET')<log.index('JOSHUA:USER_MACHINE_PREPARED')
assert log.index(rows[0])<log.index(rows[1])<log.index('JOSHUA:USER_PAIR_RETURNED')<log.index('JOSHUA:KERNEL_HALTED')
print(f'PASS: {mode} actual CPL3 roots/CS, exact fault/watchdog and independent B continuation.')
