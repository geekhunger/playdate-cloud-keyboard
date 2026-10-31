"""Exercise the native C implementation against the official Unicode corpus."""
import ctypes
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class UnicodeConformance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        library = Path(cls.temp.name) / ('unicode.dylib' if sys.platform == 'darwin' else 'unicode.so')
        subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-dynamiclib' if sys.platform == 'darwin' else '-shared', '-fPIC',
            str(ROOT/'src/unicode.c'), '-o', str(library)], check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.next = cls.lib.brb_grapheme_next
        cls.next.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        cls.next.restype = ctypes.c_size_t
        cls.case = cls.lib.brb_unicode_case
        cls.case.argtypes = [ctypes.c_char_p, ctypes.c_bool, ctypes.c_void_p, ctypes.c_size_t]
        cls.case.restype = ctypes.c_size_t

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def boundaries(self, raw):
        buffer = ctypes.create_string_buffer(raw)
        result = [0]
        while result[-1] < len(raw):
            offset = result[-1]
            length = self.next(ctypes.addressof(buffer)+offset, len(raw)-offset)
            self.assertGreater(length, 0)
            result.append(offset+length)
        return result

    def test_official_grapheme_breaks(self):
        count = 0
        for number, line in enumerate((ROOT/'tools/data/unicode/GraphemeBreakTest.txt').read_text().splitlines(), 1):
            tokens = line.split('#')[0].split()
            if not tokens: continue
            raw = b''
            expected = []
            for token in tokens:
                if token == '÷': expected.append(len(raw))
                elif token != '×': raw += chr(int(token,16)).encode()
            with self.subTest(line=number): self.assertEqual(self.boundaries(raw), expected)
            count += 1
        self.assertEqual(count, 853)

    def test_official_emoji_sequences_are_atomic(self):
        count = 0
        for line in (ROOT/'tools/data/unicode/emoji-test.txt').read_text().splitlines():
            content = line.split('#')[0].strip()
            if not content: continue
            points, status = content.split(';')
            raw = ''.join(chr(int(cp,16)) for cp in points.split()).encode()
            with self.subTest(sequence=points): self.assertEqual(self.boundaries(raw), [0,len(raw)])
            count += 1
        self.assertGreater(count, 5000)

    def test_full_case_mappings_and_capacity(self):
        for raw, upper, expected in [('élan Ωß', True, 'ÉLAN ΩSS'),
                ('ÉLAN Ω', False, 'élan ω'), ('e\u0301',True,'E\u0301'),
                ('İ',False,'i\u0307'), ('👨‍👩‍👧‍👦🇩🇪👍🏽',True,'👨‍👩‍👧‍👦🇩🇪👍🏽')]:
            output = ctypes.create_string_buffer(161)
            length = self.case(raw.encode(),upper,output,161)
            self.assertEqual(output.value.decode(),expected)
            self.assertEqual(length,len(expected.encode()))
        output=ctypes.create_string_buffer(2)
        self.assertEqual(self.case('ß'.encode(),True,output,2),ctypes.c_size_t(-1).value)
        self.assertEqual(output.value,b'')

    def test_truncated_utf8_is_bounded(self):
        for raw in [b'\xf0', b'\xf0\x9f', b'\xf0\x9f\x91', b'\xed\xa0\x80', b'\xc0\x80', b'\xff']:
            buffer=ctypes.create_string_buffer(raw)
            self.assertEqual(self.next(buffer,len(raw)),0)

if __name__ == '__main__': unittest.main()
