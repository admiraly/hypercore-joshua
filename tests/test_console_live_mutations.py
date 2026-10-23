"""Replay real key inputs against independent state/sources, then corrupt captures."""
import argparse
import ast
import copy
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
from console_live_oracle import LiveReference
from console_sources import Sources
from console_pixels import raster,validate_pixels
from keyboard_runtime_oracle import validate_output_quiet

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--boot-media',choices=['iso','fat'],default='iso')
a=p.parse_args()
out=a.output
manifest=json.loads((out/'manifest.json').read_text())
summary=json.loads((out/('console-'+a.boot_media+'-summary.json')).read_text())
assert summary['independentLiveOracle'] and summary['features'] and summary['commands']==20,'published complete independent campaign'
assert manifest['console'] and manifest.get('consoleTest','None')=='None','ordinary console only'
serial=(out/('console-'+a.boot_media+'-serial.log')).read_text()
module=ast.parse((Path(__file__).with_name('test_console_runtime.py')).read_text())
pattern=next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='READY_PATTERN' for t in n.targets))
match=re.search(pattern,serial)
assert match
addresses=dict(zip('model grid editor command response facts window decoder controller'.split(),map(lambda x:int(x,16),match.groups())))
regs=(out/('console-'+a.boot_media+'-ready-registers.log')).read_text()
sources=Sources(out,manifest,serial,addresses,regs)
keys='editor model grid command facts response window decoder controller'.split()+list(sources.addresses)

def load(suffix):
    return {name:(out/('console-'+a.boot_media+'-'+suffix+'-'+name+'.bin')).read_bytes() for name in keys}

ready=load('ready')
sources.facts(ready)
reference=LiveReference(sources.anchor)
trace=json.loads((out/('console-'+a.boot_media+'-live-events.json')).read_text())
assert trace[0]=={'snapshot':'ready','terminal':False} and trace[-1]=={'snapshot':'halted','terminal':True}
checks=0
samples=[]
last_snapshot=None
for event in trace:
    if 'snapshot' in event:
        name=event['snapshot']
        actual=load(name)
        sources.facts(actual)
        reference.check(actual,event['terminal'])
        checks+=1
        if name in ['status','feature-4','full-line','halted']:
            samples.append((copy.deepcopy(reference),actual))
        last_snapshot=name
    else:
        code,modifier=event['code'],event['modifier']
        if modifier=='ctrl':
            assert code=='c'
            token=3
        elif code in {'ret','backspace','left','right','home','end','delete','up','down'}:
            token={'ret':13,'backspace':8,'left':0x100,'right':0x101,'home':0x102,'end':0x103,'delete':0x104,'up':0x105,'down':0x106}[code]
        else:
            text={'spc':' ','minus':'-'}.get(code,code)
            assert len(text)==1 and modifier in (None,'shift')
            token=ord(text.upper() if modifier=='shift' else text)
        assert event['token']==token,'actual qcode to reference token binding'
        facts=None
        if token==13:
            assert event['beforeSubmit']==last_snapshot=='before-submit-'+str(reference.completed+1)
            validate_output_quiet((out/('console-'+a.boot_media+'-'+last_snapshot+'-i8042.log')).read_text())
            facts=sources.facts(load(last_snapshot),next_enter=True)
        else:assert event['beforeSubmit'] is None
        reference.event(token,facts)
        checks+=1
assert reference.stopped and reference.completed==20 and len(samples)==4


def reject(action):
    global checks
    try:action()
    except AssertionError:checks+=1
    else:raise AssertionError('corrupt live evidence accepted')

# All complete records are checked, including zero tails and inactive cells.
for expected,actual in samples:
    expected.check(actual)
    checks+=1
    for name in ['editor','model','grid','command','facts','response']:
        for offset in sorted({0,7,len(actual[name])//2,len(actual[name])-1}):
            damaged=dict(actual)
            blob=bytearray(actual[name]);blob[offset]^=1;damaged[name]=bytes(blob)
            reject(lambda:expected.check(damaged))
    # A valid forged mem JSON/facts/grid/pixel combination could satisfy the old
    # self-consistency checks. It must fail the independent pre-input reference.
    ready_value,length=struct.unpack_from('<2Q',actual['response'])
    body=actual['response'][16:16+length]
    if body.startswith(b'{"command":"mem"'):
        damaged=dict(actual)
        blob=bytearray(actual['facts'])
        value=struct.unpack_from('<Q',blob,16)[0]
        forged=value^1
        struct.pack_into('<Q',blob,16,forged)
        damaged['facts']=bytes(blob)
        data=json.loads(body)
        assert data['free_pages']==value
        data['free_pages']=forged
        raw=(json.dumps(data,separators=(',',':'))+'\n').encode()
        damaged['response']=struct.pack('<2Q',ready_value,len(raw))+raw.ljust(4096,b'\0')
        grid=bytearray(actual['grid'])
        cols,rows=struct.unpack_from('<2Q',grid,56)
        flat=b''.join(grid[104+y*100:104+y*100+cols] for y in range(2,rows-2))
        needle=b'"free_pages":'+str(value).encode()
        at=flat.index(needle)+len(b'"free_pages":')
        assert len(str(forged))==len(str(value))
        for i,char in enumerate(str(forged).encode()):
            row,col=divmod(at+i,cols)
            grid[104+(row+2)*100+col]=char
        damaged['grid']=bytes(grid)
        font=Path(__file__).resolve().parents[1]/'kernel/console/font.inc'
        width,height,pixels=raster(grid,font)
        ppm=f'P6\n{width} {height}\n255\n'.encode()+pixels
        validate_pixels(grid,ppm,font) # the internally consistent forgery is valid
        checks+=1
        reject(lambda:expected.check(damaged))
# Stable owned sources must not silently change even to other in-range values.
for name in sources.addresses:
    damaged=dict(ready)
    blob=bytearray(ready[name]);blob[-1]^=1;damaged[name]=bytes(blob)
    reject(lambda:sources.facts(damaged))
for name in addresses:
    damaged=dict(addresses);damaged[name]+=8
    reject(lambda:Sources(out,manifest,serial,damaged,regs))
wrong_regs=re.sub(r'(?m)(TR\s*=\s*[0-9a-fA-F]+\s+)[0-9a-fA-F]+',r'\g<1>0000000000001000',regs.replace('\\r\\n','\n').replace('\r',''))
assert wrong_regs!=regs
reject(lambda:Sources(out,manifest,serial,addresses,wrong_regs))
with tempfile.TemporaryDirectory() as directory:
    scratch=Path(directory)
    for name in ['boot.lst','BOOTX64.EFI']:shutil.copyfile(out/name,scratch/name)
    for name in ['boot.lst','BOOTX64.EFI']:
        original=(scratch/name).read_bytes()
        changed=bytearray(original);changed[-1]^=1;(scratch/name).write_bytes(changed)
        reject(lambda:Sources(scratch,manifest,serial,addresses,regs))
        (scratch/name).write_bytes(original)
print(f'PASS: {checks} live input replay/source/state baseline and mutation checks')
