# Integrate the native C keyboard

The keyboard is native C. It does not require the brb server, account model or
application screens. The host owns validation, saved text, network operations and
its surrounding menu. See the compiled example in
[`examples/keyboard_minimal.c`](../examples/keyboard_minimal.c).

## Copy the dependencies

Compile these C files and link the math library (`-lm`):

```text
keyboard.c       keyboard_model.c   input_field.c   pill.c
unicode.c        unicode_font.c     emoji_set.c
```

Copy their local headers alongside them:

```text
keyboard.h             keyboard_labels.h        keyboard_model.h
keyboard_crank.h       keyboard_geometry.h      keyboard_scroll.h
elastic_marker.h       float_motion.h          input_timing.h
message_motion.h       keyboard_cursor_blink.h   popup.h
input_field.h          pill.h                  scroll_follow.h
unicode.h              unicode_font.h          emoji_set.h
emoji_everyday_data.h  unicode_case_data.h     unicode_font_data.h
unicode_grapheme_data.h
```

Keep `pd_api.h` on the include path using the standard Playdate C SDK build
support. Include these existing image tables under your game's `Source/keyboard/`:

```text
glyphs-table-12-15.png
footer-table-23-23.png
dpad-table-41-41.png
```

`pdc` compiles them into the `keyboard/glyphs`, `keyboard/footer` and
`keyboard/dpad` image tables. No background image is needed. The optional
`Source/unicode/glyphs.bin` supplies the broader Unicode fallback. The small
emoji artwork is compiled from `emoji_everyday_data.h`; it does not require a
network request. Retain the supplied Unicode/font/emoji license notices when
copying the associated data or artwork.

## Initialize, open, update, close

1. Keep one `HelloKeyboard` instance alive while the editor is open; initialize
   it once with `hello_keyboard_init_with_config` and a default-initialized
   `HelloKeyboardConfig`. The SDK, font and optional Unicode renderer must remain
   alive for the instance's use. Do not reinitialize a visible instance.
   The example loads the built-in `/System/Fonts/Roobert-10-Bold.pft` and sets
   `config.font` and `config.font_size`; no extra font asset is copied.
2. Before opening, release your host menu items. `config.menu_actions` selects
   `HELLO_KEYBOARD_MENU_CLEAR`, `DONE`, `CANCEL` and `HIDE` bits. Legacy defaults
   remain `ALL=7` (Clear/Done/Cancel). `HIDE=8` adds a No/Yes option only for a
   masked/password field; other field types create no Hide item. The native brb app selects
   Hide only, while this library retains Clear/Done/Cancel defaults. Each password open starts with No, displaying its text. Do not
   delete owned items behind its back. Keep the SDK's three-item menu limit in
   mind when combining optional actions.
3. Set the title and open with `hello_keyboard_open_configured`. `Config.field`
   supplies the instance policy; `OpenOptions.field` overrides it for one view.
   The legacy typed-open APIs remain adapters with an explicit type override.
4. Forward button state, fresh presses, releases and crank delta to
   `hello_keyboard_update` once per visible frame, then call
   `hello_keyboard_draw`. Drawing acknowledges completed boundary transitions;
   update-only use cannot finish a render-dependent input gate.
5. `HELLO_KEYBOARD_DONE` keeps the editor open so the host can validate the text
   or show feedback. On success, consume/copy the borrowed text, then close it.
   `HELLO_KEYBOARD_CANCELLED` already closes it. Restore your host menu afterward.
6. Close your optional Unicode renderer on host shutdown.

The keyboard validates the field's configured type and length. Server success,
nickname rules and other product validation remain with the host.

## Field policy and Unicode capacity

`HelloKeyboardFieldPolicy` has `type`, `min_length`, `max_length`,
`trim_for_length`, `max_bytes` and `length_unit`. Length defaults to extended
grapheme clusters; explicitly select `HELLO_KEYBOARD_LENGTH_BYTES` for byte-based
contracts such as passwords. Required fields use a minimum above zero. The
default is optional plain text with a 160-grapheme maximum. An explicit maximum
of zero permits only an empty value; a zero-initialized legacy Config selects
the library defaults. `max_bytes=0` selects the independent 4096-byte ceiling.
Invalid policies are rejected atomically by the setters/open functions.

