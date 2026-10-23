"""Verify the rendered announcement is exactly what the EFI image embeds."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-directory', type=Path, default=Path('out'))
parser.add_argument('--voice-directory', type=Path, default=Path('out/piper'))
args = parser.parse_args()
pcm = (args.voice_directory / 'joshua-announcement.pcm').read_bytes()
metadata = json.loads((args.voice_directory / 'voice.json').read_text())
assert 256 <= len(pcm) <= 1572864 and len(pcm) % 256 == 0
assert hashlib.sha256(pcm).hexdigest() == metadata['pcm_sha256']
with wave.open(str(args.voice_directory / 'joshua-announcement.wav'), 'rb') as wav:
    assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 48000)
    assert wav.readframes(wav.getnframes()) == pcm, 'Preview differs from boot clip'
samples = list(struct.iter_unpack('<hh', pcm))
assert all(left == right for left, right in samples)
peak = max(abs(left) for left, _ in samples)
assert 25000 <= peak <= 26000
assert 2.5 <= len(samples) / 48000 <= 8.192
image = (args.output_directory / 'BOOTX64.EFI').read_bytes()
pe = struct.unpack_from('<I', image, 0x3c)[0]
count = struct.unpack_from('<H', image, pe + 6)[0]
optional_size = struct.unpack_from('<H', image, pe + 20)[0]
sections = pe + 24 + optional_size
voice = None
for i in range(count):
    offset = sections + i * 40
    if image[offset:offset+8].rstrip(b'\0') == b'.voice':
        voice = struct.unpack_from('<IIII', image, offset + 8)
        assert struct.unpack_from('<I', image, offset + 36)[0] == 0x40000040
assert voice is not None
size, rva, raw_size, raw = voice
assert (size, rva, raw_size, raw) == (0x181000, 0x34d000, 0x181000, 0x9000)
assert image[raw:raw+len(pcm)] == pcm, 'EFI differs from previewed PCM'
marker = b'JOSHUA:PIPER_PCM\r\n\0'
assert image[raw+len(pcm):raw+len(pcm)+len(marker)] == marker, 'Voice marker missing'
print('PASS: WAV/PCM/provenance/EFI byte equivalence, stereo, duration, peak and voice permissions.')
