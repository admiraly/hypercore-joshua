"""Independent screen model, using actual Assembly parse/build/compose helpers."""
import ctypes as C
import json
import mmap
from pathlib import Path
import zlib
import random
import struct
import sys

MAX = (1 << 64)-1
GRID_MAGIC = 0x3144495247484f4a
MODEL_MAGIC = 0x314c444f4d484f4a
EDITOR_MAGIC = 0x3152444553484f4a
COMMAND_MAGIC = 0x31444e4d43484f4a
FACTS_MAGIC = 0x3154434146484f4a
RESPONSE_MAGIC = 0x3150534552484f4a
lib = C.CDLL(sys.argv[1])
call = lib.host_model_abi
call.argtypes = [C.c_void_p]*6+[C.c_uint64]
call.restype = C.c_uint64
checks = 0
names = 'help about status mem ps devices clear halt files net model agents grant revoke bench logs shutdown'.split()


def require(ok,msg):
    global checks
    checks += 1
    assert ok,msg


def command(text):
    words = [word for word in text.split(' ') if word]
    name = words[0].lower() if words else ''
    ident = names.index(name)+1 if name in names else (255 if words else 0)
    status = flag = action = 0
    if len(words)>2:
        status = 2
    elif ident == 255:
        status = 127
    elif len(words)==2:
        if 3<=ident<=6 and words[1]=='--json':
            flag = 1
        else:
            status = 2
    elif ident>=9:
        status = 69
    else:
        action = {7:1,8:2}.get(ident,0)
    return ident,status,flag,action


facts_values = [1000000,12345,6,8,32,2,1,80,7,7,0,3,0,3,0]
fields = 'image_bytes free_pages live_allocations candidate_ranges owned_table_pages enabled_cpus apic_mode timer_eois keyboard_irqs keyboard_eois keyboard_drops process_a_state process_a_status process_b_state process_b_status'.split()
groups = {3:'image_bytes enabled_cpus apic_mode timer_eois',4:'free_pages live_allocations candidate_ranges owned_table_pages',5:'process_a_state process_a_status process_b_state process_b_status',6:'apic_mode enabled_cpus keyboard_irqs keyboard_eois keyboard_drops'}


def response(values):
    ident,status,flag,action = values
    if status:
        return {2:'USAGE ERROR (2)\n',69:'UNAVAILABLE (69)\n',127:'UNKNOWN COMMAND (127)\n'}[status]
    if ident not in groups:
        return {0:'',1:'HELP ABOUT STATUS MEM PS DEVICES CLEAR HALT\nSTATUS|MEM|PS|DEVICES --json\n',2:'JOSHUA / HYPERCORE\nBSP DEVELOPMENT CONSOLE\nASSEMBLY RUNTIME. NO AI EXECUTION.\nQEMU EVIDENCE; PHYSICAL QUALIFICATION OPEN.\n',7:'CONSOLE CLEARED\n',8:'HALTING: QUIESCING KEYBOARD\n'}[ident]
    name = names[ident-1]
    data = {key:facts_values[fields.index(key)] for key in groups[ident].split()}
    if flag:
        return json.dumps({'command':name,'status':0,**data},separators=(',',':'))+'\n'
    return name.upper()+'\n'+''.join(f'{key}={value}\n' for key,value in data.items())


def editor(active='',cursor=None,submitted=''):
    if cursor is None:
        cursor = len(active)
    blob = bytearray(1568)
    struct.pack_into('<3Q',blob,0,EDITOR_MAGIC,len(active),cursor)
    struct.pack_into('<Q',blob,64,len(submitted))
    blob[96:96+len(active)] = active.encode('ascii')
    blob[352:352+len(submitted)] = submitted.encode('ascii')
    return bytes(blob)


