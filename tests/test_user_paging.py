"""Independent QMP snapshot oracle for private tables, backing and exact ownership."""
import argparse
import json
from pathlib import Path
import re
import struct

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output',type=Path)
parser.add_argument('--boot-media',choices=['iso','fat'],default='iso')
parser.add_argument('--seed',action='store_true')
args=parser.parse_args()
log=(args.output/f'qemu-{args.boot_media}-serial.log').read_text()
listing=(args.output/'boot.lst').read_text().splitlines()
image=(args.output/'BOOTX64.EFI').read_bytes()
def symbol(name):
    for i,line in enumerate(listing):
        if re.search(r'(?<![\w.])'+re.escape(name)+r':',line):
            for candidate in listing[i:]:
                found=re.match(r'\s*\d+\s+([0-9A-F]{8})\s',candidate)
                if found:return int(found[1],16)
    raise AssertionError(name)
pe=struct.unpack_from('<I',image,0x3c)[0]
sections={}
table=pe+24+struct.unpack_from('<H',image,pe+20)[0]
for i in range(struct.unpack_from('<H',image,pe+6)[0]):
    offset=table+i*40
    sections[image[offset:offset+8].rstrip(b'\0').decode()]=struct.unpack_from('<IIII',image,offset+8)
tss=int(re.search(r'TSS_READY SELECTOR=[0-9A-F]+ BASE=([0-9A-F]+)',log)[1],16)
base=tss-sections['.data'][1]-symbol('kernel_tss')
seed={'elfPlan':base+sections['.bss'][1]+symbol('user_elf_plan'),
      'processes':base+sections['.bss'][1]+symbol('user_processes'),
      'context':base+sections['.bss'][1]+symbol('user_context'),
      'allocator':base+sections['.data'][1]+symbol('frame_allocator'),
      'records':base+sections['.bss'][1]+symbol('frame_records'),
      'builder':base+sections['.data'][1]+symbol('owned_paging'),
      'roots':[int(x,16) for x in re.search(r'USER_ROOTS_PROVED ROOT_A=([0-9A-F]+) ROOT_B=([0-9A-F]+)',log).groups()],
      'kernelRoot':int(re.search(r'JOSHUA:OWNED_PAGING CR3=([0-9A-F]+)',log)[1],16)}
if args.seed:
    print(json.dumps(seed));raise SystemExit(0)
snapshot=json.loads((args.output/f'qemu-{args.boot_media}-user-paging.json').read_text())
assert snapshot['seed']==seed
pages={int(key,16):value for key,value in snapshot['pages'].items()}
MASK=0x000ffffffffff000
NX=1<<63
BASE=0x400000000000

def page(address):
    assert address and address%4096==0 and address<=MASK
    result=pages[address]
    assert len(result)==512 and all(0<=x<1<<64 for x in result)
    return result

