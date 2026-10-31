#include "input_timing.h"
#include <assert.h>
#include <stdio.h>

_Static_assert(BRB_INPUT_DEFAULT_HOLD_MS==350,"hold policy changed");
_Static_assert(BRB_INPUT_DEFAULT_TAP_WINDOW_MS==500,"tap policy changed");
_Static_assert(BRB_INPUT_DEFAULT_REPEAT_MS==100,"repeat policy changed");

int main(void) {
    BrbInputTimingConfig timing=brb_input_timing_defaults();
    assert(brb_input_timing_valid(&timing));
    assert(timing.hold_ms==350&&timing.tap_window_ms==500&&timing.repeat_ms==100);
    assert(timing.cursor_delay_ms==250&&timing.layer_lock_ms==460);
    assert(!brb_input_timing_valid(NULL));
    timing.repeat_ms=0;assert(!brb_input_timing_valid(&timing));
    timing=brb_input_timing_defaults();timing.hold_ms=UINT32_MAX;assert(!brb_input_timing_valid(&timing));
    const uint32_t start=UINT32_MAX-100;
    assert(brb_input_elapsed(start+349,start)==349);
    assert(!brb_input_reached(start+349,start,350)&&brb_input_reached(start+350,start,350));
    assert(!brb_input_due(start+349,start+350)&&brb_input_due(start+350,start+350));
    for(uint32_t duration=1;duration<2000;duration++)for(uint32_t elapsed=0;elapsed<=duration+1;elapsed++) {
        const unsigned before=elapsed>=duration?21:elapsed*21/duration;
        assert(brb_input_progress(elapsed,duration,21)==before);
    }
    assert(brb_input_progress(UINT32_MAX-1,UINT32_MAX,21)==20);
    assert(brb_input_progress(UINT32_MAX,350,21)==21);
    assert(brb_input_progress(10,0,21)==0&&brb_input_progress(10,350,0)==0);
    puts("input timing: unchanged defaults, progress parity, validation and clock wrap passed");
}
