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
from keyboard_runtime_oracle import validate_lapic, validate_queue_indices, validate_stopped_lapic, validate_refill_evidence, validate_output_quiet

p=argparse.ArgumentParser()
p.add_argument('output',type=pathlib.Path)
p.add_argument('--qemu',required=True)
p.add_argument('--firmware',type=pathlib.Path,required=True)
p.add_argument('--boot-media',choices=['Iso','Fat'],default='Iso')
p.add_argument('--cpu',default='qemu64')
p.add_argument('--timeout',type=int,default=90)
a=p.parse_args();out=a.output.resolve();manifest=json.loads((out/'manifest.json').read_text())
mode=manifest.get('keyboardTest')
assert mode in ['Text','Df','Missing','Pending40','Pending44','PendingBoth','DrainExhaust','BadRip','BadSs','BadFlags','BadRsp','BadAddress','BadRoot','UnexpectedTimer','UnexpectedKeyboard','Overflow','OverflowPending'], 'requires an explicit keyboard image'
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
        def physical(address,size,name):
            path=out/(tag+'-'+name+'.bin')
            qmp('pmemsave',val=address,size=size,filename=str(path))
            value=path.read_bytes();assert len(value)==size
            return value
        def log():return serial.read_text(errors='replace')
        while 'JOSHUA:KEYBOARD_WINDOW ADDRESS=' not in log() and time.monotonic()<deadline:
            assert process.poll() is None,'QEMU exited'
            assert 'JOSHUA:KERNEL_FAULT' not in log(),log()
            time.sleep(.02)
        assert 'JOSHUA:KEYBOARD_WINDOW ADDRESS=' in log(),'window not reached: '+log()
        baseline_decoder=None
        if mode=='Overflow':
            address=int(re.search(r'DECODER=([0-9A-F]{16})',log())[1],16)
            baseline_decoder=struct.unpack('<11Q64H',physical(address,216,'decoder-before'))
            assert baseline_decoder[:11]==(0x3144424b48534f4a,0,0,0,0,0,0,0,64,0,0),'full queue baseline metadata'
            assert list(baseline_decoder[11:])==[0x5151]*64,'full queue baseline storage'
        if mode=='OverflowPending':
            address_match=re.search(r'ADDRESS=([0-9A-F]{16}) DECODER=([0-9A-F]{16}) CONTROLLER=([0-9A-F]{16}) FIXTURE=([0-9A-F]{16})',log())
            arm_addresses=[int(value,16) for value in address_match.groups()]
            before=struct.unpack('<11Q64H',physical(arm_addresses[1],216,'decoder-before'))
            assert before==(0x3144424b48534f4a,)+(0,)*74,'empty queue baseline before controlled producer'
            arm=struct.unpack('<20Q64H',physical(arm_addresses[0],288,'armed-window'))
            assert arm[1]==1 and arm[13:18]==(0,0,0,0,0),'armed window baselines'
            def inject(pairs):
                events=[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':'a'}}} for _ in range(pairs) for down in [True,False]]
                qmp('input-send-event',events=events)
            inject(8)
            trace=[]
            for threshold,pairs in [(16,8),(32,1)]:
                while True:
                    assert time.monotonic()<deadline,'controlled producer deadline'
                    assert 'JOSHUA:KERNEL_FAULT' not in log(),'premature controlled-producer stop'
                    evidence=struct.unpack('<18Q',physical(arm_addresses[3],144,'refill-'+str(threshold)))
                    assert evidence[15]==0,'producer refill timed out before injection'
                    assert evidence[11]<=threshold,'collector skipped refill threshold'
                    if evidence[11]==threshold:
                        assert evidence[0]==2,'collector refill outside armed interval'
                        trace.append({'collectorReads':threshold,'stage':evidence[0],'pairsInjected':pairs})
                        inject(pairs)
                        break
                    time.sleep(.003)
            (out/(tag+'-producer.json')).write_text(json.dumps(trace,indent=2)+'\n')
        if mode in ['Pending44','PendingBoth','UnexpectedKeyboard','Overflow']:
            qmp('send-key',**{'keys':[{'type':'qcode','data':'a'}],'hold-time':50})
        if mode in ['Text','Df']:
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
        match=re.search(r'JOSHUA:KEYBOARD_WINDOW ADDRESS=([0-9A-F]{16}) DECODER=([0-9A-F]{16}) CONTROLLER=([0-9A-F]{16}) FIXTURE=([0-9A-F]{16})',log())
        assert match,'snapshot addresses'
        addresses=[int(v,16) for v in match.groups()]
        window=physical(addresses[0],288,'window');w=struct.unpack('<20Q64H',window)
        decoder=physical(addresses[1],216,'decoder');d=struct.unpack('<11Q64H',decoder)
        controller=physical(addresses[2],56,'controller');c=struct.unpack('<7Q',controller)
        fixture=struct.unpack('<18Q',physical(addresses[3],144,'fixture'))
        stops=['OverflowPending','DrainExhaust','BadRip','BadSs','BadFlags','BadRsp','BadAddress','BadRoot','UnexpectedTimer','UnexpectedKeyboard']
        stopped=mode in stops
        assert ('JOSHUA:KERNEL_FAULT' in log())==stopped,'typed kernel stop expectation'
        if stopped:
            assert 'JOSHUA:KEYBOARD_WINDOW_DONE' not in log() and 'JOSHUA:KERNEL_HALTED' not in log(),'stop resumed normal continuation'
        else:
            assert 'JOSHUA:KEYBOARD_WINDOW_DONE' in log() and 'JOSHUA:KERNEL_HALTED' in log(),'window deadline: '+log()
        apic_mode=re.search(r'JOSHUA:APIC_READY MODE=([0-9A-F]{16})',log())
        assert apic_mode and int(apic_mode[1],16)==(2 if manifest.get('timerTest')=='X2' else 1),'actual APIC mode'
        assert w[0]==0x3157424b48534f4a,'window publication'
        assert d[0]==0x3144424b48534f4a and d[1:6]==(0,0,0,0,0),'decoder transient state'
        validate_queue_indices(d[6],d[7],d[8])
        if mode not in ['Overflow','OverflowPending']:
            assert d[8]==0 and d[9]==0 and d[10]==0,'queue empty/no drops/protocol errors'
        if stopped:
            if mode=='OverflowPending':
                assert fixture[0]==3 and fixture[1]==fixture[6]==0x11,'joint pending proof'
                assert fixture[8:11]==(0,0,0),'armed IRQ/EOI baselines'
                validate_refill_evidence(fixture)
                assert fixture[14]&1==0,'actual output quiet at cleanup'
                assert w[1]==3 and w[13]==0 and w[14]>=2 and w[14]==w[15] and w[16]>=1 and w[17]==2 and w[19]==0,'terminal reentry and preserved failure2'
                assert c[0:3]==(0,0,11) and c[5]==0 and c[4]&~12==0x70,'overflow source retirement'
                assert d[6:11]==(0,16,16,0,0) and list(d[11:27])==[97]*16,'exact32 translated bytes/16 make events'
                validate_lapic(lapic)
            elif mode=='DrainExhaust':
                assert fixture[0:4]==(6,1,0,0) and fixture[6]==1,'actual timer pending before drain'
                assert w[1]==2 and w[13:18]==(0,0,0,0,0) and w[18]==1025 and w[19]==0,'exact exhaustion before EOI/return'
                assert c[0:3]==(0,0,11) and c[5]==0 and c[4]&~12==0x70,'exhaustion source quiescence'
                validate_stopped_lapic(lapic,set(),{64},True)
            else:
                assert fixture[0]==5 and fixture[4:6]==(0,0),'frame rejection evidence'
                assert w[1]==(3 if mode.startswith('Unexpected') else 1),'rejected ownership phase'
                assert w[13:18]==(0,0,0,0,0) and w[18:20]==(0,0),'rejection before any EOI/return/consume'
                assert c[0:3]==(0x3143424b48534f4a,0,10) and c[5]==1,'active controller at rejected entry'
                assert fixture[7]==(w[3]&~15)-192,'actual rejected CPU frame location'
                frame=struct.unpack('<24Q',physical(fixture[7],192,'rejected-frame'))
                assert frame[19]==(w[6]^1 if mode=='BadRip' else w[6]),'rejected RIP'
                assert frame[20]==w[8] and frame[23]==(w[9]^8 if mode=='BadSs' else w[9]),'rejected selectors'
                assert frame[21]&~0x10000==(0x40202 if mode=='BadFlags' else 0x202),'rejected flags'
                assert frame[22]==w[3]+(8 if mode=='BadRsp' else 0),'rejected old RSP'
                if mode=='UnexpectedKeyboard':
                    assert fixture[1]==fixture[6]==0x10,'actual unexpected keyboard pending proof'
                    validate_stopped_lapic(lapic,{68},set(),True)
                else:validate_stopped_lapic(lapic,{64},set(),False)
                cr3=re.search(r'CR3=([0-9a-fA-F]+)',registers)
                expected_root=w[2]
                if mode=='BadRoot':
                    first=re.search(r'JOSHUA:USER_RESULT INDEX=0000000000000001 ROOT=([0-9A-F]{16})',log())
                    assert first,'private root evidence'
                    expected_root=int(first[1],16);assert expected_root!=w[2]
                assert cr3 and int(cr3[1],16)==expected_root,'actual rejected root'
        else:
            validate_lapic(lapic)
            assert w[1]==3 and w[14]==w[15] and w[16]>=w[13] and w[18]<=1024,'IRQ/EOI/drain accounting'
            assert c[0:3]==(0,0,11) and c[5]==0 and c[4]&~12==0x70,'controller terminal state'
            if mode.startswith('Pending'):
                mask={'Pending40':1,'Pending44':0x10,'PendingBoth':0x11}[mode]
                assert fixture[0:4]==(3,mask,0,0) and fixture[6]==mask,'actual pending sources before EOI'
                assert w[13:18]==(0,int(bool(mask&16)),int(bool(mask&16)),int(bool(mask&1)),0),'exact drain EOIs without active ticks'
                assert 1<=w[18]<=1024 and w[19]==0,'nonempty drain and no consumer input'
            elif mode=='Overflow':
                assert baseline_decoder and d[:9]==baseline_decoder[:9] and d[10:]==baseline_decoder[10:] and d[9]==baseline_decoder[9]+1,'drop newest preserves every old event and queue field'
                assert w[17]==0 and w[14]>0 and w[19]==0 and fixture[14]&1==0,'real full-queue input with no consumer'
            elif mode=='Missing':
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
        expected_route=0x44 if stopped and mode not in ['DrainExhaust','OverflowPending'] else 0x10044
        assert values and int(values[-1],16)&0x1afff==expected_route,'actual IOAPIC route mask/vector'
        if mode in ['Overflow','OverflowPending']:
            status=qmp('human-monitor-command',**{'command-line':'i /b 0x64'})
            (out/(tag+'-i8042-status.log')).write_text(status)
            validate_output_quiet(status)
        evidence='bounded collection and terminal reentry' if mode=='OverflowPending' else ('exact stop before EOI/return' if stopped else ('pending-vector drain' if mode.startswith('Pending') else ('deadline IRQ/IRETQ' if mode=='Missing' else 'PS2 key delivery and IRQ/IRETQ')))
        print(f'PASS: {mode} {a.boot_media} actual {evidence}, physical queue/controller/route and exact LAPIC state; broader qualification pending.')
        stream.write(b'{"execute":"quit"}\n');stream.flush();process.wait(timeout=5)
    finally:
        if client:client.close()
        if process and process.poll() is None:process.kill();process.wait(timeout=5)
