"""Exact Assembly keyboard IRQ return policy with owned host stacks and guards."""
import ctypes as C
import mmap
import random
import sys
U64=C.c_uint64
names=['ready','phase','root','wait_rsp','low','high','wait_rip','drain_rip','code','data','flags','text_start','text_end','ticks','irqs','keois','teois','failure','windows','event_count']
class Window(C.Structure):_fields_=[(n,U64) for n in names]+[('events',C.c_uint16*64)]
lib=C.CDLL(sys.argv[1]);call=lib.host_keyboard_frame_abi
call.argtypes=[U64]*4;call.restype=C.c_int
checks=0

def check(ok,why='fixture'):
    global checks
    assert ok,why
    checks+=1

class Fixture:
    def __init__(self):
        self.memory=mmap.mmap(-1,17*4096);self.base=C.addressof(C.c_char.from_buffer(self.memory))
        self.ctx=Window();self.ctx.ready=0x3157424b48534f4a;self.ctx.phase=1;self.ctx.root=0x2000
        self.ctx.low=self.base+4096;self.ctx.high=self.ctx.low+65536
        self.ctx.wait_rsp=self.ctx.high-512;self.ctx.wait_rip=0x101000;self.ctx.drain_rip=0x101010
        self.ctx.code=0x58;self.ctx.data=0x60;self.ctx.flags=0x202;self.ctx.text_start=0x100000;self.ctx.text_end=0x110000
        self.reset_frame()
    def reset_frame(self):
        self.address=(self.ctx.wait_rsp&~15)-192
        self.frame=(U64*24).from_address(self.address)
        for i in range(24):self.frame[i]=0
        self.frame[19]=self.ctx.wait_rip if self.ctx.phase==1 else self.ctx.drain_rip
        self.frame[20]=self.ctx.code;self.frame[21]=self.ctx.flags if self.ctx.phase==1 else 0x202
        self.frame[22]=self.ctx.wait_rsp;self.frame[23]=self.ctx.data
    def run(self,expected=0,address=None,root=None):
        address=self.address if address is None else address
        root=self.ctx.root if root is None else root
        ctx_before=bytes(self.ctx);frame_before=bytes(self.frame)
        check(call(C.addressof(self.ctx),address,root,0)==expected,'policy/ABI')
        check(bytes(self.ctx)==ctx_before and bytes(self.frame)==frame_before,'read-only policy')

for phase in [1,2]:
    for flags in [0x202,0x602]:
        f=Fixture();f.ctx.phase=phase;f.ctx.flags=flags;f.reset_frame();f.run()
        for offset in range(19,24):
            for bit in range(64):
                f=Fixture();f.ctx.phase=phase;f.ctx.flags=flags;f.reset_frame();f.frame[offset]^=1<<bit;f.run(1)
for name,values in {'ready':[0,1], 'phase':[0,3,4,(1<<64)-1], 'root':[0,1,0x2001],
    'low':[0,1], 'high':[0,1], 'wait_rsp':[0,1], 'code':[0,1,0x5b,0x5c,0x10000,0x60],
    'data':[0,1,0x63,0x64,0x10000,0x58], 'flags':[0,2,0x200,0x203,0x246,0x402,0x702,0x240202],
    'wait_rip':[0,0x100fff,0x110000], 'text_start':[0x101001,0x110000], 'text_end':[0,0x101000]}.items():
    for value in values:
        f=Fixture();setattr(f.ctx,name,value);f.run(1)
f=Fixture();f.ctx.low=f.ctx.high;f.run(1)
f=Fixture();f.ctx.low=f.ctx.high-4096;f.run(1)
f=Fixture();f.ctx.low=f.ctx.high-69632;f.run(1)
f=Fixture();f.run(1,root=0x3000)
for phase in [1,2]:
    f=Fixture();f.ctx.phase=phase;f.reset_frame()
    for address in [0,1,f.address-16,f.address+16,f.ctx.high,f.ctx.low,f.ctx.low+4096]:f.run(1,address=address)
# Random exact old-RSP/frame relationships, including old_RSP == high and 8-byte skew.
rng=random.Random(38)
for _ in range(500):
    f=Fixture();f.ctx.phase=rng.choice([1,2]);f.ctx.flags=rng.choice([0x202,0x602])
    f.ctx.wait_rsp=f.ctx.high-rng.randrange(0,1024)*8;f.reset_frame();f.run()
    f.frame[22]^=8;f.run(1)
# Insufficient helper reserve rejects before frame access, even with plausible RIP.
f=Fixture();f.ctx.wait_rsp=f.ctx.low+4096+176;f.reset_frame();f.run(1)
# Proposed frame points into a PROT_NONE guard; exact-address proof must reject it.
f=Fixture();libc=C.CDLL(None);libc.mprotect.argtypes=[C.c_void_p,C.c_size_t,C.c_int]
check(libc.mprotect(f.base,4096,0)==0)
try:f.run(1,address=f.base)
finally:check(libc.mprotect(f.base,4096,3)==0)
print(f'PASS: {checks} keyboard exact-frame/selector/flags/RSP/root/read-bound/ABI checks.')
