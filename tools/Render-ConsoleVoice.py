"""Host-only John-voice clip research. Output is not yet a boot-image input."""
import argparse
import array
import hashlib
import importlib.metadata
import json
from pathlib import Path
import re
import shutil
import string
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
VOICE = ROOT / '.tools' / 'piper-voices'
PINNED = {
    'en_US-john-medium.onnx': '789c6c875726e627ddee93d51d8727859abe9c091c3d141591f4b83c2072e988',
    'en_US-john-medium.onnx.json': 'af60f177b6b550f3d7a302720c0fb89e7f94a82b5dca464775ef63b1c69ba09a',
    'REPOSITORY_CARD.md': '33e93643fce5180886300f9f0070eeeab8af148efb8e84361f8904def80fd9fb',
    'MODEL_CARD': 'a056da65cd70dcbda96fc3420fe8d3442dbf3c8b75f49cdf800a715b42e299d1',
}
PUNCTUATION = dict(zip(string.punctuation, (
    'exclamation mark', 'double quote', 'number sign', 'dollar sign', 'percent',
    'ampersand', 'apostrophe', 'left parenthesis', 'right parenthesis', 'asterisk',
    'plus', 'comma', 'hyphen', 'period', 'slash', 'colon', 'semicolon',
    'less than', 'equals', 'greater than', 'question mark', 'at sign',
    'left bracket', 'backslash', 'right bracket', 'caret', 'underscore',
    'backtick', 'left brace', 'vertical bar', 'right brace', 'tilde')))
DIGITS = ('zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine')
MAX_CLIP_BYTES = 48000 * 4 * 5
MAX_TOTAL_BYTES = 64 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def vocabulary(source):
    words = set()
    data = bytearray()
    label = None

    def flush():
        words.update(word.lower() for word in re.findall(r'[A-Za-z]+', data.decode('ascii')))
        data.clear()

    for line in source.splitlines():
        text = line.split(';', 1)[0].strip()
        if not text:
            continue
        match = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', text)
        if match:
            flush()
            label = match.group(1)
            text = text[match.end():].strip()
            if not text:
                continue
        match = re.fullmatch(r'(?i:db)\s+(.+)', text)
        if match:
            if label is None or label.startswith('response_fields_') or label == 'response_diagnostics':
                raise ValueError('Unexpected byte data region')
            values = [value.strip() for value in match.group(1).split(',')]
            if not all(re.fullmatch(r'[0-9]{1,3}', value) for value in values):
                raise ValueError('Unsupported byte literal syntax')
            data.extend(bytes(int(value) for value in values))
            continue
        # Only the current four field arrays and the diagnostic index are
        # recognized as metadata. Validate their complete expression forms;
        # unknown descriptor labels or textual operands must fail closed.
        field_labels = {'response_fields_status', 'response_fields_mem',
                        'response_fields_ps', 'response_fields_devices'}
        field_pattern = (r'(?i:dd)\s+(response_key_[a-z0-9_]+)-console_response_data\s*,'
                         r'\s*\1_end-\1\s*,\s*CONSOLE_FACTS\.[a-z0-9_]+')
        diagnostic_word = (r'(?i:dd)\s+(response_(?:status|mem|ps|devices)_(?:upper|lower))'
                           r'-console_response_data\s*,\s*\1_end-\1')
        diagnostic_fields = (r'(?i:dd)\s+response_fields_(?:status|mem|ps|devices)'
                             r'-console_response_data\s*,\s*[0-9]+')
        if (label in field_labels and re.fullmatch(field_pattern, text)) or (
                label == 'response_diagnostics' and
                (re.fullmatch(diagnostic_word, text) or re.fullmatch(diagnostic_fields, text))):
            flush()
            continue
        raise ValueError('Unsupported response data syntax: ' + text)
    flush()
    if not words:
        raise ValueError('No console response words found')
    return words


def asset_specs(source):
    result = [('word:' + word, word) for word in sorted(vocabulary(source))]
    result += [('digit:' + str(i), word) for i, word in enumerate(DIGITS)]
    result += [('letter:' + letter, 'letter ' + letter) for letter in string.ascii_lowercase]
    result += [('symbol:' + str(ord(char)), PUNCTUATION[char]) for char in string.punctuation]
    return result