For example, a message view uses:

```c
HelloKeyboardFieldPolicy message = {
    .type = HELLO_INPUT_PLAINTEXT, .min_length = 1, .max_length = 160,
    .trim_for_length = true, .max_bytes = HELLO_KEYBOARD_TEXT_BYTES,
    .length_unit = HELLO_KEYBOARD_LENGTH_GRAPHEMES
};
HelloKeyboardOpenOptions options = {.field = &message};
hello_keyboard_open_configured(&keyboard, draft, &options);
```

Initial text may be shorter than the minimum. Selection never skips an item
because the field is full or its type rejects that item: the selected value is
validated when the direction is released. Invalid input is discarded with
feedback, preserving the previously committed text. Confirmed Save also checks
the minimum. Feedback resolver codes are `FIELD_REQUIRED`, `FIELD_TOO_SHORT`,
`FIELD_TOO_LONG`, `FIELD_INVALID_TYPE` and `FIELD_INVALID`.

Trimming counts and removes complete edge whitespace graphemes, including
Unicode whitespace and BOM/FEFF. Whitespace joined to a visible combining mark
is retained. Minimum and maximum use the trimmed value when configured;
successful Save normalizes the buffer in place. Failed validation preserves
the raw editable text. Password policies disable trimming explicitly.

`HELLO_KEYBOARD_TEXT_BYTES` is 4096; `HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS` is
160. `HELLO_KEYBOARD_MAX_TEXT` remains a compatibility alias for byte capacity,
not the default character count. `HELLO_KEYBOARD_ITEM_BYTES` remains 160 for
single keyboard items and transforms. Large text, candidate and preview buffers
belong to the keyboard/model instances; rendering allocates neither large stack
arrays nor per-frame heap storage. A per-view policy override never changes
the instance default used by later opens.

For normal entry, hold a D-pad direction, choose a character using A/B or the
crank, then release that direction to insert the selected character. A begins
at the left and advances right; B begins at the last fully visible item and
advances left. The first real crank movement immediately accepts an entry item:
negative travel chooses the last fully visible item, positive travel the first.
That first sample's excess is discarded, so every accepted entry is presented.
The selected row then follows its normal centering curve. The next detent still
requires the initial breakaway angle; reversing uses the separate reversal
threshold. There is no uncommitted marker preview that can switch sides when
the crank changes direction. Once selected,
all directions traverse the whole group; viewport edges never trigger wrapping. This
choice direction does not change their idle gestures or crank direction. An idle
selection never inserts itself or dismisses its group. A release without a
selected character inserts nothing and closes the group. Both paths immediately
clear selection state and group-owned holds. Continue forwarding releases even when a
transition temporarily rejects new selection input. A/B entry into an offscreen
endpoint also waits for that endpoint to settle; crank entry keeps its distinct
policy. An accepted held button resumes repeating after the transition. Short
presses released during travel are discarded. A newly pressed button that stays
down, with the same group/item and its associated D-pad direction still held,
becomes a repeat intent only after the actual row and marker settle. Its first
deadline remains relative to the physical press (175ms in brb); an overdue
deadline produces at most one step, then the ordinary 50ms repeat. No rejected
tap is replayed, and a release, group change, opposite control or modal cancels
the intent. Fitted groups keep their existing immediate press/repeat path.

The held direction's group is filled black with white content; the individual
XOR marker contrasts as a light capsule, with exactly 2px vertical clearance
inside the group. Once released, the group immediately
returns to its inactive white fill and black outline.

Accepted text goes directly into the model's fixed-size RAM buffer; acceptance
does not allocate or perform file/network operations. Rendering consumes the
latest buffer in the frame's separate draw step. This coalesces intermediate
paint work without delaying text behind timers or replaying stale input jobs.

