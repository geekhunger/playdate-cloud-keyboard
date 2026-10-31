#include "keyboard_geometry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check(HelloKeyboardGeometry geometry,int requested) {
    const HelloKeyboardPillBounds left=hello_keyboard_side_bounds(geometry,requested,false);
    const HelloKeyboardPillBounds right=hello_keyboard_side_bounds(geometry,requested,true);
    assert(geometry.vertical_width<=300&&geometry.vertical_width%2==0);
    assert(geometry.left_anchor==geometry.vertical_x);
    assert(geometry.right_anchor==geometry.vertical_x+geometry.vertical_width-1);
    assert(geometry.left_anchor+geometry.right_anchor==399);
    assert(left.width==right.width&&left.width<=156&&left.width<=geometry.side_cap);
    assert(left.x>=8&&left.x+left.width<=164);
    assert(right.x>=236&&right.x+right.width<=392);
    assert(left.x+right.x+left.width==400);
    assert(fabsf((float)left.x+(float)(left.width-1)/2-geometry.left_anchor)<=0.5f);
    assert(fabsf((float)right.x+(float)(right.width-1)/2-geometry.right_anchor)<=0.5f);
}
int main(void) {
    HelloKeyboardGeometry normal=hello_keyboard_geometry(254,45);
    assert(normal.vertical_width==254&&normal.left_anchor==73&&normal.side_cap==130);check(normal,156);
    HelloKeyboardGeometry large=hello_keyboard_geometry(300,45);
    assert(large.vertical_width==300&&large.left_anchor==50&&large.side_cap==84);check(large,156);
    HelloKeyboardGeometry short_pair=hello_keyboard_geometry(58,45);
    assert(short_pair.vertical_width==118&&short_pair.side_cap==46);check(short_pair,45);
    for(int vertical=0;vertical<=450;vertical++)
        for(int minimum=38;minimum<=156;minimum++) {
            const HelloKeyboardGeometry geometry=hello_keyboard_geometry(vertical,minimum);
            assert(geometry.side_cap>=minimum);
            for(int requested=20;requested<=240;requested+=11)check(geometry,requested);
        }
    for(int requested=0;requested<=500;requested++) {
        const HelloKeyboardPillBounds left=hello_keyboard_full_row_side_bounds(requested,false);
        const HelloKeyboardPillBounds right=hello_keyboard_full_row_side_bounds(requested,true);
        assert(left.width==right.width&&left.width<=188&&left.width>=38);
        assert(left.x==8&&right.x+right.width==392);
        assert(right.x-left.x-left.width>=8);
    }
    assert(hello_keyboard_full_row_side_bounds(186,false).x==8);
    assert(hello_keyboard_full_row_side_bounds(186,true).x==206);
    puts("keyboard geometry: inclusive anchors, symmetric centers, screen/D-pad caps and short-pair safety passed");
}
