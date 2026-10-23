"""Independent byte-level model for the production Assembly line editor."""
import ctypes as C
import random
import struct
import sys

SIZE = 1568
MAX = (1 << 64) - 1
MAGIC = 0x3152444553484f4a
lib = C.CDLL(sys.argv[1])
call = lib.host_editor_abi
call.argtypes = [C.c_void_p, C.c_uint64, C.c_uint64, C.c_uint64]
call.restype = C.c_uint64
checks = 0


def require(value, message):
    global checks
    checks += 1
    assert value, message


class Model:
    def __init__(self):
        self.line = self.draft = self.submitted = ''
        self.cursor = self.draft_cursor = self.position = 0
        self.history = [''] * 8
        self.count = self.next = 0
        self.submissions = self.rejected = self.cancellations = 0

    def image(self):
        fields = [MAGIC, len(self.line), self.cursor, self.count, self.next,
                  self.position, len(self.draft), self.draft_cursor,
                  len(self.submitted), self.submissions, self.rejected,
                  self.cancellations]
        def buf(s):
            return s.encode('ascii').ljust(128, b'\0')
        return (struct.pack('<12Q', *fields) + buf(self.line) + buf(self.draft)
                + buf(self.submitted) + struct.pack('<8Q', *map(len, self.history))
                + b''.join(map(buf, self.history)))

    def event(self, key):
        if key > 65535:
            return 1
        if key == 13:
            self.submitted = self.line
            self.submissions = min(MAX, self.submissions + 1)
            if self.line:
                self.history[self.next] = self.line
                self.next = (self.next + 1) % 8
                self.count = min(8, self.count + 1)
            self.line = self.draft = ''
            self.cursor = self.draft_cursor = self.position = 0
            return 2 << 32
        if key == 3:
            self.line = self.draft = ''
            self.cursor = self.draft_cursor = self.position = 0
            self.cancellations = min(MAX, self.cancellations + 1)
            return 4 << 32
        if key == 0x105:
            if self.position == self.count:
                return 0
            if not self.position:
                self.draft, self.draft_cursor = self.line, self.cursor
            self.position += 1
            self.line = self.history[(self.next - self.position) % 8]
            self.cursor = len(self.line)
        elif key == 0x106:
            if not self.position:
                return 0
            self.position -= 1
            if self.position:
                self.line = self.history[(self.next - self.position) % 8]
                self.cursor = len(self.line)
            else:
                self.line, self.cursor = self.draft, self.draft_cursor
        elif key in (0x100, 0x101, 0x102, 0x103):
            target = {0x100: max(0, self.cursor-1),
                      0x101: min(len(self.line), self.cursor+1),
                      0x102: 0, 0x103: len(self.line)}[key]
            if target == self.cursor:
                return 0
            self.cursor = target
        elif key == 8 or key == 0x104:
            at = self.cursor - (key == 8)
            if at < 0 or at >= len(self.line):
                return 0
            self.line = self.line[:at] + self.line[at+1:]
            self.cursor = at
            self.position = 0
        elif 32 <= key <= 126:
            if len(self.line) == 127:
                self.rejected = min(MAX, self.rejected + 1)
                return 3 << 32
            self.line = self.line[:self.cursor] + chr(key) + self.line[self.cursor:]
            self.cursor += 1
            self.position = 0
        else:
            return 0
        return 1 << 32


