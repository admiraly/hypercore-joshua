"""Exact Assembly APIC policy and controller staging with host-only register models."""
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


class Descriptor(C.Structure):
    _fields_ = [("type", U64), ("start", U64), ("virtual", U64),
                ("pages", U64), ("attributes", U64)]


class BootInfo(C.Structure):
    _fields_ = [("magic", U64), ("version", C.c_uint32), ("size", C.c_uint32),
                ("framebuffer", U64), ("framebuffer_size", U64),
                ("width", C.c_uint32), ("height", C.c_uint32),
                ("stride", C.c_uint32), ("pixel_format", C.c_uint32),
                ("memory_map", U64), ("memory_map_size", U64),
                ("descriptor_size", U64), ("descriptor_version", C.c_uint32),
                ("reserved", C.c_uint32), ("acpi", U64),
                ("image_base", U64), ("image_size", U64)]


class Apic(C.Structure):
    _fields_ = [(n, U64) for n in ['ready', 'mode', 'lapic', 'ticks', 'eois',
                'returns', 'wait_rsp', 'selftest', 'uc_pages']] + [('staged', U64 * 9)]

class Acpi(C.Structure):
    _fields_ = [(n, U64) for n in ['ready', 'boot', 'builder', 'records', 'capacity',
                'bsp_id', 'count', 'enabled', 'lapic', 'pcat', 'xsdt', 'madt',
                'io_records', 'io_capacity', 'io_count']]

lib.host_apic_get.argtypes = [C.c_uint]
lib.host_apic_get.restype = U64
lib.host_apic_policy.restype = C.c_int
lib.host_apic_stage.argtypes = [U64, C.c_uint]
lib.host_apic_stage.restype = C.c_int

def global_at(index, kind):
    return kind.from_address(lib.host_apic_get(index))

apic = global_at(0, Apic)
acpi = global_at(1, Acpi)
ios = global_at(2, C.c_uint32 * 32)
lapic = global_at(3, C.c_uint32 * 256)
registers = global_at(4, C.c_uint32 * 2048)
event_count = global_at(5, U64)
events = (U64 * (16384 * 4)).from_address(C.addressof(event_count) + 8)
fail_write = global_at(12, U64)
lvts = [0x330, 0x340, 0x350, 0x360, 0x370, 0x2f0]