class Fixture:
    def __init__(self,width,height):
        self.cols = min(100,(width-32)//12)
        self.rows = min(48,(height-32)//16)
        self.fb = (C.c_ubyte*(width*height*4))()
        C.memset(C.addressof(self.fb),0x5a,C.sizeof(self.fb))
        self.anchor = (C.c_uint64*6)(C.addressof(self.fb),C.sizeof(self.fb),width,height,width,0)
        self.model = (C.c_uint64*8)()
        self.grid = (C.c_uint64*613)()
        self.editor = (C.c_uint64*196)()
        self.command = (C.c_uint64*5)()
        self.response = (C.c_uint64*514)()
        self.facts = (C.c_uint64*16)(FACTS_MAGIC,*facts_values)
        self.pointers = list(map(C.addressof,[self.model,self.grid,self.anchor,self.editor,self.command,self.response]))
        require(self.invoke(4)==0,'actual grid init / ABI')
        require(self.invoke(0)==0,'model init / ABI')
        self.cells = bytearray(4800)
        for y in range(self.rows):
            self.cells[y*100:y*100+self.cols] = b' '*self.cols
        title = b'JOSHUA / HYPERCORE'
        self.cells[0:len(title)] = title
        subtitle = b'BSP DEVELOPMENT CONSOLE | NO AI EXECUTION'
        self.cells[100:100+len(subtitle)] = subtitle
        ready = b'READY / BSP DEVELOPMENT CONSOLE'
        self.cells[(self.rows-1)*100:(self.rows-1)*100+len(ready)] = ready
        self.dirty = (1<<self.rows)-1
        self.cursor = (8,self.rows-2,1)
        self.cells[(self.rows-2)*100:(self.rows-2)*100+8] = b'JOSHUA> '
        self.row,self.col,self.completed = 2,0,0
        self.last = (0,0,0,0)
        self.check()

    def invoke(self,op,pointers=None):
        return call(*(pointers or self.pointers),op)

    def check(self):
        require(bytes(self.model) == struct.pack('<8Q',MODEL_MAGIC,self.row,self.col,self.completed,*self.last),'complete model state')
        expected = struct.pack('<13Q',GRID_MAGIC,*self.anchor,self.cols,self.rows,self.dirty,*self.cursor)+self.cells
        require(bytes(self.grid) == expected,'complete grid state')
        before = bytes(self.model),bytes(self.grid)
        require(self.invoke(1)==0,'read-only model check / ABI')
        require((bytes(self.model),bytes(self.grid))==before,'check immutable')
        require(bytes(self.fb)==b'\x5a'*C.sizeof(self.fb),'composition does not write pixels')

    def prompt_reference(self,active,cursor):
        row = self.rows-2
        self.dirty |= (1<<self.cursor[1]) | (1<<row)
        start = max(0,cursor-(self.cols-8-1))
        text = 'JOSHUA> '+active[start:start+self.cols-8]
        self.cells[row*100:row*100+self.cols] = text.ljust(self.cols).encode('ascii')
        self.cursor = (8+cursor-start,row,1)

    def prompt(self,active,cursor):
        blob = editor(active,cursor)
        C.memmove(self.pointers[3],blob,1568)
        require(self.invoke(2)==0,'prompt / ABI')
        self.prompt_reference(active,cursor)
        require(bytes(self.editor)==blob,'prompt source immutable')
        self.check()

    def advance(self):
        self.col = 0
        if self.row == self.rows-3:
            self.cells[200:(self.rows-3)*100] = self.cells[300:(self.rows-2)*100]
            start = (self.rows-3)*100
            self.cells[start:start+self.cols] = b' '*self.cols
            self.dirty |= ((1<<(self.rows-2))-1)&~3
        else:
            self.row += 1

    def append(self,text):
        for char in text:
            if char=='\n':
                self.advance()
            else:
                if self.col==self.cols:
                    self.advance()
                self.cells[self.row*100+self.col] = ord(char)
                self.col += 1
                self.dirty |= 1<<self.row

    def complete(self,text,active='',cursor=None,synthetic_body=None):
        if cursor is None:
            cursor = len(active)
        values = command(text)
        blob = editor(active,cursor,text)
        C.memmove(self.pointers[3],blob,1568)
        require(self.invoke(6)==0,'actual parse / ABI')
        require(bytes(self.command)==struct.pack('<5Q',COMMAND_MAGIC,*values),'independent parsed result')
        args = self.pointers[:]
        args[3] = C.addressof(self.facts)
        require(self.invoke(7,args)==0,'actual response build / ABI')
        body = response(values).encode('ascii')
        require(bytes(self.response)==struct.pack('<2Q',RESPONSE_MAGIC,len(body))+body.ljust(4096,b'\0'),'independent actual response')
        if synthetic_body is not None:
            # Defensive storage/loop-bound fixture, not a claim that the fixed
            # response formatter emits this text or that provenance was proved.
            body = synthetic_body.encode('ascii')
            require(len(body)<=4095,'synthetic schema length')
            C.memmove(self.pointers[5],struct.pack('<2Q',RESPONSE_MAGIC,len(body))
                      +body.ljust(4096,b'\0'),4112)
        inputs = bytes(self.editor),bytes(self.command),bytes(self.response)
        require(self.invoke(3)==0,'complete / ABI')
        if values[3]==1:
            for y in range(2,self.rows-2):
                self.cells[y*100:y*100+self.cols] = b' '*self.cols
                self.dirty |= 1<<y
            self.row,self.col = 2,0
        else:
            if self.col:
                self.advance()
            self.append('JOSHUA> '+text+'\n')
        self.append(body.decode('ascii'))
        self.completed = min(MAX,self.completed+1)
        self.last = values
        label = {0:'RESULT 0 / OK',2:'RESULT 2 / USAGE',69:'RESULT 69 / UNAVAILABLE',127:'RESULT 127 / UNKNOWN'}[values[1]]
        y = self.rows-1
        self.cells[y*100:y*100+self.cols] = label.ljust(self.cols).encode('ascii')
        self.dirty |= 1<<y
        self.prompt_reference(active,cursor)
        require((bytes(self.editor),bytes(self.command),bytes(self.response))==inputs,'complete sources immutable')
        self.check()


rng = random.Random(0x4a4f5348)
for shape in [(608,352),(641,403),(1232,800)]:
    f = Fixture(*shape)
    for length in [0,1,39,40,41,99,100,126,127]:
        text = ''.join(chr(32+i%95) for i in range(length))
        for cursor in range(length+1):
            f.prompt(text,cursor)
    for text in ['', 'help','about','status','mem --json','ps','devices --json',
                 'unknown','files','halt x','unknown x y','clear','halt','X'*127]:
        f.complete(text)
    for _ in range(100):
        text = rng.choice(names+['status --json','mem --json','ps --json','devices --json','unknown','clear x'])
        active = ''.join(chr(rng.randrange(32,127)) for _ in range(rng.randrange(128)))
        f.complete(text,active,rng.randrange(len(active)+1))
    # Exact-width echo followed by LF advances once; one extra character wraps.
    f.complete('x'*(f.cols-8))
    f.complete('x'*(f.cols-7))
    # Completed saturation and a preexisting pending-wrap continuation.
    f.completed = MAX
    f.model[3] = MAX
    f.complete('about')
    f.row,f.col = 2,f.cols
    f.model[1],f.model[2] = f.row,f.col
    f.complete('status')
    # Maximum schema-valid payloads exercise bounded composition/scrolling well
    # beyond current immutable response-template lengths. These synthetic host
    # fixtures qualify defensive extent handling, not production provenance.
    f.complete('help',synthetic_body='a'*4095)
    f.complete('help',synthetic_body='\n'*4095)
    f.complete('help',synthetic_body='')
    # Restore a valid source/result pair for all rejection tests.
    f.complete('help')
    records = [f.model,f.grid,f.anchor,f.editor,f.command,f.response]
    baseline = [bytes(record) for record in records]
    def restore():
        for ptr,data in zip(f.pointers,baseline):
            C.memmove(ptr,data,len(data))
    def rejected(op=3,pointers=None):
        before = [bytes(record) for record in records]
        require(f.invoke(op,pointers)==1,'invalid model inputs / ABI')
        require([bytes(record) for record in records]==before,'complete failure atomicity')
        require(bytes(f.fb)==b'\x5a'*C.sizeof(f.fb),'rejection pixel atomicity')
    for offset,value in [(0,0),(8,1),(8,f.rows-2),(16,f.cols+1),
                         (32,255),(40,3),(48,2),(56,3)]:
        restore()
        C.c_uint64.from_address(f.pointers[0]+offset).value = value
        for op in [1,2,3]:
            rejected(op)
    for record,offset,value in [(1,0,0),(2,0,0),(3,64,128),(4,0,0),
                                 (5,0,0),(5,8,4096),(5,8,MAX)]:
        restore()
        C.c_uint64.from_address(f.pointers[record]+offset).value = value
        rejected()
    for offset,value in [(16,0),(16,9),(4111,65)]:
        restore()
        C.c_ubyte.from_address(f.pointers[5]+offset).value = value
        rejected()
    # Grammatically valid clear/halt result cannot control another submission.
    for ident,action in [(7,1),(8,2)]:
        restore()
        C.memmove(f.pointers[4],struct.pack('<5Q',COMMAND_MAGIC,ident,0,0,action),40)
        rejected()
    restore()
    for index in range(6):
        for bad in [0,f.pointers[index]+1,MAX-7]:
            pointers = f.pointers[:]
            pointers[index] = bad
            rejected(pointers=pointers)
    # Every pair of supplied metadata extents, exact and partial overlap.
    for i in range(6):
        for j in range(i):
            for delta in [0,8]:
                pointers = f.pointers[:]
                pointers[i] = pointers[j]+delta
                rejected(pointers=pointers)
    for index in [0,3,4,5]:
        pointers = f.pointers[:]
        pointers[index] = C.addressof(f.fb)
        rejected(pointers=pointers)
    restore()
    f.check()
# Inaccessible pages around every metadata extent in lower/upper passes.
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p,C.c_size_t,C.c_int]
page = mmap.PAGESIZE


class Guarded:
    def __init__(self,data,end):
        self.pages = (len(data)+page-1)//page
        self.memory = mmap.mmap(-1,(self.pages+2)*page)
        self.anchor = C.c_char.from_buffer(self.memory)
        self.base = C.addressof(self.anchor)
        require(libc.mprotect(self.base,page,0)==0,'lower guard')
        require(libc.mprotect(self.base+(self.pages+1)*page,page,0)==0,'upper guard')
        self.pointer = self.base+(self.pages+1)*page-len(data) if end else self.base+page
        self.size = len(data)
        C.memmove(self.pointer,data,self.size)

    def close(self):
        require(libc.mprotect(self.base,page,3)==0,'restore lower')
        require(libc.mprotect(self.base+(self.pages+1)*page,page,3)==0,'restore upper')
        del self.anchor
        self.memory.close()


for end in [False,True]:
    f = Fixture(608,352)
    f.complete('status --json','x'*127,127)
    records = [f.model,f.grid,f.anchor,f.editor,f.command,f.response]
    guards = [Guarded(bytes(record),end) for record in records]
    pointers = [guard.pointer for guard in guards]
    try:
        f.complete('status --json','x'*127,127)
        require(f.invoke(3,pointers)==0,'guarded complete / ABI')
        require([C.string_at(guard.pointer,guard.size) for guard in guards]
                ==[bytes(record) for record in records],'guarded exact complete state')
        require(f.invoke(2,pointers)==0,'guarded prompt / ABI')
        require(f.invoke(1,pointers)==0,'guarded check / ABI')
        before = [C.string_at(guard.pointer,guard.size) for guard in guards]
        C.c_ubyte.from_address(pointers[5]+4111).value = 65
        require(f.invoke(3,pointers)==1,'guarded final response byte rejected')
        require(C.string_at(pointers[0],64)==before[0] and
                C.string_at(pointers[1],4904)==before[1],'guarded corruption atomicity')
        require(f.invoke(0)==0,'trusted reinit reference')
        require(f.invoke(0,pointers)==0,'guarded inactive model reinit')
        require(C.string_at(pointers[0],64)==bytes(f.model) and
                C.string_at(pointers[1],4904)==bytes(f.grid),'guarded exact reinit')
    finally:
        for guard in guards:
            guard.close()
# Host preview uses the actual integrated Assembly parse/build/model/render path.
preview = Fixture(800,600)
for text in ['help','status','mem --json','unknown','devices']:
    preview.complete(text)
preview.prompt('status --json',13)
require(preview.invoke(5)==0,'host model pixels rendered / ABI')
raw = bytes(preview.fb)
scanlines = b''.join(b'\0'+b''.join(raw[(y*800+x)*4:(y*800+x)*4+3]
                                  for x in range(800)) for y in range(600))
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
image = (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',800,600,8,2,0,0,0))
         +chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b''))
Path(sys.argv[1]).with_name('console-model-preview.png').write_bytes(image)
print(f'Console model: {checks:,} cumulative checks passed')
