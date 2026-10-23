"""Host-only exact Assembly ELF parser/copy with an independent profile oracle."""
import ctypes as C
import mmap
import random
import struct
import sys
BASE=0x400000000000
MAGIC=0x31464c4548534f4a
Plan=C.c_uint64*8
lib=C.CDLL(sys.argv[1]);call=lib.host_elf_abi
call.argtypes=[C.c_uint64]*4;call.restype=C.c_int
checks=0

def check(condition,message='fixture'):
    global checks
    assert condition,message
    checks+=1

def image(code=33,data=144,entry=7,code_off=4096,data_off=8192):
    b=bytearray(max(176,code_off+code,data_off+data))
    b[:16]=b'\x7fELF\x02\x01\x01'+bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH',b,16,2,62,1,BASE+entry,64,0,0,64,56,2,0,0,0)
    for pos,flags,offset,va,size in [(64,5,code_off,BASE,code),(120,6,data_off,BASE+4096,data)]:
        struct.pack_into('<IIQQQQQQ',b,pos,1,flags,offset,va,0,size,4096,4096)
    for i in range(code):b[code_off+i]=(i*17+1)&255
    for i in range(data):b[data_off+i]=(i*19+3)&255
    return b

def oracle(b):
    if not 176<=len(b)<=12288:return None
    if b[:16]!=b'\x7fELF\x02\x01\x01'+bytes(9):return None
    h=struct.unpack_from('<HHIQQQIHHHHHH',b,16)
    if h[:3]!=(2,62,1) or h[4:]!=(64,0,0,64,56,2,0,0,0):return None
    segments=[]
    for i in range(2):
        t,f,o,v,p,z,m,a=struct.unpack_from('<IIQQQQQQ',b,64+i*56)
        if (t,f,v,p,m,a)!=(1,5+i,BASE+i*4096,0,4096,4096):return None
        if o<4096 or o%4096 or o>len(b) or not 0<=z<=4096 or z>len(b)-o:return None
        if i==0 and z==0:return None
        segments.append((o,z))
    (co,cz),(do,dz)=segments
    if dz and co<do+dz and do<co+cz:return None
    entry=h[3]-BASE
    if not 0<=entry<cz:return None
    return co,cz,do,dz,entry

class Fixture:
    def __init__(self,b):
        self.b=bytes(b);self.src=C.create_string_buffer(self.b);self.plan=Plan(*([0xdeadbeef]*8))
        self.address=C.addressof(self.src)
    def validate(self,length=None):
        n=len(self.b) if length is None else length
        expected=oracle(self.b[:n]) if n<=len(self.b) else None
        result=call(self.address,n,C.addressof(self.plan),0)
        check(result==(0 if expected else 1),('parse',n,expected,result))
        if expected:check(list(self.plan)==[self.address,n,*expected,MAGIC],'exact plan')
        else:check(list(self.plan)==[0]*8,'invalidated plan')
        return result
    def copy(self):
        mem=mmap.mmap(-1,24576);ptr=C.addressof(C.c_char.from_buffer(mem))
        mem[:]=bytes([0xa5])*24576
        result=call(C.addressof(self.plan),ptr,24576,1)
        check(result==0,'copy')
        co,cz,do,dz,_=oracle(self.b)
        wanted=bytearray(24576);wanted[:cz]=self.b[co:co+cz];wanted[4096:4096+dz]=self.b[do:do+dz]
        check(mem[:]==wanted,'copy bytes/BSS/stack zeroing')
        return mem,ptr

for code in [1,7,4095,4096]:
    for data in [0,1,144,4095,4096]:
        f=Fixture(image(code,data,min(6,code-1)));f.validate();f.copy()
# File offsets can reverse while virtual addresses remain sorted.
f=Fixture(image(33,144,7,8192,4096));f.validate();f.copy()
# Zero-sized data may share an offset with code without a file-byte overlap.
f=Fixture(image(33,0,7,4096,4096));f.validate();f.copy()
base=image()
for n in range(176):Fixture(base).validate(n)
for n in [176,4095,4096,4128,8191,8192,len(base)-1]:Fixture(base).validate(n)
# Every header byte is mutated; the independent oracle decides profile validity.
for offset in range(176):
    for value in [0,1,0x80,0xff]:
        b=bytearray(base);b[offset]=value;Fixture(b).validate()
