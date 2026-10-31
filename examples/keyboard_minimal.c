// Complete Playdate C SDK example; the Makefile compiles this file directly.
#include "pd_api.h"
#include "keyboard.h"
#include <string.h>

// The keyboard and Unicode cache contain large persistent buffers. Keep them
// outside Playdate's small C stack, and initialize only once per app lifetime.
static PlaydateAPI* demo_pd;
static HelloKeyboard demo_keyboard;
static BrbUnicodeFont demo_unicode;
static LCDFont* demo_font;
static char demo_value[HELLO_KEYBOARD_TEXT_BYTES + 1];
static bool demo_open;

static bool demo_begin(void) {
    // All source strings and this per-view policy are copied before return.
    const HelloKeyboardLabels labels = {.save = "Save", .discard = "Discard"};
    const HelloKeyboardFieldPolicy field = {
        .type = HELLO_INPUT_PLAINTEXT,
        .min_length = 0,
        .max_length = HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS,
        .max_bytes = HELLO_KEYBOARD_TEXT_BYTES,
        .length_unit = HELLO_KEYBOARD_LENGTH_GRAPHEMES
    };
    const HelloKeyboardOpenOptions options = {.labels = &labels, .field = &field};
    hello_keyboard_set_title(&demo_keyboard, "Your text");
    demo_open = hello_keyboard_open_configured(&demo_keyboard, demo_value, &options);
    return demo_open;
}

static int demo_update(void* userdata) {
    (void)userdata;
    PDButtons current, pushed, released;
    demo_pd->system->getButtonState(&current, &pushed, &released);
    const uint32_t now = demo_pd->system->getCurrentTimeMilliseconds();
    const float crank = demo_pd->system->getCrankChange();
    demo_pd->graphics->clear(kColorWhite);

    if (!demo_open && (pushed & kButtonA)) demo_begin();
    if (demo_open) {
        // Deliver releases even while an animation rejects fresh selections.
        // A+B/Return/Done opens the decision; only a fresh Save emits DONE.
        HelloKeyboardResult result = hello_keyboard_update(&demo_keyboard, now,
            current, pushed, released, crank);
        if (result == HELLO_KEYBOARD_DONE) {
            // DONE keeps the editor open for optional host validation. The text
            // is borrowed: copy it before close/reopen or releasing the instance.
            const char* text = hello_keyboard_text(&demo_keyboard);
            memcpy(demo_value, text, strlen(text) + 1);
            hello_keyboard_close(&demo_keyboard);
            demo_open = false;
        } else if (result == HELLO_KEYBOARD_CANCELLED) {
            // Discard already restored initial text and closed the editor.
            demo_open = false;
        }
        if (demo_open) {
            // Draw completes render-dependent boundary gates; update-only use
            // cannot acknowledge a presented endpoint. Never skip this step.
            hello_keyboard_draw(&demo_keyboard, now);
            return 1;
        }
    }

    const char* hint = "A: edit text";
    demo_pd->graphics->setDrawMode(kDrawModeCopy);
    demo_pd->graphics->drawText(hint, strlen(hint), kASCIIEncoding, 12, 12);
    brb_text_draw_wrapped(&demo_unicode, demo_font, demo_value, 12, 40, 376, false);
    return 1;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg) {
    (void)arg;
    if (event == kEventInit) {
        demo_pd = pd;
        // A system font is available on both Simulator and physical Playdate.
        const char* error = NULL;
        demo_font = pd->graphics->loadFont("/System/Fonts/Roobert-10-Bold.pft", &error);
        if (!demo_font) {
            pd->system->error("Could not load system font: %s", error ? error : "unknown");
            return 0;
        }
        // An unavailable optional atlas still permits embedded Everyday emoji.
        brb_unicode_font_init(&demo_unicode, pd);
        HelloKeyboardConfig config;
        hello_keyboard_default_config(&config);
        config.unicode_font = &demo_unicode;
        config.font = demo_font;
        config.font_size = pd->graphics->getFontHeight(demo_font);
        // Library defaults are deliberate. An app may explicitly choose 0.5
        // button_navigation_scale as brb does; crank behavior is independent.
        hello_keyboard_init_with_config(&demo_keyboard, pd, &config);
        pd->display->setRefreshRate(30);
        demo_begin();
        pd->system->setUpdateCallback(demo_update, NULL);
    } else if (event == kEventTerminate) {
        if (demo_open) hello_keyboard_close(&demo_keyboard);
        demo_open = false;
        brb_unicode_font_close(&demo_unicode);
        // The SDK reclaims application-lifetime loaded assets on process exit.
    }
    return 0;
}
