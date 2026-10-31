#ifndef HELLO_KEYBOARD_H
#define HELLO_KEYBOARD_H

#include "pd_api.h"
#include "input_field.h"
#include "keyboard_model.h"
#include "elastic_marker.h"
#include "unicode_font.h"
#include "scroll_follow.h"
#include "keyboard_scroll.h"
#include "keyboard_labels.h"
#include "keyboard_cursor_blink.h"

#include <stdbool.h>

#define HELLO_KEYBOARD_MAX_TITLE 96
#define HELLO_KEYBOARD_MAX_ERROR 96
#define HELLO_KEYBOARD_DEFAULT_FEEDBACK_MS 5000

/** Resolve a caller-defined feedback code into a single-line message.
 * The caller owns userdata for as long as the configured callback may run.
 * The returned message is copied before show_feedback returns; returning NULL
 * selects fallback. Codes, translations, validation and business rules belong
 * to the caller, and code may be NULL when the caller has no code to supply.
 */
typedef const char* (*HelloKeyboardFeedbackResolver)(void* userdata,
    const char* code, const char* fallback);
typedef void (*HelloKeyboardEmojiSetSelected)(void* userdata,const BrbEmojiSet* set);

typedef struct {
    // Reversible first-face gesture only; catalogs, field policy and validation
    // scratch are not copied. Storage is per instance, never on the ARM stack.
    char text[HELLO_KEYBOARD_TEXT_BYTES+1];
    char pending_text[HELLO_KEYBOARD_ITEM_BYTES+1];
    size_t cursor;
    uint32_t pending,pending_at,group_at,at;
    int group,index,pending_side;
    HelloKeyboardView view;
    bool upper,valid;
    PDButtons face;
} HelloKeyboardChordCheckpoint;

/** Feedback replaces the title temporarily using the existing error banner. */
typedef struct {
    uint32_t duration_ms; // Zero selects the 5-second library default.
    HelloKeyboardFeedbackResolver resolver;
    void* userdata;
} HelloKeyboardFeedbackConfig;

/** Per-view options copied before opening. A field override applies only to
 * this view; omitting it uses Config.field without inheriting the last override.
 * NULL options, a NULL labels pointer or a NULL feedback pointer reset the
 * corresponding configuration to library defaults for the new view.
 */
typedef struct {
    const HelloKeyboardLabels* labels;
    const HelloKeyboardFieldPolicy* field;
    const HelloKeyboardFeedbackConfig* feedback;
} HelloKeyboardOpenOptions;

typedef enum {
    HELLO_KEYBOARD_NONE = 0,
    HELLO_KEYBOARD_DONE,
    HELLO_KEYBOARD_CANCELLED
} HelloKeyboardResult;

typedef enum {
    HELLO_KEYBOARD_DIALOG_CANCEL=0,
    HELLO_KEYBOARD_DIALOG_SUBMIT
} HelloKeyboardDialogKind;

enum { HELLO_KEYBOARD_MENU_CLEAR=1,HELLO_KEYBOARD_MENU_DONE=2,
    HELLO_KEYBOARD_MENU_CANCEL=4,HELLO_KEYBOARD_MENU_HIDE=8,HELLO_KEYBOARD_MENU_ALL=7 };

typedef enum {
    HELLO_KEYBOARD_DPAD_BLACK_NEUTRAL=0,
    HELLO_KEYBOARD_DPAD_UP,
    HELLO_KEYBOARD_DPAD_RIGHT,
    HELLO_KEYBOARD_DPAD_DOWN,
    HELLO_KEYBOARD_DPAD_LEFT,
    HELLO_KEYBOARD_DPAD_ALL_LIGHT
} HelloKeyboardDpadState;

