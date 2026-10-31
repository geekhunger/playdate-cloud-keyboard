#ifndef HELLO_KEYBOARD_SCROLL_H
#define HELLO_KEYBOARD_SCROLL_H

#include "scroll_follow.h"

// Keyboard endpoint travel uses a bounded cubic ease-out over 540ms. It starts
// at the accepted distance/180ms
// velocity and reaches the complete last cell without an exponential tail.
#define HELLO_KEYBOARD_SCROLL_ENDPOINT_MS 540u

typedef struct {
    float target;
    uint32_t remaining_ms;
    float time_scale; // Zero-initialized legacy motions use the 1.0 profile.
} HelloKeyboardScrollMotion;

static inline uint32_t hello_keyboard_scroll_milliseconds(float duration) {
    if(duration>=(float)INT32_MAX)return INT32_MAX;
    const uint32_t rounded=(uint32_t)(duration+0.5f);
    return rounded?rounded:1;
}

static inline float hello_keyboard_scroll_follow_update_scaled(HelloScrollFollow* follow,
    HelloKeyboardScrollMotion* motion,float target,uint32_t now,bool endpoint,float time_scale) {
    if(!isfinite(time_scale)||time_scale<=0)time_scale=1;
    const float previous_scale=isfinite(motion->time_scale)&&motion->time_scale>0?motion->time_scale:1;
    if(motion->remaining_ms&&previous_scale!=time_scale) {
        // A button-controlled endpoint can become ordinary homing with the
        // same target. Preserve its phase/position, but restore the new clock.
        motion->remaining_ms=hello_keyboard_scroll_milliseconds(
            (float)motion->remaining_ms*time_scale/previous_scale);
    }
    motion->time_scale=time_scale;
    if(!endpoint||!follow->initialized) {
        motion->target=target;motion->remaining_ms=0;
        return hello_scroll_follow_update_scaled(follow,target,now,true,time_scale);
    }
    const bool retarget=motion->target!=target||(!motion->remaining_ms&&follow->position!=target);
    if(retarget) {
        motion->target=target;
        const float duration=(float)HELLO_KEYBOARD_SCROLL_ENDPOINT_MS*time_scale;
        motion->remaining_ms=hello_keyboard_scroll_milliseconds(duration);
    }
    uint32_t elapsed=now-follow->updated_at;
    // A newly selected endpoint must not consume time spent idle on a different
    // target. Retain the existing first-frame pause guard, then use real elapsed
    // time so a slow frame cannot indefinitely extend endpoint travel.
    if(retarget&&elapsed>50)elapsed=50;
    if(motion->remaining_ms) {
        const uint32_t step=elapsed<motion->remaining_ms?elapsed:motion->remaining_ms;
        const uint32_t remaining=motion->remaining_ms-step;
        const float ratio=(float)remaining/(float)motion->remaining_ms;
        follow->position=target+(follow->position-target)*ratio*ratio*ratio;
        motion->remaining_ms=remaining;
    } else follow->position=target;
    follow->updated_at=now;
    return follow->position;
}

static inline float hello_keyboard_scroll_follow_update(HelloScrollFollow* follow,
    HelloKeyboardScrollMotion* motion,float target,uint32_t now,bool endpoint) {
    return hello_keyboard_scroll_follow_update_scaled(follow,motion,target,now,endpoint,1.0f);
}

#endif
