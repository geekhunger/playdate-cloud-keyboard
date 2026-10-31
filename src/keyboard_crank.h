#ifndef HELLO_KEYBOARD_CRANK_H
#define HELLO_KEYBOARD_CRANK_H

#include "elastic_marker.h"
#include <stdbool.h>
#include <math.h>

// One angular scale shared by letters, numbers, symbols and emoji. This keeps
// the accepted nominal angular scale. Physical breakaway and cycle resistance
// below are multiples of this scale, rather than changes to the SDK delta.
#define HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM (360.0f / 10.0f / 1.496f / 0.9f / 0.92f)
#define HELLO_KEYBOARD_CRANK_RUNNING_FACTOR 1.20f
#define HELLO_KEYBOARD_CRANK_BREAKAWAY_FACTOR 1.55f
// A consumed direction change needs its own, heavier well. Starting a fresh
// group still uses the established lighter breakaway; cycle wells are separate.
#define HELLO_KEYBOARD_CRANK_REVERSAL_FACTOR 1.90f
#define HELLO_KEYBOARD_CRANK_EDGE_FACTOR 2.75f
#define HELLO_KEYBOARD_CRANK_FIRST_EDGE_FACTOR 0.80f
#define HELLO_KEYBOARD_CRANK_MAX_SAMPLE_DEGREES 360.0f
#define HELLO_KEYBOARD_CRANK_RELEASE_ITEMS 5
#define HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR (HELLO_KEYBOARD_CRANK_RUNNING_FACTOR*1.9074074f)
#define HELLO_KEYBOARD_CRANK_SPEED_NORMAL 360.0f
#define HELLO_KEYBOARD_CRANK_SPEED_FAST 1080.0f
#define HELLO_KEYBOARD_CRANK_SPEED_MAX_FACTOR 1.10f
#define HELLO_KEYBOARD_CRANK_SPEED_SMOOTH_MS 80.0f

typedef struct {
    float remainder;
    int release_steps;
    int direction; // Last accepted entry/detent, never the most recent noisy delta.
    float release_peak;
    float velocity, speed_factor;
    uint32_t sampled_at;
    bool sampled;
    bool wrapped; // True only after this crank sequence consumes a group wrap.
    bool entry_breakaway; // Immediate entry still owes its first angular well.
} HelloKeyboardCrankDetent;

static inline void hello_keyboard_crank_reset(HelloKeyboardCrankDetent* state) {
    *state=(HelloKeyboardCrankDetent){0};
}

static inline void hello_keyboard_crank_velocity_reset(HelloKeyboardCrankDetent* state) {
    state->velocity=0;state->speed_factor=1;state->sampled_at=0;state->sampled=false;
}

// Velocity uses the SDK's physical delta and elapsed milliseconds, never the
// number of frames. Idle samples decay velocity but cannot lower the loaded
// angular well and consume already stored pressure without new crank travel.
static inline void hello_keyboard_crank_sample_velocity(HelloKeyboardCrankDetent* state,
    float change,uint32_t now) {
    if(!isfinite(change)){hello_keyboard_crank_velocity_reset(state);return;}
    const uint32_t elapsed=now-state->sampled_at;
    const bool first=!state->sampled;
    state->sampled=true;state->sampled_at=now;
    if(first||elapsed>250) {
        state->velocity=0;
        if(change!=0)state->speed_factor=1;
        return;
    }
    if(!elapsed)return; // No invented speed from same-time partitioned travel.
    const float delta=fminf(fabsf(change),HELLO_KEYBOARD_CRANK_MAX_SAMPLE_DEGREES);
    const float velocity=delta*1000.0f/(float)elapsed;
    const float blend=1-expf(-(float)elapsed/HELLO_KEYBOARD_CRANK_SPEED_SMOOTH_MS);
    state->velocity+=(velocity-state->velocity)*blend;
    if(change!=0) {
        float fraction=(state->velocity-HELLO_KEYBOARD_CRANK_SPEED_NORMAL)/
            (HELLO_KEYBOARD_CRANK_SPEED_FAST-HELLO_KEYBOARD_CRANK_SPEED_NORMAL);
        fraction=fminf(1,fmaxf(0,fraction));
        state->speed_factor=1+(HELLO_KEYBOARD_CRANK_SPEED_MAX_FACTOR-1)*
            fraction*fraction*(3-2*fraction);
    }
}