Opposing groups with equal character counts share a width. With unequal counts,
each group uses its own axis limit (300px vertically, 188px horizontally) and
shrinks to complete visible cells plus symmetric padding. Short groups fit their
own complete content. The same bounds determine rendering and overflow gates.

A/B boundary transitions reject new taps only when the group's contents need
to scroll. A fully visible group accepts boundary taps immediately, just like
ordinary taps inside the group. The crank keeps its separate boundary policy. The first consumed boundary of a
new crank sequence uses 80% of the ordinary boundary angle; subsequent boundaries
keep their original resistance. This does not change the five-item release curve
or the normal per-character detents. Direction reversal has its own stronger
breakaway resistance (`HELLO_KEYBOARD_CRANK_REVERSAL_FACTOR`), followed by the
existing five-item release curve. A new group still uses the lighter initial
breakaway; neither setting changes a cycle boundary threshold.

Use `visible_prefix_items[group - 1]` or
`hello_keyboard_set_visible_prefix(keyboard, group, count)` to give an individual
group an exact initial cell count. Zero retains automatic sizing; values are
bounded by the 64-item group limit. brb configures ten cells only for group 1,
so all ten leading digits fit while the remaining symbols scroll.

## Captions and feedback per view

`HelloKeyboardLabels` configures the three menu titles and all footer text.
Each field is optional: `NULL` selects its default, and `""` intentionally hides
that caption. The keyboard owns a UTF-8-safe copy of at most 48 bytes per caption.
Stack-local source strings can therefore be released after configuration.
Changing captions never changes their actions or their gesture timing.

For an active character group, `selection_text` supplies the shared caption
after the right-hand A/B pair (default `Cursor`). The two 20px icons keep
a 4px gap, matching the icon-to-caption spacing. On the opposite side, the
native 20px D-pad precedes `select_text` (default `Select`). Its directional state
remains visible while a group is selected. Its outline shares the A/B border
thickness (2px). A completely idle footer centers configurable `escape_text` (`– Escape –`);
any physical key hides it. Held-navigation leaves the center empty; the
D-pad appears on the left beside Select for character selection or beside
`navigation_text` (default `Cursor / Layer`) in held-A navigation. Its light
arms track available directions currently held, returning to black on release. Click and hold feedback use a white interior with a black outline;
the timing and accepted/rejected input rules remain independent of rendering.