def qword(address):
    assert address%8==0
    return page(address&~4095)[(address&4095)//8]

def words(address,count):return [qword(address+8*i) for i in range(count)]

elf_file=sections['.rodata'][3]+symbol('user_elf_start')
elf_source=base+sections['.rodata'][1]+symbol('user_elf_start')
co,_,_,cz,_,_=struct.unpack_from('<QQQQQQ',image,elf_file+72)
do,_,_,dz,_,_=struct.unpack_from('<QQQQQQ',image,elf_file+128)
entry_offset=struct.unpack_from('<Q',image,elf_file+24)[0]-BASE
assert words(seed['elfPlan'],8)==[elf_source,symbol('user_elf_end')-symbol('user_elf_start'),co,cz,do,dz,entry_offset,0x31464c4548534f4a]
allocator=words(seed['allocator'],7)
assert allocator[0]==0x314d41524653484a
assert allocator[2]==seed['records'] and allocator[4]==4096
assert 0<allocator[3]<=allocator[4]
records=[words(seed['records']+24*i,3) for i in range(allocator[3])]
for i,(physical,count,owner) in enumerate(records):
    assert physical and physical%4096==0 and count and owner and physical+count*4096<=1<<52
    for other,length,_ in records[:i]:assert physical>=other+length*4096 or physical+count*4096<=other

def owned(address,owner,pages_count=None):
    matches=[r for r in records if r[2]==owner and r[0]<=address<r[0]+r[1]*4096]
    assert len(matches)==1,(hex(address),hex(owner),matches)
    if pages_count is not None:assert matches[0]==[address,pages_count,owner]

context=words(seed['context'],18)
assert context[0]==0x315253554153484a and context[1]==0 and context[2]==3
assert context[3]==seed['builder'] and context[4]==seed['processes']
assert context[7]==seed['kernelRoot'] and context[8:10]==[0,0]
builder=words(seed['builder'],15)
assert builder[1]==seed['kernelRoot'] and builder[3]==0 and builder[14]==1
kernel=page(seed['kernelRoot'])
assert kernel[128]==0
for entry in kernel:assert not entry&1 or not entry&4
private_tables=[]
backing_frames=[]
processes=[]

# Validate the owned kernel format, including large-leaf reserved address bits.
def validate_kernel_entry(entry,shift,limit):
    assert entry&1
    leaf=shift==12 or shift in [30,21] and bool(entry&128)
    if leaf:
        low=(1<<shift)-1
        address_mask=MASK&~low
        allowed=address_mask|NX|0xfb
        if shift!=12:allowed|=1<<12 # large PAT, not a physical address bit
        assert entry&~allowed==0
        physical=entry&address_mask
        assert physical+(1<<shift)<=limit
        selector=((entry>>3)&3)|(((entry>>(7 if shift==12 else 12))&1)<<2)
    else:
        assert shift in [39,30,21] and entry&~(MASK|NX|0x23)==0
        physical=entry&MASK
        assert physical and physical+4096<=limit
        selector=0
    return leaf,physical,selector

# Return effective supervisor alias attributes and normalized inherited PAT selector.
def kernel_alias(address):
    table=seed['kernelRoot'];rw=True;user=True;nx=False
    for shift in [39,30,21,12]:
        owned(table,0x50414745)
        entry=page(table)[(address>>shift)&511]
        leaf,physical,selector=validate_kernel_entry(entry,shift,builder[4])
        rw=rw and bool(entry&2);user=user and bool(entry&4);nx=nx or bool(entry&NX)
        if leaf:
            physical|=address&((1<<shift)-1)
            assert physical==address
            return rw,user,nx,selector
        table=physical
    raise AssertionError('walk')

rows=re.findall(r'JOSHUA:USER_RESULT [^\r\n]+',log)
assert len(rows)==2
logged=[{k:int(v,16) for k,v in re.findall(r'([A-Z0-9_]+)=([0-9A-F]{16})',row)} for row in rows]
# Check every whole paging allocation, including pages not yet used as tables.
for physical,count,owner in records:
    if owner==0x50414745:
        assert count in [1,64]
        for address in range(physical,physical+count*4096,4096):
            assert kernel_alias(address)==(True,False,True,0)

for index,root in enumerate(seed['roots']):
    process=words(seed['processes']+index*144,18);processes.append(process)
    for offset,key in [(0,'ROOT'),(6,'STATE'),(8,'TICKS'),(9,'EOIS'),(10,'VECTOR'),(11,'ERROR'),(12,'RIP'),(13,'CR2'),(16,'UNKNOWN'),(17,'CS')]:
        assert process[offset]==logged[index][key]
    assert process[0]==root and process[5]==0x55530001+index
    assert process[6] in [3,4,5] and process[16]==1 and process[17]&7==3
    backing=process[4]
    owned(backing,process[5],6)
    backing_frames.extend(range(backing,backing+6*4096,4096))
    pml4=page(root)
    for slot in range(512):
        if slot!=128:assert pml4[slot]&~32==kernel[slot]&~32 # per-root hardware accessed bit
    path=[root];entry=pml4[128]
    for level in [1,2,3]:
        assert entry&7==7 and entry&~(MASK|0x27)==0
        table=entry&MASK;path.append(table)
        values=page(table)
        if level<3:
            assert all(v==0 for v in values[1:])
            entry=values[0]
    assert path==process[:4]
    for table in path:owned(table,0x50414745)
    private_tables.extend(path)
    pt=page(path[-1])
    for slot,entry in enumerate(pt):
        if slot not in [0,1,3,4,5,6]:assert entry==0;continue
        ordinal=slot if slot<2 else slot-1
        expected=backing+ordinal*4096
        assert entry&MASK==expected
        assert entry&~(MASK|NX|0xff)==0
        assert entry&7==(5 if slot==0 else 7)
        assert bool(entry&NX)==(slot!=0)
        rw,user,nx,selector=kernel_alias(expected)
        assert not user and nx and rw==(slot!=0)
        assert selector==((entry>>3)&3)|(((entry>>7)&1)<<2)
    code=b''.join(struct.pack('<Q',x) for x in page(backing))
    elf=sections['.rodata'][3]+symbol('user_elf_start')
    source_offset,_,_,size,_,_=struct.unpack_from('<QQQQQQ',image,elf+72)
    source=elf+source_offset
    assert code[:size]==image[source:source+size] and code[size:]==bytes(4096-size)
    data=page(backing+4096)
    assert data[1]==0x4a53485500000000|index and data[2]==1
    data_offset,_,_,data_size,_,_=struct.unpack_from('<QQQQQQ',image,elf+128)
    expected_data=image[elf+data_offset+128:elf+data_offset+data_size]
    raw_data=struct.pack('<512Q',*page(backing+4096))
    assert raw_data[128:data_size]==expected_data and raw_data[data_size:]==bytes(4096-data_size)
assert len(set(private_tables))==8 and seed['kernelRoot'] not in private_tables
assert len(set(backing_frames))==12 and not set(backing_frames)&set(private_tables)
print('PASS: QMP private roots/parents/all leaves, effective permissions/PAT/aliases, exact ledger ownership and private code/data.')
