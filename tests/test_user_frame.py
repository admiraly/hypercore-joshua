"""Exact Assembly CPL3 returning/discarded-frame policy; host-only owned storage."""
import ctypes as C
import mmap
import random
import sys
U64=C.c_uint64
lib=C.CDLL(sys.argv[1])
checks=0
BASE=0x400000000000

class Builder(C.Structure):
    _fields_=[(n,U64) for n in ['allocator','root','table_count','failed','limit',
        'pool_next','pool_end','old_root','image_base','image_end','guard','fb_start','fb_end','pat','ready']]
class Process(C.Structure):
    _fields_=[(n,U64) for n in ['root','pdpt','pd','pt','backing','owner','state','status',
        'ticks','eois','fault_vector','fault_error','fault_rip','fault_cr2','kernel_low','kernel_high','unknown_calls','last_cs']]
class Context(C.Structure):
    _fields_=[(n,U64) for n in ['ready','failed','stage','builder','processes','payload','payload_size',
        'kernel_root','active','phase','bootstrap_frame','bootstrap_low','bootstrap_high','saved_cr0','code','data','np','fault_top']]
for name in ['host_user_frame','host_user_fault']:
    getattr(lib,name).argtypes=[C.POINTER(Context),U64,U64]
    getattr(lib,name).restype=C.c_int

lib.host_user_frame_abi.argtypes=[C.POINTER(Context),U64,U64,U64]
lib.host_user_frame_abi.restype=C.c_int

class Fixture:
    def __init__(self):
        self.memory=mmap.mmap(-1,3*16384)
        self.base=C.addressof(C.c_char.from_buffer(self.memory))
        self.builder=Builder();self.builder.root=0x1000;self.builder.ready=1
        self.records=(Process*2)()
        for i,p in enumerate(self.records):
            p.root=0x5000+i*4096;p.owner=0x55530001+i;p.state=1
            p.kernel_low=self.base+i*16384;p.kernel_high=p.kernel_low+16384
        self.ctx=Context()
        self.ctx.ready=0x315253554153484a;self.ctx.stage=3
        self.ctx.builder=C.addressof(self.builder);self.ctx.processes=C.addressof(self.records)
        self.ctx.kernel_root=0x1000;self.ctx.active=1;self.ctx.phase=1
        self.ctx.code=0x73;self.ctx.data=0x7b;self.ctx.payload_size=512
        self.ctx.fault_top=self.base+3*16384
        self.frame=(U64*24).from_address(self.records[0].kernel_high-192)
        self.frame[19:]=[BASE+2,0x73,0x202,BASE+0x7000,0x7b]
        self.fault=(U64*7).from_address(self.ctx.fault_top-56)
        self.fault[:]=[14,4,BASE,0x73,0x202,BASE+0x6000,0x7b]
    def check(self,expected=0,fault=False,ptr=None,root=None):
        global checks
        words=self.fault if fault else self.frame
        ptr=C.addressof(words) if ptr is None else ptr
        root=self.records[0].root if root is None else root
        before=bytes(self.ctx),bytes(self.records),bytes(self.memory)
        fn=lib.host_user_fault if fault else lib.host_user_frame
        assert fn(C.byref(self.ctx),ptr,root)==expected
        assert lib.host_user_frame_abi(C.byref(self.ctx),ptr,root,int(fault))==expected
        assert before==(bytes(self.ctx),bytes(self.records),bytes(self.memory))
        checks+=1

f=Fixture();f.check();f.check(fault=True)
for ptr in [0,1,4096,(1<<64)-1,0xffff800000000000]:
    f.check(2,ptr=ptr);f.check(2,fault=True,ptr=ptr)
for field,value in [('ready',0),('failed',1),('stage',2),('active',0),('active',3),('phase',0),('phase',2),('kernel_root',0x9000)]:
    f=Fixture();setattr(f.ctx,field,value);f.check(2);f.check(2,fault=True)
for mutation in ['builder_ready','builder_failed','state','owner','root','kernel_high','kernel_low']:
    f=Fixture()
    if mutation=='builder_ready':f.builder.ready=0
    elif mutation=='builder_failed':f.builder.failed=1
    elif mutation=='state':f.records[0].state=2
    elif mutation=='owner':f.records[0].owner+=1
    elif mutation=='root':f.records[0].root+=1
    elif mutation=='kernel_high':f.records[0].kernel_high+=1
    elif mutation=='kernel_low':f.records[0].kernel_low+=4096
    f.check(2)
    if mutation not in ['kernel_high','kernel_low']:f.check(2,fault=True)
for index,value in [(19,0),(19,BASE+512),(20,0x7b),(23,0),(21,2),(21,0),
                    (22,BASE+0x2fff),(22,BASE+0x7001),(22,0xffffffffffffffff)]:
    f=Fixture();f.frame[index]=value;f.check(1)
for index,value in [(20,0x70),(20,0x10073)]:
    f=Fixture();f.frame[index]=value;f.check(2)
for bit in range(64):
    if (1<<bit)&0x10ed7:continue
    f=Fixture();f.frame[21]|=1<<bit;f.check(1)
rng=random.Random(3202)
for _ in range(2000):
    f=Fixture();f.frame[19]=BASE+rng.randrange(512)
    f.frame[21]=0x202|(rng.getrandbits(17)&0x10cd5)
    f.frame[22]=BASE+0x3000+rng.randrange(0x4001)
    f.check()
# Discarded faults must not demand a usable RIP/RSP/SS/flags or exact user CS.
allowed={0,1,3,4,5,6,7,10,11,12,13,14,16,17,19}
for vector in range(32):
    f=Fixture();f.fault[0]=vector
    f.check(0 if vector in allowed else 2,fault=True)
for index,value in [(2,(1<<64)-1),(3,0x83),(4,(1<<64)-1),(5,0x800000000000),(6,0)]:
    f=Fixture();f.fault[index]=value;f.check(fault=True)
for index,value in [(3,0x70),(3,0x10073),(0,256)]:
    f=Fixture();f.fault[index]=value;f.check(2,fault=True)
for ptr in [0,4096,0x800000001000]:
    f=Fixture();f.ctx.fault_top=ptr;f.check(2,fault=True)
# Both active identities must select only their own fixed entry-stack frame/root.
f=Fixture();f.ctx.active=2
f.frame=(U64*24).from_address(f.records[1].kernel_high-192)
f.frame[19:]=[BASE+2,0x73,0x202,BASE+0x7000,0x7b]
f.check(root=f.records[1].root);f.check(fault=True,root=f.records[1].root)
f.check(2,root=f.records[0].root);f.check(2,fault=True,root=f.records[0].root)
f.check(2,ptr=f.records[0].kernel_high-192,root=f.records[1].root)
# Exclude every origin on the wrong ordinary IST location, before frame reads.
for delta in [-4096,-64,-8,8,64,4096]:
    f=Fixture();f.check(2,fault=True,ptr=C.addressof(f.fault)+delta)
print(f'PASS: {checks} user IRQ/discarded-fault cases, each with eight-register ABI sentinels.')