With no active group, a short A tap changes the letter-page variant: lowercase
with punctuation above, or uppercase with emoji above. Holding A enables the
navigation mode. Idle case-action captions are exactly `Shift` for lowercase
input and `shift` for uppercase input. The former number/cursor page remains
removed. The wrapper has a base keyboard and an emoji-set picker by default;
held-A Up/Down cycles these views, and releasing A retains the current view.
The SDK-independent model starts with one view; the wrapper configures one or
two using `hello_keyboard_model_set_view_count`. `hello_keyboard_model_layer`
and `hello_keyboard_model_layer_count` both take the model instance. All field
types show the same complete base keyboard;
number fields can select any displayed character, then reject non-digits on
commit with `FIELD_INVALID_TYPE`. Their digit count is an explicit field policy.
D-pad Left/Right in the held-A mode moves
the input cursor by complete grapheme clusters. Its first step is immediate;
holding a direction uses the same delay and repeat interval as A/B character
navigation (175ms/50ms in brb, with its 0.5 navigation scale). Each update moves
at most one cluster, with no catch-up queue; both text ends stop movement.
Releasing A or the held direction stops repeat immediately.
The input cursor stays visible during D-pad or crank movement (and input-preview
position changes). Stopping begins a fresh visible blink phase, followed by the
normal 420ms visible/hidden phases. The instance-owned blink helper is updated
from input; rendering only reads it. A cursor over text uses the actual contextual
glyph position and width, so narrow letters do not cover neighboring glyphs.
The crank also moves the grapheme cursor in this mode, beginning when A reaches
the hold threshold or a D-pad chord has already entered navigation. It shares
the character crank's start, running, reversal and speed resistance, without
group-edge resistance or wrapping. Outward pressure at a text boundary is
discarded; releasing A discards remaining cursor-crank pressure, including crank
input in that release snapshot. `hello_keyboard_model_move_cursor` exposes the
bounded grapheme movement without changing text, case or hold state.
With one view, navigation keeps the D-pad and `Cursor` caption visible.
Left/Right light their corresponding arms; Up/Down have no mapping and leave
the icon neutral, with the same caption. With multiple views, the default hint follows the held axis:
`Cursor` for Left/Right, `Layer` for Up/Down, and `Cursor / Layer` otherwise.
Explicit custom navigation captions remain literal when the control is visible.
The base view's right-hand caption always keeps `Shift`/`shift`, including while
A is held and the picker is enabled. While choosing views from the picker, the default layer
word has one character per view: `Ly`, `Lyr`, `Layr`, `Layer`, then additional
`a` characters. Only the current view's character is uppercase (for example
`laaAyer` for view 4 of 7). `hello_keyboard_layer_caption` provides the bounded
formatter; explicit non-default `layer_text` remains a custom-caption override.
An empty custom caption hides it. While idle A is held, B cannot delete;
entering this mode also cancels an older idle B hold. An idle B press immediately
deletes one preceding grapheme; its short release deletes nothing more. Keeping
B held deletes the next grapheme at the hold threshold, then repeats at the
configured repeat interval (350ms and 100ms by default). Releasing A leaves the next fresh
gesture ready immediately. Within an active group, every accepted A/B press
selects the next item immediately, including repeated presses of the same
button. A advances left to right and B right to left. Holding either button
continues selection after the configured initial hold delay, then at the
configured repeat interval. Releasing does not select an extra item. There is
no per-item case inversion, reserved second tap, deferred tap step or
`TOGGLE` gesture; use the idle A Shift/shift tap to choose the global case.
The removed `immediate_button_taps` option and setter are no longer needed.
Actual endpoint wraps retain their existing presentation gate.
Held selection repeats advance at most one item per update;
blocked or late repeats never build a catch-up queue. D-pad cursor navigation
uses the same bounded single-step repeat policy; deletion retains its separate
repeat policy. Button feedback fills
white from bottom to top inside the shared black outline.

Uppercase replaces the Space item with a thin Return icon. Committing Return
never inserts its glyph or a newline. Return, a Done menu/request and the A+B
tap all open the same editing decision described below. Opening never checks
completion or trims the draft; validation runs only after a fresh Save tap.
Pending Return is never painted into the input preview. A selection delivered
after Return in the same SDK snapshot is discarded. Confirmed Save ends all
current input gestures and requires held face buttons to be
released before reuse, including when host validation keeps the keyboard open.

Use `HelloKeyboardOpenOptions` for view-specific captions and feedback. A missing
options pointer or missing field resets that part to library defaults, so a
message view's `save` label does not leak into another view. The older
`hello_keyboard_open` / `hello_keyboard_open_field` calls intentionally retain
instance settings; use them when that persistence is desired.

`hello_keyboard_set_labels` can rename a visible menu in place. If the host SDK
lacks `setMenuItemTitle`, it returns `false` without changing captions; it never
rebuilds or duplicates menu items. Hide's No/Yes option captions are copied on
open; its title and the confirmation captions update immediately, while new
option captions apply on the next open.

## Emoji-set view

`HelloKeyboardConfig.emoji_input_enabled` is the instance's emoji-entry policy.
The default helper and NULL config enable it. Set it false before initialization,
or call `hello_keyboard_set_emoji_input_enabled(&keyboard, false)` while closed,
to retain the ordinary Up digits/symbols group under Shift instead of its emoji
alternative. D-pad, A/B and crank use that same visible inventory; case mode and
the other letter groups remain unchanged. The emoji-set view becomes unreachable.
The setter returns false
without mutation while a field is open; configure the next view after closing.
Initialize configs with the default helper: an explicitly zeroed boolean
disables emoji input. The other Shift groups keep their existing positions,
widths and input behavior; the top/bottom pair uses the actual visible Up
inventory. Catalogs, implementation, artwork and message rendering remain
available; enabling the instance again restores emoji entry on its next open.
Feature access decisions belong to the host application, outside this library.

