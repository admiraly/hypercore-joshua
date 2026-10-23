"""Host-only synthetic table fixtures for the shipped bounded paging walker."""
import ctypes as C
import mmap
import random
import sys

U64 = C.c_uint64
PAGE = 4096
NX = 1 << 63
lib = C.CDLL(sys.argv[1])


class Frame(C.Structure):
    _fields_ = [("address", U64), ("level", U64)]


class Descriptor(C.Structure):
    _fields_ = [("type", U64), ("start", U64), ("virtual", U64),
                ("pages", U64), ("attributes", U64)]


class Inventory(C.Structure):
    _fields_ = [("entries", C.POINTER(Frame)), ("count", U64), ("capacity", U64),
                ("memory_map", U64), ("map_size", U64), ("descriptor_size", U64),
                ("physical_bits", U64), ("flags", U64), ("root", U64)]


class Range(C.Structure):
    _fields_ = [("start", U64), ("end", U64)]


class RangeSet(C.Structure):
    _fields_ = [("entries", C.POINTER(Range)), ("count", U64), ("capacity", U64)]


lib.host_paging_build.argtypes = [C.POINTER(Inventory)]
lib.host_paging_build.restype = C.c_int
lib.host_paging_reserve.argtypes = [C.POINTER(Inventory), C.POINTER(RangeSet)]
lib.host_paging_reserve.restype = C.c_int
checks = 0


class Fixture:
    def __init__(self, pages=16):
        self.pages = pages
        self.memory = mmap.mmap(-1, pages * PAGE)
        self.base = C.addressof(C.c_char.from_buffer(self.memory))
        self.words = (U64 * (pages * 512)).from_buffer(self.memory)
        self.descriptor = Descriptor(4, self.base, 0, pages, 0xF)
        self.frames = (Frame * 16384)()
        self.state = Inventory(self.frames, 0, 16384, C.addressof(self.descriptor),
                               C.sizeof(self.descriptor), 40, 48, 3, self.base)

    def address(self, index):
        return self.base + index * PAGE

    def set(self, table, slot, entry):
        self.words[table * 512 + slot] = entry

    def link(self, table, slot, target, flags=3):
        self.set(table, slot, self.address(target) | flags)

    def build(self, expected=None):
        global checks
        snapshot = bytes(self.memory)
        status = lib.host_paging_build(C.byref(self.state))
        assert bytes(self.memory) == snapshot, "walker wrote table contents"
        if expected is None:
            assert status == 1 and self.state.count == 0
        else:
            assert status == 0
            result = {(self.frames[i].address, self.frames[i].level)
                      for i in range(self.state.count)}
            assert self.state.count == len(result) == len(expected)
            assert result == {(self.address(index), level) for index, level in expected}
        checks += 1


f = Fixture()
f.build({(0, 4)})
f.link(0, 0, 1)
f.link(1, 0, 2)
f.link(2, 0, 3)
f.set(3, 0, 0x12345083)  # PT bit 7 is PAT, never a child pointer.
f.build({(0, 4), (1, 3), (2, 2), (3, 1)})
f.link(0, 1, 1)  # Same-level sharing must not duplicate frames.
f.build({(0, 4), (1, 3), (2, 2), (3, 1)})
f.set(0, 2, 0xFFFFFFFFFFFFFFFE)  # Absent entries may contain arbitrary software bits.
f.build({(0, 4), (1, 3), (2, 2), (3, 1)})
f.set(2, 0, 0x201083)  # 2 MiB leaf with PAT bit 12.
f.build({(0, 4), (1, 3), (2, 2)})
f.set(1, 0, 0x40001083)  # 1 GiB leaf with PAT bit 12.
f.build({(0, 4), (1, 3)})
f.state.flags = 1  # Disable modeled 1GiB support.
f.build()
f.state.flags = 3
f.set(1, 0, 0x40002083)  # Reserved large-page address bits.
f.build()
f.link(1, 0, 2)
f.set(2, 0, 0x202083)
f.build()
f.set(2, 0, 0x200083)
f.set(0, 0, f.address(1) | 0x83)  # PML4 PS is not a leaf.
f.build()

