"""Host-only fixed announcement renderer. Run inside a piper-tts==1.8.0 venv."""
import argparse
import array
import hashlib
import importlib.metadata
import json
from pathlib import Path
import subprocess
import sys
import urllib.request
import wave

REVISION = '5041a7a49ae5b3a0f30e6837769eb03fdbb231bd'
FILES = {
    'en_US-john-medium.onnx': '789c6c875726e627ddee93d51d8727859abe9c091c3d141591f4b83c2072e988',
    'en_US-john-medium.onnx.json': 'af60f177b6b550f3d7a302720c0fb89e7f94a82b5dca464775ef63b1c69ba09a',
    'REPOSITORY_CARD.md': '33e93643fce5180886300f9f0070eeeab8af148efb8e84361f8904def80fd9fb',
    'MODEL_CARD': 'a056da65cd70dcbda96fc3420fe8d3442dbf3c8b75f49cdf800a715b42e299d1',
}
TEXT = 'Joshua, online. Hyper core boot checkpoint reached.'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-directory', type=Path, default=Path('out/piper'))
    parser.add_argument('--model-directory', type=Path, default=Path('.tools/piper-voices'))
    args = parser.parse_args()
    if importlib.metadata.version('piper-tts') != '1.8.0':
        raise SystemExit('Install pinned piper-tts==1.8.0 in a host venv.')
    args.output_directory.mkdir(parents=True, exist_ok=True)
    args.model_directory.mkdir(parents=True, exist_ok=True)
    for name, expected in FILES.items():
        path = args.model_directory / name
        if not path.exists():
            remote = 'README.md' if name == 'REPOSITORY_CARD.md' else 'en/en_US/john/medium/' + name
            url = f'https://huggingface.co/rhasspy/piper-voices/resolve/{REVISION}/{remote}'
            urllib.request.urlretrieve(url, path)
        if sha(path) != expected:
            raise SystemExit(f'Checksum mismatch: {path}')
    # The voice repository has no separate LICENSE file: its pinned root card
    # declares MIT in YAML. Preserve that declaration plus the per-voice card.
    (args.output_directory / 'VOICE-REPOSITORY-CARD.md').write_bytes(
        (args.model_directory / 'REPOSITORY_CARD.md').read_bytes())
    (args.output_directory / 'JOHN-MODEL-CARD.md').write_bytes(
        (args.model_directory / 'MODEL_CARD').read_bytes())
    raw = args.output_directory / 'piper-source.wav'
    # Zero noise removes inference randomness; the captured PCM is the build input.
    subprocess.run([sys.executable, '-m', 'piper', '-m',
                    str(args.model_directory / 'en_US-john-medium.onnx'),
                    '-f', str(raw), '--length-scale', '1.05',
                    '--noise-scale', '0', '--noise-w-scale', '0'],
                   input=TEXT + '\n', text=True, check=True)
    with wave.open(str(raw), 'rb') as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, 22050):
            raise SystemExit('Unexpected Piper source format.')
        samples = array.array('h', wav.readframes(wav.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    # Deterministic integer linear resampling; duplicate mono into stereo.
    frames = len(samples) * 48000 // 22050
    peak = max(abs(x) for x in samples)
    if not peak or not 120000 <= frames <= 393216:
        raise SystemExit('Silent or out-of-bounds announcement.')
    result = array.array('h')
    for i in range(frames):
        index, fraction = divmod(i * 22050, 48000)
        following = samples[min(index + 1, len(samples) - 1)]
        value = (samples[index] * (48000 - fraction) + following * fraction) // 48000
        value = value * 26000 // peak
        result.extend((value, value))
    # Two HDA BDL entries each require a 128-byte-aligned start.
    while len(result) % 128:
        result.append(0)
    if sys.byteorder != 'little':
        result.byteswap()
    pcm = result.tobytes()
    if len(pcm) > 393216 * 4:
        raise SystemExit('Padded announcement exceeds DMA buffer.')
    target = args.output_directory / 'joshua-announcement.pcm'
    target.write_bytes(pcm)
    preview = args.output_directory / 'joshua-announcement.wav'
    with wave.open(str(preview), 'wb') as wav:
        wav.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        wav.writeframes(pcm)
    metadata = dict(text=TEXT, voice='en_US-john-medium', revision=REVISION,
                    model_sha256=FILES, piper='1.8.0',
                    onnxruntime=importlib.metadata.version('onnxruntime'),
                    noise_scale=0, noise_w_scale=0, length_scale=1.05,
                    pcm_bytes=len(pcm), pcm_sha256=sha(target), wav_sha256=sha(preview))
    (args.output_directory / 'voice.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f'Piper announcement: {len(pcm)/192000:.3f}s; SHA256 {sha(target)}')


if __name__ == '__main__':
    main()