`hello_keyboard_default_config` and a NULL config enable
`emoji_picker_enabled`; set it to false before initialization for the single
base view while retaining Shift+Up emoji entry when `emoji_input_enabled` is
true. Initialize configs with the default helper, since an explicitly
zeroed boolean disables the picker. The only shipped catalog entry is the real
Everyday set. No unavailable themes or placeholder artwork are shown.

Optional `emoji_sets` and `emoji_set_count` configure a borrowed immutable array
of `const BrbEmojiSet*`. The array, every set, its ID/inventory strings and pixel
data must outlive the keyboard instance, including later close/open cycles.
Each usable set supplies a valid 2..64-grapheme keyboard inventory within the
existing group byte budget and valid original artwork. Catalog size has no
theme-count cap; rendering visits at most five visible rows and three original
glyphs per row, with no allocation or atlas/file IO for the previews. Displayed
names are the set IDs, with the shipped name shown as Everyday.

In this view, ordinary Up/Down and the crank select a catalog row, clamping at
both ends. A short A release uses that set and returns to the base view without
typing, shifting case or submitting. B alone is unmapped; a fresh A+B tap
still opens the editing decision. Held A retains the shared navigation mode: Left/Right
move the input cursor in either view, Up/Down changes view, and releasing A
does not confirm a set or change the view. The picker has its own configurable
`picker_title`, `picker_select` and `use_set` captions.

`hello_keyboard_emoji_set` returns the selected borrowed pointer. Its selection
persists across close/open; each new field opens on the base view. Using a set
copies its inventory into owned group 6 and updates the configured
`BrbUnicodeFont`. With the picker enabled, that inventory owns group 6 instead
of a separate `config.groups[5]` override. Update/draw synchronize a shared font
to the instance's selected set, so alternating keyboard instances retain their
own artwork. Previews draw directly from each candidate's original bits and
never temporarily change the selected renderer.

Optional `on_emoji_set_selected(void* userdata, const BrbEmojiSet* set)` and
`emoji_set_userdata` notify the host only when a confirmed selection changes
the set pointer. The host owns any dependent message-layout cache invalidation;
opening, drawing and reselecting the same set do not notify or perform IO. No
on-disk preference is needed for the current one-set catalog.

## Editing decision and password visibility

A fresh overlapping A+B tap opens an editing decision immediately for every
field type, except while an existing held-A cursor/layer gesture owns the
keyboard. No hold threshold, timer or opening progress applies. Pressing both
buttons together or pressing one while the other remains down opens on that
fresh overlap. A complete A+B press/release coalesced into one SDK sample also
opens once. Merely stale held bits, rejected buttons, or a release of one button
in the same sample as a fresh opposite press do not form a new chord.

While held A owns cursor/layer navigation, B is unmapped and remains rejected
until its physical release, even when A is released first. A fresh simultaneous
A+B still opens the decision from idle. The chord consumes selection, deletion,
direction releases and crank input in that frame, with no text commit or
masking toggle. A/B singles remain immediate. If the second button overlaps
before a direction/crank action or hold repeat consumes the first gesture,
the instance restores its pre-press draft, cursor, case and pending selection
before opening. The checkpoint is bounded instance storage; it adds no input
delay, allocation or IO. A first B press may therefore render one deleted frame
before a slightly offset A completes the chord and restores it. Accepted edits,
hold repeats and view changes are never rolled back. An accepted active-group selection hold plus a fresh
opposite tap opens immediately as well. Opening cancels gesture timers without
changing the text, cursor, case or pending selection; there is no short-chord
pause or resume state before the dialog opens.

