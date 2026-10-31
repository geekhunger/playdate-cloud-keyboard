#ifndef HELLO_KEYBOARD_MODEL_H
#define HELLO_KEYBOARD_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "unicode.h"
#include "input_timing.h"
#include "input_field.h"

#define HELLO_KEYBOARD_TEXT_BYTES HELLO_INPUT_TEXT_BYTES
#define HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS HELLO_INPUT_DEFAULT_MAX_CHARACTERS
#define HELLO_KEYBOARD_ITEM_BYTES 160
// Compatibility byte-capacity name; field.max_length owns the character limit.
#define HELLO_KEYBOARD_MAX_TEXT HELLO_KEYBOARD_TEXT_BYTES
#define HELLO_KEYBOARD_BUTTON_NAVIGATION_SCALE_MIN 0.25f
#define HELLO_KEYBOARD_BUTTON_NAVIGATION_SCALE_MAX 2.0f
#define HELLO_KEYBOARD_RETURN_CODEPOINT 0x23ce
#define HELLO_KEYBOARD_RETURN_TEXT "⏎"
#define HELLO_KEYBOARD_GROUP_COUNT 6
#define HELLO_KEYBOARD_NUMBERS_GROUP 5
#define HELLO_KEYBOARD_EMOJI_GROUP 6
#define HELLO_KEYBOARD_MAX_GROUP_CHARS 64
#define HELLO_KEYBOARD_MAX_UTF8_BYTES 4
#define HELLO_KEYBOARD_MAX_GROUP_BYTES 2560 // Fixed budget, independent of cluster count.
// Compatibility name for callers using the historical standalone default.
// Actual gestures use each model's configured timing.
#define HELLO_KEYBOARD_HOLD_MS BRB_INPUT_DEFAULT_HOLD_MS

typedef enum {
    HELLO_KB_UP = 0,
    HELLO_KB_RIGHT = 1,
    HELLO_KB_DOWN = 2,
    HELLO_KB_LEFT = 3
} HelloKeyboardDirection;

typedef enum {
    HELLO_KB_BUTTON_A = 0,
    HELLO_KB_BUTTON_B = 1
} HelloKeyboardButton;

typedef enum {
    HELLO_KB_VIEW_KEYBOARD = 1,
    HELLO_KB_VIEW_EMOJI_SETS = 2
} HelloKeyboardView;

typedef enum {
    HELLO_KB_TRANSFORM_UPPER,
    HELLO_KB_TRANSFORM_LOWER,
    HELLO_KB_TRANSFORM_TOGGLE
} HelloKeyboardTransformAction;

// Optional full-item transform hook, e.g. emoji variants for case-mode changes.
// False means "not handled": built-in Unicode UPPER/TOGGLE remains available.
// LOWER is the unshifted selection mode and retains configured text on false.
// True requires terminated, single-line UTF-8 within capacity; invalid output
// leaves the original item unchanged.
typedef bool (*HelloKeyboardTransform)(const char* input,
    HelloKeyboardTransformAction action, char* output, size_t capacity, void* userdata);

typedef enum {
    HELLO_KB_HOLD_NONE = 0,
    HELLO_KB_HOLD_CASE, // Idle A: tap toggles case; hold chooses a layer.
    HELLO_KB_HOLD_TOOLS, // Reserved former tap guard; never produced.
    HELLO_KB_HOLD_DELETE,
    HELLO_KB_HOLD_REPEAT
} HelloKeyboardHoldType;

typedef struct {
    bool active;
    uint32_t at;
    int group;
    HelloKeyboardHoldType hold;
    bool handled;
    uint32_t next_delete_at;
    uint32_t next_selection_at;
} HelloKeyboardHold;

typedef struct {
    char groups[HELLO_KEYBOARD_GROUP_COUNT + 1][HELLO_KEYBOARD_MAX_GROUP_BYTES + 1];
    char text[HELLO_KEYBOARD_TEXT_BYTES + 1];
    char candidate[HELLO_KEYBOARD_TEXT_BYTES + 1]; // Instance-owned insertion validation scratch.
    size_t cursor;
    HelloKeyboardFieldPolicy field;
    float button_navigation_scale;
    HelloKeyboardView view;
    unsigned view_count;
    bool upper;
    bool shifted_up_enabled; // Default true; false retains the ordinary Up inventory under Shift.
    bool alternate; // Compatibility metadata; it never selects a runtime view.
    int group;
    int index;
    uint32_t pending;
    char pending_text[HELLO_KEYBOARD_ITEM_BYTES + 1];
    HelloKeyboardTransform transform;
    void* transform_userdata;
    int pending_side;
    uint32_t group_at;
    uint32_t pending_at;
    uint32_t layer_at;
    bool layer_from;
    uint32_t feedback_at[2];
    bool feedback_active[2];
    HelloKeyboardHold hold[2];
    HelloInputValidation input_rejected;
    bool cursor_hold;
    HelloKeyboardDirection cursor_direction;
    uint32_t cursor_next_at;
    bool selection_repeated, selection_wrapped; // Consume after each dispatch.
    bool submit_requested; // Return committed; caller consumes this once.
    bool view_confirm_requested; // Short A in the emoji-set view; caller consumes once.
    BrbInputTimingConfig timing;
} HelloKeyboardModel;

void hello_keyboard_model_init(HelloKeyboardModel* model, const char* text, size_t cursor);
void hello_keyboard_model_init_with_groups(
    HelloKeyboardModel* model,
    const char* text,
    size_t cursor,
    const char* const groups[HELLO_KEYBOARD_GROUP_COUNT]);