typedef struct {
    // Each group contains 2..64 grapheme clusters, at most 2,560 UTF-8 bytes.
    // Long groups scroll horizontally; configured text is never compressed.
    // [0..3]: lowercase Up punctuation, Right/Down/Left letters;
    // [4]: the number-field Up group; [5]: uppercase Up emoji.
    // NULL/invalid uses the corresponding default.
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT];
    LCDFont* font;
    int font_size;
    const char* title;
    HelloKeyboardFieldPolicy field; // Instance default; an open option can override it.
    unsigned menu_actions; // CLEAR/DONE/CANCEL/HIDE bits; legacy defaults remain 7.
    HelloKeyboardTransform transform;
    void* transform_userdata;
    BrbUnicodeFont* unicode_font;
    bool emoji_input_enabled; // Default true; false keeps ordinary Shift+Up symbols and hides the emoji-set view.
    bool emoji_picker_enabled; // Default true: base keyboard plus emoji-set view.
    // Borrowed immutable catalog, including each set's strings/artwork. Keep it
    // alive for this instance. NULL/empty uses the shipped Everyday set only.
    const BrbEmojiSet* const* emoji_sets;
    size_t emoji_set_count;
    HelloKeyboardEmojiSetSelected on_emoji_set_selected;
    void* emoji_set_userdata;
    // Per-instance gesture policy. A zero-initialized config uses defaults.
    BrbInputTimingConfig timing;
    float button_navigation_scale; // A/B selection only; zero config uses 1.0.
    // App-owned row geometry. Zero keeps the library's original row heights.
    int group_height;
    // Optional first-view cell count per group. Zero keeps automatic sizing;
    // larger counts are capped at 64 and the available axis width.
    unsigned visible_prefix_items[HELLO_KEYBOARD_GROUP_COUNT];
    // Labels are copied into the instance, up to 48 UTF-8 bytes per field,
    // without cutting a code point. NULL/invalid fields use library defaults;
    // an empty string hides the caption. Custom footer text clips to the
    // existing cells; physical button icons and geometry stay fixed. Active
    // selection uses select_text after the D-pad and selection_text after the
    // A/B pair; left/right are retained
    // for compatibility with earlier callers.
    // hold_lower/hold_upper name the idle case action; layer_text names the
    // held-A layer caption (a single layer keeps the case caption instead;
    // multiple layers generate the default from their count and active index).
    HelloKeyboardLabels labels;
    // The config is copied; callback userdata remains owned by the caller.
    HelloKeyboardFeedbackConfig feedback;
} HelloKeyboardConfig;

typedef struct {
    PlaydateAPI* pd;
    HelloKeyboardConfig config;
    char group_storage[HELLO_KEYBOARD_GROUP_COUNT][HELLO_KEYBOARD_MAX_GROUP_BYTES + 1];
    char title_storage[HELLO_KEYBOARD_MAX_TITLE + 1];
    HelloKeyboardLabelStorage label_storage;
    uint32_t title_started_at;
    char error_storage[HELLO_KEYBOARD_MAX_ERROR + 1];
    uint32_t error_started;
    uint32_t error_until;
    HelloKeyboardModel model;
    const BrbEmojiSet* selected_emoji_set;
    size_t emoji_picker_index;
    char initial_text[HELLO_KEYBOARD_TEXT_BYTES + 1];
    struct {
        char raw[HELLO_KEYBOARD_TEXT_BYTES+HELLO_KEYBOARD_ITEM_BYTES+1];
        char shown[HELLO_KEYBOARD_TEXT_BYTES+HELLO_KEYBOARD_ITEM_BYTES+1];
    } preview;
    bool visible;
    HelloInputField input_field;
    HelloKeyboardCursorBlink cursor_blink;
    PDButtons chord_held_faces; // Fresh, unrejected face presses still physically down.
    HelloKeyboardChordCheckpoint chord_checkpoint;
    PDButtons deferred_repeat_button;
    int deferred_repeat_group,deferred_repeat_index;
    uint32_t deferred_repeat_at;
    bool cancel_dialog_active;
    HelloKeyboardDialogKind cancel_dialog_kind;
    bool cancel_dialog_ready; // Both opening face buttons have been released.
    bool dialog_save_confirmed; // Internal: only a fresh Save tap may emit DONE.
    uint32_t cancel_dialog_started_at;
    int requested_action;
    float crank_remainder;
    int crank_release_steps;
    int crank_release_direction; // Last accepted crank entry/detent direction.
    float crank_release_peak;
    float crank_velocity, crank_speed_factor;
    uint32_t crank_sampled_at;
    bool crank_sampled;
    bool crank_wrapped;
    bool crank_entry_breakaway;
    int crank_marker_rebase_group;
    int crank_marker_pinned_group;
    int selection_lock_group, selection_lock_index;
    bool selection_lock_crank;
    PDButtons rejected_face_buttons;
    PDButtons footer_dpad_buttons; // Raw held directions for the group-free layer chooser.
    PDButtons footer_buttons; // All six physical buttons, before logical input consumption.
    BrbElasticMarker selection_marker;
    HelloScrollFollow group_scroll[HELLO_KEYBOARD_GROUP_COUNT + 1];
    HelloKeyboardScrollMotion group_scroll_motion[HELLO_KEYBOARD_GROUP_COUNT + 1];
    PDMenuItem* clear_item;
    PDMenuItem* done_item;
    PDMenuItem* cancel_item;
    PDMenuItem* hide_item; // HIDE is available only for masked/password fields.
    char hide_option_storage[2][HELLO_KEYBOARD_MAX_LABEL+1];
    const char* hide_options[2];
    // Loaded once: glyphs, shared 23px hold art, and five D-pad states.
    LCDBitmapTable* art[3];
    bool initial_alternate;
} HelloKeyboard;

