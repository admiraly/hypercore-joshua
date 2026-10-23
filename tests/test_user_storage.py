"""Independent table decoding for the exact Assembly builder; host only."""
import ctypes as C
import mmap
import sys

lib = C.CDLL(sys.argv[1])
U64 = C.c_uint64
NX = 1 << 63
MASK = 0x000FFFFFFFFFF000
checks = 0


class Range(C.Structure):
    _fields_ = [("start", U64), ("end", U64)]


class RangeSet(C.Structure):
    _fields_ = [("entries", C.POINTER(Range)), ("count", U64), ("capacity", U64)]


class Record(C.Structure):
    _fields_ = [("base", U64), ("pages", U64), ("owner", U64)]


class Allocator(C.Structure):
    _fields_ = [("ready", U64), ("ranges", C.POINTER(RangeSet)), ("records", C.POINTER(Record)),
                ("count", U64), ("capacity", U64), ("total", U64), ("free", U64)]


class Builder(C.Structure):
    _fields_ = [(name, U64) for name in ["allocator", "root", "table_count", "failed", "limit",
                "pool_next", "pool_end", "old_root", "image_base", "image_end", "guard",
                "fb_start", "fb_end", "pat", "ready"]]


lib.host_frames_init.argtypes = [C.POINTER(Allocator), C.POINTER(RangeSet), C.POINTER(Record), U64]
lib.host_frames_init.restype = C.c_int
lib.host_owned_init.argtypes = [C.POINTER(Builder), C.POINTER(Allocator), U64]
lib.host_owned_init.restype = C.c_int
for name in ["host_owned_map", "host_owned_preserved"]:
    getattr(lib, name).argtypes = [C.POINTER(Builder), U64, U64, U64]
    getattr(lib, name).restype = C.c_int
assert C.sizeof(Builder) == 120


def read(address):
    return U64.from_address(address).value


def walk(root, address):
    table = root
    for shift in [39, 30, 21, 12]:
        entry = read(table + ((address >> shift) & 511) * 8)
        if not entry & 1:
            return None
        if shift == 12 or shift in [21, 30] and entry & 128:
            size = 1 << shift
            physical = (entry & MASK & -size) + (address & (size - 1))
            cache = ((entry >> 3) & 3) | (((entry >> (7 if shift == 12 else 12)) & 1) << 2)
            return physical, entry, size, cache
        assert entry & 0x87 == 3, "parent must be writable, supervisor, non-leaf"
        table = entry & MASK
    raise AssertionError("missing leaf")


