#ifndef HELLO_KEYBOARD_GEOMETRY_H
#define HELLO_KEYBOARD_GEOMETRY_H

#include <stdbool.h>

enum {
    HELLO_KEYBOARD_SCREEN_WIDTH=400,
    HELLO_KEYBOARD_OUTER_MARGIN=8,
    HELLO_KEYBOARD_SIDE_INNER_EDGE=164,
    HELLO_KEYBOARD_SIDE_MAX_WIDTH=156,
    HELLO_KEYBOARD_VERTICAL_MAX_WIDTH=300,
    HELLO_KEYBOARD_SIDE_MIN_WIDTH=38,
    HELLO_KEYBOARD_GROUP_PADDING=12
};

typedef struct {
    int vertical_width, vertical_x;
    int left_anchor, right_anchor, side_cap;
} HelloKeyboardGeometry;
typedef struct { int x, width; } HelloKeyboardPillBounds;

// Without a central control, character pills may use their whole screen half.
// Keep the same outer padding and at least eight pixels between the pair.
static inline HelloKeyboardPillBounds hello_keyboard_full_row_side_bounds(int natural_width,bool right) {
    const int maximum=(HELLO_KEYBOARD_SCREEN_WIDTH-2*HELLO_KEYBOARD_OUTER_MARGIN-8)/2;
    int width=natural_width;
    if(width<HELLO_KEYBOARD_SIDE_MIN_WIDTH)width=HELLO_KEYBOARD_SIDE_MIN_WIDTH;
    if(width>maximum)width=maximum;
    return (HelloKeyboardPillBounds){right?HELLO_KEYBOARD_SCREEN_WIDTH-HELLO_KEYBOARD_OUTER_MARGIN-width:HELLO_KEYBOARD_OUTER_MARGIN,width};
}

// All anchors refer to inclusive visible pixel edges. Even vertical widths
// put the pair's mirror axis exactly at screen pixel199.5. A side pill with
// an even width may then differ from its anchor by half a pixel, symmetrically.
static inline HelloKeyboardGeometry hello_keyboard_geometry(int natural_vertical,int minimum_side) {
    if(minimum_side<HELLO_KEYBOARD_SIDE_MIN_WIDTH)minimum_side=HELLO_KEYBOARD_SIDE_MIN_WIDTH;
    if(minimum_side>HELLO_KEYBOARD_SIDE_MAX_WIDTH)minimum_side=HELLO_KEYBOARD_SIDE_MAX_WIDTH;
    const int half=HELLO_KEYBOARD_SCREEN_WIDTH/2;
    const int minimum_vertical=2*(half-HELLO_KEYBOARD_SIDE_INNER_EDGE)+minimum_side;
    int maximum_vertical=HELLO_KEYBOARD_SCREEN_WIDTH-2*HELLO_KEYBOARD_OUTER_MARGIN-minimum_side;
    if(maximum_vertical>HELLO_KEYBOARD_VERTICAL_MAX_WIDTH)maximum_vertical=HELLO_KEYBOARD_VERTICAL_MAX_WIDTH;
    int width=natural_vertical;
    if(width<minimum_vertical)width=minimum_vertical;
    if(width>maximum_vertical)width=maximum_vertical;
    if(width%2)width+=width<maximum_vertical?1:-1;
    const int left=(HELLO_KEYBOARD_SCREEN_WIDTH-width)/2;
    int cap=2*(left-HELLO_KEYBOARD_OUTER_MARGIN);
    const int inner_cap=2*(HELLO_KEYBOARD_SIDE_INNER_EDGE-left);
    if(cap>inner_cap)cap=inner_cap;
    if(cap>HELLO_KEYBOARD_SIDE_MAX_WIDTH)cap=HELLO_KEYBOARD_SIDE_MAX_WIDTH;
    return (HelloKeyboardGeometry){width,left,left,HELLO_KEYBOARD_SCREEN_WIDTH-1-left,cap};
}

static inline HelloKeyboardPillBounds hello_keyboard_side_bounds(HelloKeyboardGeometry geometry,int natural_width,bool right) {
    int width=natural_width;
    if(width<HELLO_KEYBOARD_SIDE_MIN_WIDTH)width=HELLO_KEYBOARD_SIDE_MIN_WIDTH;
    if(width>geometry.side_cap)width=geometry.side_cap;
    const int left=(2*geometry.left_anchor-(width-1))/2;
    return (HelloKeyboardPillBounds){right?HELLO_KEYBOARD_SCREEN_WIDTH-left-width:left,width};
}

#endif