static inline float hello_keyboard_crank_release_factor(const HelloKeyboardCrankDetent* state) {
    if(state->release_steps<=0)return HELLO_KEYBOARD_CRANK_RUNNING_FACTOR;
    const int steps=state->release_steps>HELLO_KEYBOARD_CRANK_RELEASE_ITEMS?
        HELLO_KEYBOARD_CRANK_RELEASE_ITEMS:state->release_steps;
    const float t=(float)(HELLO_KEYBOARD_CRANK_RELEASE_ITEMS-steps)/
        (float)(HELLO_KEYBOARD_CRANK_RELEASE_ITEMS-1);
    // Advance the release phase reluctantly, retaining the strong initial
    // well. OutCubic then removes excess resistance and gently joins ordinary
    // running travel at the final item, with flat slopes at both ends.
    const float phase=t*t*t;
    const float remaining=1-phase;
    const float curve=remaining*remaining*remaining;
    return HELLO_KEYBOARD_CRANK_RUNNING_FACTOR+
        (fmaxf(HELLO_KEYBOARD_CRANK_RUNNING_FACTOR,state->release_peak)-
        HELLO_KEYBOARD_CRANK_RUNNING_FACTOR)*curve;
}

static inline float hello_keyboard_crank_threshold(const HelloKeyboardCrankDetent* state,bool wraps) {
    const int direction=state->remainder<0?-1:1;
    const bool reversing=state->direction!=0&&state->direction!=direction;
    const float breakaway=reversing?HELLO_KEYBOARD_CRANK_REVERSAL_FACTOR:
        (state->direction==0||state->entry_breakaway)?HELLO_KEYBOARD_CRANK_BREAKAWAY_FACTOR:0;
    const float ordinary=fmaxf(hello_keyboard_crank_release_factor(state),
        breakaway);
    // A group boundary remains one well. Neither breakaway nor taper pressure
    // compounds it into a second wall, including a reversal during release.
    const float edge=HELLO_KEYBOARD_CRANK_RUNNING_FACTOR*HELLO_KEYBOARD_CRANK_EDGE_FACTOR*
        (state->wrapped?1:HELLO_KEYBOARD_CRANK_FIRST_EDGE_FACTOR);
    const float speed=isfinite(state->speed_factor)?
        fminf(HELLO_KEYBOARD_CRANK_SPEED_MAX_FACTOR,fmaxf(1,state->speed_factor)):1;
    return HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM*(wraps?edge:ordinary)*speed;
}

static inline bool hello_keyboard_crank_add(HelloKeyboardCrankDetent* state,float change) {
    if(!isfinite(change)||!isfinite(state->remainder)) {
        hello_keyboard_crank_reset(state);return false;
    }
    if(change==0)return true;
    // A reversal first unwinds the loaded detent. Tiny back-and-forth travel
    // therefore cannot move the angular origin or build a one-way ratchet.
    // A full turn per SDK sample is already faster than useful fine selection.
    // Bound corrupt deltas so the caller can drain all real travel immediately.
    change=fminf(HELLO_KEYBOARD_CRANK_MAX_SAMPLE_DEGREES,
        fmaxf(-HELLO_KEYBOARD_CRANK_MAX_SAMPLE_DEGREES,change));
    state->remainder+=change;
    return true;
}

