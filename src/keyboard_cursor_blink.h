#ifndef HELLO_KEYBOARD_CURSOR_BLINK_H
#define HELLO_KEYBOARD_CURSOR_BLINK_H

#include <stdbool.h>
#include <stdint.h>

enum { HELLO_KEYBOARD_CURSOR_BLINK_DEFAULT_PERIOD_MS = 420 };

typedef struct {
    uint32_t anchor;
    bool active;
    bool initialized;
} HelloKeyboardCursorBlink;

static inline void hello_keyboard_cursor_blink_reset(HelloKeyboardCursorBlink* blink) {
    if(blink)*blink=(HelloKeyboardCursorBlink){0};
}

// Observe from input/update, including the first update after movement ends.
// Until actual activity occurs, retain the legacy global idle blink phase.
static inline void hello_keyboard_cursor_blink_observe(
    HelloKeyboardCursorBlink* blink,uint32_t now,bool moving) {
    if(!blink)return;
    if(moving||blink->active) {
        blink->anchor=now;
        blink->initialized=true;
    }
    blink->active=moving;
}

// Rendering is read-only: repeated draws never extend the visible phase.
// A zero period selects the default duration of each visible/hidden phase.
static inline bool hello_keyboard_cursor_blink_visible(
    const HelloKeyboardCursorBlink* blink,uint32_t now,uint32_t period_ms) {
    if(!period_ms)period_ms=HELLO_KEYBOARD_CURSOR_BLINK_DEFAULT_PERIOD_MS;
    if(blink&&blink->active)return true;
    const uint32_t elapsed=blink&&blink->initialized?now-blink->anchor:now;
    return (elapsed/period_ms)%2==0;
}

#endif
