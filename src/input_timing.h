#ifndef BRB_INPUT_TIMING_H
#define BRB_INPUT_TIMING_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

// SDK-independent gesture mechanics. A product owns the configured policy;
// these defaults preserve the keyboard's established standalone behavior.
enum {
    BRB_INPUT_DEFAULT_HOLD_MS=350,
    BRB_INPUT_DEFAULT_TAP_WINDOW_MS=500,
    BRB_INPUT_DEFAULT_REPEAT_MS=100,
    BRB_INPUT_DEFAULT_CURSOR_DELAY_MS=250,
    BRB_INPUT_DEFAULT_LAYER_LOCK_MS=460
};
typedef struct {
    uint32_t hold_ms, tap_window_ms, repeat_ms;
    uint32_t cursor_delay_ms, layer_lock_ms;
} BrbInputTimingConfig;
#define BRB_INPUT_TIMING_DEFAULTS {BRB_INPUT_DEFAULT_HOLD_MS,BRB_INPUT_DEFAULT_TAP_WINDOW_MS,BRB_INPUT_DEFAULT_REPEAT_MS,BRB_INPUT_DEFAULT_CURSOR_DELAY_MS,BRB_INPUT_DEFAULT_LAYER_LOCK_MS}

static inline BrbInputTimingConfig brb_input_timing_defaults(void) {
    return (BrbInputTimingConfig)BRB_INPUT_TIMING_DEFAULTS;
}
static inline bool brb_input_timing_valid(const BrbInputTimingConfig* config) {
    return config&&config->hold_ms&&config->hold_ms<=INT32_MAX&&
        config->tap_window_ms&&config->tap_window_ms<=INT32_MAX&&
        config->repeat_ms&&config->repeat_ms<=INT32_MAX&&
        config->cursor_delay_ms&&config->cursor_delay_ms<=INT32_MAX&&
        config->layer_lock_ms&&config->layer_lock_ms<=INT32_MAX;
}
static inline uint32_t brb_input_elapsed(uint32_t now,uint32_t started_at) {
    return now-started_at;
}
static inline bool brb_input_reached(uint32_t now,uint32_t started_at,uint32_t duration) {
    return brb_input_elapsed(now,started_at)>=duration;
}
static inline bool brb_input_due(uint32_t now,uint32_t deadline) {
    return (int32_t)(now-deadline)>=0;
}
static inline unsigned brb_input_progress(uint32_t elapsed,uint32_t duration,unsigned steps) {
    if(!duration||!steps)return 0;
    if(elapsed>=duration)return steps;
    // Normal gesture durations take only 32-bit arithmetic on the device.
    if(elapsed<=UINT32_MAX/steps)return (unsigned)(elapsed*steps/duration);
    return (unsigned)((uint64_t)elapsed*steps/duration);
}

#endif