class Fixture:
    def __init__(self, pages=1024, record_capacity=32):
        self.mapping = mmap.mmap(-1, (pages + 512) * 4096)
        raw = C.addressof(C.c_char.from_buffer(self.mapping))
        self.base = (raw + 0x1FFFFF) & -0x200000
        self.array = (Range * 128)()
        self.array[0] = Range(self.base, self.base + pages * 4096)
        self.ranges = RangeSet(self.array, 1, 128)
        self.records = (Record * record_capacity)()
        self.allocator = Allocator()
        assert lib.host_frames_init(C.byref(self.allocator), C.byref(self.ranges), self.records, record_capacity) == 0
        self.state = Builder()
        assert lib.host_owned_init(C.byref(self.state), C.byref(self.allocator), 1 << 47) == 0
        self.old_memory = mmap.mmap(-1, 5 * 4096)
        self.old = C.addressof(C.c_char.from_buffer(self.old_memory))
        self.old_words = (U64 * (5 * 512)).from_buffer(self.old_memory)
        self.state.old_root = self.old

    def map(self, start, end, flags, expected=0, preserved=False):
        global checks
        func = lib.host_owned_preserved if preserved else lib.host_owned_map
        assert func(C.byref(self.state), start, end, flags) == expected
        if expected:
            assert self.state.failed == 1
        owned_pages = set()
        for i in range(self.allocator.count):
            record = self.records[i]
            assert record.owner == 0x50414745
            owned_pages.update(range(record.base, record.base + record.pages * 4096, 4096))
        assert self.state.root in owned_pages
        # Inspect every owned parent and leaf independently, not just queried VAs.
        queue = [(self.state.root, 4, 0)]
        seen = set()
        for table, level, prefix in queue:
            assert table in owned_pages and table not in seen
            seen.add(table)
            shift = 12 + 9 * (level - 1)
            for slot in range(512):
                entry = read(table + slot * 8)
                if not entry & 1:
                    continue
                address = prefix | (slot << shift)
                assert not entry & 4
                if level == 1 or level == 2 and entry & 128:
                    assert not (entry & 2 and not entry & NX)
                    size = 1 << shift
                    assert entry & MASK & -size == address
                else:
                    assert level > 1 and not entry & 128
                    assert entry & 0x9F == 3
                    queue.append((entry & MASK, level - 1, address))
        assert len(seen) == self.state.table_count
        assert self.state.table_count <= len(owned_pages)
        assert self.allocator.total - self.allocator.free == len(owned_pages)
        checks += 1

    def old_leaf(self, address, size, cache=0, physical=None):
        if physical is None:
            physical = address
        self.old_words[(address >> 39) & 511] = self.old + 4096 | 3
        self.old_words[512 + ((address >> 30) & 511)] = self.old + 8192 | 3
        self.old_words[1024 + ((address >> 21) & 511)] = self.old + 12288 | 3
        flags = 3 | ((cache & 3) << 3) | ((cache >> 2) << (7 if size == 4096 else 12))
        if size == 4096:
            self.old_words[1536 + ((address >> 12) & 511)] = physical | flags
        elif size == 0x200000:
            self.old_words[1024 + ((address >> 21) & 511)] = physical | flags | 128
        else:
            self.old_words[512 + ((address >> 30) & 511)] = physical | flags | 128


for cache in range(8):
    f = Fixture()
    flags = NX | 3 | ((cache & 3) << 3) | ((cache >> 2) << 7)
    f.map(0x1000, 0x5000, flags)
    for address in range(0x1000, 0x5000, 4096):
        pa, entry, size, index = walk(f.state.root, address)
        assert pa == address and size == 4096 and index == cache
        assert entry & (NX | 7) == NX | 3
    assert walk(f.state.root, 0) is None
    f.map(0x200000, 0x600000, flags)
    for address in [0x200000, 0x201234, 0x5FFFFF]:
        pa, entry, size, index = walk(f.state.root, address)
        assert pa == address and size == 0x200000 and index == cache
        assert entry & (NX | 7) == NX | 3

class UserContext(C.Structure):
    _fields_ = [(n,U64) for n in ['ready','failed','stage','builder','processes','payload',
                'payload_size','kernel_root','active','phase','bootstrap_frame',
                'bootstrap_low','bootstrap_high','saved_cr0','code','data','np']]

class Process(C.Structure):
    _fields_ = [(n,U64) for n in ['root','pdpt','pd','pt','backing','owner','state','status',
                'ticks','eois','fault_vector','fault_error','fault_rip','fault_cr2',
                'kernel_low','kernel_high','unknown_calls','last_cs']]

lib.host_user_storage.argtypes = [C.POINTER(UserContext)]
lib.host_user_storage.restype = C.c_int
lib.host_user_boundary.argtypes = [C.POINTER(UserContext),U64,U64,C.POINTER(U64)]
lib.host_user_boundary.restype = U64

