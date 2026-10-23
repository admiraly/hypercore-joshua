"""Actual q35 PS/2 -> IOAPIC -> IRQ/IRETQ window, with physical QMP evidence.
This bounded target suite is separate from the full private-root/regression campaign.
"""
import argparse
import json
import pathlib
import re
import shutil
import socket
import struct
import subprocess
import time
from keyboard_runtime_oracle import validate_lapic, validate_queue_indices

p=argparse.ArgumentParser()
p.add_argument('output',type=pathlib.Path)
p.add_argument('--qemu',required=True)
p.add_argument('--firmware',type=pathlib.Path,required=True)
p.add_argument('--boot-media',choices=['Iso','Fat'],default='Iso')
p.add_argument('--cpu',default='qemu64')
p.add_argument('--timeout',type=int,default=90)
a=p.parse_args();out=a.output.resolve();manifest=json.loads((out/'manifest.json').read_text())
mode=manifest.get('keyboardTest')
assert mode in ['Text','Df','Missing'], 'requires an explicit keyboard image'
tag='keyboard-'+a.boot_media.lower();serial=out/(tag+'-serial.log');serial.write_text('')
variables=out/(tag+'-vars.fd');shutil.copyfile(a.firmware/'edk2-i386-vars.fd',variables)
with socket.socket() as reserve:
    reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