for off in [24,32,40,72,80,88,96,104,112,128,136,144,152,160,168]:
    for value in [0,1,4095,4096,8192,12288,(1<<64)-1,1<<63]:
        b=bytearray(base);struct.pack_into('<Q',b,off,value);Fixture(b).validate()
b=bytearray(base);b[64:120],b[120:176]=b[120:176],b[64:120];Fixture(b).validate()
rng=random.Random(36)
for _ in range(2000):
    b=bytearray(base)
    for _ in range(rng.randrange(1,5)):
        b[rng.randrange(176)]=rng.randrange(256)
    Fixture(b).validate()
# Bounds failure must not dereference bogus source or mutate unproved output.
f=Fixture(base);saved=bytes(f.plan)
for pointer,length,output in [(0,len(base),C.addressof(f.plan)),((1<<64)-8,176,C.addressof(f.plan)),(f.address,len(base),(1<<64)-32),(f.address,len(base),f.address)]:
    before=bytes(f.src);check(call(pointer,length,output,0)==1);check(bytes(f.plan)==saved and bytes(f.src)==before)
# Copy failures leave every destination byte unchanged, including stale plans.
f=Fixture(base);f.validate();mem,ptr=f.copy();saved=mem[:]
for field in range(8):
    old=f.plan[field];f.plan[field]=0 if field==1 else old^1
    check(call(C.addressof(f.plan),ptr,24576,1)==1);check(mem[:]==saved)
    f.plan[field]=old
for dest,size in [(ptr+1,24576),(ptr,24575),(ptr,24577),((1<<64)-4095,24576),(f.address,24576),(C.addressof(f.plan),24576)]:
    check(call(C.addressof(f.plan),dest,size,1)==1);check(mem[:]==saved)
# Source mutation after validation is revalidated; source/plan overlap also rejects.
f.src[0]=b'X';check(call(C.addressof(f.plan),ptr,24576,1)==1);check(mem[:]==saved)
f.src[0]=b'\x7f'
old=f.plan[0];f.plan[0]=C.addressof(f.plan);check(call(C.addressof(f.plan),ptr,24576,1)==1);check(mem[:]==saved);f.plan[0]=old
# Aligned exact/partial aliases must reach the overlap checks, not fail alignment.
for displacement in [-4096,0,4096]:
    arena=mmap.mmap(-1,49152);arena[:]=bytes([0x5a])*49152
    arena_base=C.addressof(C.c_char.from_buffer(arena));source=arena_base+8192
    C.memmove(source,bytes(base),len(base));p=Plan()
    check(call(source,len(base),C.addressof(p),0)==0)
    saved=arena[:]
    check(call(C.addressof(p),source+displacement,24576,1)==1)
    check(arena[:]==saved,'aligned destination/source overlap wrote bytes')
for relative in [-32,4096,24576-32]:
    arena=mmap.mmap(-1,40960);arena[:]=bytes([0x5a])*40960
    arena_base=C.addressof(C.c_char.from_buffer(arena));destination=arena_base+4096
    p=Plan.from_address(destination+relative)
    source=C.create_string_buffer(bytes(base))
    check(call(C.addressof(source),len(base),C.addressof(p),0)==0)
    saved=arena[:]
    check(call(C.addressof(p),destination,24576,1)==1)
    check(arena[:]==saved,'aligned destination/plan overlap wrote bytes')
# Guard-page source placement proves truncated reads never cross declared buffers.
libc=C.CDLL(None);libc.mprotect.argtypes=[C.c_void_p,C.c_size_t,C.c_int]
for size in [1,15,16,63,64,119,175,176]:
    guard=mmap.mmap(-1,8192);address=C.addressof(C.c_char.from_buffer(guard));start=address+4096-size
    C.memmove(start,bytes(base[:size]),size);check(libc.mprotect(address+4096,4096,0)==0)
    p=Plan();check(call(start,size,C.addressof(p),0)==1);check(list(p)==[0]*8)
    check(libc.mprotect(address+4096,4096,3)==0)
print(f'PASS: {checks} ELF profile/plan/copy/oracle/bounds checks, each helper call with eight integer ABI sentinels.')
