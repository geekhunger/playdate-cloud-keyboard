#include "keyboard_cursor_blink.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_legacy_idle_and_reset(void) {
    HelloKeyboardCursorBlink blink={0};
    for(uint32_t now=0;now<4000;now+=7) {
        hello_keyboard_cursor_blink_observe(&blink,now,false);
        assert(!blink.initialized&&!blink.active&&!blink.anchor);
        assert(hello_keyboard_cursor_blink_visible(&blink,now,0)==((now/420)%2==0));
    }
    hello_keyboard_cursor_blink_observe(&blink,999,true);
    hello_keyboard_cursor_blink_reset(&blink);
    assert(!blink.initialized&&!blink.active&&!blink.anchor);
    assert(!hello_keyboard_cursor_blink_visible(&blink,420,420));
}

static void test_continuous_movement_and_stop(void) {
    HelloKeyboardCursorBlink blink={0};
    for(uint32_t now=430;now<8000;now+=11) {
        hello_keyboard_cursor_blink_observe(&blink,now,true);
        assert(blink.active&&blink.initialized&&blink.anchor==now);
        assert(hello_keyboard_cursor_blink_visible(&blink,now,420));
        assert(hello_keyboard_cursor_blink_visible(&blink,now+2000,420));
    }
    hello_keyboard_cursor_blink_observe(&blink,8000,false);
    assert(!blink.active&&blink.anchor==8000);
    const HelloKeyboardCursorBlink stopped=blink;
    for(uint32_t elapsed=0;elapsed<1680;elapsed++) {
        hello_keyboard_cursor_blink_observe(&blink,8000+elapsed,false);
        assert(hello_keyboard_cursor_blink_visible(&blink,8000+elapsed,420)==((elapsed/420)%2==0));
        assert(!memcmp(&blink,&stopped,sizeof(blink)));
    }
    // Resuming during a hidden phase immediately shows the cursor; stopping
    // starts a complete visible phase even after a single moving observation.
    hello_keyboard_cursor_blink_observe(&blink,10000,true);
    assert(hello_keyboard_cursor_blink_visible(&blink,10000,420));
    hello_keyboard_cursor_blink_observe(&blink,10001,false);
    assert(hello_keyboard_cursor_blink_visible(&blink,10420,420));
    assert(!hello_keyboard_cursor_blink_visible(&blink,10421,420));
}

static void test_rollover_custom_period_and_read_only_draw(void) {
    HelloKeyboardCursorBlink blink={0};
    const uint32_t stop=UINT32_MAX-100;
    hello_keyboard_cursor_blink_observe(&blink,stop-20,true);
    hello_keyboard_cursor_blink_observe(&blink,stop,false);
    assert(hello_keyboard_cursor_blink_visible(&blink,stop,420));
    assert(hello_keyboard_cursor_blink_visible(&blink,stop+419,420));
    assert(!hello_keyboard_cursor_blink_visible(&blink,stop+420,420));
    assert(!hello_keyboard_cursor_blink_visible(&blink,stop+839,420));
    assert(hello_keyboard_cursor_blink_visible(&blink,stop+840,420));
    const HelloKeyboardCursorBlink before=blink;
    for(unsigned draw=0;draw<10;draw++) {
        assert(hello_keyboard_cursor_blink_visible(&blink,stop+9,10));
        assert(!hello_keyboard_cursor_blink_visible(&blink,stop+10,10));
        assert(!memcmp(&before,&blink,sizeof(blink)));
    }
}

int main(void) {
    test_legacy_idle_and_reset();
    test_continuous_movement_and_stop();
    test_rollover_custom_period_and_read_only_draw();
    puts("keyboard cursor blink tests: OK");
    return 0;
}
