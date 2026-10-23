"""Host-only oracle tests. Runtime routines are assembled without translation."""
import ctypes as C
import random
import sys

lib = C.CDLL(sys.argv[1])
U64 = C.c_uint64


class Range(C.Structure):
    _fields_ = [("start", U64), ("end", U64)]


class RangeSet(C.Structure):
    _fields_ = [("entries", C.POINTER(Range)), ("count", U64), ("capacity", U64)]


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


lib.host_range_reserve.argtypes = [C.POINTER(RangeSet), U64, U64]
lib.host_range_reserve.restype = C.c_int
lib.host_image_check.argtypes = [C.POINTER(BootInfo), C.c_uint32]
lib.host_image_check.restype = C.c_int
lib.host_candidates_build.argtypes = [C.POINTER(BootInfo), C.POINTER(RangeSet), C.c_uint32]
lib.host_candidates_build.restype = C.c_int
assert C.sizeof(BootInfo) == 104 and BootInfo.image_base.offset == 88


def make_set(ranges, capacity=64):
    array = (Range * capacity)()
    for index, pair in enumerate(ranges):
        array[index] = Range(*pair)
    return array, RangeSet(array, len(ranges), capacity)


def actual(context):
    return sorted((context.entries[i].start, context.entries[i].end)
                  for i in range(context.count))


def subtract(ranges, start, end):
    result = []
    for left, right in ranges:
        if right <= start or left >= end:
            result.append((left, right))
        else:
            if left < start:
                result.append((left, start))
            if right > end:
                result.append((end, right))
    return sorted(result)


# Fixed cases cover deletion/swap, both trims, splits, adjacency and repeated cuts.
checks = 0
for ranges, reservations in [
    ([(0, 0x10000)], [(0, 0x10000)]),
    ([(0, 0x10000)], [(0, 0x3000)]),
    ([(0, 0x10000)], [(0x9000, 0x20000)]),
    ([(0, 0x10000)], [(0x3000, 0x7000), (0x3000, 0x7000)]),
    ([(0x1000, 0x2000), (0x3000, 0x4000)], [(0x2000, 0x3000)]),
    ([(0, 0x1000), (0x3000, 0x5000)], [(0, 0x4000)]),
    ([], [(0, 0x1000)]),
]:
    array, context = make_set(ranges)
    expected = sorted(ranges)
    for start, end in reservations:
        assert lib.host_range_reserve(C.byref(context), start, end) == 0
        expected = subtract(expected, start, end)
        assert actual(context) == expected
        checks += 1

for start, end in [(0, 0), (0x2000, 0x1000), (1, 0x1000), (0, 0x1001)]:
    array, context = make_set([(0, 0x10000)])
    assert lib.host_range_reserve(C.byref(context), start, end) == 1
    checks += 1
array, context = make_set([(0, 0x10000)], capacity=1)
assert lib.host_range_reserve(C.byref(context), 0x3000, 0x7000) == 1
checks += 1
context.count = 2
assert lib.host_range_reserve(C.byref(context), 0, 0x1000) == 1
checks += 1

rng = random.Random(0x4A4F53485541)
for trial in range(200):
    ranges = [(i * 0x20000, i * 0x20000 + 0x10000) for i in range(8)]
    rng.shuffle(ranges)
    array, context = make_set(ranges, capacity=128)
    expected = sorted(ranges)
    for cut in range(32):
        start = rng.randrange(0, 0x110) * 0x1000
        end = start + rng.randrange(1, 32) * 0x1000
        assert lib.host_range_reserve(C.byref(context), start, end) == 0
        expected = subtract(expected, start, end)
        assert actual(context) == expected, (trial, cut)
        checks += 1

# Candidate selection excludes non-WB, runtime and unknown descriptors, plus
# page zero, image and rounded framebuffer. Image spans two adjacent loader types.
descriptors = (Descriptor * 7)(
    Descriptor(7, 0, 0, 0x100, 0xF),
    Descriptor(1, 0x100000, 0, 0x30, 8),
    Descriptor(2, 0x130000, 0, 0xC6, 8),
    Descriptor(7, 0x200000, 0, 0x100, 0x8000000000000008),
    Descriptor(7, 0x300000, 0, 0x100, 0),
    Descriptor(0xFFFFFFFF, 0x400000, 0, 0x100, 8),
    Descriptor(7, 0x500000, 0, 0x100, 8),
)
boot = BootInfo()
boot.memory_map = C.addressof(descriptors)
boot.memory_map_size = C.sizeof(descriptors)
boot.descriptor_size = 40
boot.image_base = 0x100000
boot.image_size = 0xF6000
boot.framebuffer = 0x501123
boot.framebuffer_size = 0x2111
assert lib.host_image_check(C.byref(boot), 32) == 0
array, context = make_set([], capacity=64)
assert lib.host_candidates_build(C.byref(boot), C.byref(context), 32) == 0
assert actual(context) == [(0x1000, 0x100000), (0x500000, 0x501000), (0x504000, 0x600000)]
checks += 2

# Force an image overlap into conventional candidates to exercise reservation
# subtraction independently of the production loader-type coverage gate.
descriptors[0] = Descriptor(7, 0, 0, 0x200, 8)
array, context = make_set([], capacity=64)
assert lib.host_candidates_build(C.byref(boot), C.byref(context), 32) == 0
assert (0x1F6000, 0x200000) in actual(context)
assert not any(left < 0x1F6000 and right > 0x100000 for left, right in actual(context))
checks += 1

# Image coverage must fail on gaps, wrong types, overflow and invalid sizes.
descriptors[0] = Descriptor(7, 0, 0, 0x100, 8)
for base, size in [(0, 0xF7000), (0x100001, 0xF7000), (0x100000, 0xF5000),
                   (0x100000, 0xF6001), (0x100000, 0xF7000),
                   (0xFFFFFFFFFFFFF000, 0xF6000)]:
    boot.image_base, boot.image_size = base, size
    assert lib.host_image_check(C.byref(boot), 32) == 1
    checks += 1
boot.image_base, boot.image_size = 0x100000, 0xF6000
descriptors[2].type = 7
assert lib.host_image_check(C.byref(boot), 32) == 1
checks += 1
boot.framebuffer = 0xFFFFFFFFFFFFF000
array, context = make_set([], capacity=64)
assert lib.host_candidates_build(C.byref(boot), C.byref(context), 32) == 1
assert context.count == 0
checks += 1
print(f"PASS: {checks} reservation/oracle checks, including 6,400 deterministic randomized cuts.")
