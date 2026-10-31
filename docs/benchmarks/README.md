# Native C action-count benchmark

The current C model needs **62.2% fewer button presses** than the **Playdate SDK 3.1.2 built-in keyboard** on this fixed English/German corpus: **480 versus 1,270 presses for 177 characters**, including spaces. Against bill’s 1.0.1 ABC keyboard it uses **39.1% fewer** (480 versus 788).

| Corpus | Characters | Current C | SDK keyboard | Fewer presses |
| --- | ---: | ---: | ---: | ---: |
| English, 6 phrases | 95 | 247 | 661 | 62.6% |
| German, 6 phrases | 82 | 233 | 609 | 61.7% |
| Combined | 177 | 480 | 1,270 | 62.2% |

| Combined comparison | Baseline presses | Current C presses | Fewer presses |
| --- | ---: | ---: | ---: |
| Playdate SDK 3.1.2 keyboard | 1,270 | 480 | 62.2% |
| bill’s 1.0.1 ABC | 788 | 480 | 39.1% |
| Wrap+ prototype | 609 | 480 | 21.2% |
| Circle D-pad prototype | 819 | 480 | 41.4% |

**Button presses counted; typing time not measured.** This small development corpus is not a representative language or usability study.

## Method

One D-pad or A/B press counts once. Every press has a matching release in every layout: the combined C sequence contains 480 presses and 480 releases. Each C character uses a D-pad press to open its group, the shortest sequence of A or B taps, and a D-pad release to accept. All 177 release commits are executed and checked against the expected text. No idle acceptance or forced commit is used.

The driver compiles and executes the production `keyboard_model.c` with its default groups. It searches both button directions through real model calls, then replays the best sequence. It checks the text after every character. It uses logical group endpoints; presentation animation, viewport-dependent entry hints, physical finger travel, and learning time are outside the model. Custom fonts, groups, and view configurations may change the experience.

All layouts use individual taps; held auto-repeat is excluded. Each phrase starts fresh. The corpus contains lowercase ASCII letters and spaces only. Corrections, uppercase, punctuation, numbers, emoji, Done/Cancel, and crank movement are excluded. Crank rotation is a distinct input mechanism and is not equated with a button press.

The SDK comparison uses an independent state model checked against installed `CoreLibs/keyboard.lua` from SDK **3.1.2** (SHA-256 `46c83d151a34b20a35fa7e0eb5299e90cfb945e12d2b4ac31ae18ab11070a5ea`). The public [SDK keyboard documentation](https://sdk.play.date/3.1.2/Inside%20Playdate.html#M-keyboard) describes its API; the installed implementation supplies the exact navigation rules. No SDK implementation or assets are bundled here.

The SDK has four columns: symbols, uppercase, lowercase, and menu (Space/OK/Delete/Cancel). Horizontal navigation wraps through the columns; letter and symbol rows wrap vertically, and menu rows clamp. Upper/lower rows move together. A inserts the selected letter or Space and retains focus. Each phrase starts from a **fresh module/session**, initially uppercase `A`, with the menu on `OK`: lowercase entry includes the initial Right press. Space needs menu navigation and A; there is no assumed Space shortcut. Normal capitalization is used. Reopening with `show("")` alone retains letter selection and is not the reset modeled here.

The script searches shortest D-pad routes in the complete four-column state, replays every route and A insertion, and verifies the exact phrase. The JSON includes those routes. SDK animation/debounce delays and held repetition are excluded from this tap-count metric.

The remaining comparison layouts preserve the definitions from the [archived prototype study](../../legacy/lua/docs/benchmarks/benchmark.py). The ABC reconstruction was additionally checked against bill’s 1.0.1/build 3 source at commit `0c6275a208d7266965a2263f08d70123f169c189`:

- **bill’s 1.0.1 ABC:** 44 keys with the archived pixel coordinates and directional nearest-neighbour rule, starting on `a`. D-pad routes are shortest paths; A accepts each character. Space is key 41. The coordinates, neighbour scoring, initial cursor, and Space/acceptance behavior reproduce that release; the count is computed from those rules.
- **Wrap+:** 6×5 alphabet/punctuation grid with horizontal and vertical wrapping, starting on `e`. A accepts letters; a one-press Space shortcut leaves the cursor in place.
- **Circle D-pad:** five circularly navigable groups, starting at the first. A opens a group; left/right select from the middle outward. A accepts before the next letter, except that Space accepts the pending letter as part of its one-press shortcut. There are no substituted idle waits.

The legacy Lua benchmark describes a different layout and acceptance mechanism. Its 35.4% result is not the result for this C library.

## Reproduce

Requires Python 3 and a host C11 compiler (`clang` by default; override `CC`). No SDK, device, network, account, or save data is required.

```sh
python3 tools/benchmark_keyboard.py
```

The command writes [benchmark.json](benchmark.json) with the full [corpus](corpus.json), per-phrase replay results, totals, and SHA-256 hashes of the evaluated sources. Temporary host binaries are removed automatically. Reduction is `100 × (1 − current presses / baseline presses)`.
