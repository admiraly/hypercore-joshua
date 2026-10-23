"""Exact Assembly context policy with independently built interrupt frames; host only."""
import ctypes as C
import mmap
import random
import sys

U64 = C.c_uint64
lib = C.CDLL(sys.argv[1])
checks = 0

class Context(C.Structure):
    _fields_ = [(n, U64) for n in ['ready', 'state', 'current', 'ticks', 'eois',
                'restores', 'records', 'text_start', 'text_end', 'code', 'data']]

class Record(C.Structure):
    _fields_ = [(n, U64) for n in ['low', 'high', 'frame', 'ticks', 'progress']]

lib.host_context_check.argtypes = [C.POINTER(Context), C.c_uint, U64]
lib.host_context_check.restype = C.c_int
lib.host_context_select.argtypes = [C.POINTER(Context), U64, C.c_uint, C.POINTER(U64)]
lib.host_context_select.restype = C.c_int
lib.host_context_abi.argtypes = [C.POINTER(Context), U64, C.c_uint]
lib.host_context_abi.restype = C.c_int

class Fixture:
    def __init__(self):
        self.memory = mmap.mmap(-1, 3 * 32768)
        self.base = C.addressof(C.c_char.from_buffer(self.memory))
        self.records = (Record * 3)()
        self.ctx = Context(0x315854434153484a, 0, 0, 0, 0, 0,
                           C.addressof(self.records), 0x1000, 0x7000, 0x28, 0x30)
        for i in range(3):
            low = self.base + i * 32768
            self.records[i] = Record(low, low + 16384, 0, 0, 0)
            self.frame(i)

    def frame(self, index, old_rsp=None, flags=None):
        rec = self.records[index]
        old_rsp = rec.high if old_rsp is None else old_rsp
        ptr = (old_rsp & -16) - 192
        words = (U64 * 24).from_address(ptr)
        for i in range(24):
            words[i] = 0
        for i in range(15):
            words[4+i] = 0x1020304050607080 + i + index * 256
        words[19:] = [0x1234, 0x28, (2 if index == 0 else 0x202) if flags is None else flags, old_rsp, 0x30]
        rec.frame = ptr
        return words

    def snapshot(self):
        return bytes(self.ctx), bytes(self.records), bytes(self.memory)

    def check(self, index, ptr=None, expected=0):
        global checks
        ptr = self.records[index].frame if ptr is None else ptr
        before = self.snapshot()
        assert lib.host_context_check(C.byref(self.ctx), index, ptr) == expected
        assert self.snapshot() == before
        checks += 1

    def select(self, reason, expected=0, frame=None):
        global checks
        index = self.ctx.current
        frame = self.records[index].frame if frame is None else frame
        before = self.snapshot()
        out = U64(0xffff)
        assert lib.host_context_select(C.byref(self.ctx), frame, reason, C.byref(out)) == expected
        if expected:
            assert out.value == 0 and self.snapshot() == before
        else:
            assert out.value == self.records[self.ctx.current].frame
        checks += 1

f = Fixture()
for i in range(3):
    f.check(i)
# Invalid nonexistent frame pointers must be rejected before dereference.
for ptr in [0, 1, 4096, (1 << 64)-1, 0x800000000000, 0xffff800000000000]:
    f.check(1, ptr, 1)
for index in [3, 4, 0xffffffff]:
    before = f.snapshot()
    assert lib.host_context_check(C.byref(f.ctx), index, 1) == 1
    assert f.snapshot() == before
    checks += 1

rng = random.Random(30)
for _ in range(2000):
    f = Fixture()
    index = rng.randrange(3)
    rsp = f.records[index].low + rng.randrange(4500, 16000)
    flags = 2 | (0x200 if index else 0) | (rng.getrandbits(17) & (0x10cd5 if index else 0xcd5))
    f.frame(index, rsp, flags)
    f.check(index)

