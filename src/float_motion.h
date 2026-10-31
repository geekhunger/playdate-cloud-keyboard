#ifndef HELLO_FLOAT_MOTION_H
#define HELLO_FLOAT_MOTION_H

#include <math.h>
#include <stdint.h>

static inline int hello_floating_offset(uint32_t now, float phase_offset) {
    const float phase = (float)now * (6.283185307f / 1600.0f) + phase_offset;
    return (int)lroundf(sinf(phase) * 2.0f);
}

#endif
