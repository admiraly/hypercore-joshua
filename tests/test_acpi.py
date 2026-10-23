"""Independent table decoding for the exact Assembly builder; host only."""
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


for cache in range(8):
    f = Fixture()
    flags = NX | 3 | ((cache & 3) << 3) | ((cache >> 2) << 7)
    f.map(0x1000, 0x5000, flags)
    for address in range(0x1000, 0x5000, 4096):
        pa, entry, size, index = walk(f.state.root, address)
        assert pa == address and size == 4096 and index == cache
        assert entry & (NX | 7) == NX | 3
    assert walk(f.state.root, 0) is None
    f.map(0x200000, 0x600000, flags)
    for address in [0x200000, 0x201234, 0x5FFFFF]:
        pa, entry, size, index = walk(f.state.root, address)
        assert pa == address and size == 0x200000 and index == cache
        assert entry & (NX | 7) == NX | 3

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

import random
import struct

class AcpiContext(C.Structure):
    _fields_ = [(name,U64) for name in ['ready','boot','builder','records','capacity','bsp_id',
                                      'count','enabled','lapic','pcat','xsdt','madt']]
class Cpu(C.Structure):
    _fields_ = [(name,C.c_uint32) for name in ['apic','uid','flags','kind']]
lib.host_acpi_build.argtypes = [C.POINTER(AcpiContext)]
lib.host_acpi_readable.argtypes = [C.POINTER(AcpiContext),U64,U64]
assert C.sizeof(AcpiContext) == 96 and C.sizeof(Cpu) == 16

def cpu(apic=0,uid=0,flags=1,x2=False,tail=b''):
    if x2: return struct.pack('<BBHIII',9,16+len(tail),0,apic,flags,uid)+tail
    return struct.pack('<BBBBI',0,8+len(tail),uid,apic,flags)+tail

def override(base=0xfee00000): return struct.pack('<BBHQ',5,12,0,base)
def sdt(signature,payload):
    result = bytearray(36)+payload
    result[:4] = signature
    struct.pack_into('<I',result,4,len(result))
    result[8] = 1
    result[9] = -sum(result) & 255
    return result

class AcpiFixture(Fixture):
    def __init__(self):
        super().__init__()
        self.acpi_memory = mmap.mmap(-1,0x20000)
        self.acpi_base = C.addressof(C.c_char.from_buffer(self.acpi_memory))
        self.map(self.acpi_base,self.acpi_base+0x20000,NX|1)
        self.descriptors = (Descriptor*2)(Descriptor(9,self.acpi_base,0,32,0),Descriptor())
        self.boot = BootInfo()
        self.boot.memory_map = C.addressof(self.descriptors)
        self.boot.memory_map_size = 40
        self.boot.descriptor_size = 40
        self.boot.acpi = self.acpi_base+0x100
        self.cpus = (Cpu*256)()
        self.ctx = AcpiContext(0,C.addressof(self.boot),C.addressof(self.state),C.addressof(self.cpus),256,0)
        self.xsdt = self.acpi_base+0x2000
        self.madt = self.acpi_base+0x4000
        self.other = self.acpi_base+0x3000
        self.tables()
    def put(self,address,data): C.memmove(address,bytes(data),len(data))
    def tables(self,records=None,children=None,rsdp_length=36,base=0xfee00000):
        if records is None: records=cpu()+cpu(1,1)
        madt = sdt(b'APIC',struct.pack('<II',base,1)+records)
        self.put(self.madt,madt)
        if children is None: children=[self.other,self.madt]
        # Unrelated payload/length is deliberately untrusted and not consumed.
        self.put(self.other,sdt(b'TEST',b''))
        self.put(self.xsdt,sdt(b'XSDT',b''.join(struct.pack('<Q',p) for p in children)))
        rsdp = bytearray(rsdp_length)
        rsdp[:8] = b'RSD PTR '
        rsdp[15] = 2
        struct.pack_into('<IQ',rsdp,20,rsdp_length,self.xsdt)
        rsdp[8] = -sum(rsdp[:20]) & 255
        rsdp[32] = -sum(rsdp) & 255
        self.put(self.boot.acpi,rsdp)
    def build(self,expected=0):
        global checks
        before=bytes(self.acpi_memory)
        metadata=bytes(self.descriptors)
        status=lib.host_acpi_build(C.byref(self.ctx))
        assert status==expected,(status,expected)
        assert bytes(self.acpi_memory)==before and bytes(self.descriptors)==metadata
        if status: assert self.ctx.ready==0 and self.ctx.count==0 and self.ctx.enabled==0
        else:
            assert self.ctx.ready==0x314950434153484a and self.ctx.xsdt==self.xsdt and self.ctx.madt==self.madt
            assert sum(bool(self.cpus[i].flags & 1) for i in range(self.ctx.count))==self.ctx.enabled
        checks+=1
    def readable(self,address,length,expected):
        global checks
        assert lib.host_acpi_readable(C.byref(self.ctx),address,length)==expected
        checks+=1

f=AcpiFixture()
f.build()
assert [(r.apic,r.uid,r.flags,r.kind) for r in f.cpus[:f.ctx.count]]==[(0,0,1,0),(1,1,1,0)]
f.tables(cpu(tail=b'future')+cpu(0x1234,0x2345,2,True)+bytes([200,4,1,2])+override(0x100001000),rsdp_length=4096)
f.build()
assert f.ctx.lapic==0x100001000 and f.ctx.count==2 and f.ctx.enabled==1
# Every low-level read rejection must occur without table dereference.
for address,length in [(0,36),(2**64-16,36),(1<<47,36),(f.acpi_base,0),(f.acpi_base,65537),
                       (f.acpi_base+0x1ffff,2),(0xdeadc000,36)]: f.readable(address,length,1)