class UserFixture(Fixture):
    def __init__(self, pages=1024, capacity=32):
        super().__init__(pages,capacity)
        self.processes = (Process * 2)()
        self.payload = C.create_string_buffer(bytes(range(256))*16)
        self.user = UserContext()
        self.user.builder = C.addressof(self.state)
        self.user.processes = C.addressof(self.processes)
        self.user.payload = C.addressof(self.payload)
        self.user.payload_size = 4096

    def prepare(self,expected=0):
        global checks
        old_free = self.allocator.free
        assert lib.host_user_storage(C.byref(self.user)) == expected
        assert not self.user.ready
        if expected:
            assert self.user.failed == 1 and self.user.stage == 0
        else:
            assert self.user.stage == 1 and self.user.failed == 0
            assert self.allocator.free == old_free-12
            assert self.processes[0].backing != self.processes[1].backing
            for i,p in enumerate(self.processes):
                assert p.owner == 0x55530001+i
                owned = [r for r in self.records[:self.allocator.count]
                         if r.base == p.backing and r.pages == 6 and r.owner == p.owner]
                assert len(owned) == 1
                assert C.string_at(p.backing,4096) == self.payload.raw[:4096]
                content = bytearray(5*4096)
                content[8:16] = (0x4a53485500000000|i).to_bytes(8,'little')
                assert C.string_at(p.backing+4096,5*4096) == content
        checks += 1

f = UserFixture()
f.prepare()
import random
rng = random.Random(3201)
for _ in range(2000):
    blocks = [p.backing for p in f.processes]
    cursor = rng.choice(blocks)+rng.choice([-8192,-1,0,1,4095,4096,8192])
    end = cursor+rng.randrange(1,10000)
    inside = next((b for b in blocks if b<=cursor<b+4096),None)
    expected = min(end,inside+4096) if inside is not None else min([end]+[b for b in blocks if b>cursor])
    flag = U64(99)
    before = bytes(f.user),bytes(f.processes)
    assert lib.host_user_boundary(C.byref(f.user),cursor,end,C.byref(flag)) == expected
    assert flag.value == (inside is not None)
    assert before == (bytes(f.user),bytes(f.processes))
    checks += 1

for field,value in [('payload_size',0),('payload_size',4097),('payload',0),('processes',0),('builder',0),('stage',1),('ready',1),('failed',1)]:
    f = UserFixture()
    setattr(f.user,field,value)
    before = bytes(f.processes),f.allocator.free
    assert lib.host_user_storage(C.byref(f.user)) == 1
    assert (bytes(f.processes),f.allocator.free) == before
    checks += 1
for field in ['root','backing','owner','state','last_cs']:
    f = UserFixture()
    setattr(f.processes[1],field,1)
    before = bytes(f.processes),f.allocator.free
    assert lib.host_user_storage(C.byref(f.user)) == 1
    assert (bytes(f.processes),f.allocator.free) == before
    checks += 1
# Exhaustion after a first allocation quarantines, never publishes readiness/reuses it.
f = UserFixture(pages=71)
f.prepare(1)
assert f.processes[0].backing and not f.processes[1].backing
before = f.allocator.free,bytes(f.processes)
assert lib.host_user_storage(C.byref(f.user)) == 1
assert before == (f.allocator.free,bytes(f.processes))
checks += 1
print(f'PASS: {checks} user backing ownership/code-boundary fixtures.')


lib.host_user_roots.argtypes = [C.POINTER(UserContext)]
lib.host_user_roots.restype = C.c_int
USER_BASE = 0x400000000000

def map_backing(f, cache=0, bad_code=False):
    cache_flags = ((cache & 3) << 3) | ((cache >> 2) << 7)
    for p in f.processes:
        # Exact production builder; code identity alias sealed RO/NX.
        assert lib.host_owned_map(C.byref(f.state),p.backing,p.backing+4096,
                                  NX | (3 if bad_code else 1) | cache_flags) == 0
        assert lib.host_owned_map(C.byref(f.state),p.backing+4096,p.backing+6*4096,
                                  NX | 3 | cache_flags) == 0

def raw_page(address):
    return (U64 * 512).from_address(address)

