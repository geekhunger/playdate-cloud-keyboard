#ifndef HELLO_PILL_H
#define HELLO_PILL_H

#include "pd_api.h"

void hello_pill_fill(PlaydateAPI* pd, int x, int y, int width, int height,
                     LCDColor color);
void hello_pill_draw(PlaydateAPI* pd, int x, int y, int width, int height,
                     LCDColor fill, LCDColor outline, int outline_width);

#endif
