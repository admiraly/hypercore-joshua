"""Mutate actual captured display/grid pairs against the production pixel oracle."""
import struct
import sys
from pathlib import Path
from console_pixels import validate_pixels

out=Path(sys.argv[1])
font=Path(__file__).resolve().parents[1]/'kernel/console/font.inc'
grid=(out/'console-iso-halted-grid.bin').read_bytes()
ppm=(out/'console-iso-screen.ppm').read_bytes()
validate_pixels(grid,ppm,font) # a rejection-only oracle must not qualify
checks=1

def reject(g,p):
    global checks
    try:
        validate_pixels(g,p,font)
    except AssertionError:
        checks+=1
    else:
        raise AssertionError('mutation accepted')

for field,value in [(0,0),(3,607),(4,351),(5,0),(6,2),(7,1),(8,1),(9,1),(10,100),(11,48),(12,2)]:
    changed=bytearray(grid)
    struct.pack_into('<Q',changed,field*8,value)
    reject(changed,ppm)
# Printable changes must disagree with actual pixels, not only fail validation.
cols,rows=struct.unpack_from('<2Q',grid,56)
for row,col in [(0,0),(2,0),(rows-2,8),(rows-1,0)]:
    changed=bytearray(grid)
    offset=104+row*100+col
    changed[offset]=ord('X') if changed[offset]!=ord('X') else ord('Y')
    reject(changed,ppm)
changed=bytearray(grid)
struct.pack_into('<Q',changed,96,1) # retired underline reintroduced
reject(changed,ppm)
# Corrupt actual screenshot in margin, transcript, prompt and footer.
header_end=ppm.index(b'\n255\n')+5
width=validate_pixels(grid,ppm,font)[0]
for x,y in [(0,0),(16,48),(16+8*12,16+(rows-2)*16+15),(16,16+(rows-1)*16)]:
    changed=bytearray(ppm)
    changed[header_end+(y*width+x)*3]^=1
    reject(grid,changed)
reject(grid,ppm[:-1])
reject(grid,ppm+b'\0')
reject(grid,ppm.replace(b'P6\n',b'P3\n',1))
print(f'PASS: {checks} captured console pixel baseline/mutation checks')
