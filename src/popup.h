#ifndef HELLO_POPUP_H
#define HELLO_POPUP_H

#include "pd_api.h"
#include "message_motion.h"

enum {
    HELLO_POPUP_WIDTH=272,
    HELLO_POPUP_PADDING=8,
    HELLO_POPUP_FOOTER_TOP=211,
    HELLO_POPUP_FOOTER_ICON_Y=215,
    HELLO_POPUP_FOOTER_ICON_SIZE=20,
    HELLO_POPUP_FOOTER_GAP=4,
    HELLO_POPUP_FOOTER_LEFT=3,
    HELLO_POPUP_FOOTER_RIGHT=396,
    HELLO_POPUP_TEXT_INSET=16
};

typedef struct { int x,y,width,height; } HelloPopupBounds;

// A two-line message body without metadata. The caller supplies its own font
// height and message padding; only the bubble follows the shared motion path.
static inline HelloPopupBounds hello_popup_layout(uint32_t now,int line_height,
    int padding,bool footer) {
    if(line_height<1)line_height=14;
    if(padding<0)padding=0;
    const int available=footer?HELLO_POPUP_FOOTER_TOP:240;
    int height=2*line_height+2*padding;
    if(height>available-16)height=available-16;
    const HelloMessageMotion motion=hello_message_motion(now,false);
    return (HelloPopupBounds){(400-HELLO_POPUP_WIDTH)/2+motion.x-2,
        (available-height)/2+motion.y,HELLO_POPUP_WIDTH,height};
}

static inline void hello_popup_draw_shell(PlaydateAPI* pd,HelloPopupBounds bounds) {
    static LCDPattern dim={0,0,0,0,0,0,0,0,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55};
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->fillRect(0,0,400,240,(LCDColor)dim);
    pd->graphics->fillRoundRect(bounds.x,bounds.y,bounds.width,bounds.height,12,kColorWhite);
    pd->graphics->drawRoundRect(bounds.x,bounds.y,bounds.width,bounds.height,12,2,kColorBlack);
}

static inline void hello_popup_draw_footer(PlaydateAPI* pd) {
    pd->graphics->fillRect(0,HELLO_POPUP_FOOTER_TOP,400,240-HELLO_POPUP_FOOTER_TOP,kColorWhite);
    pd->graphics->drawLine(HELLO_POPUP_FOOTER_LEFT,HELLO_POPUP_FOOTER_TOP,
        HELLO_POPUP_FOOTER_RIGHT,HELLO_POPUP_FOOTER_TOP,1,kColorBlack);
}

// The host owns the start timestamp and clipped text rendering. A short line
// remains centered. Long lines pause first, then loop at the existing 25px/s.
static inline int hello_popup_marquee_offset(uint32_t now,uint32_t started_at,
    int text_width,int viewport_width) {
    if(text_width<=viewport_width)return 0;
    const uint32_t elapsed=now-started_at;
    return elapsed<=800?0:(int)(((elapsed-800)/40)%(uint32_t)(text_width+32));
}

#endif