for index in range(3):
    for field, bad in [(19, 0), (19, 0x7000), (20, 0x2b), (20, 0x10028),
                       (23, 0), (23, 0x10030), (22, 0), (22, 0xffffffffffffffff)]:
        f = Fixture()
        words = (U64 * 24).from_address(f.records[index].frame)
        words[field] = bad
        f.check(index, expected=1)
    for bit in range(64):
        if (1 << bit) & (0x10ed7 if index else 0xed7):
            continue
        f = Fixture()
        words = (U64 * 24).from_address(f.records[index].frame)
        words[21] |= 1 << bit
        f.check(index, expected=1)
    for flags in [0, 0x200 if index == 0 else 2]:
        f = Fixture()
        words = (U64 * 24).from_address(f.records[index].frame)
        words[21] = flags
        f.check(index, expected=1)

for mutation in ['overlap', 'wrap', 'zero', 'unaligned', 'short', 'high', 'text', 'selector']:
    f = Fixture()
    if mutation == 'overlap':
        f.records[2].low = f.records[1].low
    elif mutation == 'wrap':
        f.records[2].high = 0
    elif mutation == 'zero':
        f.records[2].low = 0
    elif mutation == 'unaligned':
        f.records[1].low += 1
    elif mutation == 'short':
        f.records[1].high = f.records[1].low + 4096
    elif mutation == 'high':
        f.records[1].high = 0x800000001000
    elif mutation == 'text':
        f.ctx.text_end = f.ctx.text_start
    elif mutation == 'selector':
        f.ctx.data = f.ctx.code
    f.check(1, expected=1)
    f.select(0, expected=1)

for _ in range(100):
    f = Fixture()
    f.select(0)
    assert f.ctx.state == 1 and f.ctx.current == 1
    for tick in range(64):
        index = 1 + (tick & 1)
        assert f.ctx.current == index
        f.frame(index, f.records[index].low + rng.randrange(4500, 16000),
                0x202 | (rng.getrandbits(17) & (0x10cd5 if index else 0xcd5)))
        f.select(1)
        f.ctx.eois += 1  # Adapter owns EOI; policy requires exactly the prior count.
        assert f.ctx.ticks == tick + 1 and f.ctx.restores == tick + 1
        assert f.ctx.current == (0 if tick == 63 else 3-index)
    assert f.ctx.state == 2 and f.records[1].ticks == f.records[2].ticks == 32
    f.select(1, expected=1)

for field, value in [('ready', 0), ('state', 2), ('current', 3), ('ticks', 1), ('eois', 1), ('restores', 1)]:
    f = Fixture()
    setattr(f.ctx, field, value)
    f.select(0, expected=1, frame=f.records[0].frame)
for field, value in [('state', 0), ('current', 0), ('current', 2), ('ticks', 64), ('eois', 1), ('restores', 1)]:
    f = Fixture()
    f.select(0)
    setattr(f.ctx, field, value)
    f.select(1, expected=1, frame=f.records[1].frame)
f = Fixture()
f.select(0)
f.select(0, expected=1)
f.select(2, expected=1)
for index in [0, 1, 2]:
    f = Fixture()
    f.select(0)
    words = (U64 * 24).from_address(f.records[index].frame)
    words[20] = 0
    # Tick1 reads current1 and next2; bootstrap is read on the final tick only.
    if index == 0:
        for tick in range(63):
            f.select(1)
            f.ctx.eois += 1
    f.select(1, expected=1)

# ABI check on successful launch and timer, plus malformed-frame rejection.
f = Fixture()
assert lib.host_context_abi(C.byref(f.ctx), f.records[0].frame, 0) == 0
assert lib.host_context_abi(C.byref(f.ctx), f.records[1].frame, 1) == 0
f.ctx.eois += 1
assert lib.host_context_abi(C.byref(f.ctx), 1, 1) == 1
checks += 3
print(f'PASS: {checks} kernel-context frame/schedule/ABI checks.')