f.readable(f.acpi_base+4095,2,0)
f.descriptors[0].pages=1
f.descriptors[1]=Descriptor(10,f.acpi_base+4096,0,31,0)
f.boot.memory_map_size=80
f.readable(f.acpi_base+4095,2,0)
f.descriptors[1].attributes=0x2000
f.readable(f.acpi_base+4095,2,1)
f.descriptors[1].attributes=0
f.descriptors[1].type=7
f.readable(f.acpi_base+4095,2,1)
# Layout, signature and independent checksum failures clear a previous ready state.
for offset,value,status in [(0,0,2),(15,1,2),(8,1,3),(32,1,3)]:
    f=AcpiFixture();f.build();C.c_ubyte.from_address(f.boot.acpi+offset).value = value
    f.build(status)
for length in [0,35,4097,2**32-1]:
    f=AcpiFixture();C.c_uint32.from_address(f.boot.acpi+20).value=length;f.build(4)
f=AcpiFixture();U64.from_address(f.boot.acpi+24).value=0
# Repair RSDP extended checksum after changing XSDT pointer.
C.c_ubyte.from_address(f.boot.acpi+32).value=0
C.c_ubyte.from_address(f.boot.acpi+32).value=-sum(C.string_at(f.boot.acpi,36))&255
f.build(1)
for records,status in [(b'\x00',4),(b'\x00\x00',4),(b'\x00\x01',4),(b'\x00\x09'+bytes(6),4),
                       (b'\x09\x02',4),(b'\x05\x02',4),(cpu(flags=0),5),(cpu(flags=2),5),
                       (cpu(1,1),5),(cpu()+cpu(0,1),5),(cpu()+cpu(1,0),5),
                       (cpu()+override()+override(),5),(cpu()+override(0),5),
                       (cpu()+override(0xfee00001),5),(cpu()+override(1<<47),5)]:
    f=AcpiFixture();f.tables(records);f.build(status)
f=AcpiFixture();f.tables(cpu()+cpu(0,0,0));f.build() # inactive IDs grant no authority
for children,status in [([],5),([0],1),([0xdeadc000],1)]:
    f=AcpiFixture();f.tables(children=children);f.build(status)
f=AcpiFixture();f.tables(children=[f.madt,f.madt]);f.build(5)
f=AcpiFixture();f.tables(children=[f.other]*257);f.build(6)
for capacity,status in [(0,6),(257,6),(1,6)]:
    f=AcpiFixture();f.ctx.capacity=capacity;f.build(status)
f=AcpiFixture();f.tables(b''.join(cpu(i,i,1,True) for i in range(256)));f.build()
assert f.ctx.count==256
f.tables(b''.join(cpu(i,i,1,True) for i in range(257)));f.build(6)
# 500 independent normalized CPU-list oracles with unknown and extended records.
rng=random.Random(0x41435049)
for _ in range(500):
    f=AcpiFixture()
    model=[(0,0,1,0)]
    records=[cpu()]
    for i in range(1,rng.randint(2,30)):
        flags=rng.randrange(4)
        x2=bool(rng.randrange(2))
        apic=i+256 if x2 else i
        uid=i+1024 if x2 else i
        tail=bytes(rng.randrange(8))
        records.append(cpu(apic,uid,flags,x2,tail))
        if flags&3:model.append((apic,uid,flags,9 if x2 else 0))
    records.append(bytes([200,3,0]))
    rng.shuffle(records)
    f.tables(b''.join(records));f.build()
    assert sorted((r.apic,r.uid,r.flags,r.kind) for r in f.cpus[:f.ctx.count])==sorted(model)
# Explicit signature/layout/checksum gates for both description-table layers.
for target,offset,value,status in [('xsdt',0,0,2),('xsdt',9,1,3),('madt',9,1,3)]:
    f=AcpiFixture();C.c_ubyte.from_address(getattr(f,target)+offset).value ^= value if offset==9 else 0xFF
    f.build(status)
for target,length,status in [('xsdt',35,4),('xsdt',37,4),('xsdt',65537,4),('madt',43,4),('madt',65537,4)]:
    f=AcpiFixture();C.c_uint32.from_address(getattr(f,target)+4).value=length;f.build(status)
# Prove rejection before dereferencing genuinely nonexistent table storage.
f=AcpiFixture();f.boot.acpi=0xdeadc100
f.descriptors[0]=Descriptor(9,0xdeadc000,0,1,0)
f.build(1)
# Wrong identity and U/S leaf attributes cannot authorize firmware reads.
for mutation in ['address','user']:
    f=AcpiFixture();address=f.boot.acpi
    table=f.state.root
    for shift in [39,30,21]: table=read(table+((address>>shift)&511)*8)&MASK
    entry=U64.from_address(table+((address>>12)&511)*8)
    entry.value ^= 4096 if mutation=='address' else 4
    f.build(1)
# Full 64-KiB MADT remains bounded, spanning many pages with unknown records.
f=AcpiFixture();f.tables(cpu()+bytes([200,2])*32742);f.build()
assert C.c_uint32.from_address(f.madt+4).value==65536
# Reserved CPU flag bits are retained without granting enabled authority.
f=AcpiFixture();f.tables(cpu(flags=0x80000001)+cpu(1,1,0x80000000));f.build()
assert f.ctx.count==1 and f.cpus[0].flags==0x80000001
# Every firmware integer nonvolatile register survives parser and reader calls.
f=AcpiFixture()
lib.host_acpi_abi.argtypes=[C.POINTER(AcpiContext)]
assert lib.host_acpi_abi(C.byref(f.ctx))==0
checks+=1
print(f'PASS: {checks} ACPI readability/topology cases, including 500 seeded CPU-list oracles.')