// Consume only real angular travel. The caller supplies whether the next item
// crosses its group boundary; no clock or update queue is involved.
static inline int hello_keyboard_crank_take(HelloKeyboardCrankDetent* state,bool wraps) {
    if(!isfinite(state->remainder)){hello_keyboard_crank_reset(state);return 0;}
    const int direction=state->remainder<0?-1:1;
    const float threshold=hello_keyboard_crank_threshold(state,wraps);
    if(fabsf(state->remainder)<threshold)return 0;
    const bool reversal=state->direction!=0&&state->direction!=direction;
    const float peak=fmaxf(hello_keyboard_crank_release_factor(state),
        reversal?HELLO_KEYBOARD_CRANK_REVERSAL_FACTOR:HELLO_KEYBOARD_CRANK_BREAKAWAY_FACTOR);
    state->remainder-=(float)direction*threshold;
    state->direction=direction;
    state->entry_breakaway=false;
    if(wraps) {
        state->wrapped=true;
        state->release_peak=HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR;
        state->release_steps=HELLO_KEYBOARD_CRANK_RELEASE_ITEMS;
    } else if(reversal) {
        // The first reverse detent already consumed the peak. Only a consumed
        // direction change restarts the five-item curve; noise cannot rearm it.
        state->release_peak=peak;
        state->release_steps=HELLO_KEYBOARD_CRANK_RELEASE_ITEMS-1;
    } else if(state->release_steps>0)state->release_steps--;
    return direction;
}

// Smooth, flat-ended pressure reflects the same physical angular threshold
// that gates selection. Unwinding through zero does not switch the spring law.
static inline float hello_keyboard_crank_preload(const HelloKeyboardCrankDetent* state,bool wraps) {
    float fraction=fabsf(state->remainder)/hello_keyboard_crank_threshold(state,wraps);
    if(fraction>1)fraction=1;
    const float eased=fraction*fraction*(3-2*fraction);
    return state->remainder<0?-eased:eased;
}

// The keyboard owns its visual tuning; both input mechanisms share the same
// horizontal jelly, while their detents, hold timing and gates stay separate.
static inline const BrbElasticMarkerMotion* hello_keyboard_marker_motion(void) {
    static const BrbElasticMarkerMotion motion={
        .head_omega=42.0f,.head_damping=0.78f,
        .tail_omega=19.0f,.tail_damping=0.64f,
        .maximum_tail=36.0f,.entry_tail=18.0f
    };
    return &motion;
}

static inline BrbElasticMarkerMotion hello_keyboard_button_marker_motion(float time_scale) {
    BrbElasticMarkerMotion motion=*hello_keyboard_marker_motion();
    motion.head_omega/=time_scale;motion.tail_omega/=time_scale;
    return motion;
}

static inline void hello_keyboard_button_marker_update(BrbElasticMarker* marker,int tag,
    float target,float minimum,float maximum,uint32_t now,float time_scale) {
    const BrbElasticMarkerMotion motion=hello_keyboard_button_marker_motion(time_scale);
    brb_elastic_marker_jelly_update(marker,tag,target,0,minimum,maximum,now,&motion);
}

static inline void hello_keyboard_marker_update(BrbElasticMarker* marker,int tag,
    float target,float pull,float minimum,float maximum,uint32_t now) {
    brb_elastic_marker_jelly_update(marker,tag,target,pull,minimum,maximum,now,
        hello_keyboard_marker_motion());
}

static inline void hello_keyboard_marker_enter(BrbElasticMarker* marker,
    float minimum,float maximum) {
    brb_elastic_marker_stretch_entry(marker,marker->target,minimum,maximum,
        hello_keyboard_marker_motion()->entry_tail);
}

static inline void hello_keyboard_crank_marker_update(BrbElasticMarker* marker,int tag,
    float target,float pull,float minimum,float maximum,uint32_t now) {
    hello_keyboard_marker_update(marker,tag,target,pull,minimum,maximum,now);
}

#endif
