#include "keyboard_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Captured from the pre-extraction implementation. This records externally
// observable state, not struct bytes. Explicit contract changes are recorded:
// selection commits on D-pad release; idle A holds choose layers without case
// changes. Idle B deletes immediately on press; its release adds nothing.
// The retired delete-arming column remains zero for baseline comparison. Selected-item A advances left-to-right and B right-to-left.
// Character tap/hold intervals remain; D-pad cursor repeats now share the
// configured button delay/interval (350/100 by default, rather than250/100).
static FILE* actual;
static void snapshot(const char* label,const HelloKeyboardModel* m) {
    fprintf(actual,"%s text=%s cursor=%zu group=%d index=%d pending=%u upper=%d alt=%d tap=%d A=%d/%d/%d B=%d/%d/%d\n",
        label,m->text,m->cursor,m->group,m->index,m->pending,m->upper,m->alternate,0,
        m->hold[0].active,m->hold[0].handled,m->hold[0].hold,
        m->hold[1].active,m->hold[1].handled,m->hold[1].hold);
}
static void hold_case(uint32_t start) {
    HelloKeyboardModel m;hello_keyboard_model_init(&m,"",0);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_A,start);snapshot("case press",&m);
    hello_keyboard_model_update(&m,start+349);snapshot("case 349",&m);
    hello_keyboard_model_update(&m,start+350);snapshot("case 350",&m);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_A,start+351);snapshot("case release",&m);
}
static void tap_then_repeat(uint32_t start,HelloKeyboardButton button) {
    HelloKeyboardModel m;hello_keyboard_model_init(&m,"",0);
    hello_keyboard_model_direction(&m,HELLO_KB_LEFT,start);
    const int first=button==HELLO_KB_BUTTON_A?0:8,direction=button==HELLO_KB_BUTTON_A?1:-1;
    hello_keyboard_model_press(&m,button,start+10);assert(m.index==first);
    hello_keyboard_model_release(&m,button,start+20);assert(m.index==first);
    snapshot(button==HELLO_KB_BUTTON_A?"repeat A tap":"repeat B tap",&m);
    hello_keyboard_model_press(&m,button,start+40);snapshot("repeat second press",&m);assert(m.index==first+direction);
    hello_keyboard_model_update(&m,start+389);snapshot("repeat 349",&m);assert(m.index==first+direction&&!m.upper);
    hello_keyboard_model_update(&m,start+390);snapshot("repeat 350",&m);assert(m.index==first+2*direction&&!m.upper);
    hello_keyboard_model_update(&m,start+2000);snapshot("repeat no catchup",&m);assert(m.index==first+3*direction&&!m.upper);
    hello_keyboard_model_release(&m,button,start+2001);snapshot("repeat release",&m);assert(m.index==first+3*direction);
    hello_keyboard_model_direction_release(&m,HELLO_KB_LEFT,start+2002);snapshot("repeat commit",&m);
    assert(!strcmp(m.text,button==HELLO_KB_BUTTON_A?"g":"f"));
}

int main(int argc,char** argv) {
    actual=tmpfile();assert(actual);
    hold_case(1000);hold_case(UINT32_MAX-100);
    HelloKeyboardModel m;hello_keyboard_model_init(&m,"abcdef",6);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_B,1000);
    snapshot("first B press deletes",&m);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_B,1020);snapshot("first B tap",&m);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_B,1520);snapshot("second B at 500",&m);
    hello_keyboard_model_update(&m,1869);snapshot("delete 349",&m);
    hello_keyboard_model_update(&m,1870);snapshot("delete 350",&m);
    hello_keyboard_model_update(&m,1969);snapshot("repeat 99",&m);
    hello_keyboard_model_update(&m,1970);snapshot("repeat 100",&m);
    hello_keyboard_model_update(&m,2470);snapshot("repeat bounded lag",&m);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_B,2471);snapshot("delete release",&m);
    hello_keyboard_model_init(&m,"abcdef",6);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_B,1000);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_B,1020);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_B,1521);snapshot("second B at 501",&m);
    hello_keyboard_model_update(&m,1871);snapshot("first B hold deletes",&m);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_B,1872);
    hello_keyboard_model_direction(&m,HELLO_KB_DOWN,2330);snapshot("direction after B hold 459",&m);
    hello_keyboard_model_direction(&m,HELLO_KB_DOWN,2331);snapshot("direction after B hold 460",&m);
    hello_keyboard_model_init(&m,"",0);
    hello_keyboard_model_direction(&m,HELLO_KB_UP,1000);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_B,1001);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_B,1002);snapshot("selection tap",&m);
    hello_keyboard_model_update(&m,1750);snapshot("selection idle 749",&m);
    hello_keyboard_model_update(&m,1751);snapshot("selection idle 750",&m);
    hello_keyboard_model_update(&m,31001);snapshot("selection idle 30000",&m);
    hello_keyboard_model_direction_release(&m,HELLO_KB_UP,31002);snapshot("selection direction release",&m);
    hello_keyboard_model_init(&m,"abcdef",6);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_A,999);
    hello_keyboard_model_direction(&m,HELLO_KB_LEFT,1000);snapshot("cursor press",&m);assert(m.cursor==5);
    hello_keyboard_model_update(&m,1349);snapshot("cursor 349",&m);assert(m.cursor==5);
    hello_keyboard_model_update(&m,1350);snapshot("cursor 350",&m);assert(m.cursor==4);
    hello_keyboard_model_update(&m,1449);snapshot("cursor repeat 99",&m);assert(m.cursor==4);
    hello_keyboard_model_update(&m,1450);snapshot("cursor repeat 100",&m);assert(m.cursor==3);
    hello_keyboard_model_direction_release(&m,HELLO_KB_LEFT,1451);snapshot("cursor release",&m);assert(m.cursor==3&&!m.cursor_hold);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_A,1452);snapshot("cursor mode release",&m);
    tap_then_repeat(1000,HELLO_KB_BUTTON_A);tap_then_repeat(1000,HELLO_KB_BUTTON_B);
    tap_then_repeat(UINT32_MAX-100,HELLO_KB_BUTTON_A);tap_then_repeat(UINT32_MAX-100,HELLO_KB_BUTTON_B);
    rewind(actual);
    if(argc==2&&!strcmp(argv[1],"--record")) {
        int c;while((c=fgetc(actual))!=EOF)putchar(c);
    } else {
        FILE* expected=fopen("tests/fixtures/input-timing-repeat-only.trace","rb");assert(expected);
        unsigned line=1;int a,e;
        do {a=fgetc(actual);e=fgetc(expected);if(a!=e){fprintf(stderr,"Gesture parity changed at baseline line %u\n",line);assert(a==e);}if(a=='\n')line++;}while(a!=EOF);
        fclose(expected);puts("input timing: press-only steps, hold repeat, cursor timing and explicit release commit passed");
    }
    fclose(actual);
}
