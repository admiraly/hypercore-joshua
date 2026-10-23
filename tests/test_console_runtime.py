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
from keyboard_runtime_oracle import validate_lapic, validate_output_quiet, validate_queue_indices

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--qemu',required=True)
p.add_argument('--firmware',type=Path,required=True)
p.add_argument('--boot-media',choices=['Iso','Fat'],default='Iso')
p.add_argument('--cpu',default='qemu64')
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--idle-seconds',type=int,default=30)
a = p.parse_args()
assert 30 <= a.idle_seconds <= 120,'idle evidence requires at least30 seconds'
out = a.output.resolve()
summary_path = out/('console-'+a.boot_media.lower()+'-summary.json')
summary_path.unlink(missing_ok=True) # stale success never survives a failed rerun
manifest = json.loads((out/'manifest.json').read_text())
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
        while 'JOSHUA:CONSOLE_READY MODEL=' not in log():
            healthy()
            time.sleep(.02)
        match = re.search(r'CONSOLE_READY MODEL=([0-9A-F]{16}) GRID=([0-9A-F]{16}) EDITOR=([0-9A-F]{16}) COMMAND=([0-9A-F]{16}) RESPONSE=([0-9A-F]{16}) FACTS=([0-9A-F]{16}) WINDOW=([0-9A-F]{16}) DECODER=([0-9A-F]{16}) CONTROLLER=([0-9A-F]{16})',log())
        assert match,'complete evidence addresses'
        addresses = dict(zip(['model','grid','editor','command','response','facts','window','decoder','controller'],[int(x,16) for x in match.groups()]))
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
        def snapshot(suffix,terminal=False):
            if not terminal:
                idle()
            qmp('stop')
            values = {name:physical(name,size,suffix) for name,size in
                      [('model',64),('grid',4904),('editor',1568),('command',40),('response',4112),('facts',128),('window',288),('decoder',216),('controller',56)]}
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
            assert w[13]==w[16]==w[17]==0,'no timer ticks/EOIs or collection failure'
            assert w[14]==w[15],'actual IRQ44/EOI accounting'
            assert w[2]==int(re.search(r'OWNED_PAGING CR3=([0-9A-F]+)',log())[1],16),'retained owned root'
            assert 'CR3='+format(w[2],'016x') in regs.lower().replace('cr3=','CR3='),'actual current root'
            if not terminal:
                assert int(re.search(r'RSP=([0-9A-Fa-f]+)',regs)[1],16)==w[3],'actual exact wait RSP'
                assert int(re.search(r'RIP=([0-9A-Fa-f]+)',regs)[1],16)==w[6],'actual exact wait continuation'
                assert w[4]+4096+192<=w[3]<=w[5] and w[10]==0x202,'owned stack/flags metadata'
                assert 'HLT=1' in regs and int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200,'IF1 idle'
                qmp('cont')
            return values
        baseline = snapshot('ready')
        assert struct.unpack('<8Q',baseline['model'])==(0x314c444f4d484f4a,2,0,0,0,0,0,0),'initial model'
        start = time.monotonic()
        while time.monotonic()-start<a.idle_seconds:
            healthy()
            time.sleep(.1)
        after_idle = snapshot('idle')
        assert after_idle['model']==baseline['model'] and after_idle['editor']==baseline['editor'],'idle preserves input/model'
        assert after_idle['window']==baseline['window'],'idle has no synthetic input/deadline progress'
        def key(code,modifier=None):
            events=[]
            if modifier:
                events.append({'type':'key','data':{'down':True,'key':{'type':'qcode','data':modifier}}})
            events += [{'type':'key','data':{'down':down,'key':{'type':'qcode','data':code}}} for down in [True,False]]
            if modifier:
                events.append({'type':'key','data':{'down':False,'key':{'type':'qcode','data':modifier}}})
            qmp('input-send-event',events=events)
            time.sleep(.035)
        def type_text(text):
            for char in text:
                key({' ':'spc','-':'minus'}.get(char,char.lower()),'shift' if char.isupper() else None)
        def editor_text(data):
            length,cursor = struct.unpack_from('<2Q',data,8)
            assert length<=127 and cursor<=length,'bounded editor'
            assert data[96+length:224]==b'\0'*(128-length),'editor zero tail'
            return data[96:96+length].decode('ascii'),cursor
        # Actual editing: statux + Backspace + s -> status, then JSON diagnostic.
        type_text('statux')
        key('backspace')
        type_text('s --json')
        edited = snapshot('edited')
        assert editor_text(edited['editor'])==('status --json',13),'real PS2 edits and prompt'
        key('ret')
        status = snapshot('status')
        m = struct.unpack('<8Q',status['model'])
        assert m[3:]==(1,3,0,1,0),'fresh completed status result'
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
        # History recall and cancellation must use actual device events too.
        key('up')
        history = snapshot('history')
        assert editor_text(history['editor'])==('status --json',13),'real history recall'
        key('c','ctrl')
        cancelled = snapshot('cancelled')
        assert editor_text(cancelled['editor'])==('',0),'real Ctrl-C cancellation'
        assert struct.unpack_from('<Q',cancelled['editor'],88)[0]==1,'cancellation count'
        type_text('halt')
        key('ret')
        while 'JOSHUA:KERNEL_HALTED' not in log():
            healthy()
            time.sleep(.02)
        terminal = snapshot('halted',True)
        regs = registers()
        assert 'HLT=1' in regs and 'CPL=0' in regs and not(int(re.search(r'RFL=([0-9A-Fa-f]+)',regs)[1],16)&0x200),'permanent IF0 ring0 halt'
        m = struct.unpack('<8Q',terminal['model'])
        assert m[3:]==(2,8,0,0,2),'halt follows exact valid command'
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
        temporary=summary_path.with_suffix('.json.tmp')
        temporary.write_text(json.dumps({'bootMedia':a.boot_media,'cpu':a.cpu,'idleSeconds':a.idle_seconds,'commands':2,'checks':'real editing/history/JSON/idle/halt pixels; full campaign pending'},indent=2)+'\n')
        temporary.replace(summary_path)

        print(f'PASS: Console {a.boot_media} real PS2 editing/history/JSON, {a.idle_seconds}s timer-free idle, owned IRQs and physical IF0 halt retirement; broader campaign pending.')
    finally:
        if client:
            client.close()
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
