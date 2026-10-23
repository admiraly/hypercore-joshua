"""Actual PS2 console smoke; physical metadata and hardware retirement evidence.
This does not replace the complete interactive/negative/regression campaign.
"""
import argparse
import json
import hashlib
import zlib
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time
import sys
from console_pixels import validate_pixels
from console_live_oracle import LiveReference
from console_sources import Sources
from console_timer_oracle import validate_timer_fixture
from keyboard_runtime_oracle import validate_lapic, validate_output_quiet, validate_queue_indices

READY_PATTERN = r'(?m)^JOSHUA:CONSOLE_READY MODEL=([0-9A-F]{16}) GRID=([0-9A-F]{16}) EDITOR=([0-9A-F]{16}) COMMAND=([0-9A-F]{16}) RESPONSE=([0-9A-F]{16}) FACTS=([0-9A-F]{16}) WINDOW=([0-9A-F]{16}) DECODER=([0-9A-F]{16}) CONTROLLER=([0-9A-F]{16})\r?\n'

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--qemu',required=True)
p.add_argument('--firmware',type=Path,required=True)
p.add_argument('--boot-media',choices=['Iso','Fat'],default='Iso')
p.add_argument('--cpu',default='qemu64')
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--idle-seconds',type=int,default=30)
p.add_argument('--fixture',choices=['None','BadEditor','BadFacts','Saturation','UnexpectedTimer','PendingTimer','DrainExhaust'],default='None')
p.add_argument('--features',action='store_true',help='exercise command grammar through real PS2 input')
a = p.parse_args()
MAX=(1<<64)-1
if a.fixture=='Saturation':
    a.features=True
seed=MAX-1 if a.fixture=='Saturation' else 0
assert 30 <= a.idle_seconds <= 120,'idle evidence requires at least30 seconds'
out = a.output.resolve()
summary_path = out/('console-'+a.boot_media.lower()+'-summary.json')
trace_path=out/('console-'+a.boot_media.lower()+'-live-events.json')
trace_path.unlink(missing_ok=True)
summary_path.unlink(missing_ok=True) # stale success never survives a failed rerun
manifest = json.loads((out/'manifest.json').read_text())
assert manifest.get('consoleTest','None')==a.fixture,'exact console fixture identity'
assert manifest.get('console') is True,'requires separately built Console profile'
for field in ['faultTest','contextTest','userTest','elfTest','keyboardTest']:
    assert manifest.get(field)=='None','conflicting console fixture: '+field
assert manifest.get('timerTest') in ['None','X2'],'conflicting timer fixture'
artifacts = {item['path']:item for item in manifest['artifacts']}
for name in ['BOOTX64.EFI','joshua-uefi.img','joshua-m0.iso','JOSHUA-Console-Test.iso']:
    content = (out/name).read_bytes()
    expected = artifacts[name]
    assert len(content)==expected['bytes'],'artifact size: '+name
    assert hashlib.sha256(content).hexdigest()==expected['sha256'].lower(),'artifact hash: '+name
assert (out/'joshua-m0.iso').read_bytes()==(out/'JOSHUA-Console-Test.iso').read_bytes(),'named media identity'

tag = 'console-'+a.boot_media.lower()
serial = out/(tag+'-serial.log')
serial.write_text('')
variables = out/(tag+'-vars.fd')
shutil.copyfile(a.firmware/'edk2-i386-vars.fd',variables)
with socket.socket() as reserve:
    reserve.bind(('127.0.0.1',0))
    port = reserve.getsockname()[1]
