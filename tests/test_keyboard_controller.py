"""Modeled host ports executing actual Assembly keyboard command policy."""
import ctypes as C
import collections
import sys
U64=C.c_uint64
AUTH=0x3141554b48534f4a
READY=0x3143424b48534f4a
lib=C.CDLL(sys.argv[1]);call=lib.host_keyboard_controller_abi
call.argtypes=[U64]*4;call.restype=C.c_int
Callback=C.CFUNCTYPE(C.c_uint32,C.c_uint32,C.c_uint32)
lib.host_keyboard_backend.argtypes=[Callback]
checks=0

def check(ok,why='fixture'):
    global checks
    assert ok,why
    checks+=1

class Model:
    def __init__(self,config=0x47):
        self.config=config;self.queue=collections.deque();self.trace=[];self.polls=0
        self.await_config=False;self.scanning=True;self.set_parameter=False;self.scan_set=None
        self.resends={};self.attempts=collections.Counter();self.bad_reply={};self.f5_input=[];self.f4_input=[]
        self.busy=False;self.silent=False;self.aux=False;self.error=0;self.readback_flip=0;self.interface=0
        self.errors=[];self.callback=Callback(self.safe_access);lib.host_keyboard_backend(self.callback)
    def safe_access(self,op,value):
        try:return self.access(op,value)
        except Exception as error:
            self.errors.append(str(error));return 0xef
    def access(self,op,value):
        if op==0:
            self.polls+=1
            return (2 if self.busy else 0)|(1 if self.queue else 0)|(0x20 if self.aux and self.queue else 0)|self.error
        if op==1:
            if not self.queue:raise AssertionError('data read without OBF')
            value=self.queue.popleft();self.trace.append(('R',value));return value
        self.trace.append(('C' if op==2 else 'D',value))
        if op==2:
            if value==0xad:self.config|=0x10
            elif value==0xa7:self.config|=0x20
            elif value==0x20:
                if not self.silent:self.queue.append(self.config^self.readback_flip)
            elif value==0x60:self.await_config=True
            elif value==0xab:
                if not self.silent:self.queue.append(self.interface)
            elif value==0xae:self.config&=~0x10
            else:raise AssertionError(('unauthorized controller command',value))
        elif self.await_config:
            self.config=value;self.await_config=False
        else:
            self.attempts[value]+=1
            if self.silent:return 0
            if self.attempts[value]<=self.resends.get(value,0):self.queue.append(0xfe);return 0
            if value in self.bad_reply:self.queue.append(self.bad_reply[value]);return 0
            if value==0xf5:
                self.queue.extend(self.f5_input);self.queue.append(0xfa);self.scanning=False
            elif value==0xf0:self.queue.append(0xfa);self.set_parameter=True
            elif value==2 and self.set_parameter:self.queue.append(0xfa);self.set_parameter=False;self.scan_set=2
            elif value==0xf4:self.queue.append(0xfa);self.scanning=True;self.queue.extend(self.f4_input)
            else:raise AssertionError(('unauthorized keyboard command',value))
        return 0

class Fixture:
    def __init__(self,config=0x47):
        self.model=Model(config);self.authority=(U64*6)(AUTH,1,1,0,1,0)
        self.ctx=(U64*7)(0,0,0,C.addressof(self.authority),0,0,0)
    def invoke(self,op,expected=0):
        result=call(C.addressof(self.ctx),0,0,op)
        check(not self.model.errors,('backend contract',self.model.errors))
        check(result==expected,('status/ABI',op,expected,result))
        return result
    def prepare(self):
        self.invoke(0)
        check(list(self.ctx)==[READY,0,8,C.addressof(self.authority),(self.model.config&12)|0x61,0,len(self.model.f5_input)])
        check(not self.model.scanning and self.model.scan_set==2)
        check(self.model.trace.index(('C',0xab))<self.model.trace.index(('C',0xae)))
        check(('D',0xf4) not in self.model.trace)
    def enable(self):
        trace=len(self.model.trace);self.invoke(1)
        check(self.ctx[5]==1 and self.ctx[2]==10 and self.model.scanning)
        check(self.model.trace[trace:]==[('D',0xf4),('R',0xfa)],'F4 ACK->input transition')
    def disable(self):
        trace=len(self.model.trace);pending=list(self.model.queue);self.invoke(2)
        check(self.ctx[0]==0 and self.ctx[1]==0 and self.ctx[2]==11 and self.ctx[5]==0)
        check(self.model.config==(self.ctx[4]) and self.model.config&0x13==0x10)
        check(list(self.model.queue)==pending,'quiesce read post-F4 input as a reply')
        check(all(op!='R' for op,_ in self.model.trace[trace:]),'post-F4 configuration read')

