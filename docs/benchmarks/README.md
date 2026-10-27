# Keyboard study · 27 October 2026

Cloud Keyboard needs **35.4% fewer button presses** than the ABC grid shipped in bill’s 1.0.1 on the fixed bilingual prototype corpus: **509 versus 788** for 177 characters, including spaces. This is an action-count model, not a typing-speed or usability study.

| Text | ABC | Cloud Multi-Tap | Fewer presses |
| --- | ---: | ---: | ---: |
| English · 6 phrases, 95 characters | 411 | 277 | 32.6% |
| German · 6 phrases, 82 characters | 377 | 232 | 38.5% |
| Combined | 788 | 509 | 35.4% |

## What counts

One D-pad or A/B press is one action. ABC uses its original 44-key geometry and directional neighbour rule, starts on `a`, and retains the cursor between characters. Wrap+ uses a 6×5 toroidal alphabet grid, starts on `e`, and has a one-press Space shortcut. Circle opens a group with A, selects letters with the D-pad, and uses A to accept before navigating again. The alternative Circle row replaces acceptance presses with 750 ms pauses.

Cloud uses direct D-pad groups and A/B cycling. Changing sides within the same group commits the previous letter without reselecting the group. Reselecting a direction commits and starts a fresh cycle. Space waits for the previous letter’s 750 ms acceptance; the last letter also waits. The Cloud sequence has 27 such acceptance pauses over the full corpus. The shipped Lua model reproduced all 12 phrases and the 509-press total in Playdate SDK 3.1.2.

The corpus is the same small set used while developing the prototypes, not a representative language dataset. It is lowercase ASCII only. Every phrase starts fresh; Done/Cancel, learning, mistakes, corrections, uppercase, punctuation and numeric-layer transitions are excluded. The result therefore does not establish a general speed or ease-of-use advantage.

## Crank, separately

Full-group crank selection uses **162 D-pad presses + 15 Space presses + 162 continuous turns**: **339 separate control gestures**, compared with 509 presses for button-only entry, or **33.4% fewer**. A turn may cross multiple letters: the same sequence involves **465 selection ticks**, approximately **38.9 revolutions of total absolute rotation** at the default sensitivity of 1.496. Negative turns start at the last letter; positive turns start at the first.

A continuous turn is deliberately counted as one gesture here, while button taps are counted individually. This metric describes interaction structure. **It is not a measured time saving. No additional crank speed percentage or WPM has been established.** Physical turn speed, direction changes, pauses, and individual preference matter.

## Reproduce

```sh
python3 docs/benchmarks/benchmark.py
```

The script has no external dependencies and writes `benchmark.json` alongside itself, including the full corpus, totals, Circle and Wrap+ comparisons, and crank selection ticks. It does not operate a device or access save data.