storage = (C.c_uint64 * (SIZE // 8))()
pointer = C.addressof(storage)
model = Model()
require(call(pointer, 0, 0, 0) == 0, 'init ABI')
require(bytes(storage) == model.image(), 'initial state')


def event(key):
    expected = model.event(key)
    actual = call(pointer, key, 0, 2)
    require(actual == expected, ('result', key, actual, expected))
    require(bytes(storage) == model.image(), ('state', key))
    before = bytes(storage)
    require(call(pointer, 0, 0, 1) == 0, 'valid state / ABI')
    require(bytes(storage) == before, 'read-only validation')


# Full insertion, unsuccessful edits, browsing, draft restoration and wraparound.
for key in [0x105, 0x106, 8, 0x104] + [ord('a')]*127 + [ord('b'), 0x102, 0x104, 8, 13]:
    event(key)
for n in range(20):
    for key in map(ord, 'history' + str(n)):
        event(key)
    event(13)
for key in [ord('d'), ord('r'), 0x100, 0x105, 0x105, 0x100, 0x106, 0x106,
            0x105, ord('X'), 0x106, 3, 13]:
    event(key)
# All 16-bit input values, with actual supported events mixed into the campaign.
for key in range(65536):
    event(key)
for key in [65536, MAX]:
    event(key)
rng = random.Random(0x4a4f5348)
keys = list(range(32, 127)) + [3, 8, 13]*12 + list(range(0x100, 0x107))*12
for _ in range(20000):
    event(rng.choice(keys))
# Saturation is independent of normal successful event accounting.
for offset, name, key in [(72, 'submissions', 13), (88, 'cancellations', 3)]:
    setattr(model, name, MAX)
    C.memmove(pointer, model.image(), SIZE)
    event(key)
event(3)
for _ in range(127):
    event(65)
model.rejected = MAX
C.memmove(pointer, model.image(), SIZE)
event(66)

# Each corruption must be rejected without modifying any byte, by both helpers.
valid = Model().image()
mutations = []
for offset, value in [(0, 0), (8, 128), (16, 1), (24, 9), (32, 8),
                      (40, 1), (48, 128), (56, 1), (64, 128), (480, 1)]:
    blob = bytearray(valid)
    struct.pack_into('<Q', blob, offset, value)
    mutations.append(blob)
for offset in [96, 223, 224, 351, 352, 479, 544, 1567]:
    blob = bytearray(valid)
    blob[offset] = 65
    mutations.append(blob)
for blob in mutations:
    C.memmove(pointer, bytes(blob), SIZE)
    for operation in [1, 2]:
        require(call(pointer, 65, 0, operation) == 1, 'corrupt state rejection / ABI')
        require(bytes(storage) == bytes(blob), 'corruption atomicity')
# Address arithmetic rejects before dereference, even for init.
for bad in [0, pointer+1, MAX-7]:
    for operation in range(3):
        require(call(bad, 65, 0, operation) == 1, 'invalid address / ABI')
require(call(pointer, 0, 0, 0) == 0, 'reinitialize corrupt trusted storage')
require(bytes(storage) == valid, 'reinitialization clears complete storage')
# Place the complete extent immediately before and after inaccessible pages.
# This proves bounded accesses for valid caller-provided storage, not arbitrary
# pointer mapping validation.
import mmap
page = mmap.PAGESIZE
arena = mmap.mmap(-1, page * 3)
anchor = C.c_char.from_buffer(arena)
base = C.addressof(anchor)
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
require(libc.mprotect(base, page, 0) == 0, 'leading guard')
require(libc.mprotect(base + page*2, page, 0) == 0, 'trailing guard')
try:
    for location in [base + page, base + page*2 - SIZE]:
        require(call(location, 0, 0, 0) == 0, 'guarded init')
        for key in [65]*127 + [66, 0x102, 0x104, 13, 0x105, 3]:
            require((call(location, key, 0, 2) & 0xffffffff) == 0,
                    'guarded event / ABI')
            require(call(location, 0, 0, 1) == 0, 'guarded validation')
        C.c_uint64.from_address(location + 8).value = MAX
        for operation in [1, 2]:
            require(call(location, 65, 0, operation) == 1,
                    'guarded malformed length rejected before buffer reads')
finally:
    require(libc.mprotect(base, page, 3) == 0, 'restore leading guard')
    require(libc.mprotect(base + page*2, page, 3) == 0, 'restore trailing guard')
    del anchor
    arena.close()
print(f'Console editor: {checks:,} cumulative checks passed')
