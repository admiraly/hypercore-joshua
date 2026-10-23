"""Exact Assembly ACPI keyboard-authority checks; modeled paging lookup only."""
import ctypes as C
import mmap
import random
import struct
import sys
U64=C.c_uint64
lib=C.CDLL(sys.argv[1]);call=lib.host_keyboard_authority_abi
call.argtypes=[U64]*4;call.restype=C.c_int
lib.host_keyboard_alias.argtypes=[U64]*3
ACPI_MAGIC=0x314950434153484a
AUTH_MAGIC=0x3141554b48534f4a
checks=0

def check(ok,why='fixture'):
    global checks
    assert ok,why
    checks+=1

def table(sig,payload,rev=1):
    b=bytearray(36)+bytearray(payload);b[:4]=sig;b[8]=rev
    struct.pack_into('<I',b,4,len(b));b[9]=-sum(b)&255
    return b

def iso(source=1,gsi=1,flags=0,bus=0,tail=b''):
    return struct.pack('<BBBBIH',2,10+len(tail),bus,source,gsi,flags)+tail

class Fixture:
    def __init__(self):
        self.mem=mmap.mmap(-1,0x20000);self.base=C.addressof(C.c_char.from_buffer(self.mem))
        self.map=(U64*5)(9,self.base,0,32,0)
        self.boot=(U64*13)();self.boot[6]=C.addressof(self.map);self.boot[7]=40;self.boot[8]=40
        self.builder=(U64*15)();self.builder[1]=0x1000;self.builder[4]=1<<47
        self.ios=(C.c_uint32*32)();self.ios[0:4]=[2,0xfec00000,0,24]
        self.ctx=(U64*15)();self.ctx[0]=ACPI_MAGIC;self.ctx[1]=C.addressof(self.boot);self.ctx[2]=C.addressof(self.builder)
        self.ctx[10]=self.base+0x100;self.ctx[11]=self.base+0x1000
        self.ctx[12]=C.addressof(self.ios);self.ctx[13]=8;self.ctx[14]=1
        self.output=(U64*6)(*([0xa5a5]*6))
        self.fadt=self.base+0x18000
        self.tables()
    def put(self,address,data):C.memmove(address,bytes(data),len(data))
    def tables(self,records=b'',flags=2,rev=3,length=244,children=None):
        if children is None:children=[self.fadt,self.ctx[11]]
        self.put(self.ctx[10],table(b'XSDT',b''.join(struct.pack('<Q',p) for p in children)))
        self.put(self.ctx[11],table(b'APIC',struct.pack('<II',0xfee00000,1)+records))
        f=table(b'FACP',bytes(max(0,length-36)),rev)
        if len(f)>=111:struct.pack_into('<H',f,109,flags)
        f[9]=0;f[9]=-sum(f)&255;self.put(self.fadt,f)
    def mutate(self,address,offset,value,width=1,checksum=True):
        C.memmove(address+offset,value.to_bytes(width,'little'),width)
        if checksum:
            length=C.c_uint32.from_address(address+4).value
            C.c_ubyte.from_address(address+9).value=0
            C.c_ubyte.from_address(address+9).value=-sum(C.string_at(address,length))&255
    def run(self,expected=True,gsi=1,index=0,pin=1):
        lib.host_keyboard_alias(self.base,self.base+0x20000,(1<<63)|1)
        before=bytes(self.mem)
        result=call(C.addressof(self.ctx),C.addressof(self.output),0,0)
        check(result==(0 if expected else 1),('status',expected,result))
        if expected:check(list(self.output)==[AUTH_MAGIC,self.fadt,gsi,index,pin,self.ctx[5]],'published authority')
        else:check(list(self.output)==[0]*6,'failure publication')
        check(bytes(self.mem)==before,'firmware mutation')

Fixture().run()
for rev,length in [(3,244),(4,244),(5,268),(6,276),(7,276)]:
    f=Fixture();f.tables(rev=rev,length=length);f.run()
for rev,length in [(0,244),(1,244),(2,244),(3,116),(3,243),(5,244),(5,267),(6,268),(6,275)]:
    f=Fixture();f.tables(rev=rev,length=length);f.run(False)
for bit in range(16):
    f=Fixture();f.tables(flags=2|(1<<bit));f.run(bit<6)
f=Fixture();f.tables(flags=0);f.run(False)
for flags in range(16):
    f=Fixture();f.tables(iso(flags=flags));f.run(flags in [0,1,4,5])
    f=Fixture();f.tables(iso(source=3,gsi=3,flags=flags));f.run((flags&3)!=2 and (flags>>2)!=2)
for bit in range(4,16):
    f=Fixture();f.tables(iso(flags=1<<bit));f.run(False)
