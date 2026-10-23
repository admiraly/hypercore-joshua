"""Independent MTRR interval/PAT oracle for exact Assembly UC proof; host only."""
import ctypes as C
import itertools
import random
import struct
import sys

U64=C.c_uint64
lib=C.CDLL(sys.argv[1])
class Context(C.Structure):
    _fields_=[(n,U64) for n in ['ready','bits','raw','pat']]
lib.host_cache_uc.argtypes=[C.POINTER(Context),U64,U64]
MAGIC=0x314548434143484a
checks=0

def oracle(bits, page, pat, default, enabled, pairs, wc=True):
    if not 32<=bits<=52 or page<0x100000 or page%4096 or page+4096>1<<bits:return 1
    if pat not in [0,4,5,6,7]:return 1
    if not enabled:return 0
    valid={0,4,5,6}|({1} if wc else set())
    if default not in valid:return 1
    found=set()
    for base,size,kind,active in pairs:
        if not active:continue
        if kind not in valid or size<4096 or size&(size-1) or base%size or base+size>1<<bits:return 1
        if base<=page<base+size:found.add(kind)
    if not found:resolved=default
    elif 0 in found:resolved=0
    elif len(found)==1:resolved=next(iter(found))
    elif found=={4,6}:resolved=4
    else:return 1
    return 0 if resolved==0 or pat==0 else 1

def fixture(bits=36,page=0xfee00000,pat=6,default=0,enabled=True,pairs=(),wc=True,index=0):
    raw=(C.c_ubyte*4112)(*([0xCC]*4112))
    C.memset(raw,0,4096)
    struct.pack_into('<QQ',raw,0,len(pairs)|(0x400 if wc else 0),default|(0x800 if enabled else 0))
    for i,(base,size,kind,active) in enumerate(pairs):
        mask=((1<<bits)-size)&~4095
        struct.pack_into('<QQ',raw,16+i*16,base|kind,mask|(0x800 if active else 0))
    ctx=Context(MAGIC,bits,C.addressof(raw),pat<<(index*8))
    return raw,ctx

def run(expected=None,**kw):
    global checks
    raw,ctx=fixture(**kw)
    before=bytes(raw),bytes(ctx)
    status=lib.host_cache_uc(C.byref(ctx),kw.get('page',0xfee00000),kw.get('index',0))
    if expected is None:expected=oracle(kw.get('bits',36),kw.get('page',0xfee00000),kw.get('pat',6),kw.get('default',0),kw.get('enabled',True),kw.get('pairs',()),kw.get('wc',True))
    assert status==expected,(status,expected,kw)
    assert (bytes(raw),bytes(ctx))==before
    checks+=1

for pat in range(8):
    for default in [0,1,4,5,6,7]:
        for enabled in [False,True]:run(pat=pat,default=default,enabled=enabled)
# Every overlap order, especially UC mixed with otherwise undefined types.
for types in [(0,1,4,5,6),(4,6),(1,5),(0,1),(6,6),(0,0,0)]:
    for order in set(itertools.permutations(types)):
        run(pairs=[(0xfe000000,0x2000000,t,True) for t in order])
# Exact maximum raw snapshot size, last active pair and untouched outside canary.
pairs=[(0,4096,0,False)]*254+[(0,1<<36,0,True)]
run(pairs=pairs,default=6)
for index in range(8):run(index=index,pat=0,default=6)
for bits in [32,36,40,48,52]:
    run(bits=bits,pairs=[(0,1<<bits,0,True)])
    for page in [0xfffff,0x100001,(1<<bits)-4096,1<<bits]:run(bits=bits,page=page)
# Low-level malformed encodings not representable as valid interval fixtures.
for mutation in ['mask_hole','base_alignment','base_high','mask_high','mask_reserved','base_reserved','bad_type','def_reserved','not_ready','bad_width','bad_index']:
    raw,ctx=fixture(pairs=[(0xfe000000,0x2000000,0,True)])
    if mutation=='mask_hole':U64.from_buffer(raw,24).value^=1<<30
    if mutation=='base_alignment':U64.from_buffer(raw,16).value|=4096
    if mutation=='base_high':U64.from_buffer(raw,16).value|=1<<40
    if mutation=='mask_high':U64.from_buffer(raw,24).value|=1<<40
    if mutation=='mask_reserved':U64.from_buffer(raw,24).value|=1
    if mutation=='base_reserved':U64.from_buffer(raw,16).value|=0x100
    if mutation=='bad_type':U64.from_buffer(raw,16).value|=3
    if mutation=='def_reserved':U64.from_buffer(raw,8).value|=0x100
    if mutation=='not_ready':ctx.ready=0
    if mutation=='bad_width':ctx.bits=53
    index=8 if mutation=='bad_index' else 0
    assert lib.host_cache_uc(C.byref(ctx),0xfee00000,index)==1,mutation
    checks+=1
# WC support applies to active variable/default types; inactive payload is ignored.
run(wc=False,pairs=[(0xfe000000,0x2000000,1,True)])
run(wc=False,pairs=[(0xfe000000,0x2000000,1,False)])
rng=random.Random(0x55434d545252)
for _ in range(6000):
    bits=rng.choice([32,36,40,48,52])
    page=rng.randrange(256,1<<min(bits-12,20))*4096
    pairs=[]
    for i in range(rng.randrange(8)):
        size=1<<rng.randint(12,min(bits,32))
        base=(page//size)*size if rng.randrange(2) else rng.randrange((1<<bits)//size)*size
        pairs.append((base,size,rng.choice([0,1,4,5,6]),bool(rng.randrange(2))))
    run(bits=bits,page=page,pat=rng.randrange(8),default=rng.choice([0,1,4,5,6]),enabled=bool(rng.randrange(2)),pairs=pairs,wc=bool(rng.randrange(2)),index=rng.randrange(8))
print(f'PASS: {checks} effective-UC oracle cases including 6,000 seeded MTRR/PAT combinations.')