for cache in range(8):
    f = UserFixture()
    f.prepare()
    map_backing(f,cache)
    old_root = bytes(raw_page(f.state.root))
    count = f.state.table_count
    assert lib.host_user_roots(C.byref(f.user)) == 0
    assert f.user.stage == 2 and f.user.ready == 0 and not f.user.failed
    assert f.state.table_count == count+8
    assert bytes(raw_page(f.state.root)) == old_root
    assert len({getattr(p,n) for p in f.processes for n in ['root','pdpt','pd','pt']}) == 8
    for p in f.processes:
        root = raw_page(p.root)
        for i in range(512):
            assert root[i] == (p.pdpt|7 if i == 128 else raw_page(f.state.root)[i])
        for table,child in [(p.pdpt,p.pd),(p.pd,p.pt)]:
            page = raw_page(table)
            assert page[0] == child|7 and list(page)[1:] == [0]*511
        leaves = raw_page(p.pt)
        expected = [0]*512
        for i in range(6):
            slot = i if i < 2 else i+1
            flags = 5 if i == 0 else NX|7
            expected[slot] = (p.backing+i*4096)|flags|((cache&3)<<3)|((cache>>2)<<7)
        assert list(leaves) == expected
        # The private user path shares no table frames with supervisor subtrees.
        paging_pages = set()
        for r in f.records[:f.allocator.count]:
            if r.owner == 0x50414745:
                paging_pages.update(range(r.base,r.base+r.pages*4096,4096))
        assert {p.root,p.pdpt,p.pd,p.pt} <= paging_pages
        for i in range(6):
            pa,entry,_,_ = walk(f.state.root,p.backing+i*4096)
            assert pa == p.backing+i*4096
            assert entry & (NX|7) == NX|(1 if i==0 else 3)
    checks += 1

for mutation in ['slot','shared_user','active','phase','stage','ready','failed','builder_active','root','owner','writable_code','missing_alias']:
    f = UserFixture()
    f.prepare()
    if mutation != 'missing_alias':
        map_backing(f,bad_code=mutation=='writable_code')
    if mutation == 'slot':
        raw_page(f.state.root)[128] = 3
    elif mutation == 'shared_user':
        raw_page(f.state.root)[0] = 7
    elif mutation in ['active','phase','ready','failed']:
        setattr(f.user,mutation,1)
    elif mutation == 'stage':
        f.user.stage = 0
    elif mutation == 'builder_active':
        f.state.ready = 1
    elif mutation == 'root':
        f.user.kernel_root += 4096
    elif mutation == 'owner':
        f.processes[0].owner += 1
    before = bytes(raw_page(f.state.root))
    assert lib.host_user_roots(C.byref(f.user)) == 1
    assert f.user.failed and not f.user.ready and f.user.stage != 2
    assert bytes(raw_page(f.state.root)) == before
    checks += 1
print(f'PASS: {checks} user backing/private-root/cache-policy fixtures.')


lib.host_user_proof.argtypes = [C.POINTER(UserContext)]
lib.host_user_proof.restype = C.c_int

def proof_fixture(cache=0):
    f = UserFixture()
    f.prepare()
    map_backing(f,cache)
    assert lib.host_user_roots(C.byref(f.user)) == 0
    return f

for cache in range(8):
    f = proof_fixture(cache)
    before = bytes(f.mapping)
    assert lib.host_user_proof(C.byref(f.user)) == 0
    assert f.user.ready == 0x315253554153484a and f.user.stage == 3
    assert bytes(f.mapping) == before
    checks += 1
for field in ['root','pdpt','pd','pt']:
    f = proof_fixture()
    setattr(f.processes[1],field,1)  # Invalid table pointer rejected before read.
    assert lib.host_user_proof(C.byref(f.user)) == 1 and not f.user.ready
    checks += 1
for level in ['root','pdpt','pd','pt']:
    for flag in [1,2,4,8,128,NX]:
        f = proof_fixture()
        p = f.processes[0]
        slot = 128 if level == 'root' else 0
        raw_page(getattr(p,level))[slot] ^= flag
        before = bytes(f.mapping)
        assert lib.host_user_proof(C.byref(f.user)) == 1 and not f.user.ready
        assert bytes(f.mapping) == before
        checks += 1
