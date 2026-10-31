#include "pill.h"

static int pill_radius(int width,int height) {
    // Narrow selection markers can be vertical capsules; keep the radius
    // within both dimensions while preserving existing wide-pill geometry.
    return (width<height?width:height)/2;
}

void hello_pill_fill(PlaydateAPI* pd, int x, int y, int width, int height,
                     LCDColor color) {
    pd->graphics->fillRoundRect(x, y, width, height, pill_radius(width,height), color);
}

void hello_pill_draw(PlaydateAPI* pd, int x, int y, int width, int height,
                     LCDColor fill, LCDColor outline, int outline_width) {
    hello_pill_fill(pd, x, y, width, height, fill);
    if (outline_width > 0)
        pd->graphics->drawRoundRect(x, y, width, height, pill_radius(width,height),
                                    outline_width, outline);
}
