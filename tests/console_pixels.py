"""Visible-display oracle. Font data is shared; raster placement is independent.
Checks grid/display consistency, not independent transcript provenance or GOP padding.
"""
import re
import struct
from pathlib import Path

BG = bytes((2, 6, 12))
FG = bytes((170, 220, 255))


def raster(grid, font_path):
    assert len(grid) == 4904, 'grid extent'
    g = struct.unpack_from('<13Q', grid)
    magic, base, size, width, height, stride, fmt, cols, rows, dirty, cx, cy, enabled = g
    assert magic == 0x3144495247484f4a, 'grid magic'
    assert 608 <= width <= 16384 and 352 <= height <= 16384, 'bounded geometry'
    assert stride >= width and size >= stride * height * 4, 'surface extent'
    assert base and base + size <= 1 << 64 and fmt in (0, 1), 'surface identity'
    assert cols == min(100, (width - 32) // 12) and rows == min(48, (height - 32) // 16), 'grid geometry'
    assert dirty == 0 and enabled in (0, 1) and cx < cols and cy < rows, 'rendered cursor/state'
    font = []
    for line in Path(font_path).read_text().splitlines():
        if line.strip().startswith('db '):
            font.append([int(x) for x in line.split(';')[0].strip()[3:].split(',')])
    assert len(font) == 95 and all(len(r) == 7 and all(0 <= x < 32 for x in r) for r in font), 'font bounds'
    result = bytearray(BG * (width * height))
    for y in range(48):
        for x in range(100):
            char = grid[104 + y * 100 + x]
            if y >= rows or x >= cols:
                assert char == 0, 'inactive grid storage'
                continue
            assert 32 <= char <= 126, 'printable cell'
            glyph = font[char - 32]
            for dy in range(16):
                for dx in range(12):
                    ink = (dy < 14 and dx < 10 and glyph[dy // 2] & (16 >> (dx // 2)))
                    ink = ink or (enabled and (x, y) == (cx, cy) and dy >= 14)
                    if ink:
                        offset = ((16 + y * 16 + dy) * width + 16 + x * 12 + dx) * 3
                        result[offset:offset + 3] = FG
    return width, height, bytes(result)


def validate_pixels(grid, ppm, font_path):
    header = re.match(rb'^P6\n(\d+) (\d+)\n255\n', ppm)
    assert header, 'QEMU P6 header'
    width, height, expected = raster(grid, font_path)
    assert tuple(map(int, header.groups())) == (width, height), 'display geometry'
    assert ppm[header.end():] == expected, 'complete visible framebuffer differs from rendered grid'
    return width, height, expected
