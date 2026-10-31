#include "popup.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { CALL_MODE, CALL_FILL, CALL_ROUND_FILL, CALL_ROUND_OUTLINE, CALL_LINE };
typedef struct {
    int kind,x,y,width,height,radius,stroke;
    LCDColor color;
} DrawCall;
static DrawCall calls[8];
static unsigned call_count;

static void record(DrawCall call) {
    assert(call_count<sizeof(calls)/sizeof(calls[0]));
    calls[call_count++]=call;
}
static LCDBitmapDrawMode mode(LCDBitmapDrawMode value) {
    record((DrawCall){.kind=CALL_MODE,.color=(LCDColor)value});
    return kDrawModeCopy;
}
static void fill(int x,int y,int width,int height,LCDColor color) {
    record((DrawCall){CALL_FILL,x,y,width,height,0,0,color});
}
static void round_fill(int x,int y,int width,int height,int radius,LCDColor color) {
    record((DrawCall){CALL_ROUND_FILL,x,y,width,height,radius,0,color});
}
static void round_outline(int x,int y,int width,int height,int radius,int stroke,LCDColor color) {
    record((DrawCall){CALL_ROUND_OUTLINE,x,y,width,height,radius,stroke,color});
}
static void line(int x1,int y1,int x2,int y2,int stroke,LCDColor color) {
    record((DrawCall){CALL_LINE,x1,y1,x2,y2,0,stroke,color});
}
static void assert_bounds(HelloPopupBounds bounds,int x,int y,int width,int height) {
    assert(bounds.x==x&&bounds.y==y&&bounds.width==width&&bounds.height==height);
}

static void test_two_line_layout_and_bubble_motion(void) {
    assert_bounds(hello_popup_layout(0,14,8,true),62,83,272,44);
    assert_bounds(hello_popup_layout(0,14,8,false),62,98,272,44);
    assert_bounds(hello_popup_layout(0,20,10,true),62,75,272,60);
    assert_bounds(hello_popup_layout(0,0,-1,true),62,91,272,28);
    assert_bounds(hello_popup_layout(0,1000,8,true),62,8,272,195);
    assert_bounds(hello_popup_layout(0,1000,8,false),62,8,272,224);
    static const struct {uint32_t now;int x,y;} positions[]={
        {0,62,83},{200,63,82},{400,64,83},{600,65,84},
        {800,66,83},{1000,65,82},{1200,64,83},{1400,63,84},{1600,62,83}
    };
    for(unsigned i=0;i<sizeof(positions)/sizeof(positions[0]);i++) {
        assert_bounds(hello_popup_layout(positions[i].now,14,8,true),positions[i].x,positions[i].y,272,44);
        assert_bounds(hello_popup_layout(positions[i].now+1600,14,8,true),positions[i].x,positions[i].y,272,44);
    }
    for(uint32_t now=0;now<3200;now+=7) {
        const HelloPopupBounds bounds=hello_popup_layout(now,14,8,true);
        assert(bounds.x>=62&&bounds.x<=66&&bounds.y>=82&&bounds.y<=84);
        assert(bounds.width==272&&bounds.height==44&&bounds.y+bounds.height<211);
    }
}

static void test_shell_and_fixed_footer(PlaydateAPI* pd) {
    static const uint8_t dim[]={0,0,0,0,0,0,0,0,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55};
    assert(HELLO_POPUP_FOOTER_TOP==211&&HELLO_POPUP_FOOTER_ICON_Y==215);
    assert(HELLO_POPUP_FOOTER_ICON_SIZE==20&&HELLO_POPUP_FOOTER_GAP==4);
    assert(HELLO_POPUP_FOOTER_LEFT==3&&HELLO_POPUP_FOOTER_RIGHT==396);
    for(uint32_t now=0;now<=1600;now+=200) {
        const HelloPopupBounds bounds=hello_popup_layout(now,14,8,true);
        call_count=0;hello_popup_draw_shell(pd,bounds);
        assert(call_count==4&&calls[0].kind==CALL_MODE&&calls[0].color==(LCDColor)kDrawModeCopy);
        assert(calls[1].kind==CALL_FILL&&calls[1].x==0&&calls[1].y==0&&calls[1].width==400&&calls[1].height==240);
        assert(calls[1].color>kColorXOR&&!memcmp((const void*)calls[1].color,dim,sizeof(dim)));
        for(unsigned i=2;i<4;i++) {
            assert(calls[i].x==bounds.x&&calls[i].y==bounds.y&&calls[i].width==bounds.width&&calls[i].height==bounds.height);
            assert(calls[i].radius==12);
        }
        assert(calls[2].kind==CALL_ROUND_FILL&&calls[2].color==kColorWhite);
        assert(calls[3].kind==CALL_ROUND_OUTLINE&&calls[3].stroke==2&&calls[3].color==kColorBlack);
        call_count=0;hello_popup_draw_footer(pd);
        assert(call_count==2&&calls[0].kind==CALL_FILL&&calls[0].x==0&&calls[0].y==211);
        assert(calls[0].width==400&&calls[0].height==29&&calls[0].color==kColorWhite);
        assert(calls[1].kind==CALL_LINE&&calls[1].x==3&&calls[1].y==211);
        assert(calls[1].width==396&&calls[1].height==211&&calls[1].stroke==1&&calls[1].color==kColorBlack);
    }
}

static void test_marquee_pause_speed_loop_and_rollover(void) {
    for(int rollover=0;rollover<2;rollover++) {
        const uint32_t start=rollover?UINT32_MAX-400:1000;
        for(uint32_t elapsed=0;elapsed<20000;elapsed+=13) {
            assert(hello_popup_marquee_offset(start+elapsed,start,239,240)==0);
            assert(hello_popup_marquee_offset(start+elapsed,start,240,240)==0);
        }
        assert(hello_popup_marquee_offset(start,start,300,240)==0);
        assert(hello_popup_marquee_offset(start+800,start,300,240)==0);
        assert(hello_popup_marquee_offset(start+839,start,300,240)==0);
        assert(hello_popup_marquee_offset(start+840,start,300,240)==1);
        assert(hello_popup_marquee_offset(start+879,start,300,240)==1);
        assert(hello_popup_marquee_offset(start+880,start,300,240)==2);
        assert(hello_popup_marquee_offset(start+800+331*40,start,300,240)==331);
        assert(hello_popup_marquee_offset(start+800+332*40,start,300,240)==0);
        assert(hello_popup_marquee_offset(start+840+332*40,start,300,240)==1);
    }
}

int main(void) {
    struct playdate_graphics graphics={0};PlaydateAPI pd={0};pd.graphics=&graphics;
    graphics.setDrawMode=mode;graphics.fillRect=fill;graphics.fillRoundRect=round_fill;
    graphics.drawRoundRect=round_outline;graphics.drawLine=line;
    test_two_line_layout_and_bubble_motion();
    test_shell_and_fixed_footer(&pd);
    test_marquee_pause_speed_loop_and_rollover();
    puts("popup tests: OK");
    return 0;
}
