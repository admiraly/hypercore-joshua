"""Host-only allocation/ownership oracle against exact shipped Assembly."""
import ctypes as C
import mmap
import random
import sys

PAGE = 4096
U64 = C.c_uint64
lib = C.CDLL(sys.argv[1])
checks = 0


class Range(C.Structure):
    _fields_ = [("start", U64), ("end", U64)]


class RangeSet(C.Structure):
    _fields_ = [("entries", C.POINTER(Range)), ("count", U64), ("capacity", U64)]


class Record(C.Structure):
    _fields_ = [("base", U64), ("pages", U64), ("owner", U64)]


class Allocator(C.Structure):
    _fields_ = [("ready", U64), ("ranges", C.POINTER(RangeSet)),
                ("records", C.POINTER(Record)), ("count", U64), ("capacity", U64),
                ("total_pages", U64), ("free_pages", U64)]


lib.host_frames_init.argtypes = [C.POINTER(Allocator), C.POINTER(RangeSet), C.POINTER(Record), U64]
lib.host_frames_init.restype = C.c_int
lib.host_frame_alloc.argtypes = [C.POINTER(Allocator), U64, U64, U64, C.POINTER(U64)]
lib.host_frame_alloc.restype = C.c_int
lib.host_frame_free.argtypes = [C.POINTER(Allocator), U64, U64, U64]
lib.host_frame_free.restype = C.c_int
lib.host_frame_abi.argtypes = [C.POINTER(Allocator)]
lib.host_frame_abi.restype = C.c_int
assert C.sizeof(Allocator) == 56 and C.sizeof(Record) == 24


class Fixture:
    def __init__(self, intervals=None, range_capacity=128, record_capacity=32):
        self.mapping = mmap.mmap(-1, 128 * PAGE + 0x10000)
        raw_base = C.addressof(C.c_char.from_buffer(self.mapping))
        offset = (-raw_base) % 0x10000
        self.memory = memoryview(self.mapping)[offset:offset + 128 * PAGE]
        self.memory[:] = b'\xA5' * len(self.memory)
        self.base = raw_base + offset
        if intervals is None:
            intervals = [(0, 12), (16, 36), (48, 64)]
        self.array = (Range * range_capacity)()
        self.context = RangeSet(self.array, len(intervals), range_capacity)
        self.records = (Record * record_capacity)()
        self.state = Allocator()
        self.eligible = set()
        for index, (start, end) in enumerate(intervals):
            self.array[index] = Range(self.base + start * PAGE, self.base + end * PAGE)
            self.eligible.update(range(self.base + start * PAGE, self.base + end * PAGE, PAGE))
        self.live = {}
        status = lib.host_frames_init(C.byref(self.state), C.byref(self.context), self.records, record_capacity)
        assert status == 0
        self.verify()

    def snapshot(self):
        return (bytes(self.state), bytes(self.context), bytes(self.array),
                bytes(self.records), bytes(self.memory))

    def verify(self):
        available = set()
        for i in range(self.context.count):
            left, right = self.array[i].start, self.array[i].end
            assert left % PAGE == right % PAGE == 0 and left < right
            interval = set(range(left, right, PAGE))
            assert not available & interval
            available |= interval
        occupied = set()
        for base, (pages, owner, pattern) in self.live.items():
            span = set(range(base, base + pages * PAGE, PAGE))
            assert not occupied & span
            occupied |= span
            offset = base - self.base
            assert self.memory[offset:offset + pages * PAGE] == bytes([pattern]) * (pages * PAGE)
        assert available == self.eligible - occupied
        assert occupied <= self.eligible
        assert self.state.free_pages == len(available)
        assert self.state.total_pages == len(self.eligible)
        assert self.state.count == len(self.live)
        assert self.context.count + self.state.count <= self.context.capacity
        assert {(self.records[i].base, self.records[i].pages, self.records[i].owner)
                for i in range(self.state.count)} == {(base, value[0], value[1]) for base, value in self.live.items()}
        # Reserved gaps must never be written by allocation or free.
        for page in range(self.base, self.base + len(self.memory), PAGE):
            if page not in self.eligible:
                offset = page - self.base
                assert self.memory[offset:offset + PAGE] == b'\xA5' * PAGE

    def expected_status(self, pages, owner, alignment):
        if not pages or pages > 1 << 35 or not owner or not alignment or alignment > 1 << 35 or alignment & (alignment - 1):
            return 1
        if self.state.count == self.state.capacity:
            return 3
        if pages > self.state.free_pages:
            return 2
        metadata_fit = False
        for i in range(self.context.count):
            left, right = self.array[i].start, self.array[i].end
            aligned = (left + alignment * PAGE - 1) & -(alignment * PAGE)
            end = aligned + pages * PAGE
            if end <= right:
                delta = int(aligned > left) + int(end < right) - 1
                if self.context.count + delta + self.state.count + 1 <= self.context.capacity:
                    return 0
                metadata_fit = True
        return 3 if metadata_fit else 2

    def alloc(self, pages, owner, alignment=1):
        global checks
        expected = self.expected_status(pages, owner, alignment)
        before = self.snapshot()
        result = U64(0xFFFFFFFFFFFFFFFF)
        status = lib.host_frame_alloc(C.byref(self.state), pages, owner, alignment, C.byref(result))
        assert status == expected, (status, expected, pages, owner, alignment)
        if status:
            assert result.value == 0 and self.snapshot() == before
        else:
            base = result.value
            assert base % (alignment * PAGE) == 0 and base not in self.live
            assert set(range(base, base + pages * PAGE, PAGE)) <= self.eligible
            offset = base - self.base
            assert self.memory[offset:offset + pages * PAGE] == bytes(pages * PAGE)
            pattern = 1 + (owner % 254)
            self.memory[offset:offset + pages * PAGE] = bytes([pattern]) * (pages * PAGE)
            self.live[base] = (pages, owner, pattern)
        self.verify()
        checks += 1
        return status, result.value

    def free(self, base, pages, owner):
        global checks
        expected = 0 if base in self.live and self.live[base][:2] == (pages, owner) else 1
        before = self.snapshot()
        status = lib.host_frame_free(C.byref(self.state), base, pages, owner)
        assert status == expected
        if status:
            assert self.snapshot() == before
        else:
            del self.live[base]
        self.verify()
        checks += 1
        return status


