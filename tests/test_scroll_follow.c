#include "keyboard_scroll.h"
#include <assert.h>
#include <stdio.h>

static void test_endpoint_profile_change_preserves_phase(void) {
    HelloScrollFollow follow={0};HelloKeyboardScrollMotion motion={0};
    hello_scroll_follow_update(&follow,600,0,false);
    hello_keyboard_scroll_follow_update_scaled(&follow,&motion,0,20,true,.5f);
    assert(motion.remaining_ms==250&&motion.time_scale==.5f);
    const float position=follow.position;
    hello_keyboard_scroll_follow_update(&follow,&motion,0,20,true);
    assert(follow.position==position&&motion.remaining_ms==500&&motion.time_scale==1);
    HelloScrollFollow reference=follow,accelerated=follow;
    HelloKeyboardScrollMotion expected={.target=0,.remaining_ms=500,.time_scale=1},fast=motion;
    hello_keyboard_scroll_follow_update_scaled(&accelerated,&fast,0,20,true,.5f);
    assert(accelerated.position==position&&fast.remaining_ms==250);
    for(uint32_t now=40;now<=520;now+=20) {
        const float actual=hello_keyboard_scroll_follow_update(&follow,&motion,0,now,true);
        const float baseline=hello_keyboard_scroll_follow_update(&reference,&expected,0,now,true);
        assert(actual==baseline&&motion.remaining_ms==expected.remaining_ms);
        if(now==260)assert(actual>0); // The old half-speed clock must not survive.
    }
    assert(follow.position==0&&!motion.remaining_ms);
    for(uint32_t now=40;now<=280;now+=20)
        hello_keyboard_scroll_follow_update_scaled(&accelerated,&fast,0,now,true,.5f);
    assert(accelerated.position==0&&!fast.remaining_ms);
    HelloKeyboardScrollMotion legacy={.target=0,.remaining_ms=400};
    reference=(HelloScrollFollow){.position=80,.updated_at=0,.initialized=true};
    hello_keyboard_scroll_follow_update(&reference,&legacy,0,0,true);
    assert(reference.position==80&&legacy.remaining_ms==400&&legacy.time_scale==1);
}

int main(void) {
    test_endpoint_profile_change_preserves_phase();
    HelloScrollFollow follow = {0};
    assert(hello_scroll_follow_update(&follow, 0, 0, false) == 0);
    float position = hello_scroll_follow_update(&follow, 24, 33, true);
    assert(position > 0 && position < 24);
    // Crossing a section adds 30 px to the next destination; the viewport
    // must catch up without adopting that extra distance in one frame.
    float next = hello_scroll_follow_update(&follow, 78, 66, true);
    assert(next > position && next - position < 15);
    position = next;
    for (uint32_t now = 99; now < 3000; now += 33) {
        next = hello_scroll_follow_update(&follow, 78, now, true);
        assert(next >= position && next <= 78);
        position = next;
    }
    assert(position == 78);
    next = hello_scroll_follow_update(&follow, 24, 3000, true);
    assert(next < 78 && next > 24);
    assert(hello_scroll_follow_update(&follow, 0, 3033, false) == 0);
    // Returning to the top must reveal the full first row promptly, while
    // still moving through intermediate positions without overshooting.
    for (uint32_t frame_ms = 16; frame_ms <= 33; frame_ms += 17) {
        HelloScrollFollow top = {0};
        hello_scroll_follow_update(&top, 24, 0, false);
        position = 24;
        for (uint32_t now = frame_ms; now <= 400; now += frame_ms) {
            next = hello_scroll_follow_update(&top, 0, now, true);
            assert(next >= 0 && next <= position);
            assert(position - next < 4);
            position = next;
        }
        assert(position == 0);
    }
    HelloScrollFollow tail = {0};
    hello_scroll_follow_update(&tail, 1, 0, false);
    assert(hello_scroll_follow_update(&tail, 0, 33, true) == 0);
    puts("scroll follow tests: OK");
}