for mutation in ['extra_user','guard','backing_alias','table_alias','foreign_owner','kernel_user','clone_change','table_in_backing','duplicate_record']:
    f = proof_fixture()
    p,q = f.processes
    if mutation == 'extra_user':
        raw_page(p.pt)[511] = q.backing|NX|7
    elif mutation == 'guard':
        raw_page(p.pt)[2] = p.backing|NX|7
    elif mutation == 'backing_alias':
        q.backing = p.backing
    elif mutation == 'table_alias':
        q.pt = p.pt
    elif mutation == 'foreign_owner':
        q.owner = p.owner
    elif mutation == 'kernel_user':
        raw_page(f.state.root)[0] = 7
    elif mutation == 'clone_change':
        raw_page(q.root)[1] = 3
    elif mutation == 'table_in_backing':
        q.pt = q.backing
    elif mutation == 'duplicate_record':
        f.records[f.allocator.count] = f.records[0]
        f.allocator.count += 1
    before = bytes(f.mapping)
    assert lib.host_user_proof(C.byref(f.user)) == 1 and not f.user.ready
    assert bytes(f.mapping) == before
    checks += 1
print(f'PASS: {checks} user storage/root/ownership/permission proof checks.')

for _ in range(200):
    f = UserFixture()
    f.prepare()
    selectors = {}
    for pid,p in enumerate(f.processes):
        for i in range(6):
            cache = rng.randrange(8)
            selectors[pid,i] = cache
            flags = NX|(1 if i==0 else 3)|((cache&3)<<3)|((cache>>2)<<7)
            assert lib.host_owned_map(C.byref(f.state),p.backing+i*4096,p.backing+(i+1)*4096,flags) == 0
    assert lib.host_user_roots(C.byref(f.user)) == 0
    assert lib.host_user_proof(C.byref(f.user)) == 0
    # Decode every actual private leaf independently of builder/proof helpers.
    for pid,p in enumerate(f.processes):
        for i in range(6):
            slot = i if i < 2 else i+1
            entry = raw_page(p.pt)[slot]
            decoded = ((entry >> 3) & 3) | (((entry >> 7) & 1) << 2)
            assert decoded == selectors[pid,i]
            assert entry & MASK == p.backing+i*4096
            assert entry & (NX|7) == (5 if i==0 else NX|7)
    checks += 1
f = UserFixture()
f.prepare()
map_backing(f)
f.state.pool_next = f.state.pool_end
f.allocator.capacity = f.allocator.count  # New paging chunk cannot get a record.
before = f.allocator.free,f.allocator.count,bytes(raw_page(f.state.root))
assert lib.host_user_roots(C.byref(f.user)) == 1
assert f.user.failed and not f.user.ready
assert before == (f.allocator.free,f.allocator.count,bytes(raw_page(f.state.root)))
checks += 1
print(f'PASS: {checks} user storage/root/proof checks including mixed PAT selectors and capacity failure.')

lib.host_user_storage_abi.argtypes = [C.POINTER(UserContext),U64,U64,U64]
lib.host_user_storage_abi.restype = C.c_int
for operation in [0,1,2]:
    for valid in [False,True]:
        f = UserFixture()
        if operation > 0:
            f.prepare()
            map_backing(f)
        if operation > 1:
            assert lib.host_user_roots(C.byref(f.user)) == 0
        if not valid:
            f.user.failed = 1
        assert lib.host_user_storage_abi(C.byref(f.user),operation,0,0) == (0 if valid else 1)
        checks += 1
f = UserFixture()
f.prepare()
for offset in [-4096,0,4096]:
    cursor = f.processes[0].backing+offset
    assert lib.host_user_storage_abi(C.byref(f.user),3,cursor,cursor+8192) == 0
    checks += 1
print(f'PASS: {checks} user storage/root/proof/oracle/ABI checks.')