void hello_keyboard_init(HelloKeyboard* keyboard, PlaydateAPI* pd);
void hello_keyboard_default_config(HelloKeyboardConfig* config);
void hello_keyboard_init_with_config(HelloKeyboard* keyboard, PlaydateAPI* pd,
                                     const HelloKeyboardConfig* config);
// Copies a valid policy without restarting active gesture/state machines.
bool hello_keyboard_set_timing(HelloKeyboard* keyboard,const BrbInputTimingConfig* timing);
// Valid finite range is 0.25..2.0; invalid values leave instance/model unchanged.
bool hello_keyboard_set_button_navigation_scale(HelloKeyboard* keyboard,float scale);
// The selected borrowed set persists across close/open. Selecting a different
// set updates group 6, the configured Unicode renderer and the optional callback.
const BrbEmojiSet* hello_keyboard_emoji_set(const HelloKeyboard* keyboard);
void hello_keyboard_set_title(HelloKeyboard* keyboard, const char* title);
/** Copy all labels into owned instance storage; NULL restores defaults.
 * Safe before opening and while visible. Visible menu items retain their
 * callbacks and userdata and are renamed in place, with no added/removed items.
 * Hide option captions are copied when opening; its title and popup captions
 * update immediately, and new option captions apply on the next open.
 * Returns false without changing any label when a visible keyboard's SDK
 * lacks setMenuItemTitle (or keyboard is NULL). Source strings may be released
 * immediately after the call. This changes captions only, never their actions.
 */
bool hello_keyboard_set_labels(HelloKeyboard* keyboard, const HelloKeyboardLabels* labels);
/** Copy the feedback policy; NULL resets to 5000ms with no resolver.
 * A zero duration also selects 5000ms. The caller retains ownership of userdata.
 */
void hello_keyboard_set_feedback_config(HelloKeyboard* keyboard,
                                        const HelloKeyboardFeedbackConfig* feedback);
/** Resolve and immediately copy a message, then display it for the configured
 * duration. NULL fallback means empty text; a NULL resolver result uses fallback.
 * Existing input/clear-error behavior restores the title as usual.
 */
void hello_keyboard_show_feedback(HelloKeyboard* keyboard, const char* code,
                                  const char* fallback, uint32_t now);
void hello_keyboard_clear_error(HelloKeyboard* keyboard);
void hello_keyboard_show_error(HelloKeyboard* keyboard, const char* error,
                               uint32_t now, uint32_t duration_ms);