f = Fixture()
for pages, owner, alignment in [(0, 1, 1), (1, 0, 1), (1, 1, 0), (1, 1, 3),
                                 (1 << 36, 1, 1), (1, 1, 1 << 36), (1, 1, 1 << 35)]:
    f.alloc(pages, owner, alignment)
_, a = f.alloc(1, 1)
_, b = f.alloc(3, 2, 4)
f.free(a, 1, 2)
f.free(a, 2, 1)
f.free(a + 1, 1, 1)
f.free(f.base + 14 * PAGE, 1, 1)
f.free(a, 1, 1)
f.free(a, 1, 1)
f.free(b, 3, 2)
for _ in range(3):
    _, base = f.alloc(1, 3)
    f.free(base, 1, 3)
before = f.snapshot()
assert lib.host_frames_init(C.byref(f.state), C.byref(f.context), f.records, len(f.records)) == 1
assert f.snapshot() == before
checks += 1

# Whole-range allocation can succeed with one slot; partial allocation cannot
# consume the only metadata slot needed to return the allocation later.
f = Fixture([(0, 4)], range_capacity=1, record_capacity=4)
f.alloc(1, 1)
_, base = f.alloc(4, 1)
f.alloc(1, 2)
f.free(base, 4, 1)
f = Fixture([(0, 8)], record_capacity=1)
_, base = f.alloc(1, 1)
f.alloc(1, 2)
f.free(base, 1, 1)
f = Fixture([], record_capacity=4)
f.alloc(1, 1)

# Metadata-limited first fit must skip a partial fit for a later whole fit.
f = Fixture([(0, 8), (16, 17)], range_capacity=2, record_capacity=4)
status, base = f.alloc(1, 1)
assert status == 0 and base == f.base + 16 * PAGE
f.free(base, 1, 1)
f = Fixture([(1, 8)], range_capacity=3, record_capacity=4)
status, base = f.alloc(1, 1, 4)
assert status == 0 and f.context.count == 2
f.free(base, 1, 1)

# Exhaust real pages, then free in a shuffled order and repeat to catch leaks.
f = Fixture([(0, 16)], record_capacity=32)
for generation in range(3):
    allocations = [f.alloc(1, generation + 1)[1] for _ in range(16)]
    f.alloc(1, generation + 1)
    random.Random(generation).shuffle(allocations)
    for base in allocations:
        f.free(base, 1, generation + 1)

# Initialization failures leave metadata untouched; no RAM need be mapped at
# these addresses because initialization validates but never clears frames.
for intervals in [[(0, PAGE)], [(PAGE + 1, 2 * PAGE)], [(PAGE, PAGE)],
                  [(PAGE, 4 * PAGE), (2 * PAGE, 5 * PAGE)],
                  [(0x800000000000, 0x800000001000)]]:
    ranges = (Range * 4)()
    for i, pair in enumerate(intervals):
        ranges[i] = Range(*pair)
    context = RangeSet(ranges, len(intervals), 4)
    records = (Record * 4)()
    state = Allocator()
    snapshot = bytes(state), bytes(context), bytes(ranges), bytes(records)
    assert lib.host_frames_init(C.byref(state), C.byref(context), records, 4) == 1
    assert snapshot == (bytes(state), bytes(context), bytes(ranges), bytes(records))
    checks += 1

for bad_capacity in [0, 4097]:
    ranges = (Range * 4)(Range(PAGE, 2 * PAGE))
    context = RangeSet(ranges, 1, 4)
    records = (Record * 4)()
    state = Allocator()
    before = bytes(state), bytes(context), bytes(ranges), bytes(records)
    assert lib.host_frames_init(C.byref(state), C.byref(context), records, bad_capacity) == 1
    assert before == (bytes(state), bytes(context), bytes(ranges), bytes(records))
    checks += 1
ranges = (Range * 4)(Range(PAGE, 2 * PAGE))
records = (Record * 4)()
for count, capacity in [(5, 4), (0, 0), (1, 8193)]:
    context = RangeSet(ranges, count, capacity)
    state = Allocator()
    before = bytes(state), bytes(context), bytes(ranges), bytes(records)
    assert lib.host_frames_init(C.byref(state), C.byref(context), records, 4) == 1
    assert before == (bytes(state), bytes(context), bytes(ranges), bytes(records))
    checks += 1

f = Fixture()
assert lib.host_frame_abi(C.byref(f.state)) == 0
f.verify()
checks += 1

rng = random.Random(0x4652414D4553)
for trial in range(20):
    f = Fixture(range_capacity=64, record_capacity=16)
    for step in range(300):
        if f.live and rng.randrange(100) < 45:
            base = rng.choice(list(f.live))
            pages, owner, _ = f.live[base]
            if rng.randrange(8) == 0:
                f.free(base, pages, owner + 100)
            else:
                f.free(base, pages, owner)
        else:
            f.alloc(rng.randrange(1, 9), rng.randrange(1, 16), 1 << rng.randrange(4))
    for base, (pages, owner, _) in list(f.live.items()):
        f.free(base, pages, owner)
print(f"PASS: {checks} frame ownership/oracle checks, including 6,000 seeded mixed operations.")
