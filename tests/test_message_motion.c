#include "message_motion.h"
#include <assert.h>
#include <stdio.h>

static float distance(HelloMessageMotionSample a,HelloMessageMotionSample b) {
    return hypotf(a.x-b.x,a.y-b.y);
}
static float speed(unsigned at) {
    return distance(hello_message_motion_sample(at-1),hello_message_motion_sample(at+1))/2;
}
int main(void) {
    const HelloMessageMotionSample start=hello_message_motion_sample(0);
    assert(fabsf(start.x)<0.001f&&fabsf(start.y)<0.001f);
    float minimum=1,maximum=0;
    for(unsigned at=1;at<1600;at++) {
        const HelloMessageMotionSample p=hello_message_motion_sample(at);
        assert(p.x>=-0.001f&&p.x<=4.001f&&p.y>=-1.001f&&p.y<=1.001f);
        const float v=speed(at);assert(v>0.001f);
        if(v<minimum)minimum=v;
        if(v>maximum)maximum=v;
        const HelloMessageMotion incoming=hello_message_motion(at,false),own=hello_message_motion(at,true);
        assert(incoming.x>=0&&incoming.x<=4&&incoming.y>=-1&&incoming.y<=1);
        assert(own.x==-incoming.x&&own.y==-incoming.y);
        if(at>1)assert(fabsf(speed(at)-speed(at-1))<0.00015f);
    }
    // Equal arc-length timing: no easing near either crossing and no pauses.
    assert(maximum/minimum<1.05f);
    // Start upwards from the left-hand tip: reverse the previous preview.
    assert(hello_message_motion_sample(100).y<0);
    assert(hello_message_motion_sample(900).y<0);
    for(unsigned crossing=400;crossing<=1200;crossing+=800) {
        const HelloMessageMotionSample p=hello_message_motion_sample(crossing);
        assert(fabsf(p.x-2)<0.001f&&fabsf(p.y)<0.001f);
        assert(fabsf(speed(crossing-1)-speed(crossing+1))<0.00002f);
    }
    // Analytic interpolation has the same tangent on either side of period wrap.
    const HelloMessageMotionSample before=hello_message_motion_sample(1599),after=hello_message_motion_sample(1);
    assert(fabsf((start.x-before.x)-(after.x-start.x))<0.0001f);
    assert(fabsf((start.y-before.y)-(after.y-start.y))<0.0001f);
    for(unsigned cycle=0;cycle<100000;cycle+=131)
        for(unsigned phase=0;phase<1600;phase+=17) {
            const HelloMessageMotion a=hello_message_motion(phase,false);
            const HelloMessageMotion b=hello_message_motion(cycle*1600+phase,false);
            assert(a.x==b.x&&a.y==b.y);
        }
    // Unsigned elapsed time is continuous across the Playdate clock wrapping.
    const uint32_t selected_at=UINT32_MAX-600;
    for(uint32_t elapsed=0;elapsed<1800;elapsed+=7) {
        const uint32_t now=selected_at+elapsed;
        const HelloMessageMotion a=hello_message_motion(now-selected_at,false),b=hello_message_motion(elapsed,false);
        assert(a.x==b.x&&a.y==b.y);
    }
    puts("message motion: figure eight, constant speed and reversed traversal, bounds and periodic tangents passed");
}
