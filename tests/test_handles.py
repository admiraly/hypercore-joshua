"""Independent whole-table oracle for actual Assembly; not a privileged adapter test."""
import ctypes as C
import mmap
import random
import struct
import sys

SIZE, MAGIC, MAX = 1056, 0x314c444e4848534a, (1 << 40) - 1
MASK = (1 << 64) - 1
checks = 0
lib = C.CDLL(sys.argv[1])
call = lib.host_handle_abi
call.argtypes = [C.c_uint64] * 6
call.restype = C.c_uint64

def valid(q, owner):
    if q[:4] != [MAGIC, owner, q[2], 0] or not owner or not 1 <= q[2] <= 65535:
        return False
    for i in range(4, 132, 4):
        state, gen, obj, rights = q[i:i+4]
        if not 1 <= gen <= MAX:
            return False
        if state == 1:
            if not obj or not 1 <= rights <= 3:
                return False
        elif state in (0, 2):
            if obj or rights or (state == 2 and gen != MAX):
                return False
        else:
            return False
    return True

def model(q, op, owner, a, b):
    q = q.copy()
    def result(status=0, x=0, y=0):
        return status, (x, y), q
    if op == 0:
        if not owner or not 1 <= a <= 65535 or b or any(q):
            return result(1)
        q[:4] = [MAGIC, owner, a, 0]
        for i in range(4, 132, 4):
            q[i+1] = 1
        return result()
    if not valid(q, owner):
        return result(1)
    if op in (1, 6):
        if a or b:
            return result(1)
    elif op == 5:
        if b:
            return result(1)
    elif not 1 <= b <= 3 or (op == 2 and not a):
        return result(1)
    idx = None
    if op in (3, 4, 5):
        slot, generation, tag = a & 255, (a >> 8) & MAX, a >> 48
        if not 1 <= slot <= 32 or tag != q[2]:
            return result(2)
        idx = 4 + (slot - 1) * 4
        if q[idx] != 1 or q[idx+1] != generation:
            return result(2)
        if op in (3, 4) and ((op == 4 and not q[idx+3] & 2) or b & ~q[idx+3]):
            return result(3)
    if op == 3:
        return result(0, q[idx+2], q[idx+3])
    if op in (2, 4):
        free = next((i for i in range(4, 132, 4) if q[i] == 0), None)
        if free is None:
            return result(4)
        obj = a if op == 2 else q[idx+2]
        q[free:free+4] = [1, q[free+1], obj, b]
        token = (q[2] << 48) | (q[free+1] << 8) | ((free-4)//4+1)
        return result(0, token)
    if op in (5, 6):
        entries = [idx] if op == 5 else [i for i in range(4, 132, 4) if q[i] == 1]
        for i in entries:
            gen = q[i+1]
            q[i:i+4] = [2 if gen == MAX else 0, min(MAX, gen+1), 0, 0]
    return result()

class Table:
    def __init__(self, address=None):
        self.buffer = C.create_string_buffer(SIZE+8) if address is None else None
        self.address = ((C.addressof(self.buffer)+7)&~7) if address is None else address
        self.q = [0]*132
        C.memset(self.address, 0, SIZE)
    def set(self, index, value):
        self.q[index] = value
        C.c_uint64.from_address(self.address+8*index).value = value
    def run(self, op, owner=7, a=0, b=0):
        global checks
        before = bytes(C.string_at(self.address, SIZE))
        expected, outputs, new = model(self.q, op, owner, a, b)
        out = (C.c_uint64*2)(MASK, MASK)
        actual = call(op, self.address, owner, a, b, C.addressof(out))
        assert (actual, tuple(out)) == (expected, outputs), (op, owner, a, b, actual, tuple(out), expected, outputs)
        raw = C.string_at(self.address, SIZE)
        assert raw == struct.pack('<132Q', *new), (op, 'whole-state mismatch')
        if expected:
            assert raw == before, 'failure mutated state'
        self.q = new
        checks += 1
        return actual, outputs

# Complete capacity, attenuation, independent duplicate lifetime and stale reuse.
t = Table(); t.run(0, a=1); t.run(0, a=1)
tokens = []
for i in range(32):
    tokens.append(t.run(2, a=i+1, b=3)[1][0])
t.run(2, a=99, b=3); t.run(4, a=tokens[0], b=1)
t.run(5, a=tokens[3]); t.run(3, a=tokens[3], b=1)
dup = t.run(4, a=tokens[0], b=1)[1][0]
t.run(4, a=dup, b=1); t.run(3, a=dup, b=2)
t.run(5, a=tokens[0]); t.run(3, a=dup, b=1)
t.run(6); t.run(6)
for token in tokens + [dup]:
    t.run(3, a=token, b=1)
# Distinct table tags, owner identity, invalid/reserved argument precedence.
u = Table(); u.run(0, owner=9, a=2)
foreign = u.run(2, owner=9, a=123, b=3)[1][0]
t.run(3, a=foreign, b=1); u.run(3, owner=7, a=foreign, b=1)
for op in range(7):
    for a,b in [(0,0),(MASK,MASK),(1,4),(foreign,0),(foreign,3)]:
        t.run(op, a=a, b=b)
# Max generation: close/revoke retire permanently; no free capacity from retired slots.
r = Table(); r.run(0, a=65535)
for i in range(4,132,4):
    r.set(i+1, MAX)
max_tokens = [r.run(2, a=i+1, b=3)[1][0] for i in range(32)]
r.run(5, a=max_tokens[0]); r.run(6); r.run(2, a=1, b=1)
for token in max_tokens:
    r.run(3, a=token, b=1)
# Every header/entry field corruption checked even for unrelated operations.
base = Table(); base.run(0, a=3); live = base.run(2,a=1,b=3)[1][0]
mutations = [(0,0),(1,0),(2,0),(2,65536),(3,1)]
for i in range(4,132,4):
    mutations += [(i,3),(i+1,0),(i+1,MAX+1),(i+2,MASK),(i+3,4)]
for index,value in mutations:
    m = Table()
    for j,v in enumerate(base.q): m.set(j,v)
    m.set(index,value)
    for op in range(1,7): m.run(op,a=live,b=1)
# Initialization rejects every dirty qword and invalid owner/tag/reserved input.
for index in range(132):
    dirty=Table(); dirty.set(index,1); dirty.run(0,a=1)
for owner,a,b in [(0,1,0),(7,0,0),(7,65536,0),(7,1,1)]:
    Table().run(0,owner,a,b)
# Rights failures precede capacity; invalid argument precedes bad token.
f=Table(); f.run(0,a=4)
read_only=f.run(2,a=1,b=1)[1][0]
for i in range(31): f.run(2,a=i+2,b=3)
f.run(4,a=read_only,b=1); f.run(3,a=read_only,b=2)
f.run(4,a=MASK,b=0); f.run(5,a=MASK,b=1)
# Explicit malformed live/retired combinations, generation and rights boundaries.
for state,gen,obj,rights in [(1,1,0,1),(1,1,1,0),(1,1,1,4),(2,MAX-1,0,0),
                              (2,MAX,1,0),(2,MAX,0,1),(0,1,0,1),(0,1,1,0)]:
    bad=Table(); bad.run(0,a=5)
    for j,v in enumerate((state,gen,obj,rights)): bad.set(4+j,v)
    for op in range(1,7): bad.run(op,a=live,b=1)
# Deterministic independent model histories; retained tokens include stale/foreign ones.
randomizer = random.Random(0x4a4f5348)
for campaign in range(40):
    m=Table(); m.run(0,owner=campaign+1,a=campaign+10)
    pool=[0, MASK, foreign]
    for step in range(1000):
        op=randomizer.randrange(1,7)
        owner=campaign+1 if randomizer.randrange(20) else MASK
        a=randomizer.randrange(1,100) if op==2 else randomizer.choice(pool)
        b=randomizer.randrange(5) if op in (2,3,4) else (0 if randomizer.randrange(10) else 1)
        if op in (1,6) and randomizer.randrange(10): a=0
        status,out=m.run(op,owner,a,b)
        if not status and op in (2,4): pool.append(out[0])
# Address arithmetic rejects without dereferencing; output/ABI checked too.
for address in [0,1,MASK-7,MASK-1023,0xfffffffffffffbe0]:
    out=(C.c_uint64*2)(MASK,MASK)
    assert call(1,address,7,0,0,C.addressof(out))==1 and tuple(out)==(0,0)
    checks+=1
# Each exact extent touches a protected lower/upper page boundary in separate passes.
libc=C.CDLL(None); libc.mprotect.argtypes=[C.c_void_p,C.c_size_t,C.c_int]
for upper in (False,True):
    arena=mmap.mmap(-1,3*mmap.PAGESIZE,prot=mmap.PROT_READ|mmap.PROT_WRITE)
    address=C.addressof(C.c_char.from_buffer(arena))
    assert libc.mprotect(address,mmap.PAGESIZE,0)==0
    assert libc.mprotect(address+2*mmap.PAGESIZE,mmap.PAGESIZE,0)==0
    try:
        p=address+2*mmap.PAGESIZE-SIZE if upper else address+mmap.PAGESIZE
        m=Table(p); m.run(0,a=10)
        ts=[m.run(2,a=i+1,b=3)[1][0] for i in range(32)]
        for token in ts: m.run(3,a=token,b=1)
        m.run(6); m.run(1)
    finally:
        assert libc.mprotect(address,3*mmap.PAGESIZE,mmap.PROT_READ|mmap.PROT_WRITE)==0
        arena.close()
print(f'PASS: {checks} cumulative Assembly handle/model/ABI/guard checks.')