// Compatibility metadata only: the removed number/cursor page is never opened.
// Field types filter committed values; every field uses the same full keyboard.
void hello_keyboard_set_initial_layer(HelloKeyboard* keyboard, bool alternate);
// Instance policy for later opens; fails without mutation while visible.
// Disabled retains the emoji catalog/art; Shift+Up uses the ordinary Up group.
bool hello_keyboard_set_emoji_input_enabled(HelloKeyboard* keyboard,bool enabled);
// Invalid policies leave all state unchanged. max_bytes=0 selects 4096 bytes.
bool hello_keyboard_set_field_policy(HelloKeyboard* keyboard,const HelloKeyboardFieldPolicy* policy);
// Opens the configured field type. Initial text may be below minimum; maximum,
// byte budget and type are checked before opening. Minimum is checked on Save.
bool hello_keyboard_open_configured(HelloKeyboard* keyboard,const char* text,const HelloKeyboardOpenOptions* options);
void hello_keyboard_set_max_length(HelloKeyboard* keyboard, size_t max_length);
void hello_keyboard_set_group_height(HelloKeyboard* keyboard,int pixels);
void hello_keyboard_set_visible_prefix(HelloKeyboard* keyboard,int group,unsigned count);
bool hello_keyboard_open(HelloKeyboard* keyboard, const char* text);
bool hello_keyboard_open_field(HelloKeyboard* keyboard, const char* text,
                               HelloInputType type);
/** Open with explicit per-view labels and feedback; omitted values reset to
 * library defaults, preventing settings from leaking from the previous view.
 * Configuration/string data is copied; resolver userdata remains caller-owned.
 * Returns false if already open or text violates the existing field rules.
 * Legacy open/open_field retain instance settings for subsequent opens.
 */
bool hello_keyboard_open_field_with_options(HelloKeyboard* keyboard, const char* text,
                                            HelloInputType type,
                                            const HelloKeyboardOpenOptions* options);
void hello_keyboard_close(HelloKeyboard* keyboard);
// DONE leaves the keyboard open so the caller can validate the text and
// show an error in place. Close it explicitly after accepting the value.
// Keep drawing visible frames: completed boundary motion releases temporary
// input gates. Rejected presses/deltas are discarded; keep supplying releases.
// A fresh overlapping A+B tap, physical Return and host/menu DONE open the same editing
// decision. B remains unmapped while held A owns cursor/layer navigation.
// Release opening buttons, then tap fresh B to Discard/CANCELLED or fresh A
// to Save. Opening and choices use no hold timer or progress. Coalesced full
// taps are accepted; stale/rejected held bits never open the dialog.
// Simultaneous choices reset until both are released.
// Only a valid confirmed Save emits DONE; invalid Save retains draft/cursor/mask
// and shows field feedback. Opening the dialog never validates or trims text.
// HIDE affects only password masking, defaulting to No for each open.
HelloKeyboardResult hello_keyboard_update(HelloKeyboard* keyboard,
                                           uint32_t now,
                                           PDButtons current,
                                           PDButtons pushed,
                                           PDButtons released,
                                           float crank_change);
void hello_keyboard_draw(HelloKeyboard* keyboard, uint32_t now);
void hello_keyboard_draw_hold_indicator(HelloKeyboard* keyboard, char button,
                                        int fill, int x, int y);
// Shared native 20px control: fixed 2px black outline, bottom-up white hold
// progress (0..21), with the same physical-button glyph as the keyboard footer.
void hello_keyboard_draw_footer_indicator(HelloKeyboard* keyboard,char button,
                                          int fill,int x,int y);
// Native 20px family: black base with a light directional arm, or one continuous
// light cross interior. Uses the same 2px contour as the keyboard's A/B icons.
void hello_keyboard_draw_dpad(HelloKeyboard* keyboard,HelloKeyboardDpadState state,int x,int y);
const char* hello_keyboard_text(const HelloKeyboard* keyboard);

#endif
