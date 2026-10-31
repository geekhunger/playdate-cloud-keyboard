#ifndef HELLO_MESSAGE_MOTION_H
#define HELLO_MESSAGE_MOTION_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define HELLO_MESSAGE_MOTION_PERIOD_MS 1600u
#define HELLO_MESSAGE_MOTION_SAMPLES 64u

typedef struct { float x, y; } HelloMessageMotionSample;
typedef struct { int x, y; } HelloMessageMotion;

// One horizontal figure eight: x=2-2*cos(t), y=-sin(2*t), sampled
// at equal arc-length intervals for constant travel speed. The negative y
// reverses traversal compared with the earlier preview (clockwise first lobe).
// These offline Q12 samples occupy 256 bytes. Periodic Catmull-Rom
// interpolation keeps position and tangent continuous, including at wrap.
static const int16_t kHelloMessageMotion[HELLO_MESSAGE_MOTION_SAMPLES][2] = {
    {0, 0},
    {37, -779},
    {154, -1551},
    {362, -2302},
    {688, -3010},
    {1172, -3618},
    {1840, -4011},
    {2611, -4085},
    {3367, -3899},
    {4070, -3562},
    {4726, -3141},
    {5347, -2668},
    {5942, -2163},
    {6519, -1638},
    {7083, -1099},
    {7640, -551},
    {8192, 0},
    {8744, 551},
    {9301, 1099},
    {9865, 1638},
    {10442, 2163},
    {11037, 2668},
    {11658, 3141},
    {12314, 3562},
    {13017, 3899},
    {13773, 4085},
    {14544, 4011},
    {15212, 3618},
    {15696, 3010},
    {16022, 2302},
    {16230, 1551},
    {16347, 779},
    {16384, 0},
    {16347, -779},
    {16230, -1551},
    {16022, -2302},
    {15696, -3010},
    {15212, -3618},
    {14544, -4011},
    {13773, -4085},
    {13017, -3899},
    {12314, -3562},
    {11658, -3141},
    {11037, -2668},
    {10442, -2163},
    {9865, -1638},
    {9301, -1099},
    {8744, -551},
    {8192, 0},
    {7640, 551},
    {7083, 1099},
    {6519, 1638},
    {5942, 2163},
    {5347, 2668},
    {4726, 3141},
    {4070, 3562},
    {3367, 3899},
    {2611, 4085},
    {1840, 4011},
    {1172, 3618},
    {688, 3010},
    {362, 2302},
    {154, 1551},
    {37, 779},
};

static inline float hello_message_motion_cubic(float a, float b, float c, float d, float t) {
    return b + 0.5f * t * (c - a + t * (2.0f*a - 5.0f*b + 4.0f*c - d +
        t * (3.0f*(b-c) + d-a)));
}

static inline HelloMessageMotionSample hello_message_motion_sample(uint32_t elapsed) {
    const uint32_t position = (elapsed % HELLO_MESSAGE_MOTION_PERIOD_MS) * HELLO_MESSAGE_MOTION_SAMPLES;
    const unsigned i = position / HELLO_MESSAGE_MOTION_PERIOD_MS;
    const float t = (float)(position % HELLO_MESSAGE_MOTION_PERIOD_MS) / HELLO_MESSAGE_MOTION_PERIOD_MS;
    const unsigned a=(i+63u)%64u, b=i, c=(i+1u)%64u, d=(i+2u)%64u;
    HelloMessageMotionSample result;
    result.x=hello_message_motion_cubic(kHelloMessageMotion[a][0],kHelloMessageMotion[b][0],
        kHelloMessageMotion[c][0],kHelloMessageMotion[d][0],t)/4096.0f;
    result.y=hello_message_motion_cubic(kHelloMessageMotion[a][1],kHelloMessageMotion[b][1],
        kHelloMessageMotion[c][1],kHelloMessageMotion[d][1],t)/4096.0f;
    return result;
}

static inline HelloMessageMotion hello_message_motion(uint32_t elapsed, bool mine) {
    const HelloMessageMotionSample sample=hello_message_motion_sample(elapsed);
    int x=(int)lroundf(sample.x), y=(int)lroundf(sample.y);
    if(x<0)x=0;
    if(x>4)x=4;
    if(y < -1)y=-1;
    if(y>1)y=1;
    // Mirror both axes for own bubbles, preserving the first lobe's clockwise traversal.
    return (HelloMessageMotion){mine?-x:x,mine?-y:y};
}

#endif