Release both opening buttons before choosing. Tap fresh B to
Discard: it returns `HELLO_KEYBOARD_CANCELLED`, restores the initial text and
closes the editor. Tap fresh A to Save: it commits a
pending selection, checks the field's completion rules, applies configured
trimming and emits `HELLO_KEYBOARD_DONE` once when valid. An invalid Save returns
to editing with field feedback and preserves draft bytes, cursor and password
visibility. Choice actions run immediately on the fresh press, including a
press/release pair coalesced into one SDK sample. Continued holding and releasing
do not repeat an action. Simultaneous choices require both buttons to be released
before another fresh choice.
The opening tap cannot confirm either action. Choice-button releases cannot
leak into normal typing or deletion. Host/menu `requested_action =
HELLO_KEYBOARD_DONE` opens this same decision; it cannot bypass Save.
`hello_keyboard_model_cancel_gestures` exposes the same bounded gesture reset
to other adapters while retaining text and selection.

The popup uses the shared floating motion, dither backdrop and rounded 2px
outline. Its height matches a two-line message body. Its single text line stays
centered vertically and scrolls horizontally when longer than its clipped cell.
B/Discard and A/Save live in a fixed full-width footer at y=211; the native 20px
indicators and their labels do not follow the bubble motion. This decision uses
tap buttons without hold progress, including the A+B gesture that opens it.
`cancel_question`, `discard` and `save` are configurable, instance-owned labels;
their defaults are `Done typing? What's next?`, `Discard` and `Save`.
The compatible `submit_question` label remains available but is not used by
the unified editing decision. The separate `no`, `yes` and `hide` labels retain
their defaults `No`, `Yes` and `Hide`. The
password-only Hide menu changes visibility directly and starts at No for each
open. The same editing decision is available in password fields.

For application errors, configure `HelloKeyboardFeedbackConfig` with a display
duration and an optional resolver callback. The resolver receives the host's
error key and fallback text. Its result is copied immediately. The callback and
its `userdata` remain host-owned and must outlive their use. The default duration
is 5000 ms; a zero duration selects that default. Feedback replaces the title,
uses its existing marquee for long messages, expires back to the title and clears
when new input begins. `hello_keyboard_show_error` remains available when a caller
needs to supply the text and duration directly.

## Separate input policies from rendering

A/B are discrete controls; the crank is continuous. They have independent
ergonomic rules. Inside a group, initial A/B presses remain immediate while the marker
continues animating. A real boundary wrap temporarily rejects selection input
until the opposite endpoint is presented and settled. Rejected presses and
crank deltas are discarded rather than replayed. Releases, accepted hold gestures
and Done/Cancel continue to work.

The crank's resistance, five-character release and speed adjustment are in
`keyboard_crank.h`. Tap/hold defaults belong to `input_timing.h`. Shared horizontal
jelly rendering is configured separately and does not enlarge the glyphs.
Neither idle floating nor ordinary marker movement creates an input lock.

`button_navigation_scale` belongs to each keyboard/model instance. Its default
is `1.0`; `hello_keyboard_set_button_navigation_scale` accepts finite values
from `0.25` through `2.0` and rejects invalid values atomically. brb selects `0.5`.
A/B character and D-pad cursor navigation are scaled: the initial repeat delay,
repeat interval, row-follow time and marker spring time. With the default shared
timings, `0.5` gives a 175ms repeat start and 50ms repeat interval. The public
model helpers `hello_keyboard_model_selection_hold_ms` and
`hello_keyboard_model_selection_repeat_ms` expose rounded durations, at least
one millisecond. Marker size and trail geometry do not change.

Idle delete, the held-A navigation-mode threshold, crank selection/cursor
movement and row homing after D-pad release retain their own timing. The shared
timing struct retains tap_window_ms and cursor_delay_ms for other consumers;
this keyboard uses its character navigation helpers for D-pad cursor repeat.
A button boundary gate continues to require the actual scaled row and marker
to settle; it has no separate timeout.

## Ownership, memory and teardown

Use application-lifetime or heap storage for `HelloKeyboard` and
`BrbUnicodeFont`; both contain large persistent buffers and must not be local
variables on Playdate's small C stack. `BrbUnicodeFont` alone is about 44 KiB.
The keyboard owns its model, text, preview, checkpoint and copied group/caption
storage. `hello_keyboard_text` borrows the model buffer: copy its bytes before
reusing, reopening or releasing the instance.

