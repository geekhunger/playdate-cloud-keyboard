#ifndef HELLO_SCROLL_FOLLOW_H
#define HELLO_SCROLL_FOLLOW_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    float position;
    uint32_t updated_at;
    bool initialized;
} HelloScrollFollow;

static inline float hello_scroll_follow_update_scaled(
    HelloScrollFollow* follow,float target,uint32_t now,bool animated,float time_scale) {
    if(!isfinite(time_scale)||time_scale<=0)time_scale=1;
    uint32_t elapsed = follow->initialized ? now - follow->updated_at : 0;
    if (elapsed > 50) elapsed = 50;
    if (!follow->initialized || !animated) {
        follow->position = target;
    } else {
        const float distance = target - follow->position;
        if (distance > -0.25f && distance < 0.25f)
            follow->position = target;
        else {
            const float remaining = distance < 0 ? -distance : distance;
            float step = remaining * (float)elapsed / (180.0f*time_scale + (float)elapsed);
            // Keep the soft catch-up, but finish the pixel tail at 60 px/s.
            // Full-row visibility must not wait for an asymptotic last pixel.
            const float minimum_step = 0.06f * (float)elapsed/time_scale;
            if (step < minimum_step) step = minimum_step;
            if (step >= remaining)
                follow->position = target;
            else
                follow->position += distance < 0 ? -step : step;
        }
    }
    follow->updated_at = now;
    follow->initialized = true;
    return follow->position;
}

static inline float hello_scroll_follow_update(HelloScrollFollow* follow,
    float target,uint32_t now,bool animated) {
    return hello_scroll_follow_update_scaled(follow,target,now,animated,1.0f);
}

#endif
