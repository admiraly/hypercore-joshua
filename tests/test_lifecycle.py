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

class Context(C.Structure):
    _fields_ = [(name, U64) for name in ["ready", "builder", "pt", "records", "count"]]
class MappingRecord(C.Structure):
    _fields_ = [(name, U64) for name in ["virtual", "physical", "pages", "owner", "state"]]
BASE = 0xffff800000000000
lib.host_lifecycle_prepare.argtypes = [C.POINTER(Context), C.POINTER(Builder), C.POINTER(MappingRecord)]
lib.host_lifecycle_alloc.argtypes = [C.POINTER(Context), U64, U64, C.POINTER(U64)]
lib.host_lifecycle_free.argtypes = [C.POINTER(Context), U64, U64, U64]
lib.host_event_storage.restype = C.c_void_p
event_address = lib.host_event_storage()
event_count = U64.from_address(event_address)
events = (U64 * (65536*4)).from_address(event_address+8)

def snapshot(f):
    return (bytes(f.ctx), bytes(f.virtual_records), bytes(f.allocator), bytes(f.records),
            bytes(f.ranges), bytes(f.array), C.string_at(f.ctx.pt,4096))

def table_snapshot(root):
    result = {}
    def visit(table, level):
        if table in result: return
        result[table] = C.string_at(table, 4096)
        if level == 1: return
        for i in range(512):
            entry = read(table+i*8)
            if entry & 1 and not entry & 128: visit(entry & MASK, level-1)
    visit(root,4)
    return result

class Lifecycle(Fixture):
    def __init__(self, pages=2048, record_capacity=128):
        super().__init__(pages, record_capacity)
        self.map(self.base, self.base + pages*4096, NX | 3)
        self.ctx = Context()
        self.virtual_records = (MappingRecord * 64)()
        assert lib.host_lifecycle_prepare(C.byref(self.ctx), C.byref(self.state), self.virtual_records) == 0
        self.state.ready = 1
        self.initial_free = self.allocator.free
        self.live = {}
        self.tables = table_snapshot(self.state.root)
        del self.tables[self.ctx.pt]
    def alloc(self, pages, owner=7, expected=0):
        global checks
        event_count.value = 0
        result = U64()
        before = self.allocator.free
        old_state = snapshot(self)
        status = lib.host_lifecycle_alloc(C.byref(self.ctx), pages, owner, C.byref(result))
        assert status == expected, (status, expected)
        checks += 1
        if status:
            assert result.value == 0
            if status in (1,2,3): assert snapshot(self) == old_state and event_count.value == 0
            return 0
        va = result.value
        assert va not in self.live and event_count.value == pages
        assert self.allocator.free == before-pages
        pa = walk(self.state.root, va)[0]
        assert C.string_at(pa, pages*4096) == bytes(pages*4096)
        for i in range(pages):
            translated, entry, size, cache = walk(self.state.root, va+i*4096)
            assert translated == pa+i*4096 and size == 4096 and entry & (NX|0x107) == NX|3
            assert events[i*4] == va+i*4096 and events[i*4+1] & 1
            assert events[i*4+2] == before-pages
            assert events[i*4+3] == sum(v[0] for v in self.live.values())+pages
            assert cache == walk(self.state.root, pa+i*4096)[3]
        assert any(r.base == pa and r.pages == pages and r.owner == 0x4d415050 for r in self.records[:self.allocator.count])
        for address, data in self.tables.items(): assert C.string_at(address,4096) == data
        self.live[va] = (pages, owner, pa)
        return va
    def free(self, va, pages=None, owner=None, expected=0):
        global checks
        if pages is None: pages = self.live[va][0]
        if owner is None: owner = self.live[va][1]
        event_count.value = 0
        before = self.allocator.free
        old_state = snapshot(self)
        status = lib.host_lifecycle_free(C.byref(self.ctx), va, pages, owner)
        assert status == expected, (status, expected)
        checks += 1
        if status:
            if status == 1: assert snapshot(self) == old_state and event_count.value == 0
            return
        assert event_count.value == pages and self.allocator.free == before+pages
        for i in range(pages):
            assert walk(self.state.root, va+i*4096) is None
            assert events[i*4] == va+i*4096 and events[i*4+1] == 0
            assert events[i*4+2] == before  # free happens after ALL invalidations
            assert events[i*4+3] == sum(v[0] for v in self.live.values())-pages
        for address, data in self.tables.items(): assert C.string_at(address,4096) == data
        del self.live[va]

