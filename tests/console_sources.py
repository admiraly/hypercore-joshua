"""Bind console captures to a hashed build listing and independently located sources."""
import hashlib
import re
import struct


class Sources:
    def __init__(self,out,manifest,log,console_addresses,registers):
        entries={item['path']:item for item in manifest['artifacts']}
        listing=(out/'boot.lst').read_bytes()
        assert len(listing)==entries['boot.lst']['bytes'] and hashlib.sha256(listing).hexdigest()==entries['boot.lst']['sha256'].lower(),'listing artifact identity'
        lines=listing.decode().splitlines()
        def symbol(name):
            matches=[i for i,line in enumerate(lines) if re.search(r'(?<![\w.])'+re.escape(name)+r':',line)]
            assert len(matches)==1,'unique listing symbol '+name
            i=matches[0]
            for line in lines[i:i+8]:
                match=re.match(r'\s*\d+\s+([0-9A-F]{8})\s',line)
                if match:return int(match[1],16)
            raise AssertionError('unbound listing symbol '+name)
        image=(out/'BOOTX64.EFI').read_bytes()
        assert len(image)==entries['BOOTX64.EFI']['bytes'] and hashlib.sha256(image).hexdigest()==entries['BOOTX64.EFI']['sha256'].lower(),'EFI artifact identity'
        pe=struct.unpack_from('<I',image,60)[0]
        assert image[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',image,pe+4)[0]==0x8664
        optional=pe+24
        assert struct.unpack_from('<H',image,optional)[0]==0x20b
        self.image_size=struct.unpack_from('<I',image,optional+56)[0]
        table=optional+struct.unpack_from('<H',image,pe+20)[0]
        sections={}
        for i in range(struct.unpack_from('<H',image,pe+6)[0]):
            pos=table+i*40
            name=image[pos:pos+8].rstrip(b'\0').decode()
            size,rva=struct.unpack_from('<II',image,pos+8)
            assert name not in sections and rva+size<=self.image_size
            sections[name]=(rva,size)
        assert set(sections)=={'.text','.data','.rodata','.bss','.reloc'}
        tss=int(re.search(r'TSS_READY SELECTOR=[0-9A-F]+ BASE=([0-9A-F]+)',log)[1],16)
        actual=re.search(r'(?m)^TR\s*=\s*[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+',registers.replace('\\r\\n','\n').replace('\r',''))
        assert actual and int(actual[1],16)==tss,'actual loaded TSS base'
        self.base=tss-sections['.data'][0]-symbol('kernel_tss')
        assert self.base>0 and self.base%4096==0 and self.base+self.image_size<=1<<52
        def address(name,section,size):
            offset=symbol(name)
            rva,extent=sections[section]
            assert 0<=offset and offset+size<=extent,'symbol extent '+name
            return self.base+rva+offset
        self.addresses={}
        for name,label,size in [('model','console_model',64),('grid','console_grid',4904),('editor','console_editor',1568),('command','console_command',40),('response','console_response',4112),('facts','console_facts',128),('window','keyboard_window',288),('decoder','keyboard_decoder',216),('controller','keyboard_controller',56)]:
            assert console_addresses[name]==address(label,'.bss',size),'console address provenance '+name
        for name,label,section,size in [('boot','boot_info','.data',104),('allocator','frame_allocator','.data',56),('candidates','candidate_set','.data',24),('paging','owned_paging','.data',120),('acpi','acpi_context','.data',120),('apic','apic_context','.data',144),('processes','user_processes','.bss',288),('user','user_context','.bss',144),('anchor','console_anchor','.bss',48)]:
            self.addresses[name]=(address(label,section,size),size)
        self.ranges=address('candidate_ranges','.bss',8192*16)
        self.records=address('frame_records','.bss',4096*24)
        self.baseline=None
        self.expected_mode=2 if manifest['timerTest']=='X2' else 1

    def facts(self,v,next_enter=False):
        stable={name:v[name] for name in self.addresses}
        if self.baseline is None:self.baseline=stable
        else:assert stable==self.baseline,'read-only console preserves source records'
        boot=v['boot']
        assert len(boot)==104
        assert struct.unpack_from('<QII',boot)==(0x004f464e4948534a,1,104),'boot identity'
        assert struct.unpack_from('<2Q',boot,88)==(self.base,self.image_size),'owned image source'
        fb,size=struct.unpack_from('<2Q',boot,16)
        width,height,stride,fmt=struct.unpack_from('<4I',boot,32)
        anchor=struct.pack('<6Q',fb,size,width,height,stride,fmt)
        self.anchor=anchor
        assert v['anchor']==anchor,'anchor from owned boot source'
        assert fb and size>=stride*height*4 and stride>=width and fmt in (0,1)
        a=struct.unpack('<7Q',v['allocator'])
        assert a[0]==0x314d41524653484a and a[1]==self.addresses['candidates'][0] and a[2]==self.records and a[4]==4096
        assert a[3]<=a[4] and a[6]<=a[5],'allocator source bounds'
        c=struct.unpack('<3Q',v['candidates'])
        assert c[0]==self.ranges and c[1]<=c[2] and c[2]==8192,'candidate source bounds'
        pg=struct.unpack('<15Q',v['paging'])
        assert pg[0]==self.addresses['allocator'][0] and pg[1] and pg[1]%4096==0 and pg[3]==0 and pg[14]==1
        assert 1<=pg[2]<=4096*64 and pg[8:10]==(self.base,self.base+self.image_size),'paging source bounds'
        acpi=struct.unpack('<15Q',v['acpi'])
        assert acpi[0]==0x314950434153484a and 1<=acpi[7]<=acpi[6]<=256,'ACPI source readiness/count'
        apic=struct.unpack('<18Q',v['apic'])
        assert apic[0]==0x314349504153484a and apic[1]==self.expected_mode and apic[4]==16,'APIC source'
        user=struct.unpack('<18Q',v['user'])
        assert user[0]==0x315253554153484a and user[1:3]==(0,3) and user[3]==self.addresses['paging'][0] and user[4]==self.addresses['processes'][0] and user[7]==pg[1] and user[8:10]==(0,0),'terminal user source'
        processes=struct.unpack('<36Q',v['processes'])
        assert processes[6] in (3,4,5) and processes[24] in (3,4,5),'terminal process states'
        w=struct.unpack('<20Q64H',v['window'])
        d=struct.unpack('<11Q64H',v['decoder'])
        assert w[2]==pg[1] and w[14]==w[15] and d[9]==0,'keyboard source'
        irq=w[14]+int(next_enter)
        assert irq<1<<64
        return [self.image_size,a[6],a[3],c[1],pg[2],acpi[7],apic[1],apic[4],irq,irq,d[9],processes[6],processes[7],processes[24],processes[25]]