def to_pcm(source):
    if len(source) % 2:
        raise ValueError('Malformed 16-bit source audio')
    samples = array.array('h')
    samples.frombytes(source)
    if sys.byteorder != 'little':
        samples.byteswap()
    if not samples or len(samples) > 22050 * 5:
        raise ValueError('Empty or oversized source clip')
    peak = max(abs(value) for value in samples)
    if not peak:
        raise ValueError('Silent clip')
    result = array.array('h')
    for i in range(len(samples) * 48000 // 22050):
        index, fraction = divmod(i * 22050, 48000)
        value = (samples[index] * (48000 - fraction) + samples[min(index+1, len(samples)-1)] * fraction) // 48000
        value = value * 26000 // peak
        result.extend((value, value))
    while len(result) % 128:
        result.append(0)
    if sys.byteorder != 'little':
        result.byteswap()
    data = result.tobytes()
    if len(data) > MAX_CLIP_BYTES:
        raise ValueError('Oversized PCM clip')
    return data


def render(source, synthesize):
    blob = bytearray()
    clips = []
    for token, text in asset_specs(source):
        data = to_pcm(synthesize(text))
        if len(blob) + len(data) > MAX_TOTAL_BYTES:
            raise ValueError('Voice bank exceeds maximum size')
        clips.append(dict(token=token, text=text, offset=len(blob), bytes=len(data), sha256=sha(data)))
        blob.extend(data)
    return bytes(blob), clips


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', default='console-voice-research',
                        help='New directory name under ignored out/')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name):
        raise SystemExit('Output name must contain only ASCII letters, digits, _ or -.')
    out = ROOT / 'out'
    out.mkdir(exist_ok=True)
    if out.is_symlink():
        raise SystemExit('Redirected output root rejected.')
    target = out / args.name
    if target.exists() or target.is_symlink():
        raise SystemExit('Output already exists.')
    if importlib.metadata.version('piper-tts') != '1.8.0':
        raise SystemExit('Pinned piper-tts==1.8.0 required.')
    for name, expected in PINNED.items():
        if sha((VOICE / name).read_bytes()) != expected:
            raise SystemExit('Voice asset checksum mismatch: ' + name)
    from piper import PiperVoice
    from piper.config import SynthesisConfig
    voice = PiperVoice.load(VOICE / 'en_US-john-medium.onnx')
    config = SynthesisConfig(length_scale=1.05, noise_scale=0, noise_w_scale=0)
    def synthesize(text):
        chunks = list(voice.synthesize(text, syn_config=config))
        if any(chunk.sample_rate != 22050 or chunk.sample_width != 2 or chunk.sample_channels != 1 for chunk in chunks):
            raise ValueError('Unexpected source audio format')
        return b''.join(chunk.audio_int16_bytes for chunk in chunks)
    source_bytes = (ROOT / 'kernel/console/response-data.inc').read_bytes()
    source = source_bytes.decode('ascii')
    data, clips = render(source, synthesize)
    metadata = dict(profile='experimental-host-research-only', piper='1.8.0',
                    voice='en_US-john-medium', model_sha256=PINNED,
                    response_source_sha256=sha(source_bytes),
                    synthesis=dict(vars(config)),
                    pcm_rate=48000,
                    pcm_channels=2, pcm_sample_bits=16, bytes=len(data), sha256=sha(data), clips=clips)
    temporary = Path(tempfile.mkdtemp(prefix='.voice-', dir=out))
    try:
        (temporary / 'voice.pcm').write_bytes(data)
        (temporary / 'voice.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
        shutil.copyfile(VOICE / 'MODEL_CARD', temporary / 'JOHN-MODEL-CARD.md')
        shutil.copyfile(VOICE / 'REPOSITORY_CARD.md', temporary / 'VOICE-REPOSITORY-CARD.md')
        temporary.rename(target)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    print(f'Host research voice bank: {len(clips)} clips, {len(data)} bytes, SHA256 {sha(data)}')


if __name__ == '__main__':
    main()
