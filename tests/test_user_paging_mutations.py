"""Require the actual QMP oracle to reject corrupted permissions/ownership/data."""
import ast
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

output=Path(sys.argv[1]).resolve()
media=sys.argv[2] if len(sys.argv)>2 else 'iso'
name=f'qemu-{media}-user-paging.json'
original=json.loads((output/name).read_text())
seed=original['seed'];MASK=0x000ffffffffff000;NX=1<<63

def page(s,address):return s['pages'][f'{address:x}']
def word(s,address):return page(s,address&~4095)[(address&4095)//8]
def put(s,address,value):page(s,address&~4095)[(address&4095)//8]=value

def path(s,root):
    result=[root];entry=page(s,root)[128]
    for _ in range(3):
        table=entry&MASK;result.append(table);entry=page(s,table)[0]
    return result

paths=[path(original,r) for r in seed['roots']]
backing=[word(original,seed['processes']+i*144+32) for i in range(2)]
cases=[]
def change(label,address,value):cases.append((label,lambda s,a=address,v=value:put(s,a,v)))
def flip(label,address,mask):change(label,address,word(original,address)^mask)
for level,table in enumerate(paths[0][:3]):
    address=table+(128*8 if level==0 else 0)
    for label,mask in [('RW',2),('US',4),('NX',NX),('PWT',8),('PCD',16),('reserved',1<<62)]:flip(f'parent{level} {label}',address,mask)
for slot in [0,1]:
    leaf=paths[0][-1]+slot*8
    for label,mask in [('RW',2),('US',4),('NX',NX),('global',256),('pkey',1<<59)]:flip(f'leaf{slot} {label}',leaf,mask)
flip('mixed PAT mismatch',paths[0][-1],8)
for level in [1,2]:change(f'extra parent{level}',paths[0][level]+8,page(original,paths[0][level])[0])
for slot in [2,511]:change(f'extra user leaf{slot}',paths[0][-1]+slot*8,page(original,paths[0][-1])[0])
change('shared private path',paths[1][0]+128*8,page(original,paths[0][0])[128])
leaf=paths[0][-1]+8
change('another process writable backing',leaf,(word(original,leaf)&~MASK)|(backing[1]+4096))
flip('kernel PML4 user authority',seed['kernelRoot'],4)
flip('private code bytes',backing[0],1)
flip('B private sentinel',backing[1]+4096+8,1)
change('B private progress',backing[1]+4096+16,0)
flip('terminal metadata differs from captured result',seed['processes']+6*8,1)
change('allocator count overflow',seed['allocator']+24,4097)
count=word(original,seed['allocator']+24)
paging_done=False
checked_table=paths[0][0]
for i in range(count):
    address=seed['records']+24*i
    owner=word(original,address+16)
    if owner==0x55530001:
        change('backing owner mismatch',address+16,0x55530002)
        change('backing exact count mismatch',address+8,7)
    if owner==0x50414745 and not paging_done and word(original,address)<=checked_table<word(original,address)+word(original,address+8)*4096:
        change('paging chunk owner mismatch',address+16,0x1234)
        paging_done=True

def kernel_leaf(address):
    table=seed['kernelRoot']
    for shift in [39,30,21,12]:
        slot=table+((address>>shift)&511)*8;entry=word(original,slot)
        if shift==12 or shift in [30,21] and entry&128:return slot
        table=entry&MASK
    raise AssertionError('leaf')
flip('writable code identity alias',kernel_leaf(backing[0]),2)
flip('executable code identity alias',kernel_leaf(backing[0]),NX)
flip('read-only data identity alias',kernel_leaf(backing[0]+4096),2)
script=Path(__file__).with_name('test_user_paging.py').resolve()
with tempfile.TemporaryDirectory(prefix='paging-oracle-',dir=output.parent) as folder:
    target=Path(folder)
    for file in ['BOOTX64.EFI','boot.lst',f'qemu-{media}-serial.log']:shutil.copyfile(output/file,target/file)
    def run(snapshot):
        (target/name).write_text(json.dumps(snapshot))
        return subprocess.run([sys.executable,str(script),str(target),'--boot-media',media],capture_output=True,text=True)
    baseline=run(original)
    assert baseline.returncode==0,baseline.stderr
    for label,mutation in cases:
        snapshot=copy.deepcopy(original);mutation(snapshot);result=run(snapshot)
        assert result.returncode!=0 and ('AssertionError' in result.stderr or 'KeyError' in result.stderr),(label,result.stdout,result.stderr)
print(f'PASS: baseline accepted; {len(cases)} actual snapshot permission/ownership/data mutations rejected.')

# Exercise the actual oracle's level validator with synthetic large leaves, since
# this small QEMU profile may place all owned allocations in split4-KiB regions.
source=script.read_text()
node=next(n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef) and n.name=='validate_kernel_entry')
namespace={'MASK':MASK,'NX':NX}
exec(compile(ast.Module(body=[node],type_ignores=[]),str(script),'exec'),namespace)
validate=namespace['validate_kernel_entry'];checks=0
for shift in [21,30]:
    good=(1<<shift)|NX|0x83|(1<<12)
    leaf,physical,selector=validate(good,shift,1<<47)
    assert leaf and physical==1<<shift and selector==4
    for bit in list(range(13,shift))+[52,59,62]:
        try:validate(good|(1<<bit),shift,1<<47)
        except AssertionError:checks+=1
        else:raise AssertionError(('accepted reserved large-leaf bit',shift,bit))
for shift in [39,30,21]:
    good=0x2000|3
    validate(good,shift,1<<47)
    for bit in [3,4,6,7,59,62]:
        try:validate(good|(1<<bit),shift,1<<47)
        except AssertionError:checks+=1
        else:raise AssertionError(('accepted kernel parent bit',shift,bit))
print(f'PASS: actual level validator accepts large PAT and rejects{checks} reserved/cache-policy mutations.')