for sysflags in [0,4,8,12]:
    f=Fixture(sysflags|3|0x40);f.prepare();f.enable();f.disable()
f=Fixture();f.model.queue.extend([0x1e,0x9e,0xfa]);f.prepare();f.enable();f.disable()
f=Fixture();f.model.f5_input=[0x1e,0x9e]*16;f.prepare();f.enable();f.disable()
f=Fixture();f.model.f5_input=[0x1e]*33;f.invoke(0,1);check(f.ctx[1]==1 and f.ctx[0]==0 and f.ctx[6]==32)
# Actual set2->translated set1 reply transition: raw input directly after ACK is left.
f=Fixture();f.model.f4_input=[0x1e,0x9e,0xe0,0x4b];f.prepare();f.enable();check(list(f.model.queue)==f.model.f4_input);f.disable()
for byte in [0xf5,0xf0,2,0xf4]:
    for retries in range(4):
        f=Fixture();f.model.resends[byte]=retries;f.prepare()
        f.invoke(1)
        check(f.model.attempts[byte]==retries+1,'bounded resend count')
        check(f.ctx[1]==0 and f.ctx[0]==READY)
    f=Fixture();f.model.resends[byte]=4
    if byte==0xf4:f.prepare();f.invoke(1,1)
    else:f.invoke(0,1)
    check(f.model.attempts[byte]==4 and f.ctx[1]==1 and f.ctx[0]==0,'resend exhaustion')
for byte in [0xf5,0xf0,2,0xf4]:
    for reply in [0xfc,0xfd]:
        f=Fixture();f.model.bad_reply[byte]=reply
        if byte==0xf4:f.prepare();f.invoke(1,1)
        else:f.invoke(0,1)
        check(f.ctx[0]==0 and f.ctx[1]==1)
for reply in [0,0x1e,0xaa]:
    f=Fixture();f.model.bad_reply[0xf0]=reply;f.invoke(0,1);check(f.ctx[0]==0)
for bad in [0,0x80,0xffffffffffffffff]:
    f=Fixture();f.authority[0]=bad;f.invoke(0,1);check(f.model.polls==0 and not f.model.trace)
f=Fixture();f.ctx[3]=0;f.invoke(0,1);check(not f.model.trace)
for index,value in [(0,1),(1,1),(2,1),(4,1),(5,1),(6,1)]:
    f=Fixture();f.ctx[index]=value;f.invoke(0,1);check(f.model.polls==0 and not f.model.trace)
for status in [0x20,0x40,0x80]:
    f=Fixture();f.model.aux=status==0x20;f.model.error=status if status!=0x20 else 0
    f.invoke(0,1);check(f.ctx[0]==0 and f.ctx[1]==1)
f=Fixture();f.model.busy=True;f.invoke(0,1);check(f.model.polls==100000 and not f.model.trace,'IBF timeout bound')
f=Fixture();f.model.silent=True;f.invoke(0,1);check(f.model.polls<=100010 and f.model.polls>=100000,'OBF timeout bound')
f=Fixture();f.model.queue.extend([0x1e]*33);f.invoke(0,1);check(len(f.model.queue)==1 and f.ctx[2]==2,'flush capacity')
f=Fixture();f.model.readback_flip=0x80;f.invoke(0,1);check(f.ctx[2]==3)
f=Fixture();f.model.interface=1;f.invoke(0,1);check(f.ctx[2]==4)
# Corrupt ready configuration is rejected before touching hardware.
for op in [1,2]:
    for bad in [0,0x63,0xe1,0x161,0xffffffffffffffff]:
        f=Fixture();f.prepare()
        if op==2:f.enable()
        f.ctx[4]=bad;before=len(f.model.trace);f.invoke(op,1);check(len(f.model.trace)==before)
print(f'PASS: {checks} modeled-port keyboard command/ACK/retry/ordering/timeout/ABI checks.')
