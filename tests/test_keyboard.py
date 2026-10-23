"""Independent state/event oracle over actual Assembly set1 decoder and queue."""
import ctypes as C
import random
import sys
MAX=(1<<64)-1
MAGIC=0x3144424b48534f4a
fields=['ready','modifiers','caps','caps_down','prefix','pause_index','head','tail','count','drops','protocol_errors']
class State(C.Structure):_fields_=[(n,C.c_uint64) for n in fields]+[('events',C.c_uint16*64)]
lib=C.CDLL(sys.argv[1]);call=lib.host_keyboard_abi
call.argtypes=[C.c_uint64]*4;call.restype=C.c_uint64
checks=0

def check(ok,why='fixture'):
    global checks
    assert ok,why
    checks+=1

# Physical US key rows, independent of the Assembly lookup representation.
rows=[(2,'1234567890-=','!@#$%^&*()_+'),(16,'qwertyuiop[]','QWERTYUIOP{}'),
      (30,"asdfghjkl;'`",'ASDFGHJKL:"~'),(43,'\\zxcvbnm,./','|ZXCVBNM<>?')]
chars={start+i:(a,b) for start,lo,hi in rows for i,(a,b) in enumerate(zip(lo,hi))}
chars.update({1:('\x1b','\x1b'),14:('\b','\b'),15:('\t','\t'),28:('\r','\r'),57:(' ',' ')})
nav={0x4b:0x100,0x4d:0x101,0x47:0x102,0x4f:0x103,0x53:0x104,0x48:0x105,0x50:0x106}
class Reference:
    def __init__(self):
        self.mods=set();self.caps=False;self.caps_down=False;self.prefix=False;self.pause=[]
        self.queue=[];self.drops=0;self.errors=0
    def resync(self):
        self.mods.clear();self.caps_down=False;self.prefix=False;self.pause=[];self.errors=min(MAX,self.errors+1)
    def push(self,event):
        if len(self.queue)==64:self.drops=min(MAX,self.drops+1)
        else:self.queue.append(event)
    def decode(self,raw):
        if self.pause:
            if raw!=self.pause.pop(0):self.resync()
            return
        if raw==0xe1:
            if self.prefix:self.resync()
            else:self.pause=[0x1d,0x45,0xe1,0x9d,0xc5]
            return
        if raw==0xe0:
            if self.prefix:self.resync()
            else:self.prefix=True
            return
        if raw in [0,0xfa,0xfc,0xfd,0xfe,0xff]:
            self.prefix=False;self.pause=[];self.errors=min(MAX,self.errors+1);return
        extended=self.prefix;self.prefix=False
        code=raw%128;released=raw>=128
        modifier={0x1d:'rc' if extended else 'lc',0x38:'ra' if extended else 'la'}
        if not extended:modifier.update({0x2a:'ls',0x36:'rs'})
        if code in modifier:
            if released:self.mods.discard(modifier[code])
            else:self.mods.add(modifier[code])
            return
        if code==0x3a and not extended:
            if released:self.caps_down=False
            elif not self.caps_down:self.caps=not self.caps;self.caps_down=True
            return
        if released:return
        alt=bool(self.mods&{'la','ra'});control=bool(self.mods&{'lc','rc'});shift=bool(self.mods&{'ls','rs'})
        if extended:
            if not alt and not control:
                if code in nav:self.push(nav[code])
                elif code==0x1c:self.push(13)
                elif code==0x35:self.push(ord('/'))
            return
        if alt or code not in chars:return
        low,high=chars[code]
        if 'a'<=low<='z':
            value=ord(low)&31 if control else ord(low.upper() if shift!=self.caps else low)
        else:
            if control:return
            value=ord(high if shift else low)
        self.push(value)