f = Lifecycle()
a = f.alloc(3)
C.memset(f.live[a][2], 0xA5, 3*4096)
f.free(a, owner=8, expected=1)
f.free(a+4096, pages=2, owner=7, expected=1)
f.free(a)
f.free(a, pages=3, owner=7, expected=1)
assert f.alloc(3) == a
f.free(a)
assert f.allocator.free == f.initial_free
for pages, owner in [(0,7),(513,7),(1,0),(2**64-1,7)]: f.alloc(pages, owner, expected=1)
# Record exhaustion and complete arena exhaustion are distinct physical-independent limits.
for _ in range(64): f.alloc(1)
f.alloc(1, expected=2)
for va in list(f.live): f.free(va)
a = f.alloc(512)
f.alloc(1, expected=2)
f.free(a)
# Hardware A/D changes are allowed; address/permission corruption quarantines.
a = f.alloc(2)
U64.from_address(f.ctx.pt).value |= 0x60
f.free(a)
a = f.alloc(1)
U64.from_address(f.ctx.pt).value &= ~2
f.free(a, expected=4)
assert f.ctx.ready == 0 and f.ctx.count == 1
assert f.allocator.free == f.initial_free-1
# Mixed allocation/release with an independent virtual first-fit model.
import random
rng = random.Random(0x4d4150)
f = Lifecycle()
for _ in range(6000):
    if f.live and rng.randrange(3) == 0:
        f.free(rng.choice(list(f.live)))
    else:
        count = rng.randint(1, 23)
        occupied = set()
        for va, (n, _, _) in f.live.items(): occupied.update(range((va-BASE)//4096, (va-BASE)//4096+n))
        start = next((i for i in range(513-count) if not any(j in occupied for j in range(i,i+count))), None)
        expected = 2 if len(f.live) == 64 or start is None else 0
        va = f.alloc(count, rng.randint(1, 9), expected)
        if not expected:
            assert va == BASE + start*4096
            C.memset(f.live[va][2], 0x5A, count*4096)
    assert f.ctx.count == len(f.live)
    assert f.allocator.free == f.initial_free-sum(v[0] for v in f.live.values())
for va in list(f.live): f.free(va)
assert f.allocator.free == f.initial_free
# Private allocator metadata exhaustion and physical exhaustion are atomic.
f = Lifecycle(record_capacity=1)
f.alloc(1, expected=3)
f = Lifecycle(pages=128)
f.alloc(128, expected=2)
# A private allocator record mismatch after unmapping never frees physical frames.
f = Lifecycle()
a = f.alloc(2)
pa = f.live[a][2]
for rec in f.records[:f.allocator.count]:
    if rec.base == pa: rec.owner ^= 1
before = f.allocator.free
f.free(a, expected=4)
assert f.ctx.ready == 0 and f.allocator.free == before and f.ctx.count == 1
assert walk(f.state.root,a) is None and event_count.value == 2
# Broken identity permissions quarantine physical ownership before publication.
f = Lifecycle()
table = f.state.root
for shift in [39,30]: table = read(table+((f.base>>shift)&511)*8) & MASK
U64.from_address(table+((f.base>>21)&511)*8).value &= ~2
f.alloc(1, expected=4)
assert f.ctx.ready == 0 and f.ctx.count == 1 and not any(U64.from_address(f.ctx.pt+i*8).value for i in range(512))
# Integer ABI sentinels across an allocation/release pair.
f = Lifecycle()
lib.host_lifecycle_abi.argtypes = [C.POINTER(Context)]
assert lib.host_lifecycle_abi(C.byref(f.ctx)) == 0
assert f.ctx.count == 0 and f.allocator.free == f.initial_free
checks += 1
# All eight PAT indices, inherited from large leaves, preserve selector encoding.
for cache in range(8):
    f = Lifecycle()
    address = f.base
    table = f.state.root
    for shift in [39,30]: table = read(table+((address>>shift)&511)*8) & MASK
    leaf_ptr = table+((address>>21)&511)*8
    entry = U64.from_address(leaf_ptr)
    entry.value = (entry.value & ~0x1018) | ((cache & 3)<<3) | ((cache & 4)<<10)
    f.tables = table_snapshot(f.state.root)
    del f.tables[f.ctx.pt]
    a = f.alloc(3)
    assert all(walk(f.state.root,a+i*4096)[3] == cache for i in range(3))
    f.free(a)
print(f"PASS: {checks} lifecycle/oracle operations including 6,000 seeded mixed operations.")
