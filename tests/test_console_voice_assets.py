"""Host voice-bank encoder tests without a downloaded model or audio device."""
import importlib.util
from pathlib import Path
import struct
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('voice_assets', Path(__file__).resolve().parents[1] / 'tools/Render-ConsoleVoice.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)


class VoiceAssetsTest(unittest.TestCase):
    def test_vocabulary_and_complete_fallback(self):
        source = '; comment\nlabel:\n db 72,69,76,80,32,74,111,115,104,117,97,10\n'
        self.assertEqual(v.vocabulary(source), {'help', 'joshua'})
        specs = dict(v.asset_specs(source))
        self.assertEqual(specs['word:joshua'], 'joshua')
        self.assertEqual(specs['digit:9'], 'nine')
        self.assertEqual(specs['letter:z'], 'letter z')
        self.assertEqual(specs['symbol:34'], 'double quote')
        self.assertEqual(sum(key.startswith('symbol:') for key in specs), 32)
        self.assertEqual(sum(key.startswith('letter:') for key in specs), 26)
        self.assertEqual(v.vocabulary('label: DB\t72,69\n db 76,80\n'), {'help'})
        self.assertEqual(v.vocabulary('first: db 65\nsecond: db 66'), {'a', 'b'})
        metadata = ('response_fields_status: dd response_key_a-console_response_data, '
                    'response_key_a_end-response_key_a, CONSOLE_FACTS.a\n'
                    'response_diagnostics: dd response_status_upper-console_response_data, '
                    'response_status_upper_end-response_status_upper\n'
                    'dd response_fields_status-console_response_data, 4')
        self.assertEqual(v.vocabulary('word: db 65\n'+metadata), {'a'})
        for bad in ('label:\n db missing', 'label: dw 16968', 'label: db 65\n times 2 db 66',
                    'label: dd 65', 'db 65',
                    "response_fields_status: dd 'HELP'",
                    'response_fields_status: dd response_key_a-console_response_data, nonsense',
                    'response_fields_new: dd response_key_a-console_response_data, response_key_a_end-response_key_a, CONSOLE_FACTS.a'):
            with self.assertRaises(ValueError, msg=bad):
                v.vocabulary(bad)

    def test_pcm_format_padding_and_rejection(self):
        pcm = v.to_pcm(struct.pack('<hhh', 100, -100, 50))
        self.assertEqual(len(pcm) % 256, 0)
        samples = struct.unpack('<' + 'h' * (len(pcm)//2), pcm)
        self.assertEqual(samples[0], 26000)
        self.assertEqual(samples[2], 2080)  # independent first interpolation step
        self.assertEqual(samples[0::2], samples[1::2])
        for invalid in (b'', b'\0', b'\0\0', bytes(22050*5*2+2)):
            with self.assertRaises(ValueError):
                v.to_pcm(invalid)

    def test_bank_boundaries_and_determinism(self):
        source = 'x:\n db 65,10\n'
        calls = []
        def fake(text):
            calls.append(text)
            return struct.pack('<hh', len(text), -len(text))
        blob, clips = v.render(source, fake)
        self.assertEqual(len(calls), len(clips))
        self.assertEqual(len(clips), 69)  # one word, ten digits, 26 letters, 32 symbols
        self.assertEqual(clips[0]['offset'], 0)
        for index, clip in enumerate(clips):
            self.assertEqual(clip['offset'] % 256, 0)
            self.assertEqual(clip['bytes'] % 256, 0)
            self.assertEqual(v.sha(blob[clip['offset']:clip['offset']+clip['bytes']]), clip['sha256'])
            if index:
                self.assertEqual(clip['offset'], clips[index-1]['offset'] + clips[index-1]['bytes'])
        self.assertEqual(clips[-1]['offset']+clips[-1]['bytes'], len(blob))
        self.assertEqual((blob, clips), v.render(source, fake))
        with mock.patch.object(v, 'MAX_TOTAL_BYTES', 255):
            with self.assertRaises(ValueError):
                v.render(source, fake)


if __name__ == '__main__':
    unittest.main()