void hello_keyboard_model_clear(HelloKeyboardModel* model);
// End input gestures after host completion/validation without changing text,
// cursor position, case, groups or policy. Clears selection and submit events.
void hello_keyboard_model_reset_input(HelloKeyboardModel* model);
// Cancel gesture timers/history while preserving text, cursor, group and the
// pending selection. Modal adapters use this to pause input without committing.
void hello_keyboard_model_cancel_gestures(HelloKeyboardModel* model);
bool hello_keyboard_model_set_field_policy(HelloKeyboardModel* model,const HelloKeyboardFieldPolicy* policy);
bool hello_keyboard_model_set_button_navigation_scale(HelloKeyboardModel* model,float scale);
uint32_t hello_keyboard_model_selection_hold_ms(const HelloKeyboardModel* model);
uint32_t hello_keyboard_model_selection_repeat_ms(const HelloKeyboardModel* model);
bool hello_keyboard_model_set_timing(HelloKeyboardModel* model,const BrbInputTimingConfig* timing);
// Standalone models start in the keyboard view with one enabled view. Enabling
// two adds the built-in emoji-set chooser; case variants share the keyboard view.
// Invalid values leave state unchanged. Changing view clears input gestures and
// pending selection without committing; text, cursor and case are preserved.
bool hello_keyboard_model_set_view_count(HelloKeyboardModel* model,unsigned count);
bool hello_keyboard_model_set_view(HelloKeyboardModel* model,HelloKeyboardView view);
unsigned hello_keyboard_model_layer(const HelloKeyboardModel* model);
unsigned hello_keyboard_model_layer_count(const HelloKeyboardModel* model);
// True after idle A reaches its hold threshold or consumes a D-pad chord;
// remains true only while that same A gesture is held. In this mode Up/Down
// cycle the enabled views; Left/Right move immediately, then repeat
// with the button selection delay/interval, at most one grapheme per update.
bool hello_keyboard_model_is_choosing_layer(const HelloKeyboardModel* model);
// Move by complete graphemes without changing text, case or held gestures.
// Returns the signed number actually moved; work is bounded by the text budget.
int hello_keyboard_model_move_cursor(HelloKeyboardModel* model,int steps);
void hello_keyboard_model_commit(HelloKeyboardModel* model, bool keep_group);
void hello_keyboard_model_direction(HelloKeyboardModel* model, HelloKeyboardDirection direction, uint32_t now);
// Shared inventory resolution for presentation and input. Disabling the shifted
// Up alternative retains the configured ordinary group without changing case.
int hello_keyboard_model_direction_group(const HelloKeyboardModel* model,HelloKeyboardDirection direction);
// Releasing the selected group's direction commits its pending item once.
// `now` uses the same millisecond clock as update. This commits the current
// item without advancing a due selection repeat, then clears the group and its
// selection holds; an empty selection inserts nothing.
void hello_keyboard_model_direction_release(HelloKeyboardModel* model, HelloKeyboardDirection direction, uint32_t now);
// With a group active, A selects/advances from the left; B from the right.
// In the emoji-set view ordinary directions belong to the presentation adapter,
// B is ignored, and short A requests confirmation instead of changing case.
// With no group active, holding A owns navigation and B is ignored until a
// fresh press after A ends. Each selection press advances immediately. Holding
// repeats at the scaled selection delay/interval (one step per update, with no
// catch-up burst). Case changes only through an idle A tap. Idle B deletes on
// press, repeats at hold_ms/repeat_ms, and never deletes again on release.
void hello_keyboard_model_press(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now);
// Optional first-selection index supplied by a presentation adapter, e.g. the
// final visible item for B. Applies only while no item is pending; -1 or an
// out-of-range value keeps the logical edge. Capacity filtering keeps direction.
void hello_keyboard_model_press_from(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now, int first_index);
void hello_keyboard_model_release(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now);
void hello_keyboard_model_crank(HelloKeyboardModel* model, int steps, uint32_t now);
// Optional first item for a crank adapter. Subsequent steps still traverse the
// complete group; -1/out-of-range keeps the standalone logical-edge default.
void hello_keyboard_model_crank_from(HelloKeyboardModel* model, int steps, uint32_t now, int first_index);
void hello_keyboard_model_update(HelloKeyboardModel* model, uint32_t now);
// A presentation gate may pause only selection repeats. Blocked deadlines are
// retimed rather than queued; delete/cursor mechanics continue normally.
void hello_keyboard_model_update_with_selection(HelloKeyboardModel* model, uint32_t now, bool allowed);
void hello_keyboard_model_release_with_selection(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now, bool allowed);

const char* hello_keyboard_default_group_chars(int group);
bool hello_keyboard_model_group_is_valid(const char* characters);
const char* hello_keyboard_model_group_chars(const HelloKeyboardModel* model, int group);
int hello_keyboard_model_group_length(const HelloKeyboardModel* model, int group);
uint32_t hello_keyboard_model_group_codepoint(
    const HelloKeyboardModel* model, int group, int index);
size_t hello_keyboard_model_group_text(const HelloKeyboardModel* model,
    int group, int index, char* output, size_t capacity);
size_t hello_keyboard_model_pending_text(const HelloKeyboardModel* model,
    char* output, size_t capacity);
// Strict scalar-value UTF-8. Decode returns -1 for malformed input, 0 at NUL.
int hello_keyboard_utf8_decode(const char* text, uint32_t* codepoint);
int hello_keyboard_utf8_encode(uint32_t codepoint, char output[5]);
// SIZE_MAX denotes malformed UTF-8.
size_t hello_keyboard_utf8_count(const char* text);

#endif
