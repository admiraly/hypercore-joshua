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

f = Fixture()
f.map(0x1000, 0x5000, 1)
assert walk(f.state.root, 0x1000)[1] & (NX | 7) == 1
f.map(0x5000, 0x6000, NX | 1)
assert walk(f.state.root, 0x5000)[1] & (NX | 7) == NX | 1
for start, end, flags in [(0, 4096, NX | 3), (1, 4096, NX | 3), (4096, 4097, NX | 3),
                           (8192, 4096, NX | 3), (4096, 8192, 3), (4096, 8192, NX | 7),
                           (4096, (1 << 47) + 4096, NX | 3), (4096, 8192, NX | 2)]:
    f = Fixture()
    f.map(start, end, flags, expected=1)
f = Fixture()
f.map(4096, 8192, NX | 3)
f.map(4096, 8192, NX | 1, expected=1)
f.map(8192, 12288, NX | 3, expected=1)

# Physical fragmentation fallback reserves single-page records rather than chunks.
f = Fixture(pages=32)
assert f.records[0].pages == 1
f.map(4096, 8192, NX | 3)
assert all(f.records[i].pages == 1 for i in range(f.allocator.count))
# Metadata failure does not fall back or reuse untracked frames.
f = Fixture(record_capacity=1)
for index in range(40):
    status = lib.host_owned_map(C.byref(f.state), 0x1000 + (index << 39), 0x2000 + (index << 39), NX | 3)
    if status:
        assert f.state.failed and f.allocator.count == 1 and f.records[0].pages == 64
        checks += 1
        break
else:
    raise AssertionError("expected metadata exhaustion")

# No promotion across old 4K boundaries, even with identical PAT indices.
for mode in ['constant', 'mixed']:
    f = Fixture()
    for address in range(0x200000, 0x400000, 4096):
        cache = 0 if mode == 'constant' else (address // 4096) % 8
        f.old_leaf(address, 4096, cache)
    f.map(0x200000, 0x400000, NX | 3, preserved=True)
    for address in range(0x200000, 0x400000, 4096):
        pa, entry, size, cache = walk(f.state.root, address)
        expected = 0 if mode == 'constant' else (address // 4096) % 8
        assert pa == address and size == 4096 and cache == expected
        checks += 1
for size, start, end in [(0x200000, 0x200000, 0x400000), (0x40000000, 0x200000, 0x600000)]:
    f = Fixture()
    f.old_leaf(start & -size, size, 7)
    f.map(start, end, NX | 3, preserved=True)
    for address in [start, end - 1]:
        pa, entry, new_size, cache = walk(f.state.root, address)
        assert pa == address and new_size == 0x200000 and cache == 7
f = Fixture()
f.map(0x1000, 0x2000, NX | 3, expected=1, preserved=True)
f = Fixture()
f.old_leaf(0x200000, 0x200000, physical=0x400000)
f.map(0x200000, 0x400000, NX | 3, expected=1, preserved=True)
print(f"PASS: {checks} owned-map checks including inherited cache/leaf preservation and allocation ownership.")
