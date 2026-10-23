"""Independent bounded command grammar oracle against production Assembly."""
import ctypes as C
import itertools
import mmap
import random
import struct
import sys

MAGIC = 0x31444e4d43484f4a
EDITOR_MAGIC = 0x3152444553484f4a
MAX = (1 << 64)-1
lib = C.CDLL(sys.argv[1])
call = lib.host_command_abi
call.argtypes = [C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint64,C.c_uint64]
call.restype = C.c_uint64
checks = 0
names = 'help about status mem ps devices clear halt files net model agents grant revoke bench logs shutdown'.split()
ids = {name:i+1 for i,name in enumerate(names)}


def require(ok,message):
    global checks
    checks += 1
    assert ok,message


def model(text):
    words = text.split(' ')
    words = [word for word in words if word]
    command = ids.get(words[0].lower(),255) if words else 0
    status = json = action = 0
    if len(words) > 2:
        status = 2
    elif command == 255:
        status = 127
    elif len(words) == 2:
        if 3 <= command <= 6 and words[1] == '--json':
            json = 1
        else:
            status = 2
    elif command >= 9:
        status = 69
    elif command == 7:
        action = 1
    elif command == 8:
        action = 2
    return struct.pack('<5Q',MAGIC,command,status,json,action)


def editor(text):
    data = bytearray(1568)
    struct.pack_into('<Q',data,0,EDITOR_MAGIC)
    struct.pack_into('<Q',data,64,len(text))
    data[352:352+len(text)] = text.encode('ascii')
    return bytes(data)


source = (C.c_uint64*196)()
result_arena = (C.c_uint64*7)()
ptr = C.addressof(source)
out = C.addressof(result_arena)+8


def invoke(text):
    data = editor(text)
    C.memmove(ptr,data,1568)
    C.memset(C.addressof(result_arena),0xa5,56)
    require(call(ptr,out,0,0,0) == 0, ('parse / ABI',text))
    require(C.string_at(out,40) == model(text), ('grammar',text,list(result_arena)))
    require(bytes(source) == data, 'source immutable')
    require(result_arena[0] == 0xa5a5a5a5a5a5a5a5 and
            result_arena[6] == 0xa5a5a5a5a5a5a5a5, 'result bounds')


for text in ['', ' ', ' '*127, 'x'*127, 'halt x y', 'unknown --json', 'unknown x y']:
    invoke(text)
for name in names:
    for case in itertools.product(*[(c,c.upper()) for c in name]):
        command = ''.join(case)
        for arg in ['', ' --json',' --JSON',' x',' --json x']:
            invoke('  '+command+arg+' ')
    for token in [name[:-1],name+'x','x'+name,name+'--json']:
        invoke(token)
    for padding in range(0,128-len(name)):
        invoke(' '*padding+name)
        invoke(name+' '*padding)
# Token boundaries never require reading past submitted length.
for length in range(128):
    invoke('a'*length)
    if length >= 6:
        invoke(' '* (length-6)+'status')
    if length >= 13:
        invoke('status'+' '*(length-12)+'--json')
rng = random.Random(0x4a4f5348)
for _ in range(15000):
    if rng.randrange(2):
        words = [rng.choice(names+['unknown','STATUSX','--json'])]
        words += rng.choices(['--json','--JSON','x','halt','clear'],k=rng.randrange(4))
        text = ' '*rng.randrange(4)+(' '*rng.randrange(1,5)).join(words)+' '*rng.randrange(4)
    else:
        text = ''.join(chr(rng.randrange(32,127)) for _ in range(rng.randrange(128)))
    invoke(text)
# Full editor validation, not merely submitted-buffer validation.
valid = editor('halt')
for offset,value in [(0,0),(8,128),(16,1),(24,9),(32,8),(40,1),(48,128),
                     (56,1),(64,128),(480,1)]:
    corrupt = bytearray(valid)
    struct.pack_into('<Q',corrupt,offset,value)
    C.memmove(ptr,bytes(corrupt),1568)
    C.memset(C.addressof(result_arena),0x5a,56)
    before = bytes(result_arena)
    require(call(ptr,out,0,0,0) == 1, 'invalid editor / ABI')
    require(bytes(source) == corrupt and bytes(result_arena) == before, 'rejection atomicity')
for offset,value in [(352,31),(352,127),(356,65),(479,65),(96,65),(544,65),(1567,65)]:
    corrupt = bytearray(valid)
    corrupt[offset] = value
    C.memmove(ptr,bytes(corrupt),1568)
    before = bytes(result_arena)
    require(call(ptr,out,0,0,0) == 1, 'invalid text / padding')
    require(bytes(source) == corrupt and bytes(result_arena) == before, 'text rejection atomicity')
C.memmove(ptr,valid,1568)
for bad in [0,ptr+1,MAX-7]:
    before = bytes(source),bytes(result_arena)
    require(call(bad,out,0,0,0) == 1, 'invalid source address')
    require(call(ptr,bad,0,0,0) == 1, 'invalid result address')
    require((bytes(source),bytes(result_arena)) == before, 'bad address atomicity')
# Both exact and partial aligned overlap, including across the source end.
for target in [ptr,ptr+8,ptr+1528,ptr+1560,ptr-32]:
    require(call(ptr,target,0,0,0) == 1, 'complete interval overlap rejection')
    require(bytes(source) == valid, 'overlap source unchanged')
# Inaccessible boundaries for both complete extents.
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p,C.c_size_t,C.c_int]
page = mmap.PAGESIZE
for end in [False,True]:
    arenas = []
    try:
        pointers = []
        for size in [1568,40]:
            memory = mmap.mmap(-1,3*page)
            anchor = C.c_char.from_buffer(memory)
            base = C.addressof(anchor)
            require(libc.mprotect(base,page,0) == 0,'guard before')
            require(libc.mprotect(base+2*page,page,0) == 0,'guard after')
            arenas.append((memory,anchor,base))
            pointers.append(base+2*page-size if end else base+page)
        for text in ['halt',' '*127,'x'*127,'status --json','unknown x y']:
            C.memmove(pointers[0],editor(text),1568)
            require(call(*pointers,0,0,0) == 0,'guarded parse / ABI')
            require(C.string_at(pointers[1],40) == model(text),'guarded exact result')
        C.c_uint64.from_address(pointers[0]+64).value = MAX
        before = C.string_at(pointers[1],40)
        require(call(*pointers,0,0,0) == 1,'guarded invalid length')
        require(C.string_at(pointers[1],40) == before,'guarded rejection atomicity')
    finally:
        for memory,anchor,base in arenas:
            require(libc.mprotect(base,page,3) == 0,'restore before')
            require(libc.mprotect(base+2*page,page,3) == 0,'restore after')
        arenas.clear()
        del anchor
        # mmap objects are released after exported ctypes anchors leave scope.
print(f'Console command: {checks:,} cumulative checks passed')
