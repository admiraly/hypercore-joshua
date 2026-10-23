"""Decode exact Assembly descriptor constructors and nine-guard policy; host only."""
import ctypes as C
import random
import struct
import sys

lib = C.CDLL(sys.argv[1])
U64 = C.c_uint64
lib.host_gdt.argtypes = [C.c_void_p, C.c_void_p, U64, U64, C.POINTER(U64)]
lib.host_tss.argtypes = [C.c_void_p, U64, C.POINTER(U64)]
lib.host_idt.argtypes = [C.c_void_p, U64, U64]
lib.host_guard.argtypes = [C.POINTER(U64), U64, U64, C.POINTER(U64)]
lib.host_guard.restype = U64
checks = 0
source = (C.c_ubyte * 65536)(*([0xA5]*65536))
dest = (C.c_ubyte * 65536)()
result = (U64 * 4)()
rng = random.Random(0x545353)
for size in [0,1,7,9,65505,65512,65535,65536,2**64-1]:
    C.memset(dest,0xCC,65536)
    assert lib.host_gdt(dest,source,size,0,result) == 1
    assert bytes(dest) == bytes([0xCC])*65536
    checks += 1
for size in [8,16,128,65504]+[8*rng.randint(1,8188) for _ in range(100)]:
    C.memset(dest,0xCC,65536)
    base = rng.getrandbits(64)
    assert lib.host_gdt(dest,source,size,base,result) == 0
    assert list(result) == [size,size+8,size+16,size+31]
    data = bytes(dest)
    assert data[:size] == bytes(source[:size])
    assert struct.unpack_from('<QQ',data,size) == (0x00af9b000000ffff,0x00cf93000000ffff)
    t = data[size+16:size+32]
    decoded_base = int.from_bytes(t[2:4],'little') | t[4]<<16 | t[7]<<24 | int.from_bytes(t[8:12],'little')<<32
    assert decoded_base == base and int.from_bytes(t[:2],'little') == 103
    assert t[5:7] == bytes([0x89,0]) and t[12:] == bytes(4)
    assert data[size+32:] == bytes([0xCC])*(65504-size)
    checks += 1
# TSS byte layout, including every reserved byte and disabled I/O map.
for _ in range(100):
    tss = (C.c_ubyte * 104)(*([0xCC]*104))
    rsp0 = rng.getrandbits(64)
    tops = (U64*4)(*[rng.getrandbits(64) for _ in range(4)])
    lib.host_tss(tss,rsp0,tops)
    expected = bytearray(104)
    struct.pack_into('<Q',expected,4,rsp0)
    for i,top in enumerate(tops): struct.pack_into('<Q',expected,36+i*8,top)
    struct.pack_into('<H',expected,102,104)
    assert bytes(tss) == expected
    checks += 1
idt = (C.c_ubyte*4096)()
stubs = 0xffff800012340000
selector = 0xffe0
lib.host_idt(idt,stubs,selector)
for vector in range(256):
    gate = bytes(idt[vector*16:(vector+1)*16])
    target = int.from_bytes(gate[:2],'little') | int.from_bytes(gate[6:8],'little')<<16 | int.from_bytes(gate[8:12],'little')<<32
    assert target == stubs+vector*16 and int.from_bytes(gate[2:4],'little') == selector
    assert gate[4] == {8:1,2:2,18:3}.get(vector,4) and gate[5] == 0x8e and gate[12:] == bytes(4)
    checks += 1
# Independent guard interval model; shuffled starts must not change decisions.
guards = [0x10000,0x18000,0x28000,0x40000,0x60000,0x80000,0xa0000,0xc0000,0xe0000]
for _ in range(1000):
    shuffled = guards[:]
    rng.shuffle(shuffled)
    cursor = rng.choice(guards)+rng.choice([-4096,-1,0,1,4095,4096,8192])
    end = cursor+rng.randint(1,40)*4096
    inside = next((g for g in guards if g <= cursor < g+4096),None)
    expected = inside+4096 if inside is not None else min([end]+[g for g in guards if g>cursor])
    skip = U64()
    boundary = lib.host_guard((U64*9)(*shuffled),cursor,end,C.byref(skip))
    assert boundary == expected and skip.value == (inside is not None)
    checks += 1
# Eight distinct firmware integer nonvolatile sentinels across all four helpers.
tss = (C.c_ubyte*104)()
tops = (U64*4)(0x1000,0x2000,0x3000,0x4000)
g = (U64*9)(*guards)
args = (U64*8)(C.addressof(dest),C.addressof(source),128,C.addressof(tss),
               C.addressof(tops),C.addressof(idt),stubs,C.addressof(g))
lib.host_descriptor_abi.argtypes = [C.POINTER(U64)]
assert lib.host_descriptor_abi(args) == 0
checks += 1
print(f'PASS: {checks} descriptor/TSS/gate/guard fixtures.')