args=[a.qemu,'-machine','q35','-accel','tcg','-cpu',a.cpu,'-smp','2','-m','512','-vga','std','-nic','none','-display','none','-no-reboot','-no-shutdown',
      '-drive',f'if=pflash,format=raw,unit=0,readonly=on,file={a.firmware.resolve() / "edk2-x86_64-code.fd"}',
      '-drive',f'if=pflash,format=raw,unit=1,file={variables}', '-serial',f'file:{serial}', '-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
if a.boot_media=='Iso':args+=['-drive',f'media=cdrom,readonly=on,format=raw,file={out / "joshua-m0.iso"}','-boot','order=d,menu=off']
else:args+=['-drive',f'if=none,id=bootdisk,readonly=on,format=raw,file={out / "joshua-uefi.img"}','-device','qemu-xhci','-device','usb-storage,drive=bootdisk,removable=on','-boot','order=c,menu=off']
client=None;process=None
with (out/(tag+'-stderr.log')).open('wb') as err, (out/(tag+'-stdout.log')).open('wb') as stdout:
    try:
        process=subprocess.Popen(args,stderr=err,stdout=stdout)
        deadline=time.monotonic()+a.timeout
        while time.monotonic()<deadline:
            try:client=socket.create_connection(('127.0.0.1',port),timeout=2);break
            except OSError:
                assert process.poll() is None,'QEMU exited before QMP'
                time.sleep(.05)
        assert client,'QMP connection deadline'
        client.settimeout(5);stream=client.makefile('rwb')
        assert 'QMP' in json.loads(stream.readline()),'QMP greeting'
        def qmp(command,**arguments):
            stream.write((json.dumps({'execute':command,'arguments':arguments})+'\n').encode());stream.flush()
            while True:
                response=json.loads(stream.readline())
                assert 'error' not in response,response
                if 'return' in response:return response['return']
        qmp('qmp_capabilities')
        def log():return serial.read_text(errors='replace')
        while 'JOSHUA:KEYBOARD_WINDOW ADDRESS=' not in log() and time.monotonic()<deadline:
            assert process.poll() is None,'QEMU exited'
            assert 'JOSHUA:KERNEL_FAULT' not in log(),log()
            time.sleep(.02)
        assert 'JOSHUA:KEYBOARD_WINDOW ADDRESS=' in log(),'window not reached: '+log()
        if mode!='Missing':
            for keys in [['a'],['shift','a'],['caps_lock'],['b'],['caps_lock'],['ctrl','c'],['left'],['backspace'],['ret']]:
                qmp('send-key',**{'keys':[{'type':'qcode','data':key} for key in keys],'hold-time':50})
                time.sleep(.12)
        while 'JOSHUA:KERNEL_HALTED' not in log() and 'JOSHUA:KERNEL_FAULT' not in log() and time.monotonic()<deadline:
            assert process.poll() is None,'QEMU exited'
            time.sleep(.02)
        qmp('stop')
        registers=qmp('human-monitor-command',**{'command-line':'info registers'})
        (out/(tag+'-registers.log')).write_text(registers)
        lapic=qmp('human-monitor-command',**{'command-line':'info lapic'})
        (out/(tag+'-lapic.log')).write_text(lapic)
        match=re.search(r'JOSHUA:KEYBOARD_WINDOW ADDRESS=([0-9A-F]{16}) DECODER=([0-9A-F]{16}) CONTROLLER=([0-9A-F]{16})',log())
        assert match,'snapshot addresses'
        addresses=[int(v,16) for v in match.groups()]
        def physical(address,size,name):
            path=out/(tag+'-'+name+'.bin')
            qmp('pmemsave',val=address,size=size,filename=str(path))
            value=path.read_bytes();assert len(value)==size
            return value
        window=physical(addresses[0],288,'window');w=struct.unpack('<20Q64H',window)
        decoder=physical(addresses[1],216,'decoder');d=struct.unpack('<11Q64H',decoder)
        controller=physical(addresses[2],56,'controller');c=struct.unpack('<7Q',controller)
        if 'JOSHUA:KERNEL_FAULT' in log():
            physical((w[3]&~15)-192,192,'last-frame')
            raise AssertionError(('kernel fault',w[:20],registers))
        assert 'JOSHUA:KEYBOARD_WINDOW_DONE' in log() and 'JOSHUA:KERNEL_HALTED' in log(),'window deadline: '+log()
        apic_mode=re.search(r'JOSHUA:APIC_READY MODE=([0-9A-F]{16})',log())
        assert apic_mode and int(apic_mode[1],16)==(2 if manifest.get('timerTest')=='X2' else 1),'actual APIC mode'
        validate_lapic(lapic)
        assert w[0]==0x3157424b48534f4a and w[1]==3,'window publication/retirement'
        assert w[14]==w[15] and w[16]>=w[13] and w[18]<=1024,'IRQ/EOI/drain accounting'
        assert c[0:3]==(0,0,11) and c[5]==0 and c[4]&~12==0x70,'controller terminal state'
        assert d[0]==0x3144424b48534f4a and d[1:6]==(0,0,0,0,0),'decoder transient state'
        validate_queue_indices(d[6],d[7],d[8])
        assert d[8]==0 and d[9]==0 and d[10]==0,'queue empty/no drops/protocol errors'
        if mode=='Missing':
            assert w[13:18]==(16,0,0,16,1) and w[19]==0,'bounded missing-input deadline'
        else:
            expected=[97,65,66,3,0x100,8,13]
            assert w[17]==0 and w[14]>0 and w[19]==len(expected),'real IRQ and success'
            assert list(w[20:20+len(expected)])==expected,'ordered actual keyboard events'
            assert w[10]==0x202,'normalized drain metadata'
            returned_flags=struct.unpack('<Q',physical(addresses[0]+288,8,'returned-flags'))[0]
            assert returned_flags==(0x402 if mode=='Df' else 2),'actual post-CLI DF flags'
        flags=re.search(r'RFL=([0-9a-fA-F]+)',registers)
        assert flags and int(flags[1],16)&0x200==0 and 'CPL=0' in registers and 'HLT=1' in registers,'halted IF0 ring0'
        # Cleanup's final IOAPIC transaction reads the owned low entry. Independent
        # physical MMIO inspection verifies mask/vector rather than trusting its log.
        route=qmp('human-monitor-command',**{'command-line':'xp /1wx 0xfec00010'})
        (out/(tag+'-ioapic.log')).write_text(route)
        values=re.findall(r'0x([0-9a-fA-F]{8})\b',route)
        assert values and int(values[-1],16)&0x1afff==0x10044,'actual IOAPIC route mask/vector'
        evidence='deadline IRQ/IRETQ' if mode=='Missing' else 'PS2 key delivery and IRQ/IRETQ'
        print(f'PASS: {mode} {a.boot_media} actual {evidence}, physical queue/controller/route and LAPIC retirement; broader qualification pending.')
        stream.write(b'{"execute":"quit"}\n');stream.flush();process.wait(timeout=5)
    finally:
        if client:client.close()
        if process and process.poll() is None:process.kill();process.wait(timeout=5)
