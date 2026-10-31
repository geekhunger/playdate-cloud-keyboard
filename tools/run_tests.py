#!/usr/bin/env python3
"""Run the unchanged native keyboard regression tests using a host compiler."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MODEL = ['src/keyboard_model.c', 'src/emoji_set.c', 'src/unicode.c', 'src/input_field.c']
RENDER = MODEL + ['src/unicode_font.c', 'src/pill.c']
# Each tuple is (test name, extra sources, needs SDK headers).
TESTS = [
    ('keyboard_geometry', [], False),
    ('keyboard_crank', [], False),
    ('keyboard_labels', ['src/unicode.c'], False),
    ('keyboard_model', MODEL, False),
    ('input_timing', [], False),
    ('keyboard_cursor_blink', [], False),
    ('input_timing_trace', MODEL, False),
    ('emoji_set', ['src/emoji_set.c', 'src/unicode.c'], False),
    ('input_field', ['src/input_field.c', 'src/unicode.c'], False),
    ('elastic_marker', [], False),
    ('scroll_follow', [], False),
    ('message_motion', [], False),
    ('popup', [], True),
    ('input_timing_keyboard', RENDER, True),
    ('keyboard_art', RENDER, True),
    ('unicode_font', ['src/emoji_set.c', 'src/unicode.c', 'src/unicode_font.c'], True),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--core-only', action='store_true', help='omit SDK-header rendering tests')
    args = parser.parse_args()
    if not args.core_only and (not args.sdk or not (args.sdk / 'C_API/pd_api.h').is_file()):
        parser.error('SDK headers missing: set SDK=/path/to/PlaydateSDK or use make test-core')
    output = ROOT / 'build' / ('tests-sanitize' if args.sanitize else 'tests')
    output.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get('CC', 'clang'))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Isrc']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    count = 0
    for name, sources, sdk in TESTS:
        if sdk and args.core_only:
            continue
        sdk_flags = ['-DTARGET_SIMULATOR=1', '-DTARGET_EXTENSION=1', '-I' + str(args.sdk / 'C_API')] if sdk else []
        binary = output / name
        subprocess.run(compiler + flags + sdk_flags + [f'tests/test_{name}.c'] + sources + ['-lm', '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
        count += 1
    # The official Unicode corpus uses ctypes to load an unsanitized shared
    # library into Python. C regression executables above carry sanitizer flags.
    subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_unicode.py'], cwd=ROOT, check=True)
    print(f'{count} native C regression executables and 4 Unicode corpus tests passed', flush=True)


if __name__ == '__main__':
    main()
