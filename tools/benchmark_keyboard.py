#!/usr/bin/env python3
"""Replay the current C model and compare explicit, taps-only cursor baselines."""
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'docs/benchmarks'
SOURCES = ['src/keyboard_model.c', 'src/emoji_set.c', 'src/unicode.c', 'src/input_field.c']
CORPUS = json.loads((DEST / 'corpus.json').read_text())
BASELINES = ['Playdate SDK 3.1.2 keyboard', 'bill’s 1.0.1 ABC', 'Wrap+ prototype', 'Circle D-pad prototype']


def grid_position(index):
    if index < 30:
        return (33 + index % 6 * 46, 98 + index // 6 * 18)
    if index < 40:
        return (33 + (6 + (index - 30) % 2) * 46, 98 + (index - 30) // 2 * 18)
    return (60 + (index - 40) * 92, 193)


def grid_neighbour(index, dx, dy):
    if index >= 40 and dx:
        return index + dx if 40 <= index + dx <= 43 else index
    x, y = grid_position(index)
    candidates = []
    for other in range(44):
        cx, cy = grid_position(other)
        along = (cx - x) * dx if dx else (cy - y) * dy
        if along > 0:
            across = abs(cy - y) if dx else abs(cx - x)
            candidates.append((along * 10 + across * 24, other))
    return min(candidates)[1] if candidates else index


def grid_route(start, target):
    queue, visited = deque([(start, [])]), {start}
    while queue:
        current, path = queue.popleft()
        if current == target:
            return path
        for direction in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
            other = grid_neighbour(current, *direction)
            if other not in visited:
                visited.add(other)
                queue.append((other, path + [direction]))
    raise ValueError('Unreachable ABC key')


def abc(text):
    keys = 'abcdefghijklmnopqrstuvwxyz-_,.0123456789'
    cursor, presses, typed = 0, 0, ''
    for char in text:
        target = 41 if char == ' ' else keys.index(char)
        path = grid_route(cursor, target)
        for direction in path:
            cursor = grid_neighbour(cursor, *direction)
        typed += ' ' if cursor == 41 else keys[cursor]
        presses += len(path) + 1
    assert typed == text
    return presses


def wrap(text):
    positions = {char: divmod(index, 6) for index, char in enumerate('abcdefghijklmnopqrstuvwxyz-_,.')}
    cursor, presses = 'e', 0
    for char in text:
        if char == ' ':
            presses += 1  # Dedicated Space shortcut; retain letter cursor.
            continue
        row, col = positions[cursor]
        target_row, target_col = positions[char]
        dy, dx = abs(row - target_row), abs(col - target_col)
        presses += min(dy, 5 - dy) + min(dx, 6 - dx) + 1
        cursor = char
    return presses


def circle(text):
    groups = ['abcdefgh', 'ijklmnop', 'qrstuvwx', 'yz,.!?-_', '0123456789']
    positions = {char: (group, index) for group, values in enumerate(groups) for index, char in enumerate(values)}
    group, presses = 0, 0
    for at, char in enumerate(text):
        if char == ' ':
            presses += 1  # Space also accepts the preceding pending letter.
            continue
        target_group, index = positions[char]
        delta = abs(target_group - group)
        presses += min(delta, len(groups) - delta) + 1  # Group navigation + open.
        group = target_group
        size = len(groups[group])
        presses += min(1 + ((size // 2 - 1 - index) % size), 1 + ((index - size // 2) % size))
        if at == len(text) - 1 or text[at + 1] != ' ':
            presses += 1  # Immediate acceptance, never a substitute idle pause.
    return presses



# Independent behavioral model, not bundled SDK code. State is selected column,
# symbol row, shared upper/lower row, menu row (all zero-based). These navigation
# rules were checked against SDK 3.1.2 CoreLibs/keyboard.lua (hash in the report).
def sdk_move(state, direction):
    column, symbol, alpha, menu = state
    if direction in 'LR':
        column = (column + (1 if direction == 'R' else -1)) % 4
    else:
        delta = 1 if direction == 'D' else -1
        if column == 0:
            symbol = (symbol + delta) % 42
        elif column in (1, 2):
            alpha = (alpha + delta) % 26
        else:
            menu = min(3, max(0, menu + delta))
    return column, symbol, alpha, menu


def sdk_selected(state):
    column, _, alpha, menu = state
    if column == 2:
        return chr(ord('a') + alpha)
    if column == 1:
        return chr(ord('A') + alpha)
    if column == 3 and menu == 0:
        return ' '
    # This corpus needs no symbols or menu commands other than Space.
    return None


def sdk_route(start, target):
    queue, previous = deque([start]), {start: None}
    while queue:
        state = queue.popleft()
        if sdk_selected(state) == target:
            route = []
            while previous[state] is not None:
                state, direction = previous[state]
                route.append(direction)
            return list(reversed(route))
        for direction in 'UDLR':
            other = sdk_move(state, direction)
            if other not in previous:
                previous[other] = state, direction
                queue.append(other)
    raise ValueError('Unreachable SDK character')


def sdk_keyboard(text):
    # Fresh imported module/session for EACH phrase: uppercase A, symbol 1,
    # shared alphabet a/A, menu OK. show('') alone does not reset these rows.
    state, typed, events = (1, 0, 0, 1), '', []
    for target in text:
        route = sdk_route(state, target)
        for direction in route:
            state = sdk_move(state, direction)
            events.append(direction)
        inserted = sdk_selected(state)
        assert inserted == target
        typed += inserted
        events.append('A')
        # Normal capitalization and A entry retain focus and all row indices.
        assert typed == text[:len(typed)]
    assert typed == text
    return {'presses': len(events), 'releases': len(events), 'dpad_presses': len(events) - len(text), 'selection_presses': len(text), 'replay_verified': True, 'route': ''.join(events)}


def main():
    phrases = [text for values in CORPUS.values() for text in values]
    with tempfile.TemporaryDirectory(prefix='keyboard-benchmark-') as temp:
        binary = str(Path(temp) / 'model')
        compiler = shlex.split(os.environ.get('CC', 'clang'))
        subprocess.run(compiler + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Isrc', 'tools/benchmark_model.c'] + SOURCES + ['-lm', '-o', binary], cwd=ROOT, check=True)
        output = subprocess.run([binary], input='\n'.join(phrases) + '\n', text=True, capture_output=True, check=True)
    replays = [json.loads(line) for line in output.stdout.splitlines()]
    assert len(replays) == len(phrases) and all(row['replay_verified'] for row in replays)
    rows, index = [], 0
    for language, texts in CORPUS.items():
        for text in texts:
            sdk_replay = sdk_keyboard(text)
            rows.append({'language': language, 'text': text, 'current_c': replays[index], 'sdk_replay': sdk_replay, 'Playdate SDK 3.1.2 keyboard': sdk_replay['presses'], 'bill’s 1.0.1 ABC': abc(text), 'Wrap+ prototype': wrap(text), 'Circle D-pad prototype': circle(text)})
            index += 1
    totals = {}
    for language in ['Combined'] + list(CORPUS):
        selected = [row for row in rows if language == 'Combined' or row['language'] == language]
        native = sum(row['current_c']['presses'] for row in selected)
        baselines = {name: sum(row[name] for row in selected) for name in BASELINES}
        totals[language] = {'characters': sum(len(row['text']) for row in selected), 'phrases': len(selected), 'current_c_presses': native, 'current_c_releases': native, 'current_c_release_commits': sum(row['current_c']['release_commits'] for row in selected), 'baselines': {name: {'presses': count, 'releases': count, 'current_c_fewer_presses_percent': 100 * (1 - native / count)} for name, count in baselines.items()}}
    report = {'metric': 'One D-pad or A/B press; matching releases tracked separately. Taps only, no auto-repeat, crank, idle acceptance, corrections or Done/Cancel. Current C is executed and text checked after every direction release. ABC reproduces bill’s 1.0.1/build 3 commit 0c6275a208d7266965a2263f08d70123f169c189; Wrap+ and Circle reproduce the archived prototype study. The SDK comparison models CoreLibs/keyboard.lua 3.1.2 navigation and A entry, with route replay; it does not measure typing time.', 'scope': 'Fixed development corpus; lowercase ASCII, fresh module/session per phrase; no human timing/WPM inference.', 'sdk_baseline_provenance': {'version': '3.1.2', 'file': 'CoreLibs/keyboard.lua', 'sha256': '46c83d151a34b20a35fa7e0eb5299e90cfb945e12d2b4ac31ae18ab11070a5ea', 'documentation': 'https://sdk.play.date/3.1.2/Inside%20Playdate.html#M-keyboard', 'initial_state': 'Fresh module/session: upper A selected; symbols row 1, shared upper/lower row 1, menu row 2 (OK). Normal capitalization. show() alone retains letter rows.', 'route_legend': 'U/D/L/R are D-pad taps; A inserts selected character/Space. Each tap has one matching release.'}, 'source_sha256': {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in SOURCES + ['src/keyboard_model.h', 'src/input_timing.h', 'tools/benchmark_model.c', 'tools/benchmark_keyboard.py', 'docs/benchmarks/corpus.json']}, 'totals': totals, 'phrases': rows}
    (DEST / 'benchmark.json').write_text(json.dumps(report, indent=2) + '\n')
    for language, result in totals.items():
        print(f"{language}: {result['current_c_presses']} current C presses, {result['characters']} characters")
        for name, baseline in result['baselines'].items():
            print(f"  {name}: {baseline['presses']}; {baseline['current_c_fewer_presses_percent']:.1f}% fewer")


if __name__ == '__main__':
    main()