class Fixture:
    def __init__(self):
        self.state=State();C.memset(C.addressof(self.state),0xa5,C.sizeof(self.state));self.ref=Reference()
        check(call(C.addressof(self.state),0,0,0)==0)
        check(self.state.ready==MAGIC and bytes(self.state)[8:]==bytes(C.sizeof(State)-8),'init exact')
    def compare(self):
        s=self.state;r=self.ref
        mods=sum(1<<i for i,k in enumerate(['ls','rs','lc','rc','la','ra']) if k in r.mods)
        check((s.modifiers,s.caps,s.caps_down,s.prefix,s.pause_index,s.count,s.drops,s.protocol_errors)==
              (mods,int(r.caps),int(r.caps_down),int(r.prefix),6-len(r.pause) if r.pause else 0,len(r.queue),r.drops,r.errors),'state oracle')
        check([s.events[(s.head+i)%64] for i in range(s.count)]==r.queue,'queued events')
        check(0<=s.head<64 and s.tail==(s.head+s.count)%64,'ring indices')
    def feed(self,values):
        for raw in values:
            check(call(C.addressof(self.state),raw,0,1)==0,('decode/ABI',raw))
            self.ref.decode(raw);self.compare()
    def pop(self):
        expected=self.ref.queue.pop(0) if self.ref.queue else None
        actual=call(C.addressof(self.state),0,0,2)
        check(actual==(2 if expected is None else expected<<32),'pop/ABI')
        self.compare();return expected
    def drain(self):
        result=[]
        while self.ref.queue:result.append(self.pop())
        self.pop();return result

f=Fixture();f.feed([0x23,0xa3,0x12,0x92,0x26,0xa6,0x26,0xa6,0x18,0x98,0x1c,0x9c]);check(f.drain()==list(b'hello\r'))
# Each printable/control scan and its break across independent modifier modes.
for modifier in [[],[0x2a],[0x36],[0x3a,0xba],[0x3a,0xba,0x2a],[0x1d],[0x38],[0xe0,0x1d],[0xe0,0x38]]:
    for code in chars:
        f=Fixture();f.feed(modifier+[code,code|128]);f.drain()
# Shift overlap/release, caps repeat suppression, Pause and PrintScreen fake shifts.
f=Fixture();f.feed([0x2a,0x36,0xaa,0x1e,0x9e,0xb6,0x1e,0x9e]);check(f.drain()==list(b'Aa'))
f=Fixture();f.feed([0x3a,0x3a,0x3a,0x1e,0x9e,0xba,0x3a,0xba,0x1e]);check(f.drain()==list(b'Aa'))
f=Fixture();f.feed([0xe1,0x1d,0x45,0xe1,0x9d,0xc5,0xe0,0x2a,0xe0,0x37,0xe0,0xb7,0xe0,0xaa,0x1e]);check(f.drain()==[ord('a')])
for code in list(nav)+[0x1c,0x35]:
    f=Fixture();f.feed([0xe0,code,0xe0,code|128]);f.drain()
for bad in range(256):
    f=Fixture();f.feed([0x2a,0xe1,bad,0x1e,0x9e]);f.drain()
f=Fixture();f.feed([0x1e]*70);check(f.state.count==64 and f.state.drops==6)
f.drain();f.feed([0x30]*70);f.drain();check(f.state.drops==12)
f.state.drops=MAX;f.ref.drops=MAX;f.feed([0x1e]*65);check(f.state.drops==MAX)
f=Fixture();f.state.protocol_errors=MAX;f.ref.errors=MAX;f.feed([0xfa,0xe0,0xe0]);check(f.state.protocol_errors==MAX)
# Reset after transport error preserves queued text/caps, clears transient modifiers.
f=Fixture();f.feed([0x3a,0xba,0x1e,0x2a,0xe0]);check(call(C.addressof(f.state),0,0,3)==0);f.ref.resync();f.compare();f.drain()
# Invalid metadata and raw width reject before mutation or queue access.
for name,value in [('ready',0),('modifiers',64),('caps',2),('caps_down',2),('prefix',2),('pause_index',6),('head',64),('tail',64),('count',65),('tail',1)]:
    for op in [1,2,3]:
        f=Fixture();setattr(f.state,name,value);before=bytes(f.state)
        check(call(C.addressof(f.state),0x1e,0,op)==1);check(bytes(f.state)==before,('corruption write',name,op))
f=Fixture();f.state.prefix=1;f.state.pause_index=1;before=bytes(f.state);check(call(C.addressof(f.state),1,0,1)==1);check(bytes(f.state)==before)
f=Fixture();before=bytes(f.state);check(call(C.addressof(f.state),256,0,1)==1);check(bytes(f.state)==before)
# Deterministic mixed streams exercise ring wrap and every prefix/state interaction.
rng=random.Random(38)
for _ in range(40):
    f=Fixture()
    for _ in range(1000):
        if rng.randrange(5)==0:f.pop()
        else:f.feed([rng.randrange(256)])
    f.drain()
print(f'PASS: {checks} keyboard decode/queue/oracle/ABI checks including 40,000 mixed operations.')
