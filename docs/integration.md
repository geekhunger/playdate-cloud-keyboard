# Playdate Keyboard Library

Reusable full-screen keyboard controller for Playdate, with its native one-bit cloud design. The standalone app is an integration example; the library has no dependency on Bills, benchmark phrases, or demo state.

## Add it to a project

Copy `Source/keyboard.lua` and the entire `Source/keyboard/` directory into your project's `Source/`. Keep those paths unchanged so sprite loading works. No Python or artwork generator is needed to use or compile the library.

```lua
import 'CoreLibs/graphics'
import 'keyboard'

local note = ''
local keyboard = Keyboard.new({
    onDone = function(text, controller)
        note = text
        -- Store the result or return to your own screen here.
    end,
    onCancel = function(controller)
        -- Original note is unchanged.
    end,
})

function editNote()
    local ok, err = keyboard:show(note)
    assert(ok, err)
end

function playdate.update()
    if keyboard:isVisible() then
        keyboard:update()
        keyboard:draw()
        return
    end
    -- Your normal update and rendering here.
end
```

Call `editNote()` from the host's own button handler or screen controller. Open once, not on every update. The example app in `Source/main.lua` opens immediately and offers another edit after Done or Cancel.

## Public API

| Call | Behavior |
| --- | --- |
| `Keyboard.new(options)` | Create a controller. Options: `onDone(text, controller)`, `onCancel(controller)`, `idleAnimation` (defaults to true), `crankSensitivity` (defaults to 1.496, must be positive), `maxLength` (optional nonnegative ASCII character limit). |
| `keyboard:show(text, cursor)` | Open a new draft. Defaults to empty text and cursor at the end. Returns `true`, or `false, error` without taking focus. |
| `keyboard:isVisible()` | Whether the keyboard owns input. |
| `keyboard:update()` | Process input and timers; returns whether visible. |
| `keyboard:draw()` | Render the keyboard and animations when visible. |
| `keyboard:getText()` | Committed draft text, excluding a currently previewed character. |
| `keyboard:clear()` | Clear the draft and preview while keeping the keyboard open and preserving its case/layer. |
| `keyboard:done()` | Commit any preview, close, call `onDone`, and return the final text. |
| `keyboard:cancel()` | Discard edits, restore the initial text, close, and call `onCancel`. |
| `keyboard:close()` | Silently release input and menu entries; no callback or pending-character commit. |

Cursor positions are zero-based insertion offsets from 0 through the text length. Initial text currently supports printable ASCII only; unsupported characters such as umlauts, emoji or line breaks return an explicit error. This is a single-line keyboard, not a multiline editor.

## Host integration contract

- The library draws a full 400×240 screen. Use normal 1× display scale and draw it after your host's rendering, or pause host rendering as in the example.
- The host owns `playdate.update` and the refresh rate. 30 fps is used by the demo; animation timing is based on milliseconds.
- Opening pushes a modal `playdate.inputHandlers` handler. While visible, do not push or pop other handlers above it. Closing pops that handler and restores the previous input route. Only one keyboard may be visible at a time.
- Raw host button polling is independent of Playdate's handler stack: pause that polling while `isVisible()` is true.
- Three free custom System Menu slots are required, in this order: **Clear**, **Done**, **Cancel**. If the host has custom menu entries, it must remove them before opening the keyboard and restore them after closing. Otherwise `show()` returns an error without modifying the host menu. Clear keeps the keyboard open; closing removes the keyboard's three entries.
- Drawing saves/restores its graphics context. The library never changes the display refresh rate or replaces the host's lifecycle callbacks.
- Callback ownership stays with the host: only the host decides how to save, navigate away or discard data.

## Controls

| State | Input | Action |
| --- | --- | --- |
| Letters, no group | D-pad up/right/down/left | `abcd efgh` / `ijkl mnop` / `qrst uvwx` / `yz,. !?-_` |
| Active group | B / A tap | Cycle left / right half. Switching halves commits the previous preview. |
| Active group with preview | A or B hold 350 ms | Commit that one character in the opposite case, preserving the global case mode. |
| Active group | Crank | Cycle through the entire group in either direction. Default sensitivity is 1.496 (approximately 30° per letter step or 24° per number step). |
| Active group | 750 ms without selection input | Commit preview, or reset an empty group. |
| No group | A tap / B tap | Space / Delete before cursor. |
| No group | A hold 350 ms | Toggle upper/lower case; footer shows the destination `HOLD ABC` or `HOLD abc`. |
| No group | First B press held 350 ms | Fold into/out of the tools layer. |
| No group | B tap and release, then another B press held | Within 500 ms, hold 350 ms to start repeated Delete every 100 ms. Stops on release. |
| Tools | D-pad up | Numbers `12345 67890`. |
| Tools, no group | D-pad left/right | Move insertion cursor; holding repeats after 250 ms, then every 100 ms, until release. |
| Tools | D-pad down | Unassigned. |
| System Menu | Clear / Done / Cancel | Clear entire draft / finish with result / discard draft. |

## Build and check

```sh
make test
"$PLAYDATE_SDK_PATH/bin/pdc" Source build/CloudKeyboard.pdx
```

The focused tests check input gestures, cursor editing, menu ownership, callbacks and text-length boundaries. They do not rerun a large exhaustive matrix on each deployment.

Group and cursor tiles are cloud-shaped pills with small irregular rounded lobes. Their ground shadows fade from 50% dither density in the center toward transparent edges and remain still while the tiles float above them. The background uses the same scrolling single-pixel dots and phase on both layers. The input pill has a uniform 4 px outline with no shadow; its glyph baseline is vertically centered. Selected clouds stay white, with only the character circle and its adjacent A/B badge filled black.

`tools/build_artwork.py` regenerates the bundled code-drawn bitmap artwork using Pillow and the included native Oklahoma font bitmap. It is a development tool, not a runtime dependency. Keep the native glyph size, D-pad shape, circles and spacing unchanged when modifying artwork.

## Files

- `Source/keyboard.lua`: public controller, input focus and System Menu lifecycle.
- `Source/keyboard/model.lua`: input state and editing.
- `Source/keyboard/view.lua`: rendering and animations.
- `Source/main.lua`: minimal host example.
- `Source/demo_record.lua`: simulator-only recording utility; not required by consumers.