The SDK pointer, optional `LCDFont*`, optional `BrbUnicodeFont*`, callbacks and
their userdata are borrowed. Keep them valid throughout use. `Config.groups`,
title, captions and per-view field policies are copied during configuration;
the emoji catalog and all set contents remain borrowed immutable data.

Initialization loads the three keyboard image tables once. `close` removes
only the keyboard's system-menu items, clears input/render state and hides the
editor; it keeps loaded tables for later opens. There is no general keyboard
destructor API in this exported revision. Initialize once per application
lifetime rather than repeatedly initializing the same object. The runnable
demo closes its Unicode file/cache on `kEventTerminate`; the SDK reclaims the
application's remaining loaded assets on exit. Do not free a shared font or
renderer while any keyboard still uses it.

Character acceptance edits bounded RAM buffers without allocation or IO.
Rendering uses bounded caches; Unicode fallback may read atlas pages on cache
misses. Drawing arbitrary Unicode is therefore distinct from accepting text.
Custom catalogs draw at most five picker rows and three glyphs per row.

## Defaults and application overrides

Always start with `hello_keyboard_default_config`. Explicitly zeroing a config
turns off its emoji booleans; it does not mean the same thing as requesting all
defaults. Some numeric zero values intentionally select compatibility defaults.

| Setting | Library default | Native brb application policy |
| --- | --- | --- |
| Hold / repeat | 350 ms / 100 ms | Same shared base timings |
| Button navigation scale | 1.0 (350 ms / 100 ms selection) | 0.5 (175 ms / 50 ms selection) |
| Group height | Original library geometry | Host font height plus host padding |
| Initial visible group cells | Automatic | Group 1 requests ten cells |
| Menu actions | Clear, Done, Cancel (`ALL=7`) | Hide (`HIDE=8`), password fields only |
| Emoji input / picker | Enabled / enabled | Host chooses feature availability |
| Field length | Optional plain text; 160 graphemes, 4096 bytes | Per-view domain rules |
| Feedback lifetime | 5000 ms | Host configurable |

A/B selection and held-A D-pad cursor repeat use the navigation scale. Idle
B deletion and the held-A entry threshold retain their base timing. Crank
resistance, reversal and boundary thresholds are separate mechanisms and never
inherit the A/B scale. Set policy per instance; per-view options do not leak
into the next configured open. No setting introduces idle acceptance.

## Unicode limits

Editing uses real UTF-8 and extended grapheme clusters, including combining
marks, flags, modifiers and ZWJ sequences. No automatic transliteration or
Unicode normalization is performed. The 4096-byte storage ceiling is separate
from the 160-grapheme default field limit. Configured groups accept 2–64
graphemes and at most 2560 UTF-8 bytes each.

The shipped Everyday artwork provides 21 compact 1-bit emoji motifs from
[`assets/emoji/sets/everyday.json`](../assets/emoji/sets/everyday.json).
`src/emoji_everyday_data.h` embeds the artwork; no runtime download is needed.
Other text uses an available base-font glyph or the optional paged
Unifont/Noto atlas. Grapheme-safe editing does not promise complete Unicode
rendering: arbitrary contextual shaping and bidirectional text are not fully
implemented, and unsupported multi-base composites use a replacement glyph.
Retain [all associated notices](../THIRD_PARTY.md) when redistributing assets.

## Verify changes

`make test` and `make test-sanitize` exercise the SDK-independent model and
mock-SDK drawing/input paths. `make simulator` and `make device` compile the
runnable example for the installed Playdate SDK; `make check-stack` checks ARM
stack-usage output. `make all` produces a local debug PDX for internal checks.
See the [README](../README.md) for setup. These checks do not certify a package
on physical Playdate hardware. No hardened release package is distributed by
this source repository, and the historical Lua implementation is archived
under [`legacy/lua/`](../legacy/lua/).
