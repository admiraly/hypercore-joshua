"""Actual Assembly grid, with an independent byte-addressed pixel oracle."""
import ctypes as C
from pathlib import Path
import random
import struct
import sys
import zlib

MAX = (1 << 64)-1
SIZE = 4904
MAGIC = 0x3144495247484f4a
lib = C.CDLL(sys.argv[1])
call = lib.host_grid_abi
call.argtypes = [C.c_void_p]*2 + [C.c_uint64]*3
call.restype = C.c_uint64
checks = 0


def require(ok, msg):
    global checks
    checks += 1
    assert ok, msg


# Font is an authored data asset. Pixel placement/color/address expectations below
# are independent of the Assembly algorithm; selected literal rows check indexing.
font = []
for line in Path('kernel/console/font.inc').read_text().splitlines():
    if line.strip().startswith('db '):
        font.append([int(x) for x in line.split(';')[0].strip()[3:].split(',')])
require(len(font) == 95 and all(len(r) == 7 and max(r) < 32 for r in font), 'font bounds')
require(font[65-32] == [14,17,17,31,17,17,17], 'A raster')
require(font[126-32] == [0,0,8,21,2,0,0], 'last raster')


class Fixture:
    def __init__(self, width, height, stride, fmt):
        self.w, self.h, self.stride, self.fmt = width, height, stride, fmt
        self.n = stride*height*4
        self.memory = (C.c_ubyte*(self.n+128))()
        C.memset(C.addressof(self.memory), 0x5a, self.n+128)
        self.base = C.addressof(self.memory)+64
        self.cap = (C.c_uint64*6)(self.base, self.n, width, height, stride, fmt)
        self.state = (C.c_uint64*(SIZE//8))()
        self.ptr = C.addressof(self.state)
        self.cols = min(100, (width-32)//12)
        self.rows = min(48, (height-32)//16)
        self.cells = bytearray(4800)
        for y in range(self.rows):
            self.cells[y*100:y*100+self.cols] = b' '*self.cols
        self.dirty = (1 << self.rows)-1
        self.cursor = (0,0,0)
        self.pixels = bytearray(b'\x5a'*self.n)
        require(self.invoke(0) == 0, 'init / ABI')
        self.check()

    def invoke(self, op, index=0, value=0):
        return call(self.ptr, C.addressof(self.cap), index, value, op)

    def image(self):
        return struct.pack('<13Q', MAGIC, *self.cap, self.cols, self.rows,
                           self.dirty, *self.cursor) + self.cells

    def check(self):
        require(bytes(self.state) == self.image(), 'complete grid model')
        before = bytes(self.state)
        require(self.invoke(1) == 0, 'check / ABI')
        require(bytes(self.state) == before, 'read-only check')
        require(bytes(self.memory[:64]) == b'\x5a'*64 and
                bytes(self.memory[-64:]) == b'\x5a'*64, 'surface guards')

    def cell(self, x, y, char):
        require(self.invoke(2, y*100+x, char) == 0, 'cell / ABI')
        index = y*100+x
        if self.cells[index] != char:
            self.cells[index] = char
            self.dirty |= 1 << y
        self.check()

    def set_cursor(self, x, y, enabled):
        require(self.invoke(3, y*100+x, enabled) == 0, 'cursor / ABI')
        new = x,y,enabled
        if new != self.cursor:
            self.dirty |= (1 << y) | (1 << self.cursor[1])
            self.cursor = new
        self.check()

    def render(self):
        bg = bytes((2,6,12,0) if self.fmt == 0 else (12,6,2,0))
        fg = bytes((170,220,255,0) if self.fmt == 0 else (255,220,170,0))
        # Explicit independent per-pixel expected output preserves all unpainted
        # margins, stride padding, rows and clean cells in the complete buffer.
        for y in range(self.rows):
            if not self.dirty & (1 << y):
                continue
            for x in range(self.cols):
                glyph = font[self.cells[y*100+x]-32]
                for dy in range(16):
                    for dx in range(12):
                        ink = dy < 14 and dx < 10 and (glyph[dy//2] & (16 >> (dx//2)))
                        ink |= self.cursor == (x,y,1) and dy >= 14
                        offset = ((16+y*16+dy)*self.stride+16+x*12+dx)*4
                        self.pixels[offset:offset+4] = fg if ink else bg
        require(self.invoke(4) == 0, 'render / ABI')
        self.dirty = 0
        self.check()
        require(C.string_at(self.base,self.n) == self.pixels, 'all surface pixels / padding')


rng = random.Random(0x4a4f5348)
for shape in [(608,352,608,0), (641,403,677,1), (1232,800,1240,0), (1280,1024,1300,1)]:
    f = Fixture(*shape)
    # Every printable character, including both font endpoints.
    for i, char in enumerate(range(32,127)):
        f.cell(i % f.cols, i//f.cols, char)
    f.set_cursor(f.cols-1, f.rows-1, 1)
    f.render()
    f.render()  # Clean render must not touch framebuffer.
    for _ in range(50):
        f.cell(rng.randrange(f.cols), rng.randrange(f.rows), rng.randrange(32,127))
    f.set_cursor(0,0,1)
    f.render()
    f.set_cursor(0,0,0)
    f.render()
    baseline = bytes(f.state)
    pixels = bytes(f.memory)
    for op, index, value in [(2,4800,65), (2,MAX,65), (2,0,31), (2,0,127),
                             (2,0,1<<32), (3,0,2), (3,1<<32,1),
                             (2,f.rows*100,65)]:
        require(f.invoke(op,index,value) == 1, 'invalid mutation / ABI')
        require(bytes(f.state) == baseline and bytes(f.memory) == pixels, 'mutation atomicity')
    if f.cols < 100:
        require(f.invoke(2,f.cols,65) == 1, 'inactive column rejection')
    # Every state field and inactive/active text errors, then every entry path.
    mutations = [(0,0), (8,f.base+4), (16,f.n+4), (24,f.w+1), (32,f.h+1),
                 (40,f.stride+1), (48,2), (56,101), (64,49),
                 (72,1<<f.rows), (80,f.cols), (88,f.rows), (96,2)]
    for off, value in mutations:
        blob = bytearray(baseline)
        struct.pack_into('<Q',blob,off,value)
        for op in range(1,5):
            C.memmove(f.ptr,bytes(blob),SIZE)
            require(f.invoke(op,0,65) == 1, ('state corruption',off,op))
            require(bytes(f.state) == blob and bytes(f.memory) == pixels, 'corruption atomicity')
    for index,value in [(0,31),(0,127)]+([(f.cols,65)] if f.cols<100 else []):
        blob = bytearray(baseline)
        blob[104+index] = value
        C.memmove(f.ptr,bytes(blob),SIZE)
        require(f.invoke(4) == 1, 'invalid cell before pixels')
        require(bytes(f.state) == blob and bytes(f.memory) == pixels, 'cell corruption atomicity')
    C.memmove(f.ptr,baseline,SIZE)
    cap = bytes(f.cap)
    # Invalid anchor fields, checked overflow and insufficient total extent.
    for off,value in [(0,0),(0,MAX-3),(8,0),(8,f.n-1),(16,607),(24,351),
                       (32,f.w-1),(32,MAX),(40,2)]:
        C.memmove(C.addressof(f.cap),cap,48)
        f.cap[off//8] = value
        for op in range(5):
            require(f.invoke(op,0,65) == 1, ('anchor rejection',off,value,op))
            require(bytes(f.state) == baseline and bytes(f.memory) == pixels, 'anchor atomicity')
    C.memmove(C.addressof(f.cap),cap,48)
    # Complete framebuffer virtual interval overlap with grid and anchor.
    for target in [f.ptr, C.addressof(f.cap), f.ptr-SIZE, C.addressof(f.cap)-48]:
        f.cap[0] = target
        for op in range(5):
            require(f.invoke(op,0,65) == 1, 'direct surface overlap')
            require(bytes(f.state) == baseline, 'overlap state atomicity')
    C.memmove(C.addressof(f.cap),cap,48)
    for bad in [0,f.ptr+1,MAX-7]:
        for op in range(5):
            require(call(bad,C.addressof(f.cap),0,65,op) == 1, 'invalid grid pointer')
            require(call(f.ptr,bad,0,65,op) == 1, 'invalid anchor pointer')
# Export pixels produced by the actual Assembly renderer for visual inspection.
# This is a host surface, not a boot screenshot or an interactive console.
preview = Fixture(608,352,608,0)
for row,text in enumerate(['JOSHUA / HYPERCORE', 'ASSEMBLY RENDERER / HOST SURFACE',
                           'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz',
                           '0123456789 !@#$%^&*()_+-=[]{}', 'JOSHUA> visible cursor']):
    for col,char in enumerate(text):
        preview.cell(col,row,ord(char))
preview.set_cursor(21,5,1)
preview.render()
raw = C.string_at(preview.base,preview.n)
scanlines = b''.join(b'\0'+b''.join(raw[(y*608+x)*4:(y*608+x)*4+3]
                                  for x in range(608)) for y in range(352))
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
image = (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',608,352,8,2,0,0,0))
         +chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b''))
Path(sys.argv[1]).with_name('console-grid-preview.png').write_bytes(image)
# Direct record overlap must be rejected before any anchor-driven writes.
f = Fixture(608,352,608,0)
before = bytes(f.state)
for op in range(5):
    require(call(f.ptr,f.ptr+8,0,65,op) == 1, 'record overlap')
    require(bytes(f.state) == before, 'record overlap atomicity')
# Real inaccessible boundaries on framebuffer and both metadata extents.
import mmap
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p,C.c_size_t,C.c_int]
page = mmap.PAGESIZE


class Guarded:
    def __init__(self, size, end):
        self.pages = (size+page-1)//page
        self.mapping = mmap.mmap(-1,(self.pages+2)*page)
        self.anchor = C.c_char.from_buffer(self.mapping)
        self.base = C.addressof(self.anchor)
        require(libc.mprotect(self.base,page,0) == 0, 'guard before')
        require(libc.mprotect(self.base+(self.pages+1)*page,page,0) == 0, 'guard after')
        self.ptr = self.base+(self.pages+1)*page-size if end else self.base+page

    def close(self):
        require(libc.mprotect(self.base,page,3) == 0, 'restore before')
        require(libc.mprotect(self.base+(self.pages+1)*page,page,3) == 0, 'restore after')
        del self.anchor
        self.mapping.close()


for end in [False,True]:
    n = 608*352*4
    fb, grid, cap = Guarded(n,end), Guarded(SIZE,end), Guarded(48,end)
    try:
        C.memset(fb.ptr,0x5a,n)
        C.memmove(cap.ptr,struct.pack('<6Q',fb.ptr,n,608,352,608,0),48)
        require(call(grid.ptr,cap.ptr,0,0,0) == 0, 'guarded init / ABI')
        require(call(grid.ptr,cap.ptr,1947,126,2) == 0, 'last guarded cell')
        require(call(grid.ptr,cap.ptr,1947,1,3) == 0, 'last guarded cursor')
        require(call(grid.ptr,cap.ptr,0,0,4) == 0, 'guarded render / ABI')
        require(call(grid.ptr,cap.ptr,0,0,1) == 0, 'guarded check')
        C.c_ubyte.from_address(grid.ptr+SIZE-1).value = 65
        before = C.string_at(fb.ptr,n)
        require(call(grid.ptr,cap.ptr,0,0,4) == 1, 'last inactive byte checked')
        require(C.string_at(fb.ptr,n) == before, 'guarded failure atomicity')
    finally:
        cap.close()
        grid.close()
        fb.close()
print(f'Console grid: {checks:,} cumulative checks passed')
