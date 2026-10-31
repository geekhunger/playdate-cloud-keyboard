#!/usr/bin/env python3
"""Check GCC ARM .su per-function reports (not complete call-chain usage)."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--limit', type=int, default=2048)
    args = parser.parse_args()
    entries = []
    for report in sorted(args.directory.rglob('*.su')):
        for line in report.read_text().splitlines():
            location, size, kind = line.rsplit('\t', 2)
            entries.append((int(size), location, kind))
    if not entries:
        parser.error('no ARM stack reports; run make device first')
    failed = False
    for size, location, kind in sorted(entries, reverse=True):
        if size > args.limit or kind not in ('static', 'dynamic,bounded'):
            print(f'FAIL {size:5d} bytes {kind}: {location}')
            failed = True
    maximum = max(entries)
    print(f'{len(entries)} ARM function frames checked; maximum {maximum[0]} bytes: {maximum[1]} ({maximum[2]})')
    print(f'Per-frame bound: {args.limit} bytes. Nested calls and SDK frames need additional stack headroom.')
    raise SystemExit(1 if failed else 0)


if __name__ == '__main__':
    main()
