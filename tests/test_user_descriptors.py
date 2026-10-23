"""Authority and bounds fixtures for exact Assembly GDT audit/extension; host only."""
import ctypes as C
import random
import sys

U64 = C.c_uint64
lib = C.CDLL(sys.argv[1])
lib.host_user_gdt_audit.argtypes = [C.c_void_p, U64]
lib.host_user_gdt_audit.restype = C.c_int
lib.host_user_gdt_prepare.argtypes = [C.c_void_p, U64, C.POINTER(U64)]
lib.host_user_gdt_prepare.restype = C.c_int
checks = 0


def descriptor(access, wide=False):
    # A16-byte system descriptor has upper-base bytes that resemble dangerous
    # descriptor attributes if a walker accidentally treats them as a new entry.
    low = 0x00af00000000ffff | (access << 40)
    if not access & 0x10:
        low = 0x0000000000000067 | (access << 40)
    return [low, 0x00000000fb000123] if wide else [low]


def audit(words, expected=0, size=None):
    global checks
    data = (U64 * 8192)()
    for i, value in enumerate(words):
        data[i] = value
    before = bytes(data)
    length = len(words) * 8 if size is None else size
    assert lib.host_user_gdt_audit(data, length) == expected
    assert bytes(data) == before
    checks += 1
    return data


# Every access-byte combination, including non-present segments/system gates.
for access in range(256):
    if access & 0x10:
        rejected = bool(access & 0x80 and access & 8 and
                        (access & 4 or access & 0x60 == 0x60))
        words = [0] + descriptor(access)
    else:
        kind = access & 15
        wide = kind in [2, 9, 11]
        rejected = not wide or bool(access & 0x80 and access & 0x60 == 0x60)
        words = [0] + descriptor(access, wide)
    audit(words, int(rejected))

for access in [0x82, 0x89, 0x8b, 0x02, 0x09, 0x0b]:
    audit([0] + descriptor(access, True) + descriptor(0x9b))
    audit([0] + descriptor(access), 1)  # Truncated system descriptor.
    words = [0] + descriptor(access, True)
    words[2] |= 1 << 32
    audit(words, 1)
for access in [0xec, 0x8c, 0x0c]:  # No inherited call gate, even if not present.
    audit([0] + descriptor(access, True), 1)
for size in [0, 1, 7, 9, 65537, (1 << 64)-8]:
    audit([0], 1, size)
for ptr, length in [(0, 8), (1, 8), ((1 << 64)-8, 16)]:
    assert lib.host_user_gdt_audit(ptr, length) == 1
    checks += 1

base = [0, 0x00af9b000000ffff, 0x00cf93000000ffff, 0x0000890000000067, 0]
for size in [40, 48, 65504, 65512]:
    data = audit(base + [0] * (size // 8 - len(base)))
    before = bytes(data)
    out = (U64 * 4)()
    assert lib.host_user_gdt_prepare(data, size, out) == 0
    assert list(out) == [size | 3, (size+8) | 3, (size+16) | 3, size+23]
    assert bytes(data)[:size] == before[:size] and bytes(data)[size+24:] == before[size+24:]
    assert list(data)[size//8:size//8+3] == [0x00affb000000ffff, 0x00cff3000000ffff, 0x00cf73000000ffff]
    checks += 1
for size in [0, 32, 39, 41, 65520, 65528, 65536, (1 << 64)-1]:
    data = (U64 * 8192)()
    for i, v in enumerate(base):
        data[i] = v
    before = bytes(data)
    out = (U64 * 4)()
    assert lib.host_user_gdt_prepare(data, size, out) == 1 and bytes(data) == before
    checks += 1
for access in [0xfb, 0x9f, 0xbf, 0xec]:
    data = (U64 * 8192)()
    for i, v in enumerate(base + descriptor(access, access == 0xec)):
        data[i] = v
    before = bytes(data)
    out = (U64 * 4)()
    assert lib.host_user_gdt_prepare(data, 8*(len(base)+len(descriptor(access, access == 0xec))), out) == 1
    assert bytes(data) == before
    checks += 1

rng = random.Random(32)
for _ in range(2000):
    words = [0]
    for _ in range(rng.randrange(1, 40)):
        access = rng.choice([0x93, 0xb3, 0xf3, 0x73, 0x9b, 0xbb, 0x7b, 0x82, 0x89, 0x8b])
        words += descriptor(access, not access & 0x10)
    audit(words)
lib.host_user_gdt_abi.argtypes = [C.c_void_p, U64, U64]
lib.host_user_gdt_abi.restype = C.c_int
for helper in [0, 1]:
    for valid in [False, True]:
        data = (U64 * 8192)()
        for i, value in enumerate(base):
            data[i] = value
        if not valid:
            data[1] = descriptor(0xfb)[0]
        assert lib.host_user_gdt_abi(data, 40, helper) == (0 if valid else 1)
        checks += 1
print(f'PASS: {checks} user GDT authority/extension/ABI fixtures.')
