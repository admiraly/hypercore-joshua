"""Independent partition/atomicity oracle for actual Assembly speech segmenter."""
import ctypes as C
import random
import struct
import sys

RESPONSE_MAGIC = 0x3150534552484f4a
TOKEN_MAGIC = 0x314b4f5453484f4a
OUT_SIZE = 16 + 4095 * 8
lib = C.CDLL(sys.argv[1])
call = lib.host_speech_segment_abi
call.argtypes = [C.c_void_p, C.c_void_p, C.c_uint64, C.c_uint64, C.c_uint64]
call.restype = C.c_uint64
checks = 0


def require(condition, message):
    global checks
    checks += 1
    assert condition, message


def expected(body):
    entries = []
    i = 0
    while i < len(body):
        start = i
        byte = body[i]
        i += 1
        if 65 <= byte <= 90 or 97 <= byte <= 122:
            while i < len(body) and (65 <= body[i] <= 90 or 97 <= body[i] <= 122):
                i += 1
            kind = 1
        elif 48 <= byte <= 57:
            kind = 2
        elif byte in (10, 32):
            kind = 4
        else:
            kind = 3
        entries.append((start, i-start, kind))
    data = struct.pack('<QQ', TOKEN_MAGIC, len(entries))
    data += b''.join(struct.pack('<HHI', *entry) for entry in entries)
    return data.ljust(OUT_SIZE, b'\0')


def trial(body):
    source = (C.c_uint64 * 516)()
    output = (C.c_uint64 * (OUT_SIZE // 8 + 2))()
    C.memset(C.addressof(source), 0xa5, C.sizeof(source))
    C.memset(C.addressof(output), 0x5a, C.sizeof(output))
    src = C.addressof(source) + 8
    dst = C.addressof(output) + 8
    C.memmove(src, struct.pack('<QQ', RESPONSE_MAGIC, len(body)) + body.ljust(4096, b'\0'), 4112)
    before = bytes(source)
    require(call(src, dst, 0, 0, 0) == 0, 'success / register ABI')
    require(bytes(source) == before, 'source mutation')
    require(C.string_at(dst, OUT_SIZE) == expected(body), ('partition', body[:80]))
    require(output[0] == output[-1] == 0x5a5a5a5a5a5a5a5a, 'output guards')


for b in [b'', b'Hello, Joshua!\n', b'STATUS\nfree_pages=123\n', b'{"status":0}\n',
          bytes(range(32, 127)) + b'\n', b'A'*4095, b'1'*4095]:
    trial(b)
rng = random.Random(0x4a4f5348)
alphabet = bytes([10] + list(range(32, 127)))
for _ in range(500):
    trial(bytes(rng.choice(alphabet) for _ in range(rng.randrange(0, 4096))))

source = (C.c_uint64 * 514)()
output = (C.c_uint64 * (OUT_SIZE // 8))()
src = C.addressof(source)
dst = C.addressof(output)

def reject(srcptr, dstptr, label):
    before = bytes(source), bytes(output)
    require(call(srcptr, dstptr, 0, 0, 0) == 1, label)
    require((bytes(source), bytes(output)) == before, label + ' atomicity')

C.memmove(src, struct.pack('<QQ', RESPONSE_MAGIC, 1) + b'A' + bytes(4095), 4112)
for a,b,label in [(0,dst,'null source'),(src,0,'null output'),(src+1,dst,'unaligned source'),
                  (src,dst+1,'unaligned output'),(src,src,'identical overlap'),
                  (src,src+8,'partial overlap'),(2**64-8,dst,'source wrap'),
                  (src,2**64-8,'output wrap')]:
    reject(a,b,label)
# Output starting before source must be rejected before any record write.
arena = (C.c_uint64 * ((OUT_SIZE + 4112 + 16)//8))()
base = C.addressof(arena)
C.memset(base, 0x5a, C.sizeof(arena))
C.memmove(base+8, struct.pack('<QQ', RESPONSE_MAGIC, 1) + b'A' + bytes(4095), 4112)
before = bytes(arena)
require(call(base+8, base, 0, 0, 0) == 1, 'reverse overlap')
require(bytes(arena) == before, 'reverse overlap atomicity')
# Half-open adjacent extents are valid in either order.
for src_offset,dst_offset in [(0,4112), (OUT_SIZE,0)]:
    C.memset(base, 0x5a, C.sizeof(arena))
    payload = struct.pack('<QQ', RESPONSE_MAGIC, 1) + b'A' + bytes(4095)
    C.memmove(base+src_offset, payload, len(payload))
    require(call(base+src_offset, base+dst_offset, 0, 0, 0) == 0, 'adjacent extents')
    require(C.string_at(base+src_offset, 4112) == payload, 'adjacent source unchanged')
    require(C.string_at(base+dst_offset, OUT_SIZE) == expected(b'A'), 'adjacent output')

for offset,value,label in [(0,0,'bad magic'),(8,4096,'length'),(16,1,'bad character'),(17,1,'dirty tail')]:
    original=C.c_uint64.from_address(src+offset).value if offset in (0,8) else C.c_ubyte.from_address(src+offset).value
    field=C.c_uint64.from_address(src+offset) if offset in (0,8) else C.c_ubyte.from_address(src+offset)
    field.value=value
    reject(src,dst,label)
    field.value=original
print(f'PASS: {checks} speech segmentation, register ABI, bounds and atomicity checks.')