def reset(mode=1, controllers=1, routes=24, version=0x20):
    for obj in [apic, acpi, ios, lapic, registers, event_count, fail_write]:
        C.memset(C.addressof(obj), 0, C.sizeof(obj))
    apic.mode = mode
    acpi.bsp_id = 3
    acpi.pcat = 1
    acpi.io_count = controllers
    lapic[0x20 // 4] = 3 << 24 if mode == 1 else 3
    lapic[0x30 // 4] = (6 << 16) | 0x14
    for reg in lvts:
        lapic[reg // 4] = 0x50
    for i in range(controllers):
        ios[i * 4] = i
        ios[i * 4 + 1] = 0xfec00000 + i * 4096
        ios[i * 4 + 2] = i * routes
        registers[i * 256] = i << 24
        registers[i * 256 + 1] = ((routes - 1) << 16) | version
        for j in range(routes):
            registers[i * 256 + 0x10 + 2 * j] = 0x50

def log():
    return [tuple(events[i * 4 + j] for j in range(4)) for i in range(event_count.value)]

def run(expected=0, no_writes=False):
    global checks
    assert lib.host_apic_policy() == expected
    assert bool(apic.ready) == (expected == 0)
    if no_writes:
        assert not any(e[0] in [1, 2, 3] for e in log()), log()
    checks += 1

for version in [0x11, 0x20]:
    for mode in [1, 2]:
        for controllers, routes in [(1, 1), (1, 24), (8, 120)]:
            reset(mode, controllers, routes, version)
            run()
            assert lapic[0x320 // 4] == 0x10040
            assert lapic[0x380 // 4] == 0
            assert lapic[0xf0 // 4] == 0x1ff
            assert lapic[0x3e0 // 4] == 3
            assert not any(e[0] == 1 and e[1] == 0xb0 for e in log())
            for i in range(controllers):
                assert ios[i * 4 + 3] == routes
                for j in range(routes):
                    assert registers[i * 256 + 0x10 + 2 * j] == 0x10050

# Every unsupported version must stop before redirection reads or configuration writes.
for version in range(256):
    if version in [0x11, 0x20]:
        continue
    reset(version=version)
    run(3, no_writes=True)
    assert not any(e[0] == 11 and e[1] >= 0x10 for e in log())

for register, value, expected in [(0, 0xf000000, 3), (1, (120 << 16) | 0x20, 3),
                                  (0x10, 0x500, 4), (0x10, 0x300, 4), (0x10, 0x600, 4)]:
    reset()
    registers[register] = value
    run(expected, no_writes=True)
reset(controllers=2)
ios[6] = 23
run(3, no_writes=True)
reset()
ios[2] = 0xfffffff0
run(3, no_writes=True)
for reg, value in [(0x20, 0), (0x30, 0x6000f), (0x30, 0x40014), (0x330, 0x500)]:
    reset()
    lapic[reg // 4] = value
    run(3 if reg in [0x20, 0x30] else 4, no_writes=True)
reset()
fail_write.value = 1
run(4)
for reg in [0x100, 0x170, 0x200, 0x270]:
    reset()
    lapic[reg // 4] = 1
    run(4)

import random
rng = random.Random(28)
for _ in range(1000):
    reset(mode=rng.choice([1, 2]), routes=8)
    original = []
    for j in range(8):
        delivery = rng.choice([0, 1, 2, 4, 7])
        value = rng.randrange(256) | delivery << 8 | rng.choice([0, 0x10000])
        registers[0x10 + 2 * j] = value
        original.append(value)
    for reg in lvts:
        lapic[reg // 4] = rng.choice([0x50, 0x250, 0x450, 0x750, 0x10550])
    local = [lapic[reg // 4] for reg in lvts]
    acpi.pcat = rng.randrange(2)
    run()
    for j, value in enumerate(original):
        expected = value if (value >> 8) & 7 in [2, 4] else value | 0x10000
        assert registers[0x10 + 2 * j] == expected
    for reg, value in zip(lvts, local):
        expected = value if (value >> 8) & 7 in [2, 4] else value | 0x10000
        assert lapic[reg // 4] == expected
    assert sum(e[0] == 3 for e in log()) == acpi.pcat



# Exact staging routine with real host page tables, no privileged activation/MMIO.
boot = global_at(6, BootInfo)
builder = global_at(7, Builder)
allocator = global_at(8, Allocator)
cache = global_at(9, U64 * 4)
raw = global_at(10, U64 * 512)

def stage_fixture():
    f = Fixture()
    reset()
    for obj in [boot, cache, raw]:
        C.memset(C.addressof(obj), 0, C.sizeof(obj))
    f.old_leaf(0xfec00000, 0x200000)
    f.old_leaf(0xfee00000, 0x200000)
    C.memmove(C.addressof(builder), C.addressof(f.state), C.sizeof(builder))
    C.memmove(C.addressof(allocator), C.addressof(f.allocator), C.sizeof(allocator))
    builder.image_base, builder.image_end = 0x100000, 0x200000
    builder.fb_start, builder.fb_end = 0xe0000000, 0xe1000000
    apic.lapic = acpi.lapic = 0xfee00000
    f.descriptors = (Descriptor * 3)()
    f.descriptors[0] = Descriptor(7, f.base, 0, 1024, 0)
    boot.memory_map = C.addressof(f.descriptors)
    boot.memory_map_size = 40
    boot.descriptor_size = 40
    cache[0], cache[1], cache[2], cache[3] = 0x314548434143484a, 48, C.addressof(raw), 0x0007040600070406
    raw[0], raw[1] = 0x400, 0x800  # MTRRs enabled, default UC.
    return f

def stage(address, slot, expected):
    global checks
    before = bytes(apic.staged)
    assert lib.host_apic_stage(address, slot) == expected
    if expected:
        assert bytes(apic.staged) == before
    else:
        assert apic.staged[slot] == address | NX | 3
    checks += 1

f = stage_fixture()
stage(0xfee00000, 0, 0)
stage(0xfec00000, 1, 0)
stage(0xfec00000, 1, 1)  # Duplicate cannot replace a staged entry.
for address, slot in [(0, 0), (0xfee00001, 0), (0xfee00000, 9),
                      (0xfee00000, 1), (0xfec01000, 1), (0xfec00000, 2)]:
    f = stage_fixture()
    stage(address, slot, 1)
for kind in range(16):
    for attributes in [0, 0x1000, 0x2000, 0x20000]:
        f = stage_fixture()
        f.descriptors[1] = Descriptor(kind, 0xfec00000, 0, 1, attributes)
        boot.memory_map_size = 80
        stage(0xfec00000, 1, 0 if kind in [0, 11] and not attributes else 1)
# A permitted descriptor must not hide a later prohibited overlap.
f = stage_fixture()
f.descriptors[1] = Descriptor(11, 0xfec00000, 0, 1, 0)
f.descriptors[2] = Descriptor(7, 0xfec00000, 0, 1, 0)
boot.memory_map_size = 120
stage(0xfec00000, 1, 1)
for field in ['image', 'fb', 'owned', 'cache', 'identity', 'mode', 'limit', 'aperture']:
    f = stage_fixture()
    if field == 'image':
        builder.image_base, builder.image_end = 0xfec00000, 0xfec01000
    elif field == 'fb':
        builder.fb_start, builder.fb_end = 0xfec00000, 0xfec01000
    elif field == 'owned':
        f.records[0] = Record(0xfec00000, 1, 0x50414745)
    elif field == 'cache':
        raw[1] = 0x806  # Default WB with inherited PAT WB is not UC.
    elif field == 'identity':
        f.old_leaf(0xfec00000, 0x200000, physical=0xfee00000)
    elif field == 'mode':
        apic.mode = 2
        stage(0xfee00000, 0, 1)
        continue
    elif field == 'limit':
        builder.limit = 0xfec00000
    elif field == 'aperture':
        ios[1] = 0xfed00000
        stage(0xfed00000, 1, 1)
        continue
    stage(0xfec00000, 1, 1)
print(f'APIC policy and controller staging: {checks} cases passed.')
