# Playdate Touchtyping Keyboard

A native C text-entry keyboard for Playdate, extracted from **brb Build 91**.
Hold a D-pad direction, choose a character with A/B or the crank, then release
that direction to insert it. The model and rendering are the same native code;
the library has no account, server or network dependency.

![The native keyboard in brb Build 91, with fictional demo content](docs/media/keyboard-build91.gif)

*Recorded in brb's native renderer with fictional demo content. The standalone
example below uses the exported library.*

## Controls

| Input | Action |
| --- | --- |
| Hold D-pad direction | Open its character group |
| A / B within a group | Select forward / backward; hold to repeat |
| Crank within a group | Select with its own detent and reversal resistance |
| Release the group's direction | Insert the selected item; close the group |
| Idle A tap | Switch lowercase/punctuation and uppercase/emoji |
| Hold idle A + Left/Right or crank | Move the grapheme cursor |
| Hold idle A + Up/Down | Switch keyboard / emoji-set view |
| Idle B | Delete the previous grapheme; hold to repeat |
| Fresh A+B overlap, Return item or Done | Open Save / Discard decision |
| Release opening buttons, then fresh A / B | Save / Discard |

There is **no idle timeout or automatic character acceptance**. Releasing a
group without choosing an item inserts nothing. Save validates the field and
returns `HELLO_KEYBOARD_DONE`; the host accepts the value and closes the editor.
Discard restores the original value and closes it.

## Run the C example

Install the [Playdate SDK](https://play.date/dev/) and its supported ARM C toolchain.
Set `PLAYDATE_SDK_PATH` to your SDK directory, then:

```sh
make test-core       # SDK-independent model/helper tests
make test            # includes mock-SDK rendering; needs SDK headers
make test-sanitize   # same suite with address/undefined-behavior checks
make simulator       # compile Simulator code
make device          # compile ARM code
make check-stack     # inspect bounded ARM stack usage
make all             # create local debug touchtyping-keyboard.pdx
```

[`examples/keyboard_minimal.c`](examples/keyboard_minimal.c) is the complete C SDK
entry point compiled directly by the Makefile. It opens an editor, copies
saved text into host storage, and lets A reopen it. It initializes the library
once, uses the built-in Roobert font, and passes input updates and rendering
every visible frame. Local
builds are for internal debugging; this repository distributes source rather
than a release PDX or ZIP. Automated checks do not establish hardware validation.

## Use it in your game

Copy `src/`, the three `Source/keyboard/` image tables and their license notices.
Compile the seven C files listed in the [integration guide](docs/integration.md)
with the SDK headers and `-lm`. The optional `Source/unicode/glyphs.bin` adds
broader Unicode rendering. Embedded Everyday emoji need no external atlas.

```c
static HelloKeyboard keyboard;       // Large persistent state: avoid the ARM stack.
HelloKeyboardConfig config;
hello_keyboard_default_config(&config);
hello_keyboard_init_with_config(&keyboard, pd, &config);
hello_keyboard_open_configured(&keyboard, "", NULL);
```

Text is UTF-8; editing and cursor movement respect grapheme clusters. Defaults
allow 160 graphemes within a separate 4096-byte ceiling. Configure field type,
length, byte limit, trimming, captions and feedback for each view. Emoji entry
and the picker are configurable; the included set has 21 original 1-bit motifs.
Unicode rendering has coverage and shaping limits described in the guide.

The host owns the SDK, optional font/Unicode renderer and callback contexts.
The keyboard copies text, groups, captions and field policies; custom emoji
catalogs remain borrowed immutable data. Keep keyboard state for application
lifetime, copy borrowed result text before reuse, and close the editor before
restoring host menu items. See [ownership and lifecycle](docs/integration.md#ownership-memory-and-teardown).

Library defaults use 350 ms hold / 100 ms repeat and button-navigation scale
`1.0`. brb configures scale `0.5` (175 ms / 50 ms for selection), its own row
height and ten leading cells in group 1. Those are explicit host choices;
crank policy remains separate. API identifiers retain their original
`HelloKeyboard` / `Brb` names for compatibility.

## Source and licenses

Current implementation: native C under `src/`. The earlier Lua implementation
and its original documentation remain archived under [`legacy/lua/`](legacy/lua/);
they are not a current Lua binding or a behavior-parity claim.

Code and original artwork: [MIT](LICENSE). Font-derived assets and Unicode data
retain their [third-party licenses and credits](THIRD_PARTY.md). Playdate is a
product of Panic; this is an independent community library.