for root in [0, 1, 0x800000000000, 0x1000000000000, f.address(0) + 1]:
    test = Fixture()
    test.state.root = root
    test.build()
for width in [31, 53]:
    test = Fixture()
    test.state.physical_bits = width
    test.build()
for capacity in [0, 16385]:
    test = Fixture()
    test.state.capacity = capacity
    test.build()
for kind in [0, 8, 11, 12, 14, 15, 0xFFFFFFFF]:
    test = Fixture()
    test.descriptor.type = kind
    test.build()
for kind in [1, 2, 3, 4, 5, 6, 7, 9, 10]:
    test = Fixture()
    test.descriptor.type = kind
    test.build({(0, 4)})
test = Fixture()
test.descriptor.attributes |= 0x2000  # Read-protected RAM cannot be inspected.
test.build()
test = Fixture()
test.descriptor.start += PAGE  # Root is outside descriptor coverage.
test.descriptor.pages -= 1
test.build()
test = Fixture()
test.link(0, 0, 0)  # Recursive root at a different level.
test.build()
test = Fixture()
test.link(0, 0, 1)
test.link(1, 0, 2)
test.link(2, 0, 1)  # A later cross-level cycle.
test.build()
test = Fixture()
test.link(0, 0, 1)
test.link(1, 0, 2)
test.state.capacity = 2
test.build()
test = Fixture()
test.set(0, 0, test.address(test.pages) | 3)  # Child just beyond classified RAM.
test.build()
test = Fixture()
test.set(0, 0, 1)  # Present pointer to page zero.
test.build()
test = Fixture()
test.set(0, 0, (1 << 48) | 3)
test.build()
test = Fixture()
test.link(0, 0, 1, 3 | NX)
test.state.flags = 2
test.build()
test.state.flags = 3
test.build({(0, 4), (1, 3)})
test.state.flags = 4
test.build()

# Reserve every discovered table frame from an initially conventional pool.
test = Fixture()
test.link(0, 0, 1)
test.link(1, 0, 2)
test.link(2, 0, 3)
test.descriptor.type = 7
test.build({(0, 4), (1, 3), (2, 2), (3, 1)})
ranges = (Range * 64)()
ranges[0] = Range(test.base, test.address(test.pages))
context = RangeSet(ranges, 1, 64)
assert lib.host_paging_reserve(C.byref(test.state), C.byref(context)) == 0
assert context.count == 1
assert (ranges[0].start, ranges[0].end) == (test.address(4), test.address(test.pages))
checks += 1

# A middle-of-range table requires a split; capacity failure clears candidates.
test = Fixture()
test.state.root = test.address(1)
test.build({(1, 4)})
ranges[0] = Range(test.base, test.address(test.pages))
context = RangeSet(ranges, 1, 1)
assert lib.host_paging_reserve(C.byref(test.state), C.byref(context)) == 1
assert context.count == 0
checks += 1

rng = random.Random(0x504147494E47)
for trial in range(100):
    test = Fixture(pages=32)
    levels = {4: [0], 3: [1, 2, 3], 2: [4, 5, 6, 7], 1: [8, 9, 10, 11, 12]}
    edges = {}
    for level in [4, 3, 2]:
        for index in levels[level]:
            children = [rng.choice(levels[level - 1]) for _ in range(rng.randrange(0, 8))]
            edges[index] = children
            for slot, child in enumerate(children):
                test.link(index, slot, child)
    queue = [(0, 4)]
    expected = set(queue)
    for index, level in queue:
        for child in edges.get(index, []):
            pair = (child, level - 1)
            if pair not in expected:
                expected.add(pair)
                queue.append(pair)
    test.build(expected)
    ranges = (Range * 64)()
    ranges[0] = Range(test.base, test.address(test.pages))
    context = RangeSet(ranges, 1, 64)
    assert lib.host_paging_reserve(C.byref(test.state), C.byref(context)) == 0
    available = set()
    for i in range(context.count):
        available.update(range(ranges[i].start, ranges[i].end, PAGE))
    reserved = {test.address(index) for index, _ in expected}
    assert available == set(range(test.base, test.address(test.pages), PAGE)) - reserved
    checks += 1
print(f"PASS: {checks} paging inventory/reservation checks, including 100 seeded shared-table graphs.")