args = [a.qemu,'-machine','q35','-accel','tcg','-cpu',a.cpu,'-smp','2','-m','512','-vga','std','-nic','none','-display','none','-no-reboot','-no-shutdown',
        '-drive',f'if=pflash,format=raw,unit=0,readonly=on,file={a.firmware.resolve() / "edk2-x86_64-code.fd"}',
        '-drive',f'if=pflash,format=raw,unit=1,file={variables}','-serial',f'file:{serial}','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
if a.boot_media=='Iso':
    args += ['-drive',f'media=cdrom,readonly=on,format=raw,file={out / "JOSHUA-Console-Test.iso"}','-boot','order=d,menu=off']
else:
    args += ['-drive',f'if=none,id=bootdisk,readonly=on,format=raw,file={out / "joshua-uefi.img"}','-device','qemu-xhci','-device','usb-storage,drive=bootdisk,removable=on','-boot','order=c,menu=off']
process = client = None
with (out/(tag+'-stderr.log')).open('wb') as err, (out/(tag+'-stdout.log')).open('wb') as stdout:
    try:
        process = subprocess.Popen(args,stderr=err,stdout=stdout)
        deadline = time.monotonic()+a.timeout
        while time.monotonic()<deadline:
            try:
                client = socket.create_connection(('127.0.0.1',port),timeout=2)
                break
            except OSError:
                assert process.poll() is None,'QEMU exited before QMP'
                time.sleep(.05)
        assert client,'QMP deadline'
        client.settimeout(5)
        stream = client.makefile('rwb')
        assert 'QMP' in json.loads(stream.readline()),'QMP greeting'
        def qmp(command,**arguments):
            stream.write((json.dumps({'execute':command,'arguments':arguments})+'\n').encode())
            stream.flush()
            while True:
                raw = stream.readline()
                assert raw,'QMP disconnected'
                response = json.loads(raw)
                assert 'error' not in response,response
                if 'return' in response:
                    return response['return']
        qmp('qmp_capabilities')
        def log():
            return serial.read_text(errors='replace')
        def healthy():
            assert process.poll() is None,'QEMU exited'
            assert 'JOSHUA:KERNEL_FAULT' not in log(),log()
            assert time.monotonic()<deadline,'target deadline: '+log()
        match=None
        while not match:
            match=re.search(READY_PATTERN,log())
            if match:
                break
            healthy()
            time.sleep(.02)
        assert match,'complete evidence addresses'
        addresses = dict(zip(['model','grid','editor','command','response','facts','window','decoder','controller'],[int(x,16) for x in match.groups()]))
        if a.fixture!='None':
            marker='JOSHUA:CONSOLE_FIXTURE ID='
            while not re.search(r'CONSOLE_FIXTURE ID=[0-9A-F]{16}(?: SOURCE=[0-9A-F]{16})?\r?\n',log()):
                healthy_fixture=process.poll() is None and time.monotonic()<deadline
                assert healthy_fixture,'fixture readiness deadline'
                time.sleep(.02)
            fixture_match=re.search(r'CONSOLE_FIXTURE ID=([0-9A-F]{16})(?: SOURCE=([0-9A-F]{16}))?',log())
            assert int(fixture_match[1],16)==['None','BadEditor','BadFacts','Saturation','UnexpectedTimer','PendingTimer','DrainExhaust'].index(a.fixture),'target fixture identity'
            if fixture_match[2]:
                addresses['source']=int(fixture_match[2],16)
        sources=reference=None
        held_enter=False
        expected_images=None
        live_events=[]
        if a.fixture=='None':
            sources=Sources(out,manifest,log(),addresses,qmp('human-monitor-command',**{'command-line':'info registers'}))
            addresses.update({name:address for name,(address,_) in sources.addresses.items()})
        def physical(name,size,suffix):
            path = out/(tag+'-'+suffix+'-'+name+'.bin')
            qmp('pmemsave',val=addresses[name],size=size,filename=str(path))
            data = path.read_bytes()
            assert len(data)==size,'physical capture length'
            return data
        def registers():
            return qmp('human-monitor-command',**{'command-line':'info registers'})
        def idle():
            while True:
                healthy()
                text = registers()
                flags = re.search(r'RFL=([0-9A-Fa-f]+)',text)
                if 'HLT=1' in text and flags and int(flags[1],16)&0x200:
                    assert 'CPL=0' in text,'owned BSP kernel wait'
                    return
                time.sleep(.02)
        def snapshot(suffix,terminal=False,before_enter=False):
            global held_enter,expected_images
            if not terminal:
                idle()
            qmp('stop')
            values = {name:physical(name,size,suffix) for name,size in
                      [('model',64),('grid',4904),('editor',1568),('command',40),('response',4112),('facts',128),('window',288),('decoder',216),('controller',56)]}
            if sources:
                values.update({name:physical(name,size,suffix) for name,(_,size) in sources.addresses.items()})
                sources.facts(values)
            if 'source' in addresses:
                values['source']=physical('source',64 if a.fixture in ('UnexpectedTimer','PendingTimer','DrainExhaust') else 8,suffix)
            regs = registers()
            (out/(tag+'-'+suffix+'-registers.log')).write_text(regs)
            lapic = qmp('human-monitor-command',**{'command-line':'info lapic'})
            (out/(tag+'-'+suffix+'-lapic.log')).write_text(lapic)
            validate_lapic(lapic)
            w = struct.unpack('<20Q64H',values['window'])
            d = struct.unpack('<11Q64H',values['decoder'])
            validate_queue_indices(d[6],d[7],d[8])
            assert d[0]==0x3144424b48534f4a and d[8:11]==(0,0,0),'empty healthy real decoder'
            assert w[0]==0x3157424b48534f4a and w[1]==(3 if terminal else 1),'exact window ownership phase'
            assert w[13]==w[17]==0 and w[16]==(1 if terminal and a.fixture=='PendingTimer' else 0),'exact timer ticks/EOIs and collection state'
            assert w[14]==w[15],'actual IRQ44/EOI accounting'
            assert w[2]==int(re.search(r'OWNED_PAGING CR3=([0-9A-F]+)',log())[1],16),'retained owned root'
            assert 'CR3='+format(w[2],'016x') in regs.lower().replace('cr3=','CR3='),'actual current root'
            if before_enter:
                port=qmp('human-monitor-command',**{'command-line':'i /b 0x64'})
                validate_output_quiet(port)
                (out/(tag+'-'+suffix+'-i8042.log')).write_text(port)
            if reference:
                expected_images=reference.check(values,terminal)
                live_events.append({'snapshot':suffix,'terminal':terminal})
                (out/(tag+'-'+suffix+'-expected.json')).write_text(json.dumps({name:data.hex() for name,data in expected_images.items()},indent=2)+'\n')
            if not terminal:
                assert int(re.search(r'RSP=([0-9A-Fa-f]+)',regs)[1],16)==w[3],'actual exact wait RSP'
                assert int(re.search(r'RIP=([0-9A-Fa-f]+)',regs)[1],16)==w[6],'actual exact wait continuation'
                assert w[4]+4096+192<=w[3]<=w[5] and w[10]==0x202,'owned stack/flags metadata'
                assert 'HLT=1' in regs and int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200,'IF1 idle'
                qmp('cont')
                if held_enter:
                    qmp('input-send-event',events=[{'type':'key','data':{'down':False,'key':{'type':'qcode','data':'ret'}}}])
                    held_enter=False
            return values
        baseline = snapshot('ready')
        if sources:
            reference=LiveReference(sources.anchor)
            reference.check(baseline)
            live_events.append({'snapshot':'ready','terminal':False})
        assert struct.unpack('<8Q',baseline['model'])==(0x314c444f4d484f4a,2,0,seed,0,0,0,0),'initial model'
        if a.fixture in ('UnexpectedTimer','PendingTimer','DrainExhaust'):
            assert baseline['source']==bytes(64),'zero timer fixture baseline'
        if a.fixture=='Saturation':
            assert struct.unpack_from('<3Q',baseline['editor'],72)==(seed,seed,seed),'counter seed baseline'
            assert struct.unpack_from('<2Q',baseline['window'],112)==(seed,seed),'IRQ seed baseline'
            assert baseline['source']==bytes(8),'independent actual IRQ trace baseline'
        start = time.monotonic()
        while time.monotonic()-start<a.idle_seconds:
            healthy()
            time.sleep(.1)
        after_idle = snapshot('idle')
        assert after_idle['model']==baseline['model'] and after_idle['editor']==baseline['editor'],'idle preserves input/model'
        assert after_idle['window']==baseline['window'],'idle has no synthetic input/deadline progress'
        def key(code,modifier=None):
            global held_enter
            predicted=None
            if reference and code=='ret':
                assert not held_enter,'one outstanding Enter at a time'
                before=snapshot('before-submit-'+str(reference.completed+1),before_enter=True)
                predicted=sources.facts(before,next_enter=True)
            events=[]
            if modifier:
                events.append({'type':'key','data':{'down':True,'key':{'type':'qcode','data':modifier}}})
            events += [{'type':'key','data':{'down':down,'key':{'type':'qcode','data':code}}} for down in ([True] if reference and code=='ret' else [True,False])]
            if modifier:
                events.append({'type':'key','data':{'down':False,'key':{'type':'qcode','data':modifier}}})
            qmp('input-send-event',events=events)
            if reference:
                if modifier=='ctrl':
                    assert code=='c'
                    token=3
                elif code in {'ret','backspace','left','right','home','end','delete','up','down'}:
                    token={'ret':13,'backspace':8,'left':0x100,'right':0x101,'home':0x102,'end':0x103,'delete':0x104,'up':0x105,'down':0x106}[code]
                else:
                    text={'spc':' ','minus':'-'}.get(code,code)
                    assert len(text)==1
                    token=ord(text.upper() if modifier=='shift' else text)
                live_events.append({'token':token,'code':code,'modifier':modifier,'beforeSubmit':('before-submit-'+str(reference.completed+1)) if code=='ret' else None})
                reference.event(token,predicted)
                if code=='ret':held_enter=True
            time.sleep(.035)
        def type_text(text):
            for char in text:
                key({' ':'spc','-':'minus'}.get(char,char.lower()),'shift' if char.isupper() else None)
        def editor_text(data):
            length,cursor = struct.unpack_from('<2Q',data,8)
            assert length<=127 and cursor<=length,'bounded editor'
            assert data[96+length:224]==b'\0'*(128-length),'editor zero tail'
            return data[96:96+length].decode('ascii'),cursor
        def timer_stop():
            while not re.search(r'(?m)^JOSHUA:KERNEL_FAULT\r?$',log()):
                assert process.poll() is None and time.monotonic()<deadline,'exact timer rejection deadline: '+log()
                time.sleep(.02)
            # Wait for the terminal HLT, rather than race its preceding diagnostic.
            while 'HLT=1' not in registers():
                assert process.poll() is None and time.monotonic()<deadline,'terminal HLT deadline'
            qmp('stop')
            values={name:physical(name,len(data),'timer-rejected') for name,data in baseline.items()}
            regs=registers()
            (out/(tag+'-timer-rejected-registers.log')).write_text(regs)
            assert 'HLT=1' in regs and 'CPL=0' in regs and not(int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200),'exact terminal IF0 CPL0 HLT'
            assert int(re.search(r'CR3=([0-9A-Fa-f]+)',regs)[1],16)==struct.unpack_from('<Q',baseline['window'],16)[0],'same actual owned root'
            assert struct.unpack_from('<Q',values['window'],16)[0]==struct.unpack_from('<Q',baseline['window'],16)[0],'same window root'
            assert log().count('JOSHUA:KERNEL_FAULT')==1 and 'JOSHUA:KERNEL_HALTED' not in log() and 'JOSHUA:CONSOLE_HALTED' not in log(),'exact fault without false success'
            lapic=qmp('human-monitor-command',**{'command-line':'info lapic'})
            (out/(tag+'-timer-rejected-lapic.log')).write_text(lapic)
            validate_timer_fixture(a.fixture,values['source'],values['window'],lapic)
            return values
        def finish_timer_stop():
            stream.write(b'{"execute":"quit"}\n')
            stream.flush()
            process.wait(timeout=5)
            assert process.returncode==0,'clean expected-stop shutdown'
            temporary=summary_path.with_suffix('.json.tmp')
            temporary.write_text(json.dumps({'fixture':a.fixture,'bootMedia':a.boot_media,'cpu':a.cpu,'expectedStop':True,'checks':'exact timer ownership and physical LAPIC rejection'},indent=2)+'\n')
            temporary.replace(summary_path)
            print('PASS: Console '+a.fixture+' '+a.boot_media+' exact hardware timer rejection')
        if a.fixture=='UnexpectedTimer':
            key('a')
            failed=timer_stop()
            assert 'JOSHUA:CONSOLE_FAILURE' not in log(),'direct ISR ownership rejection'
            for name in ['model','command','facts','response']:
                assert failed[name]==baseline[name],'timer rejection preserves '+name
            assert failed['facts']==bytes(128) and failed['response']==bytes(4112),'unpublished timer rejection outputs'
            expected=bytearray(baseline['editor'])
            struct.pack_into('<2Q',expected,8,1,1)
            expected[96]=ord('a')
            assert failed['editor']==expected,'exact real input before timer'
            grid=failed['grid']
            g=struct.unpack_from('<13Q',grid)
            assert g[9]==0 and g[10:13]==(9,g[8]-2,1),'rendered actual input'
            assert grid[104+(g[8]-2)*100:104+(g[8]-2)*100+9]==b'JOSHUA> a','actual input prompt'
            qmp('screendump',filename=str(out/(tag+'-timer-rejected.ppm')))
            validate_pixels(grid,(out/(tag+'-timer-rejected.ppm')).read_bytes(),Path(__file__).resolve().parents[1]/'kernel/console/font.inc')
            finish_timer_stop()
            sys.exit(0)
        if a.fixture in ('BadEditor','BadFacts'):
            assert baseline['facts']==bytes(128) and baseline['response']==bytes(4112),'unpublished baseline outputs'
            if a.fixture=='BadFacts':
                assert struct.unpack('<Q',baseline['source'])[0]==2,'valid facts source before poisoning'
                type_text('status')
                before_reject=snapshot('pre-rejection')
                key('ret')
            else:
                before_reject=baseline
                key('a')
            while 'JOSHUA:CONSOLE_FAILURE' not in log() or not re.search(r'(?m)^JOSHUA:KERNEL_FAULT\r?$',log()):
                assert process.poll() is None and time.monotonic()<deadline,'exact rejection deadline: '+log()
                time.sleep(.02)
            qmp('stop')
            failed={name:physical(name,len(data),'rejected') for name,data in baseline.items()}
            regs=registers()
            (out/(tag+'-rejected-registers.log')).write_text(regs)
            assert 'HLT=1' in regs and 'CPL=0' in regs and not(int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200),'exact IF0 CPL0 stop'
            assert int(re.search(r'CR3=([0-9A-Fa-f]+)',regs)[1],16)==struct.unpack_from('<Q',baseline['window'],16)[0],'owned root at rejection'
            assert log().count('JOSHUA:CONSOLE_FAILURE')==1 and log().count('JOSHUA:KERNEL_FAULT')==1,'no unrelated fault'
            assert 'JOSHUA:CONSOLE_HALTED' not in log() and 'JOSHUA:KERNEL_HALTED' not in log(),'no false success'
            for name in ['model','grid','response','facts']:
                assert failed[name]==before_reject[name],'rejection preserves '+name
            assert failed['facts']==bytes(128) and failed['response']==bytes(4112),'no premature publication at rejection'
            w=struct.unpack('<20Q64H',failed['window'])
            assert w[1]==1 and w[14]==w[15] and w[14]>=1 and w[13]==w[16]==w[17]==0,'active stop with owned IRQ accounting'
            if a.fixture=='BadEditor':
                expected=bytearray(baseline['editor'])
                struct.pack_into('<Q',expected,8,128)
                assert failed['editor']==expected and failed['command']==baseline['command'],'only authored editor mutation'
            else:
                assert failed['source']==bytes(8),'authored facts source rejection'
                assert struct.unpack('<5Q',failed['command'])==(0x31444e4d43484f4a,3,0,0,0),'parsed status before source failure'
                expected=struct.pack('<12Q',0x3152444553484f4a,0,0,1,1,0,0,0,6,1,0,0)
                expected+=bytes(256)+b'status'.ljust(128,b'\0')+struct.pack('<8Q',6,0,0,0,0,0,0,0)+b'status'.ljust(1024,b'\0')
                assert failed['editor']==expected,'exact submitted status state'
            stream.write(b'{"execute":"quit"}\n')
            stream.flush()
            process.wait(timeout=5)
            assert process.returncode==0,'clean expected-stop shutdown'
            temporary=summary_path.with_suffix('.json.tmp')
            temporary.write_text(json.dumps({'fixture':a.fixture,'bootMedia':a.boot_media,'cpu':a.cpu,'expectedStop':True,'checks':'exact owned rejection; no retirement claimed'},indent=2)+'\n')
            temporary.replace(summary_path)
            print('PASS: Console '+a.fixture+' '+a.boot_media+' exact expected stop, unchanged outputs, owned root and no success marker')
            sys.exit(0)
        # Actual editing: statux + Backspace + s -> status, then JSON diagnostic.
        type_text('statux')
        key('backspace')
        type_text('s --json')
        edited = snapshot('edited')
        assert editor_text(edited['editor'])==('status --json',13),'real PS2 edits and prompt'
        key('ret')
        status = snapshot('status')
        m = struct.unpack('<8Q',status['model'])
        assert m[3:]==(min(MAX,seed+1),3,0,1,0),'fresh completed status result'
        assert struct.unpack('<5Q',status['command'])==(0x31444e4d43484f4a,3,0,1,0),'exact parser result'
        response = status['response']
        ready,length = struct.unpack_from('<2Q',response)
        assert ready==0x3150534552484f4a and length<=4095,'published bounded response'
        assert response[16+length:]==b'\0'*(4096-length),'response zero tail'
        data = json.loads(response[16:16+length])
        f = struct.unpack('<16Q',status['facts'])
        assert f[0]==0x3154434146484f4a and f[1]==0xfd000 and f[6]==2,'owned snapshot identity'
        expected_mode=2 if manifest['timerTest']=='X2' else 1
        assert f[7]==expected_mode and f[8]==16 and f[9]==f[10] and f[11]==0,'real interrupt facts'
        assert data=={'command':'status','status':0,'image_bytes':f[1],'enabled_cpus':f[6],'apic_mode':f[7],'timer_eois':f[8]},'exact JSON schema/values'
        assert editor_text(status['editor'])==('',0),'submitted input cleared'
        if a.fixture=='Saturation':
            assert struct.unpack_from('<Q',status['editor'],72)[0]==MAX and struct.unpack_from('<Q',status['source'])[0]>=2,'first saturation and independent real IRQ entries'
            assert struct.unpack_from('<2Q',status['window'],112)==(MAX,MAX),'IRQ counters saturate'
        # History recall and cancellation must use actual device events too.
        key('up')
        history = snapshot('history')
        assert editor_text(history['editor'])==('status --json',13),'real history recall'
        key('c','ctrl')
        cancelled = snapshot('cancelled')
        assert editor_text(cancelled['editor'])==('',0),'real Ctrl-C cancellation'
        assert struct.unpack_from('<Q',cancelled['editor'],88)[0]==min(MAX,seed+1),'cancellation count'
        completed = min(MAX,seed+1)
        if a.features:
            # Each result is observed before submitting the next command. No guest
            # memory writes or host-side command-string execution are used.
            cases = [('help',1,0,0),('about',2,0,0),('status',3,0,0),
                     ('mem',4,0,0),('mem --json',4,0,1),('ps',5,0,0),
                     ('ps --json',5,0,1),('devices',6,0,0),('devices --json',6,0,1),
                     ('  StAtUs   --json  ',3,0,1),('net',10,69,0),
                     ('unknown',255,127,0),('unknown a b',255,2,0),
                     ('help --json',1,2,0),('status --JSON',3,2,0),
                     ('halt extra',8,2,0),('',0,0,0),('clear',7,0,0)]
            for index,(text,command,code,flag) in enumerate(cases):
                type_text(text)
                key('ret')
                values = snapshot('feature-'+str(index))
                completed = min(MAX,completed+1)
                action = 1 if command==7 and code==0 else 0
                assert struct.unpack('<5Q',values['command']) == (0x31444e4d43484f4a,command,code,flag,action),'feature command: '+text
                assert struct.unpack('<8Q',values['model'])[3:] == (completed,command,code,flag,action),'feature completion: '+text
                assert editor_text(values['editor']) == ('',0),'feature input reset'
                ready,length = struct.unpack_from('<2Q',values['response'])
                body = values['response'][16:16+length]
                assert ready==0x3150534552484f4a and length<=4095 and values['response'][16+length:]==bytes(4096-length),'feature response publication'
                if code:
                    assert body=={2:b'USAGE ERROR (2)\n',69:b'UNAVAILABLE (69)\n',127:b'UNKNOWN COMMAND (127)\n'}[code],'feature exact error'
                elif command==0:
                    assert body==b'','empty command'
                elif flag:
                    names={3:('status',[1,6,7,8]),4:('mem',[2,3,4,5]),5:('ps',[12,13,14,15]),6:('devices',[7,6,9,10,11])}
                    fields='image_bytes free_pages live_allocations candidate_ranges owned_table_pages enabled_cpus apic_mode timer_eois keyboard_irqs keyboard_eois keyboard_drops process_a_state process_a_status process_b_state process_b_status'.split()
                    name,indices=names[command]
                    facts=struct.unpack('<16Q',values['facts'])
                    expected={'command':name,'status':0}
                    expected.update({fields[i-1]:facts[i] for i in indices})
                    assert body==(json.dumps(expected,separators=(',',':'))+'\n').encode(),'feature exact JSON bytes'
                else:
                    constants={1:b'HELP ABOUT STATUS MEM PS DEVICES CLEAR HALT\nSTATUS|MEM|PS|DEVICES --json\n',
                               2:b'JOSHUA / HYPERCORE\nBSP DEVELOPMENT CONSOLE\nASSEMBLY RUNTIME. NO AI EXECUTION.\nQEMU EVIDENCE; PHYSICAL QUALIFICATION OPEN.\n',
                               7:b'CONSOLE CLEARED\n'}
                    if command in constants:
                        assert body==constants[command],'feature exact constant response'
                    else:
                        names={3:('status',[1,6,7,8]),4:('mem',[2,3,4,5]),5:('ps',[12,13,14,15]),6:('devices',[7,6,9,10,11])}
                        fields='image_bytes free_pages live_allocations candidate_ranges owned_table_pages enabled_cpus apic_mode timer_eois keyboard_irqs keyboard_eois keyboard_drops process_a_state process_a_status process_b_state process_b_status'.split()
                        name,indices=names[command]
                        facts=struct.unpack('<16Q',values['facts'])
                        expected=name.upper()+'\n'+''.join(fields[i-1]+'='+str(facts[i])+'\n' for i in indices)
                        assert body==expected.encode(),'feature exact human response'
                qmp('screendump',filename=str(out/(tag+'-feature-'+str(index)+'.ppm')))
                # The guest remains idle with no further input between the
                # physical record capture and display capture.
                validate_pixels(expected_images['grid'] if reference else values['grid'],(out/(tag+'-feature-'+str(index)+'.ppm')).read_bytes(),Path(__file__).resolve().parents[1]/'kernel/console/font.inc')
            # Traverse a wrapped ring and restore an edited draft through actual
            # extended scancodes. Inspect each transition, not just the final text.
            recent=['status --json']+[text for text,_,_,_ in cases if text]
            type_text('draft')
            key('left')
            assert editor_text(snapshot('draft-left-once')['editor'])==('draft',4),'first Left exact cursor'
            key('left')
            draft=snapshot('draft-before-history')
            assert editor_text(draft['editor'])==('draft',3),'draft cursor before browsing'
            for position in range(1,9):
                key('up')
                browsed=snapshot('history-wrap-up-'+str(position))
                text=recent[-position]
                assert editor_text(browsed['editor'])==(text,len(text)),'exact wrapped history order'
                assert struct.unpack_from('<3Q',browsed['editor'],24)==(8,len(recent)%8,position),'wrapped ring metadata'
            key('up')
            oldest=snapshot('history-oldest-boundary')
            assert oldest['editor']==browsed['editor'],'Up at oldest is an exact no-op'
            key('left')
            moved=snapshot('history-cursor-left')
            assert editor_text(moved['editor'])==(recent[-8],len(recent[-8])-1),'Left moves inside oldest history'
            assert struct.unpack_from('<Q',moved['editor'],40)[0]==8,'cursor movement keeps browsing'
            for position in range(7,-1,-1):
                key('down')
                restored=snapshot('history-wrap-down-'+str(position))
                expected=(recent[-position],len(recent[-position])) if position else ('draft',3)
                assert editor_text(restored['editor'])==expected,'Down order and exact draft restoration'
                assert struct.unpack_from('<Q',restored['editor'],40)[0]==position,'Down browse position'
            key('down')
            assert snapshot('history-live-boundary')['editor']==restored['editor'],'Down in draft is an exact no-op'
            type_text('X')
            assert editor_text(snapshot('insert-at-draft-cursor')['editor'])==('draXft',4),'insert at restored cursor'
            key('right')
            assert editor_text(snapshot('draft-right-once')['editor'])==('draXft',5),'Right exact cursor'
            key('delete')
            assert editor_text(snapshot('draft-delete')['editor'])==('draXf',5),'Delete removes trailing t'
            key('backspace')
            assert editor_text(snapshot('right-delete-backspace')['editor'])==('draX',4),'Backspace removes trailing f'
            key('home')
            start_boundary=snapshot('draft-home')
            assert editor_text(start_boundary['editor'])==('draX',0),'Home exact cursor'
            key('left')
            assert snapshot('left-start-boundary')['editor']==start_boundary['editor'],'Left at start is an exact no-op'
            key('backspace')
            assert snapshot('backspace-start-boundary')['editor']==start_boundary['editor'],'Backspace at start is an exact no-op'
            key('right')
            assert editor_text(snapshot('start-right')['editor'])==('draX',1),'Right from start'
            type_text('z')
            assert editor_text(snapshot('insert-z')['editor'])==('dzraX',2),'insert at exact cursor'
            key('end')
            end_boundary=snapshot('draft-end')
            assert editor_text(end_boundary['editor'])==('dzraX',5),'End exact cursor'
            key('right')
            assert snapshot('right-end-boundary')['editor']==end_boundary['editor'],'Right at end is an exact no-op'
            key('delete')
            navigation=snapshot('delete-end-boundary')
            assert navigation['editor']==end_boundary['editor'],'Delete at end is an exact no-op'
            qmp('screendump',filename=str(out/(tag+'-navigation.ppm')))
            validate_pixels(expected_images['grid'] if reference else navigation['grid'],(out/(tag+'-navigation.ppm')).read_bytes(),Path(__file__).resolve().parents[1]/'kernel/console/font.inc')
            key('c','ctrl')
            assert editor_text(snapshot('navigation-cancel')['editor'])==('',0),'cancel restored/edited draft'
            # Capacity, viewport and navigation use the same actual key path.
            capacity_text='a'+'b'*125+'c'
            type_text(capacity_text)
            full=snapshot('full-line')
            assert editor_text(full['editor'])==(capacity_text,127),'127-character line'
            fg=struct.unpack_from('<13Q',full['grid'])
            assert fg[10:13]==(fg[7]-1,fg[8]-2,1),'full-line viewport cursor'
            prompt=full['grid'][104+(fg[8]-2)*100:104+(fg[8]-2)*100+fg[7]]
            assert prompt==b'JOSHUA> '+capacity_text.encode()[127-(fg[7]-9):]+b' ','full-line viewport reserves visible cursor cell'
            qmp('screendump',filename=str(out/(tag+'-full-line.ppm')))
            validate_pixels(expected_images['grid'] if reference else full['grid'],(out/(tag+'-full-line.ppm')).read_bytes(),Path(__file__).resolve().parents[1]/'kernel/console/font.inc')
            rejected_before=struct.unpack_from('<Q',full['editor'],80)[0]
            if a.fixture=='Saturation':
                assert rejected_before==seed,'accepted input preserves rejection seed'
            key('b')
            rejected=snapshot('full-rejection')
            assert editor_text(rejected['editor'])==(capacity_text,127),'full insertion preserves text'
            assert struct.unpack_from('<Q',rejected['editor'],80)[0]==min(MAX,rejected_before+1),'full rejection accounting'
            if a.fixture=='Saturation':
                key('z')
                again=snapshot('second-full-rejection')
                assert editor_text(again['editor'])==(capacity_text,127) and struct.unpack_from('<Q',again['editor'],80)[0]==MAX,'rejected counter remains saturated'
            key('home')
            assert editor_text(snapshot('home')['editor'])==(capacity_text,0),'Home exact cursor'
            key('delete')
            assert editor_text(snapshot('delete')['editor'])==('b'*125+'c',0),'Delete removes leading a'
            key('end')
            assert editor_text(snapshot('end')['editor'])==('b'*125+'c',126),'End exact cursor'
            key('backspace')
            navigated=snapshot('navigation')
            assert editor_text(navigated['editor'])==('b'*125,125),'Home/Delete/End/Backspace'
            key('c','ctrl')
            assert editor_text(snapshot('capacity-cancel')['editor'])==('',0),'capacity cancellation'
        type_text('halt')
        key('ret')
        if a.fixture=='DrainExhaust':
            failed=timer_stop()
            assert log().count('JOSHUA:CONSOLE_FAILURE')==1,'foreground drain rejection'
            assert struct.unpack('<8Q',failed['model'])[3:]==(min(MAX,completed+1),8,0,0,2),'valid halt before drain limit'
            assert struct.unpack('<5Q',failed['command'])==(0x31444e4d43484f4a,8,0,0,2),'exact halt command'
            response=b'HALTING: QUIESCING KEYBOARD\n'
            assert failed['response']==struct.pack('<2Q',0x3150534552484f4a,len(response))+response.ljust(4096,b'\0'),'halt response without completion'
            c=struct.unpack('<7Q',failed['controller'])
            assert c[:3]==(0,0,11) and c[5]==0,'retired controller despite timer pending'
            selected=qmp('human-monitor-command',**{'command-line':'xp /1wx 0xfec00000'})
            route=qmp('human-monitor-command',**{'command-line':'xp /1wx 0xfec00010'})
            assert int(re.findall(r'0x([0-9a-fA-F]{8})\b',selected)[-1],16)==0x12,'keyboard selector'
            assert int(re.findall(r'0x([0-9a-fA-F]{8})\b',route)[-1],16)&0x1afff==0x10044,'masked route'
            port=qmp('human-monitor-command',**{'command-line':'i /b 0x64'})
            validate_output_quiet(port)
            for name,text in [('ioregsel',selected),('ioapic',route),('i8042',port)]:
                (out/(tag+'-timer-rejected-'+name+'.log')).write_text(text)
            finish_timer_stop()
            sys.exit(0)
        while 'JOSHUA:KERNEL_HALTED' not in log():
            healthy()
            time.sleep(.02)
        terminal = snapshot('halted',True)
        if a.fixture=='PendingTimer':
            validate_timer_fixture(a.fixture,terminal['source'],terminal['window'],(out/(tag+'-halted-lapic.log')).read_text())
        if a.fixture=='Saturation':
            assert struct.unpack_from('<3Q',terminal['editor'],72)==(MAX,MAX,MAX),'all editor counters remain saturated'
            assert struct.unpack_from('<Q',terminal['model'],24)[0]==MAX,'completion remains saturated'
            assert struct.unpack_from('<2Q',terminal['window'],112)==(MAX,MAX),'IRQ counters remain saturated'
            assert struct.unpack('<Q',terminal['source'])[0]>struct.unpack('<Q',status['source'])[0],'independent subsequent IRQ entries'
        regs = registers()
        assert 'HLT=1' in regs and 'CPL=0' in regs and not(int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200),'permanent IF0 ring0 halt'
        m = struct.unpack('<8Q',terminal['model'])
        assert m[3:]==(min(MAX,completed+1),8,0,0,2),'halt follows exact valid command'
        grid = terminal['grid']
        g = struct.unpack_from('<13Q',grid)
        assert g[0]==0x3144495247484f4a and g[9]==0 and g[12]==0,'rendered grid / disabled cursor'
        cells = grid[104:]
        assert cells[(g[8]-1)*100:(g[8]-1)*100+g[7]]==b'HALTED / INTERRUPTS RETIRED'.ljust(g[7],b' '),'visible halt only after retirement'
        c = struct.unpack('<7Q',terminal['controller'])
        assert c[0:3]==(0,0,11) and c[5]==0,'actual controller terminal state'
        selected=qmp('human-monitor-command',**{'command-line':'xp /1wx 0xfec00000'})
        (out/(tag+'-halted-ioregsel.log')).write_text(selected)
        assert int(re.findall(r'0x([0-9a-fA-F]{8})\b',selected)[-1],16)==0x12,'q35 keyboard IRQ1 route selector'
        route=qmp('human-monitor-command',**{'command-line':'xp /1wx 0xfec00010'})
        (out/(tag+'-halted-ioapic.log')).write_text(route)
        assert int(re.findall(r'0x([0-9a-fA-F]{8})\b',route)[-1],16)&0x1afff==0x10044,'actual masked route'
        port=qmp('human-monitor-command',**{'command-line':'i /b 0x64'})
        (out/(tag+'-halted-i8042.log')).write_text(port)
        validate_output_quiet(port)
        qmp('screendump',filename=str(out/(tag+'-screen.ppm')))
        # Actual display pixels, independently rasterized from the constant halt
        # text and authored font asset, rather than trusting grid bytes alone.
        ppm = (out/(tag+'-screen.ppm')).read_bytes()
        validate_pixels(expected_images['grid'] if reference else grid,ppm,Path(__file__).resolve().parents[1]/'kernel/console/font.inc')
        header = re.match(rb'^P6\n(\d+) (\d+)\n255\n',ppm)
        assert header,'QEMU P6 screenshot header'
        width,height = map(int,header.groups())
        pixels = ppm[header.end():]
        assert (width,height)==(g[3],g[4]) and len(pixels)==width*height*3,'actual GOP display dimensions'
        assert g[7]==min(100,(width-32)//12) and g[8]==min(48,(height-32)//16),'derived screen geometry'
        font=[]
        for line in (Path(__file__).resolve().parents[1]/'kernel/console/font.inc').read_text().splitlines():
            if line.strip().startswith('db '):
                font.append([int(x) for x in line.split(';')[0].strip()[3:].split(',')])
        assert len(font)==95 and all(len(row)==7 and max(row)<32 for row in font),'font bounds'
        for x,char in enumerate('HALTED / INTERRUPTS RETIRED'.ljust(g[7])):
            glyph=font[ord(char)-32]
            for dy in range(16):
                for dx in range(12):
                    ink=dy<14 and dx<10 and glyph[dy//2]&(16>>(dx//2))
                    offset=((16+(g[8]-1)*16+dy)*width+16+x*12+dx)*3
                    assert pixels[offset:offset+3]==bytes((170,220,255) if ink else (2,6,12)),'actual visible halt pixels'
        assert g[10:12]==(8,g[8]-2),'independently expected final prompt cursor position'
        for dy in [14,15]:
            for dx in range(g[7]*12):
                offset=((16+(g[8]-2)*16+dy)*width+16+dx)*3
                assert pixels[offset:offset+3]==bytes((2,6,12)),'actual cursor underline retired'
        def png_chunk(kind,data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
        raw=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
        png=(b'\x89PNG\r\n\x1a\n'+png_chunk(b'IHDR',struct.pack('>2I5B',width,height,8,2,0,0,0))
             +png_chunk(b'IDAT',zlib.compress(raw))+png_chunk(b'IEND',b''))
        (out/(tag+'-screen.png')).write_bytes(png)
        # QEMU may close QMP before delivering a quit return; wait for process exit.
        stream.write(b'{"execute":"quit"}\n')
        stream.flush()
        process.wait(timeout=5)
        assert process.returncode==0,'clean QEMU shutdown'
        if reference:
            trace_path.write_text(json.dumps(live_events,indent=2)+'\n')
        temporary=summary_path.with_suffix('.json.tmp')
        temporary.write_text(json.dumps({'bootMedia':a.boot_media,'cpu':a.cpu,'idleSeconds':a.idle_seconds,'commands':20 if a.features else 2,'fixture':a.fixture,'features':a.features,'independentLiveOracle':bool(reference),'checks':'real PS2 commands, complete state/transcript/source-facts and pixel checks for normal profile; fixture scope recorded separately'},indent=2)+'\n')
        temporary.replace(summary_path)

        print(f'PASS: Console commands={20 if a.features else 2} features={a.features} {a.boot_media} real PS2 editing/history/JSON, {a.idle_seconds}s timer-free idle, owned IRQs and physical IF0 halt retirement; broader campaign pending.')
    finally:
        if client:
            client.close()
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