for bus in [1,255]:f=Fixture();f.tables(iso(bus=bus));f.run(False)
for source in [16,255]:f=Fixture();f.tables(iso(source=source));f.run(False)
f=Fixture();f.tables(iso()+iso());f.run(False)
f=Fixture();f.tables(iso(source=3,gsi=3)+iso(source=3,gsi=3));f.run(False)
f=Fixture();f.tables(iso(source=3,gsi=1));f.run(False)
f=Fixture();f.tables(iso(gsi=2));f.run(False) # default IRQ2 still owns GSI2
f=Fixture();f.tables(iso(gsi=24));f.ios[4:8]=[3,0xfec01000,24,24];f.ctx[14]=2;f.run(True,24,1,0)
f=Fixture();f.tables(iso(gsi=0xffffffff));f.ios[0:4]=[2,0xfec00000,0xffffffff,1];f.run(True,0xffffffff,0,0)
f=Fixture();f.tables(iso(gsi=24)+iso(source=2,gsi=24));f.ios[0:4]=[2,0xfec00000,24,24];f.run(False)
# XSDT/FADT/MADT identity, checksum, child bounds and record truncation.
f=Fixture();f.tables(children=[f.ctx[11]]);f.run(False)
f=Fixture();f.tables(children=[f.fadt,f.fadt]);f.run(False)
f=Fixture();f.tables(children=[0]);f.run(False)
f=Fixture();f.tables(children=[f.base+0x20000]);f.run(False)
for which in ['xsdt','madt','fadt']:
    for checksum in [False,True]:
        f=Fixture();address={'xsdt':f.ctx[10],'madt':f.ctx[11],'fadt':f.fadt}[which]
        f.mutate(address,0,0,checksum=checksum);f.run(False)
    f=Fixture();address={'xsdt':f.ctx[10],'madt':f.ctx[11],'fadt':f.fadt}[which]
    f.mutate(address,9,C.c_ubyte.from_address(address+9).value^1,checksum=False);f.run(False)
for records in [b'\x02',b'\x02\x00',b'\x02\x01',b'\x02\x09'+bytes(7),b'\x02\x20'+bytes(8)]:
    f=Fixture();f.tables(records);f.run(False)
f=Fixture();f.tables(bytes([200,2])*32746);f.run() # bounded maximum-size MADT
for length in [0,35,36,65537,0xffffffff]:
    f=Fixture();f.mutate(f.fadt,4,length,4,checksum=False);f.run(False)
f=Fixture();f.mutate(f.ctx[10],4,37,4);f.run(False)
f=Fixture();f.tables(children=[f.ctx[11]]*257);f.run(False)
# Trust-boundary and IOAPIC-range metadata: no wrapping/ambiguous/unsupported owner.
for idx,value in [(0,0),(5,256),(13,9),(14,0),(14,9),(13,0),(12,0)]:
    f=Fixture();f.ctx[idx]=value;f.run(False)
for first,count in [(0,0),(0,121),(0xffffffff,2),(2,1)]:
    f=Fixture();f.ios[2]=first;f.ios[3]=count;f.run(False)
f=Fixture();f.ios[4:8]=[3,0xfec01000,1,1];f.ctx[14]=2;f.run(False)
f=Fixture();f.ctx[5]=255;f.run()
for kind,attr in [(7,0),(9,0x2000)]:
    f=Fixture();f.map[0]=kind;f.map[4]=attr;f.run(False)
# Isolate full-table coverage: the complete FADT header fits the final proved
# page, but its payload crosses a guard page. Premature payload reads would fault.
f=Fixture();f.fadt=f.base+0x17fd0;f.tables();f.map[3]=24
proved_end=f.base+24*4096
check(f.fadt+36<=proved_end<f.fadt+244)
lib.host_keyboard_alias(f.base,f.base+0x20000,(1<<63)|1)
libc=C.CDLL(None);libc.mprotect.argtypes=[C.c_void_p,C.c_size_t,C.c_int]
check(libc.mprotect(proved_end,4096,0)==0)
try:result=call(C.addressof(f.ctx),C.addressof(f.output),0,0)
finally:check(libc.mprotect(proved_end,4096,3)==0)
check(result==1 and list(f.output)==[0]*6,'header readable, guarded payload denied')
# Deterministic override campaigns use an independent ISA source map oracle.
rng=random.Random(38)
for _ in range(500):
    f=Fixture();mapping=list(range(16));seen=set();records=b'';valid=True
    for _ in range(rng.randrange(0,18)):
        source=rng.randrange(16);gsi=rng.randrange(40);flags=rng.choice([0,1,4,5,3,12,15])
        if source in seen:valid=False
        seen.add(source);mapping[source]=gsi;records+=iso(source,gsi,flags)
        if source==1 and flags not in [0,1,4,5]:valid=False
    chosen=mapping[1]
    if mapping.count(chosen)!=1 or chosen>=24:valid=False
    f.tables(records);f.run(valid,chosen,0,chosen)
# Failed metadata rejects before any output write only for an unsafe output pointer.
f=Fixture();lib.host_keyboard_alias(f.base,f.base+0x20000,(1<<63)|1)
check(call(C.addressof(f.ctx),0,0,0)==1)
check(call(C.addressof(f.ctx),(1<<64)-16,0,0)==1)
print(f'PASS: {checks} keyboard FADT/ISA/GSI authority and ABI checks including 500 seeded routing oracles.')
