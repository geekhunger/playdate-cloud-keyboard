#include "keyboard_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

static void tap(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t at) {
    hello_keyboard_model_press(model, button, at);
    hello_keyboard_model_release(model, button, at + 50);
}

static void cursor_direction(HelloKeyboardModel* model,HelloKeyboardDirection direction,uint32_t now) {
    hello_keyboard_model_press(model,HELLO_KB_BUTTON_A,now);
    hello_keyboard_model_direction(model,direction,now);
}
static void cursor_release(HelloKeyboardModel* model,HelloKeyboardDirection direction,uint32_t now) {
    hello_keyboard_model_direction_release(model,direction,now);
    hello_keyboard_model_release(model,HELLO_KB_BUTTON_A,now);
}

static void test_multitap_and_direction_release(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "", 0);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    tap(&model, HELLO_KB_BUTTON_A, 20);
    tap(&model, HELLO_KB_BUTTON_A, 100);
    assert(model.pending == 'i');
    hello_keyboard_model_update(&model, 900);
    assert(model.pending == 'i' && !model.text[0]);
    hello_keyboard_model_direction_release(&model, HELLO_KB_LEFT, 901);
    assert(strcmp(model.text, "i") == 0);
}

static void test_insert_delete_and_cursor(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "ac", 1);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    for(int i=0;i<5;i++)tap(&model,HELLO_KB_BUTTON_A,20+(uint32_t)i*60);
    hello_keyboard_model_commit(&model, false);
    assert(strcmp(model.text, "abc") == 0);
    tap(&model, HELLO_KB_BUTTON_B, 500);
    assert(strcmp(model.text, "ac") == 0);
    tap(&model, HELLO_KB_BUTTON_B, 580);
    assert(strcmp(model.text, "c") == 0);
}

static void test_delete_starts_with_first_b_press(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "abc", 3);
    hello_keyboard_model_press(&model, HELLO_KB_BUTTON_B, 10);
    assert(strcmp(model.text, "ab") == 0);
    assert(model.feedback_active[HELLO_KB_BUTTON_B]);
    hello_keyboard_model_release(&model, HELLO_KB_BUTTON_B, 60);
    assert(strcmp(model.text, "ab") == 0);
    tap(&model, HELLO_KB_BUTTON_B, 90);
    assert(strcmp(model.text, "a") == 0);
    assert(model.feedback_active[HELLO_KB_BUTTON_B]);
    tap(&model, HELLO_KB_BUTTON_B, 170);
    assert(strcmp(model.text, "") == 0);
}

static void test_delete_feedback_requires_a_character(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "", 0);
    tap(&model, HELLO_KB_BUTTON_B, 10);
    tap(&model, HELLO_KB_BUTTON_B, 90);
    assert(model.text[0] == '\0');
    assert(!model.feedback_active[HELLO_KB_BUTTON_B]);
}

static void test_idle_a_case_and_layer_chooser(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model,"hi",2);
    model.alternate=true;model.layer_at=1000; // Historical metadata cannot enable a view or input lock.
    assert(hello_keyboard_model_layer(&model)==1&&hello_keyboard_model_layer_count(&model)==1);
    tap(&model,HELLO_KB_BUTTON_A,10);assert(model.upper&&!strcmp(model.text,"hi"));
    tap(&model,HELLO_KB_BUTTON_A,100);assert(!model.upper&&!strcmp(model.text,"hi"));
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_update(&model,1349);assert(!hello_keyboard_model_is_choosing_layer(&model));
    hello_keyboard_model_update(&model,1350);assert(hello_keyboard_model_is_choosing_layer(&model)&&!model.upper);
    for(int direction=0;direction<3;direction+=2) {
        hello_keyboard_model_direction(&model,(HelloKeyboardDirection)direction,1351+(uint32_t)direction);
        assert(!model.group&&!model.pending&&hello_keyboard_model_layer(&model)==1);
    }
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,1357);
    assert(!hello_keyboard_model_is_choosing_layer(&model)&&!model.upper);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,1358);
    tap(&model,HELLO_KB_BUTTON_A,1359);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,1410);assert(!strcmp(model.text,"hie"));
    for(int wrap=0;wrap<2;wrap++)for(unsigned elapsed=349;elapsed<=350;elapsed++) {
        const uint32_t start=wrap?UINT32_MAX-100:1000;
        hello_keyboard_model_init(&model,"",0);
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,start);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+elapsed);
        assert(model.upper==(elapsed==349)&&!model.text[0]&&!hello_keyboard_model_is_choosing_layer(&model));
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+elapsed+1);assert(model.upper==(elapsed==349));
    }
    hello_keyboard_model_init(&model,"",0);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,10);
    hello_keyboard_model_direction(&model,HELLO_KB_DOWN,11);
    assert(hello_keyboard_model_is_choosing_layer(&model)&&!model.group&&hello_keyboard_model_layer(&model)==1);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,12);
    hello_keyboard_model_direction_release(&model,HELLO_KB_DOWN,13);
    assert(!model.upper&&!model.pending&&!model.group&&!model.text[0]);
    tap(&model,HELLO_KB_BUTTON_A,20);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,71);assert(model.group==HELLO_KEYBOARD_EMOJI_GROUP);
    char first_emoji[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_group_text(&model,6,0,first_emoji,sizeof(first_emoji));
    tap(&model,HELLO_KB_BUTTON_A,72);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,123);assert(!strcmp(model.text,first_emoji));
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,124);
    tap(&model,HELLO_KB_BUTTON_A,125);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,176);
    char emoji_letter[HELLO_KEYBOARD_ITEM_BYTES+2];snprintf(emoji_letter,sizeof(emoji_letter),"%sE",first_emoji);assert(!strcmp(model.text,emoji_letter));
    tap(&model,HELLO_KB_BUTTON_A,177);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,228);assert(model.group==1&&!model.upper);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,229);
}

static void test_first_b_hold_deletes_without_switching_view(void) {
    for(int wrap=0;wrap<2;wrap++) {
        const uint32_t start=wrap?UINT32_MAX-100:1000;
        HelloKeyboardModel model;hello_keyboard_model_init(&model,"abcdef",6);
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,start);
        hello_keyboard_model_update(&model,start+349);assert(!strcmp(model.text,"abcde"));
        hello_keyboard_model_update(&model,start+350);assert(!strcmp(model.text,"abcd"));
        assert(model.hold[HELLO_KB_BUTTON_B].hold==HELLO_KB_HOLD_DELETE);
        hello_keyboard_model_update(&model,start+449);assert(!strcmp(model.text,"abcd"));
        hello_keyboard_model_update(&model,start+450);assert(!strcmp(model.text,"abc"));
        hello_keyboard_model_update(&model,start+1450);assert(!model.text[0]);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,start+1451);
        assert(!model.alternate&&hello_keyboard_model_layer(&model)==1);
    }
}

static void test_idle_b_hold_cannot_delete_new_group_commit(void) {
    for(int type=0;type<3;type++) {
        HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);
        model.upper=type==1;model.field.type=(type==2)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
        const HelloKeyboardDirection direction=type==2?HELLO_KB_UP:HELLO_KB_LEFT;
        const char* expected=type==2?"1":type==1?"E":"e";
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);
        hello_keyboard_model_direction(&model,direction,1010);
        assert(!model.hold[HELLO_KB_BUTTON_B].active&&!model.pending);
        hello_keyboard_model_crank(&model,1,1020);
        hello_keyboard_model_direction_release(&model,direction,1400);
        hello_keyboard_model_update(&model,1400);
        assert(!strcmp(model.text,expected));
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,1410);
        assert(!strcmp(model.text,expected));
    }
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"123",3);model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);
    hello_keyboard_model_direction(&model,HELLO_KB_DOWN,1010);
    assert(!model.hold[HELLO_KB_BUTTON_B].active&&model.group==3);
    hello_keyboard_model_update(&model,1350);assert(!strcmp(model.text,"12"));
    hello_keyboard_model_init(&model,"abc",3);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1001);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,1010);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,1020);
    assert(!model.hold[HELLO_KB_BUTTON_B].active&&!model.group);
    hello_keyboard_model_init(&model,"",0);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,900);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);
    hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,1010);
    assert(model.hold[HELLO_KB_BUTTON_B].active&&model.hold[HELLO_KB_BUTTON_B].group==4);
}

static void test_layer_chooser_cursor_holds(void) {
    const char* text="aé👨‍👩‍👧‍👦z";
    const size_t end=strlen(text),before_family=strlen("aé");
    for(int layer=0;layer<2;layer++)for(int wrap=0;wrap<2;wrap++) {
        const uint32_t start=wrap?UINT32_MAX-100:1000;
        HelloKeyboardModel model;hello_keyboard_model_init(&model,text,end);model.alternate=layer;
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,start);
        hello_keyboard_model_direction(&model,HELLO_KB_LEFT,start+1);
        assert(hello_keyboard_model_is_choosing_layer(&model)&&model.cursor_hold);
        assert(model.cursor==end-1&&!model.group&&!model.pending);
        hello_keyboard_model_update(&model,start+350);assert(model.cursor==end-1);
        hello_keyboard_model_update(&model,start+351);assert(model.cursor==before_family);
        hello_keyboard_model_update(&model,start+450);assert(model.cursor==before_family);
        hello_keyboard_model_update(&model,start+451);assert(model.cursor==1);
        hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,start+551);
        hello_keyboard_model_update(&model,start+551);assert(model.cursor==1&&!model.cursor_hold);
        hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,start+552);assert(model.cursor==before_family);
        // A release wins over a due cursor repeat even if Right stays held.
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+902);
        hello_keyboard_model_update(&model,start+902);
        assert(model.cursor==before_family&&!model.cursor_hold&&!hello_keyboard_model_is_choosing_layer(&model));
        assert(!model.upper&&model.alternate==(bool)layer&&!strcmp(model.text,text));
        hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,start+903);
        assert(model.cursor==before_family&&!model.group&&!model.pending);

        // Long frame gaps present one cursor step without catch-up bursts.
        hello_keyboard_model_init(&model,"abcdefghijk",11);model.alternate=layer;
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,start);
        hello_keyboard_model_direction(&model,HELLO_KB_LEFT,start+1);assert(model.cursor==10);
        hello_keyboard_model_update(&model,start+1001);assert(model.cursor==9);
        hello_keyboard_model_direction(&model,HELLO_KB_UP,start+1002);
        assert(!model.cursor_hold&&model.alternate==(bool)layer&&!model.group);
        hello_keyboard_model_update(&model,start+2001);assert(model.cursor==9);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+2002);
        assert(!model.upper&&!strcmp(model.text,"abcdefghijk"));
    }
}

static void test_crank_and_limit(void) {
    HelloKeyboardModel model;
    char full[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS + 1];
    memset(full, 'x', HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS);
    full[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS] = '\0';
    hello_keyboard_model_init(&model, full, HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    hello_keyboard_model_crank(&model, 1, 20);
    hello_keyboard_model_commit(&model, false);
    assert(strlen(model.text) == HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS);
}

static void test_a_starts_left_and_b_starts_right(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "", 0);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    tap(&model, HELLO_KB_BUTTON_B, 20);
    assert(model.pending == 'a');
    tap(&model, HELLO_KB_BUTTON_B, 80);
    assert(model.pending == 'h');

    hello_keyboard_model_commit(&model, true);
    tap(&model, HELLO_KB_BUTTON_A, 140);
    assert(model.pending == 'e');
    tap(&model, HELLO_KB_BUTTON_A, 200);
    assert(model.pending == 'i');
}

static void test_buttons_cross_halves_and_reverse(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "", 0);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    tap(&model, HELLO_KB_BUTTON_B, 20);
    tap(&model, HELLO_KB_BUTTON_B, 80);
    tap(&model, HELLO_KB_BUTTON_B, 140);
    tap(&model, HELLO_KB_BUTTON_B, 200);
    tap(&model, HELLO_KB_BUTTON_B, 260);
    assert(model.pending == 'b');
    assert(model.text[0] == '\0');

    tap(&model, HELLO_KB_BUTTON_A, 320);
    assert(model.pending == 'f');
    assert(model.text[0] == '\0');
    tap(&model, HELLO_KB_BUTTON_B, 380);
    assert(model.pending == 'b');
    assert(model.text[0] == '\0');
}

static void test_first_press_selects_immediately_and_hold_repeats(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model, "", 0);
    hello_keyboard_model_direction(&model, HELLO_KB_LEFT, 10);
    hello_keyboard_model_press(&model, HELLO_KB_BUTTON_A, 20);
    assert(model.pending == 'e');
    assert(model.group == 4);
    assert(model.text[0] == '\0');
    hello_keyboard_model_update(&model, 370);
    assert(model.pending == 'i');
    hello_keyboard_model_release(&model, HELLO_KB_BUTTON_A, 380);

    hello_keyboard_model_update(&model, 1000);
    assert(model.pending == 'i');
    assert(model.text[0] == '\0');
    hello_keyboard_model_update(&model, 30000);
    assert(model.pending == 'i' && !model.text[0]);
    hello_keyboard_model_direction_release(&model, HELLO_KB_LEFT, 30001);
    assert(model.pending == '\0');
    assert(strcmp(model.text, "i") == 0);
}

static void test_custom_groups_are_copied_and_used(void) {
    HelloKeyboardModel model;
    char up[] = "wxyz";
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT] = {
        up, "1234", "+-*/", "!?()", "09"
    };
    hello_keyboard_model_init_with_groups(&model, "", 0, groups);
    up[0] = 'q';

    assert(strcmp(hello_keyboard_model_group_chars(&model, 1), "wxyz") == 0);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 3), "+-*/") == 0);
    assert(hello_keyboard_model_group_length(&model, 5) == 2);

    hello_keyboard_model_direction(&model, HELLO_KB_UP, 10);
    tap(&model, HELLO_KB_BUTTON_B, 20);
    assert(model.pending == 'z');
    tap(&model, HELLO_KB_BUTTON_A, 80);
    assert(model.pending == 'w');

    hello_keyboard_model_commit(&model, false);
    hello_keyboard_model_direction(&model, HELLO_KB_DOWN, 200);
    tap(&model, HELLO_KB_BUTTON_B, 210);
    assert(model.pending == '/');
}

static void test_invalid_custom_groups_use_defaults(void) {
    HelloKeyboardModel model;
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT] = {
        "", "x", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abc", NULL, "12"
    };
    hello_keyboard_model_init_with_groups(&model, "", 0, groups);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 1), "1234567890.,'?!-:/@_#~[<{\\}>]^=;*%$€&+)(") == 0);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 2), "nomkqjplr") == 0);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 3), "tswvxzyu⎵") == 0);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 4), "eidgbfcha") == 0);
    assert(strcmp(hello_keyboard_model_group_chars(&model, 5), "12") == 0);
}

static void select_group(HelloKeyboardModel* model,int group) {
    model->field.type=group==HELLO_KEYBOARD_NUMBERS_GROUP?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
    if(group==HELLO_KEYBOARD_NUMBERS_GROUP)group=1; // Reserved legacy data has no runtime route.
    if(group==1)model->upper=false;
    if(group==HELLO_KEYBOARD_EMOJI_GROUP)model->upper=true;
    hello_keyboard_model_direction(model,group>=HELLO_KEYBOARD_NUMBERS_GROUP?HELLO_KB_UP:(HelloKeyboardDirection)(group-1),1);
    assert(model->group==group);
}

static void test_default_punctuation_navigation(void) {
    const char* punctuation="1234567890.,'?!-:/@_#~[<{\\}>]^=;*%$€&+)(";
    const int count=(int)brb_grapheme_count(punctuation);
    assert(count==40&&strlen(punctuation)==42);
    char expected[40][HELLO_KEYBOARD_MAX_UTF8_BYTES+1];
    uint32_t codepoints[40];
    size_t lengths[40],offset=0;
    int euros=0,digits=0,symbols=0,tildes=0;
    for(int index=0;index<count;index++) {
        const size_t bytes=brb_grapheme_next(punctuation+offset,strlen(punctuation)-offset);
        assert(bytes>0&&bytes<=HELLO_KEYBOARD_MAX_UTF8_BYTES);
        memcpy(expected[index],punctuation+offset,bytes);expected[index][bytes]=0;
        assert(brb_utf8_decode(expected[index],bytes,&codepoints[index])==bytes);
        for(int earlier=0;earlier<index;earlier++)assert(codepoints[earlier]!=codepoints[index]);
        if(codepoints[index]==0x20ac){assert(bytes==3);euros++;}
        if(codepoints[index]>='0'&&codepoints[index]<='9')digits++;
        else symbols++;
        if(codepoints[index]=='~')tildes++;
        if(index<10)assert(codepoints[index]==(uint32_t)"1234567890"[index]);
        lengths[index]=bytes;offset+=bytes;
    }
    assert(offset==strlen(punctuation)&&euros==1&&digits==10&&symbols==30&&tildes==1);
    assert(codepoints[21]=='~'&&codepoints[count-1]=='(');
    for(int input=0;input<4;input++) {
        HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);
        assert(!strcmp(hello_keyboard_model_group_chars(&model,1),punctuation));
        assert(hello_keyboard_model_group_length(&model,1)==count);
        select_group(&model,1);
        // A/clockwise start at the first character; B/counterclockwise at the last.
        for(int i=0;i<count+2;i++) {
            const uint32_t at=20+(uint32_t)i*60;
            if(input<2)tap(&model,input==0?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B,at);
            else hello_keyboard_model_crank(&model,input==2?1:-1,at);
            const int index=input%2==0?i%count:count-1-i%count;
            assert(model.index==index);
            assert(model.pending==codepoints[index]);
            char selected[HELLO_KEYBOARD_MAX_UTF8_BYTES+1];
            assert(hello_keyboard_model_pending_text(&model,selected,sizeof(selected))==lengths[index]);
            assert(!strcmp(selected,expected[index]));
        }
        // Commit every reachable character through the same input route.
        for(int index=0;index<count;index++) {
            hello_keyboard_model_init(&model,"",0);select_group(&model,1);
            const int steps=input%2==0?index+1:count-index;
            for(int i=0;i<steps;i++) {
                const uint32_t at=20+(uint32_t)i*60;
                if(input<2)tap(&model,input==0?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B,at);
                else hello_keyboard_model_crank(&model,input==2?1:-1,at);
            }
            assert(model.index==index&&model.pending==codepoints[index]);
            hello_keyboard_model_commit(&model,false);
            assert(!strcmp(model.text,expected[index])&&model.cursor==lengths[index]);
        }
    }
}

static void test_unicode_group_navigation(void) {
    const char* unicode[HELLO_KEYBOARD_GROUP_COUNT]={"éø🦆⎵","字λ🙂","🫶ßΩ","éA⎵","1234"};
    for(int custom=0;custom<2;custom++)for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboardModel m;
        hello_keyboard_model_init_with_groups(&m,"",0,custom?unicode:NULL);select_group(&m,group);
        const int count=hello_keyboard_model_group_length(&m,group);
        for(int i=0;i<count+2;i++) {
            tap(&m,HELLO_KB_BUTTON_A,20+(uint32_t)i*60);
            assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,i%count));
        }
        for(int i=0;i<count+2;i++) {
            tap(&m,HELLO_KB_BUTTON_B,1500+(uint32_t)i*60);
            assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,(count-i%count)%count));
        }
        hello_keyboard_model_init_with_groups(&m,"",0,custom?unicode:NULL);select_group(&m,group);
        tap(&m,HELLO_KB_BUTTON_B,20);assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,count-1));
        hello_keyboard_model_init_with_groups(&m,"",0,custom?unicode:NULL);select_group(&m,group);
        for(int i=0;i<count+2;i++) {
            hello_keyboard_model_crank(&m,1,20+(uint32_t)i);
            assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,i%count));
        }
        hello_keyboard_model_crank(&m,-2,100);assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,count-1));
        hello_keyboard_model_crank(&m,1,101);assert(m.pending==hello_keyboard_model_group_codepoint(&m,group,0));
        hello_keyboard_model_crank(&m,INT_MAX,102);hello_keyboard_model_crank(&m,INT_MIN,103);
        assert(m.index>=0&&m.index<count);
    }
    HelloKeyboardModel m;hello_keyboard_model_init(&m,"",0);select_group(&m,3);
    tap(&m,HELLO_KB_BUTTON_B,20);assert(m.pending==0x23b5);
    hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text," ")&&m.cursor==1);
}

static void test_unicode_editing_and_limits(void) {
    HelloKeyboardModel m;
    hello_keyboard_model_init(&m,"aé🫶字z",5);assert(m.cursor==3); // Never in the middle of a scalar.
    m.alternate=true;cursor_direction(&m,HELLO_KB_RIGHT,10);assert(m.cursor==7);
    cursor_release(&m,HELLO_KB_RIGHT,11);
    cursor_direction(&m,HELLO_KB_LEFT,20);assert(m.cursor==3);
    cursor_release(&m,HELLO_KB_LEFT,21);
    tap(&m,HELLO_KB_BUTTON_B,30);
    assert(!strcmp(m.text,"a🫶字z")&&m.cursor==1); // Deletes the complete two-byte é.
    m.cursor=5;tap(&m,HELLO_KB_BUTTON_B,180);
    assert(!strcmp(m.text,"a字z")&&m.cursor==1); // Deletes a complete four-byte scalar.
    m.pending=0x1f9e1;hello_keyboard_model_commit(&m,false);
    assert(!strcmp(m.text,"a🧡字z")&&m.cursor==5);
    m.alternate=true;cursor_direction(&m,HELLO_KB_RIGHT,200);assert(m.cursor==8);
    cursor_release(&m,HELLO_KB_RIGHT,201);
    m.cursor=1;cursor_direction(&m,HELLO_KB_RIGHT,300);
    hello_keyboard_model_update(&m,650);assert(m.cursor==8); // One held step crosses a complete codepoint.

    hello_keyboard_model_init(&m,"é",2);m.field.max_length=2;m.pending=0x1f9e1;
    hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,"é🧡")&&strlen(m.text)==6);
    m.pending='x';hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,"é🧡"));
    char near[166];memset(near,'x',159);memcpy(near+159,"🧡",5);
    hello_keyboard_model_init(&m,near,SIZE_MAX);assert(strlen(m.text)==163&&m.cursor==163);
    m.pending='x';hello_keyboard_model_commit(&m,false);assert(strlen(m.text)==163); // Already160 graphemes.
    m.field.max_length=161;m.pending=0x1f9e1;hello_keyboard_model_commit(&m,false);assert(strlen(m.text)==167);
    m.pending='x';hello_keyboard_model_commit(&m,false);assert(strlen(m.text)==167);
    for(const char** text=(const char*[]){"\xc0\xaf","\xe0\x80\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xf0\x9f","\x80",NULL};*text;text++) {
        assert(hello_keyboard_utf8_count(*text)==SIZE_MAX);
        assert(!hello_keyboard_model_group_is_valid(*text));
        hello_keyboard_model_init(&m,*text,SIZE_MAX);assert(!m.text[0]&&m.cursor==0);
    }
    char encoded[5];assert(hello_keyboard_utf8_encode(0xd800,encoded)==0&&!encoded[0]);
    assert(hello_keyboard_utf8_encode(0x110000,encoded)==0);
    assert(hello_keyboard_utf8_count("aé🧡字")==4);
}

static bool emoji_variants(const char* input,HelloKeyboardTransformAction action,
    char* output,size_t capacity,void* userdata) {
    int* calls=userdata;
    if(action!=HELLO_KB_TRANSFORM_UPPER)return false;
    if(strcmp(input,"👍")&&strcmp(input,"👍🏽"))return false;
    (*calls)++;
    const char* next=!strcmp(input,"👍")?"👍🏽":"👍";
    assert(strlen(next)<capacity);strcpy(output,next);return true;
}

static bool invalid_variant(const char* input,HelloKeyboardTransformAction action,
    char* output,size_t capacity,void* userdata) {
    (void)input;
    if(action!=HELLO_KB_TRANSFORM_UPPER)return false;
    const int kind=*(int*)userdata;
    if(kind==0)memset(output,'x',capacity); // No terminator.
    else if(kind==1){output[0]=(char)0xf0;output[1]=(char)0x9f;output[2]=0;} // Truncated scalar.
    else if(kind==3)strcpy(output,"a\nb");
    else if(kind==4)strcpy(output,"a\u2028b");
    else output[0]=0;
    return true;
}

static void test_graphemes_and_variant_hook(void) {
    const char* family="👨‍👩‍👧‍👦";
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"a👨‍👩‍👧‍👦🇩🇪👍🏽","éΩ","1️⃣2️⃣","👩‍💻❤️","𝔸𝔹"};
    HelloKeyboardModel m;char selected[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];
    hello_keyboard_model_init_with_groups(&m,"",0,groups);select_group(&m,1);
    assert(hello_keyboard_model_group_length(&m,1)==4);
    tap(&m,HELLO_KB_BUTTON_A,10);tap(&m,HELLO_KB_BUTTON_A,80);
    hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,family));
    hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,family));
    m.alternate=true;
    cursor_direction(&m,HELLO_KB_LEFT,200);assert(m.cursor==0);
    cursor_release(&m,HELLO_KB_LEFT,201);
    cursor_direction(&m,HELLO_KB_RIGHT,250);assert(m.cursor==strlen(family));
    cursor_release(&m,HELLO_KB_RIGHT,251);
    tap(&m,HELLO_KB_BUTTON_B,300);tap(&m,HELLO_KB_BUTTON_B,380);assert(!m.text[0]);
    char sequence[100];snprintf(sequence,sizeof(sequence),"x%s🇩🇪👍🏽éy",family);
    for(size_t cursor=1;cursor<1+strlen(family);cursor++) {
        hello_keyboard_model_init(&m,sequence,cursor);assert(m.cursor==1);
    }
    hello_keyboard_model_init(&m,sequence,strlen(sequence)-1);m.alternate=true;
    const size_t end=m.cursor;
    cursor_direction(&m,HELLO_KB_LEFT,10);assert(m.cursor==end-strlen("é"));
    cursor_release(&m,HELLO_KB_LEFT,11);
    tap(&m,HELLO_KB_BUTTON_B,20);
    assert(!strstr(m.text,"👍")); // Entire skin-tone sequence removed.
    hello_keyboard_model_init_with_groups(&m,"",0,groups);m.field.max_length=1;select_group(&m,1);
    hello_keyboard_model_crank(&m,2,10);hello_keyboard_model_commit(&m,false);
    assert(!strcmp(m.text,family));
    select_group(&m,1);hello_keyboard_model_crank(&m,1,20);assert(m.pending=='a');hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,family));
    const char* variants[HELLO_KEYBOARD_GROUP_COUNT]={"👍🙂","ab","cd","ef","12","👍🙂"};int calls=0;
    hello_keyboard_model_init_with_groups(&m,"",0,variants);
    m.transform=emoji_variants;m.transform_userdata=&calls;select_group(&m,HELLO_KEYBOARD_EMOJI_GROUP);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_A,10);
    hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"👍🏽")&&calls==1);
    hello_keyboard_model_update(&m,360);
    hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"🙂")&&calls==1);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_A,400);
    hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,"🙂"));
    const char* letters[HELLO_KEYBOARD_GROUP_COUNT]={"aé","ab","cd","ef","12","aé"};
    hello_keyboard_model_init_with_groups(&m,"",0,letters);
    m.transform=emoji_variants;m.transform_userdata=&calls;select_group(&m,HELLO_KEYBOARD_EMOJI_GROUP);
    tap(&m,HELLO_KB_BUTTON_A,10);hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"A"));
    tap(&m,HELLO_KB_BUTTON_A,90);hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"É"));
    hello_keyboard_model_init_with_groups(&m,"",0,letters);
    m.transform=emoji_variants;m.transform_userdata=&calls;select_group(&m,1);
    hello_keyboard_model_press(&m,HELLO_KB_BUTTON_A,10);
    hello_keyboard_model_release(&m,HELLO_KB_BUTTON_A,360);
    hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"é")&&calls==1);
    const char* configured_case[HELLO_KEYBOARD_GROUP_COUNT]={"Aé","ab","cd","ef","12"};
    hello_keyboard_model_init_with_groups(&m,"",0,configured_case);
    m.transform=emoji_variants;m.transform_userdata=&calls;select_group(&m,1);
    tap(&m,HELLO_KB_BUTTON_A,10);assert(m.pending=='A');
    const char* cases[HELLO_KEYBOARD_GROUP_COUNT]={"éß","ab","cd","ef","12","éß"};
    hello_keyboard_model_init_with_groups(&m,"",0,cases);select_group(&m,HELLO_KEYBOARD_EMOJI_GROUP);
    tap(&m,HELLO_KB_BUTTON_A,10);hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"É"));
    tap(&m,HELLO_KB_BUTTON_A,90);hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"SS"));
    for(int kind=0;kind<5;kind++) {
        hello_keyboard_model_init_with_groups(&m,"",0,variants);
        m.transform=invalid_variant;m.transform_userdata=&kind;select_group(&m,HELLO_KEYBOARD_EMOJI_GROUP);
        tap(&m,HELLO_KB_BUTTON_A,10);
        hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"👍"));
        hello_keyboard_model_commit(&m,false);assert(!strcmp(m.text,"👍"));
    }
    hello_keyboard_model_init_with_groups(&m,"",0,cases);m.field.max_length=1;select_group(&m,HELLO_KEYBOARD_EMOJI_GROUP);
    tap(&m,HELLO_KB_BUTTON_B,10);
    hello_keyboard_model_pending_text(&m,selected,sizeof(selected));assert(!strcmp(selected,"SS"));
    hello_keyboard_model_commit(&m,false);assert(!m.text[0]&&m.input_rejected==HELLO_FIELD_TOO_LONG);
    // The independent text byte budget retains a complete multibyte cluster
    // even when this short input exceeds the historical160-byte ceiling.
    char oversized[200];memset(oversized,'x',150);strcpy(oversized+150,family);
    hello_keyboard_model_init(&m,oversized,SIZE_MAX);assert(strlen(m.text)==strlen(oversized)&&m.cursor==strlen(oversized));
    const char* joiners[HELLO_KEYBOARD_GROUP_COUNT]={"‍b","ab","cd","ef","12"};
    hello_keyboard_model_init_with_groups(&m,"👩💻",strlen("👩"),joiners);select_group(&m,1);
    tap(&m,HELLO_KB_BUTTON_A,10);hello_keyboard_model_commit(&m,false);
    assert(!strcmp(m.text,"👩‍💻")&&m.cursor==strlen(m.text)); // Insertion joins both neighboring clusters.
    const char* accents[HELLO_KEYBOARD_GROUP_COUNT]={"́a","ab","cd","ef","12"};
    hello_keyboard_model_init_with_groups(&m,"e",1,accents);m.field.max_length=1;select_group(&m,1);
    tap(&m,HELLO_KB_BUTTON_A,10);hello_keyboard_model_commit(&m,false);
    assert(!strcmp(m.text,"é")&&m.cursor==strlen(m.text)); // Extending the existing cluster still fits.
    const char* controls[HELLO_KEYBOARD_GROUP_COUNT]={"a\nb","a\tb","a\302\205b","a\u2028b","a\u2029b"};
    hello_keyboard_model_init_with_groups(&m,"",0,controls);
    for(int group=1;group<=5;group++)
        assert(!strcmp(hello_keyboard_model_group_chars(&m,group),hello_keyboard_default_group_chars(group)));
    assert(hello_keyboard_model_group_is_valid("👨‍👩‍👧‍👦❤️"));
    assert(hello_keyboard_model_group_is_valid("\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007Fa"));
}

static bool rejected_number_variant(const char* input,HelloKeyboardTransformAction action,
    char* output,size_t capacity,void* userdata) {
    (void)input;(void)action;(void)capacity;(void)userdata;strcpy(output,"X");return true;
}
static void test_emoji_layer_and_number_policy(void) {
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);
    assert(hello_keyboard_model_group_length(&model,HELLO_KEYBOARD_EMOJI_GROUP)==21);
    model.upper=true;hello_keyboard_model_direction(&model,HELLO_KB_UP,1);
    assert(model.group==HELLO_KEYBOARD_EMOJI_GROUP);
    char first_emoji[HELLO_KEYBOARD_ITEM_BYTES+1],last_emoji[HELLO_KEYBOARD_ITEM_BYTES+1];
    hello_keyboard_model_group_text(&model,6,0,first_emoji,sizeof(first_emoji));hello_keyboard_model_group_text(&model,6,20,last_emoji,sizeof(last_emoji));
    tap(&model,HELLO_KB_BUTTON_A,10);assert(model.index==0&&!strcmp(model.pending_text,first_emoji));
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,61);assert(!strcmp(model.text,first_emoji));
    hello_keyboard_model_direction(&model,HELLO_KB_UP,100);
    tap(&model,HELLO_KB_BUTTON_B,110);assert(model.index==20&&!strcmp(model.pending_text,last_emoji));
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,161);
    // All types retain the full layout. Invalid types are rejected only when
    // the selected item is committed, after the user has seen its pending form.
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"1X2",NULL,NULL,NULL,NULL,NULL};
    for(int route=0;route<4;route++) {
        hello_keyboard_model_init_with_groups(&model,"123",3,groups);
        model.field.type=HELLO_INPUT_CONTACT_NUMBER;model.field.min_length=6;model.field.max_length=6;
        if(route==3)model.transform=rejected_number_variant;
        hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,1000);assert(model.group==2);
        if(route==0)tap(&model,HELLO_KB_BUTTON_A,1010);
        else if(route==1)tap(&model,HELLO_KB_BUTTON_B,1010);
        else hello_keyboard_model_crank(&model,route==2?1:-1,1010);
        assert(model.pending&&!model.input_rejected);hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,1061);
        assert(!strcmp(model.text,"123")&&!model.pending&&!model.group&&model.input_rejected==HELLO_FIELD_INVALID_TYPE);
    }
    hello_keyboard_model_init_with_groups(&model,"12345",5,groups);
    model.field.type=HELLO_INPUT_CONTACT_NUMBER;model.field.max_length=6;
    hello_keyboard_model_direction(&model,HELLO_KB_UP,2000);assert(model.group==1);
    tap(&model,HELLO_KB_BUTTON_A,2010);assert(model.pending=='1');
    tap(&model,HELLO_KB_BUTTON_A,2070);assert(model.pending=='X'&&model.index==1);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,2121);
    assert(!strcmp(model.text,"12345")&&model.input_rejected==HELLO_FIELD_INVALID_TYPE);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,2200);tap(&model,HELLO_KB_BUTTON_B,2210);
    assert(model.pending=='2');hello_keyboard_model_direction_release(&model,HELLO_KB_UP,2261);
    assert(!strcmp(model.text,"123452"));
    hello_keyboard_model_clear(&model);assert(model.field.type==HELLO_INPUT_CONTACT_NUMBER&&model.field.max_length==6);
    model.upper=true;hello_keyboard_model_direction(&model,HELLO_KB_UP,2300);
    assert(model.group==HELLO_KEYBOARD_EMOJI_GROUP);tap(&model,HELLO_KB_BUTTON_A,2310);assert(model.pending);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,2361);
    assert(!model.text[0]&&model.input_rejected==HELLO_FIELD_INVALID_TYPE);

}

static void test_long_group_limits_and_navigation(void) {
    char group[HELLO_KEYBOARD_MAX_GROUP_CHARS+2];
    memset(group,'a',sizeof(group));group[HELLO_KEYBOARD_MAX_GROUP_CHARS]=0;
    assert(hello_keyboard_model_group_is_valid(group));
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={group,NULL,NULL,NULL,group};
    HelloKeyboardModel model;hello_keyboard_model_init_with_groups(&model,"",0,groups);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,1);
    hello_keyboard_model_crank(&model,64,2);assert(model.index==63);
    hello_keyboard_model_crank(&model,1,3);assert(model.index==0);
    hello_keyboard_model_crank(&model,-1,4);assert(model.index==63);
    hello_keyboard_model_commit(&model,false);assert(!strcmp(model.text,"a"));
    model.alternate=true;hello_keyboard_model_direction(&model,HELLO_KB_UP,5);
    hello_keyboard_model_crank(&model,-1,6);assert(model.index==63&&model.pending=='a');
    group[HELLO_KEYBOARD_MAX_GROUP_CHARS]='a';group[HELLO_KEYBOARD_MAX_GROUP_CHARS+1]=0;
    assert(!hello_keyboard_model_group_is_valid(group));
    // Many long composed clusters must obey the unchanged fixed byte budget.
    char over[HELLO_KEYBOARD_MAX_GROUP_BYTES+2];
    over[0]='a';for(size_t i=1;i+2<sizeof(over);i+=2){over[i]=(char)0xcc;over[i+1]=(char)0x81;}
    over[sizeof(over)-2]='b';over[sizeof(over)-1]=0;
    assert(!hello_keyboard_model_group_is_valid(over));
}

static void test_group_release_commit_contract(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model,"",0);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,10);
    hello_keyboard_model_update(&model,10010);
    assert(model.group==4&&!model.pending&&!model.text[0]);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,10011);
    assert(!model.group&&!model.pending&&!model.text[0]);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,10012);
    tap(&model,HELLO_KB_BUTTON_A,10020);
    hello_keyboard_model_update(&model,40020);
    assert(model.group==4&&model.pending=='e'&&!model.text[0]);
    hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,40021);
    assert(model.group==4&&model.pending=='e'&&!model.text[0]);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,40022);
    assert(!model.group&&!model.pending&&!strcmp(model.text,"e"));
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,40023);
    hello_keyboard_model_update(&model,70023);
    assert(!strcmp(model.text,"e"));

    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"éø","12","a⎵","字λ","12","👍🏽🙂"};
    const char* expected[]={"é","1","a","字","1","👍🏽"};
    const HelloKeyboardDirection directions[]={HELLO_KB_UP,HELLO_KB_RIGHT,
        HELLO_KB_DOWN,HELLO_KB_LEFT,HELLO_KB_UP,HELLO_KB_UP};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        hello_keyboard_model_init_with_groups(&model,"",0,groups);
        select_group(&model,group);
        tap(&model,HELLO_KB_BUTTON_A,10);
        hello_keyboard_model_update(&model,30010);
        assert(model.pending&&!model.text[0]&&model.group==group);
        const HelloKeyboardDirection wrong=(HelloKeyboardDirection)((directions[group-1]+1)%4);
        hello_keyboard_model_direction_release(&model,wrong,30011);
        assert(model.pending&&!model.text[0]&&model.group==group);
        hello_keyboard_model_direction_release(&model,directions[group-1],30012);
        assert(!model.pending&&!model.group&&!strcmp(model.text,expected[group-1]));
        assert(model.cursor==strlen(expected[group-1]));
        hello_keyboard_model_direction_release(&model,directions[group-1],30013);
        assert(!strcmp(model.text,expected[group-1]));
    }

    // A pending space marker follows the same single release path.
    hello_keyboard_model_init_with_groups(&model,"",0,groups);select_group(&model,3);
    tap(&model,HELLO_KB_BUTTON_B,10);
    hello_keyboard_model_direction_release(&model,HELLO_KB_DOWN,61);
    assert(!strcmp(model.text," ")&&!model.pending&&!model.group);

    // A full buffer still presents the requested selection; release rejects
    // insertion and closes the group without mutating the existing text.
    char full[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];memset(full,'x',sizeof(full)-1);full[sizeof(full)-1]=0;
    hello_keyboard_model_init(&model,full,strlen(full));select_group(&model,4);
    tap(&model,HELLO_KB_BUTTON_A,10);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,61);
    assert(!strcmp(model.text,full)&&!model.pending&&!model.group);
    // Capacity is rechecked when committing an already selected item.
    hello_keyboard_model_init(&model,"x",1);select_group(&model,4);
    tap(&model,HELLO_KB_BUTTON_A,10);assert(model.pending=='e');model.field.max_length=1;
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,61);
    assert(!strcmp(model.text,"x")&&!model.pending&&!model.group);

    // Direction release never advances a due repeat before committing.
    for(int wrap=0;wrap<2;wrap++)for(unsigned elapsed=349;elapsed<=350;elapsed++) {
        const uint32_t start=wrap?UINT32_MAX-100:1000;
        hello_keyboard_model_init(&model,"",0);
        hello_keyboard_model_direction(&model,HELLO_KB_LEFT,start-1);
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,start);
        hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,start+elapsed);
        assert(!strcmp(model.text,"e")&&!model.pending&&!model.group);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+elapsed);
        hello_keyboard_model_update(&model,start+30000);
        assert(!strcmp(model.text,"e"));
    }
    int calls=0;
    const char* variants[HELLO_KEYBOARD_GROUP_COUNT]={NULL,NULL,NULL,NULL,NULL,"👍🙂"};
    hello_keyboard_model_init_with_groups(&model,"",0,variants);
    model.transform=emoji_variants;model.transform_userdata=&calls;select_group(&model,HELLO_KEYBOARD_EMOJI_GROUP);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,10);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,360);
    assert(!strcmp(model.text,"👍🏽")&&calls==1&&!model.pending&&!model.group);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,360);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,361);
    assert(!strcmp(model.text,"👍🏽")&&calls==1);

    // Completed selection holds cannot transform a later same-group selection,
    // even when the old physical A/B button stays down beyond its hold deadline.
    for(int side=0;side<2;side++) {
        const HelloKeyboardButton old_button=side?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B;
        const HelloKeyboardButton next_button=side?HELLO_KB_BUTTON_B:HELLO_KB_BUTTON_A;
        hello_keyboard_model_init(&model,"",0);
        hello_keyboard_model_direction(&model,HELLO_KB_LEFT,10);
        hello_keyboard_model_press(&model,old_button,20);
        hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,100);
        assert(!model.hold[old_button].active&&!model.feedback_active[old_button]);
        assert(!strcmp(model.text,side?"e":"a"));
        hello_keyboard_model_direction(&model,HELLO_KB_LEFT,110);
        hello_keyboard_model_press(&model,next_button,120);
        hello_keyboard_model_update(&model,370);
        assert(model.pending==(uint32_t)(side?'a':'e'));
        hello_keyboard_model_release(&model,old_button,380);
        assert(model.pending==(uint32_t)(side?'a':'e')&&!model.upper&&!model.alternate);
        hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,390);
        assert(!strcmp(model.text,side?"ea":"ae")&&!model.group&&!model.pending);
        assert(!model.hold[next_button].active&&!model.feedback_active[next_button]);
        hello_keyboard_model_release(&model,next_button,500);
        assert(!strcmp(model.text,side?"ea":"ae")&&!model.upper&&!model.alternate);
    }

    // Cursor releases stop without consuming a due repeat; unrelated releases do not stop it.
    hello_keyboard_model_init(&model,"abcdef",6);model.alternate=true;
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,999);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,1000);assert(model.cursor==5);
    hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,1349);
    assert(model.cursor_hold);
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,1350);
    hello_keyboard_model_update(&model,1350);
    assert(model.cursor==5&&!model.cursor_hold);
}

static void test_selection_hold_repeat_and_gate(void) {
    for(int wrap=0;wrap<2;wrap++)for(int button=0;button<2;button++) {
        const uint32_t start=wrap?UINT32_MAX-100:1000;
        HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);select_group(&model,1);
        hello_keyboard_model_press(&model,(HelloKeyboardButton)button,start);
        const int first=button==HELLO_KB_BUTTON_A?0:39,direction=button==HELLO_KB_BUTTON_A?1:-1;
        assert(model.index==first);
        hello_keyboard_model_update(&model,start+349);assert(model.index==first&&!model.selection_repeated);
        hello_keyboard_model_update(&model,start+350);assert(model.index==first+direction&&model.selection_repeated&&!model.upper);
        model.selection_repeated=false;
        hello_keyboard_model_update(&model,start+449);assert(model.index==first+direction&&!model.selection_repeated);
        hello_keyboard_model_update(&model,start+450);assert(model.index==first+2*direction);
        hello_keyboard_model_update(&model,start+1450);assert(model.index==first+3*direction); // Slow frames advance one item.
        hello_keyboard_model_release(&model,(HelloKeyboardButton)button,start+1451);
        const int stopped=model.index;hello_keyboard_model_update(&model,start+30000);assert(model.index==stopped);
        hello_keyboard_model_init(&model,"",0);select_group(&model,1);
        hello_keyboard_model_press(&model,(HelloKeyboardButton)button,start);
        hello_keyboard_model_update_with_selection(&model,start+350,false);
        hello_keyboard_model_update_with_selection(&model,start+10000,false);
        hello_keyboard_model_update_with_selection(&model,start+10001,true);assert(model.index==first);
        hello_keyboard_model_update_with_selection(&model,start+10100,true);assert(model.index==first+direction);
        hello_keyboard_model_init(&model,"",0);select_group(&model,1);
        hello_keyboard_model_press(&model,(HelloKeyboardButton)button,start);
        hello_keyboard_model_crank(&model,direction*39,start+1);
        hello_keyboard_model_update(&model,start+1000);
        assert(model.index==(direction>0?0:39)&&model.selection_wrapped);
        hello_keyboard_model_update_with_selection(&model,start+10000,false);
        assert(model.index==(direction>0?0:39));
        hello_keyboard_model_update_with_selection(&model,start+10100,true);
        assert(model.index==(direction>0?1:38));
    }
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);select_group(&model,4);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_release_with_selection(&model,HELLO_KB_BUTTON_A,1350,false);assert(model.pending=='e');
    hello_keyboard_model_init(&model,"",0);select_group(&model,4);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_release_with_selection(&model,HELLO_KB_BUTTON_A,1350,true);assert(model.pending=='i');
}

static bool replace_everything(const char* input,HelloKeyboardTransformAction action,char* output,size_t capacity,void* userdata) {
    (void)input;(void)action;(void)userdata;assert(capacity>1);strcpy(output,"X");return true;
}
static void test_return_is_submit_action(void) {
    char full[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];memset(full,'x',sizeof(full)-1);full[sizeof(full)-1]=0;
    const char* texts[]={"","abc",full};
    for(size_t run=0;run<3;run++) {
        HelloKeyboardModel model;hello_keyboard_model_init(&model,texts[run],strlen(texts[run]));
        model.field.max_length=run?strlen(texts[run]):0;model.upper=true;
        model.transform=replace_everything;
        hello_keyboard_model_direction(&model,HELLO_KB_DOWN,1000);
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1010);
        assert(model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT&&!strcmp(model.pending_text,HELLO_KEYBOARD_RETURN_TEXT));
        hello_keyboard_model_direction_release(&model,HELLO_KB_DOWN,1360);
        assert(model.submit_requested&&!model.pending&&!model.group&&!model.hold[HELLO_KB_BUTTON_B].active);
        assert(!strcmp(model.text,texts[run]));
        hello_keyboard_model_clear(&model);assert(!model.submit_requested);
    }
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);
    hello_keyboard_model_direction(&model,HELLO_KB_DOWN,1000);
    tap(&model,HELLO_KB_BUTTON_B,1010);assert(model.pending==0x23b5);
    hello_keyboard_model_direction_release(&model,HELLO_KB_DOWN,1061);assert(!strcmp(model.text," ")&&!model.submit_requested);
    hello_keyboard_model_init(&model,"",0);model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={NULL,NULL,NULL,NULL,"⏎12",NULL};
    hello_keyboard_model_init_with_groups(&model,"",0,groups);model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_model_direction(&model,HELLO_KB_UP,1000);
    tap(&model,HELLO_KB_BUTTON_A,1010);assert(model.pending=='1');
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,1061);assert(!strcmp(model.text,"1")&&!model.submit_requested);
}

static void test_first_selection_index_override(void) {
    HelloKeyboardModel model;
    for(int last=7;last<=9;last+=2) {
        hello_keyboard_model_init(&model,"",0);select_group(&model,1);
        hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,last);
        assert(model.index==last&&model.pending==(uint32_t)"1234567890"[last]&&model.pending_side==2);
        assert(model.hold[HELLO_KB_BUTTON_B].next_selection_at==1350);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,1050);
        hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_A,1100,0);
        assert(model.index==last+1); // Existing pending ignores the first-index hint.
    }
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,9);assert(model.pending=='0');
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,1050);
    tap(&model,HELLO_KB_BUTTON_A,1100);assert(model.index==10&&model.pending=='.');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,9);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,1050);
    for(int index=8;index>=0;index--) {
        tap(&model,HELLO_KB_BUTTON_B,1100+(uint32_t)(8-index)*60);
        assert(model.index==index&&model.pending==(uint32_t)"1234567890"[index]);
    }
    tap(&model,HELLO_KB_BUTTON_B,1700);assert(model.index==39&&model.pending=='(');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,9);
    hello_keyboard_model_update(&model,1350);assert(model.index==8&&model.pending=='9');
    hello_keyboard_model_update(&model,1450);assert(model.index==7&&model.pending=='8');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,100);assert(model.index==39);
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"1X2",NULL,NULL,NULL,NULL,NULL};
    hello_keyboard_model_init_with_groups(&model,"",0,groups);select_group(&model,HELLO_KEYBOARD_NUMBERS_GROUP);
    hello_keyboard_model_press_from(&model,HELLO_KB_BUTTON_B,1000,1);
    assert(model.index==1&&model.pending=='X'); // Type filtering waits for commit.
}

static void test_first_crank_index_override(void) {
    HelloKeyboardModel model;
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_crank_from(&model,-1,1000,9);
    assert(model.index==9&&model.pending=='0'&&model.pending_side==3);
    hello_keyboard_model_crank_from(&model,1,1010,0);
    assert(model.index==10&&model.pending=='.'); // A later hint cannot truncate the full group.
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_crank_from(&model,-1,1000,7);assert(model.index==7&&model.pending=='8');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_crank_from(&model,-2,1000,9);assert(model.index==8&&model.pending=='9');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_crank(&model,-1,1000);assert(model.index==39&&model.pending=='(');
    hello_keyboard_model_init(&model,"",0);select_group(&model,1);
    hello_keyboard_model_crank(&model,1,1000);assert(model.index==0&&model.pending=='1');
    hello_keyboard_model_crank_from(&model,-1,1010,9);assert(model.index==39&&model.pending=='(');
}

static void test_idle_a_navigation_disables_b_delete(void) {
    for(int order=0;order<2;order++) {
        HelloKeyboardModel model;hello_keyboard_model_init(&model,"abcdef",6);
        if(order)hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);
        const char* expected=order?"abcde":"abcdef";
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1010);
        if(!order)hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1020);
        assert(!model.hold[HELLO_KB_BUTTON_B].active);
        hello_keyboard_model_update(&model,1360);
        hello_keyboard_model_update(&model,5000);
        assert(!strcmp(model.text,expected)&&hello_keyboard_model_is_choosing_layer(&model));
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,5010);
        assert(!model.feedback_active[HELLO_KB_BUTTON_B]);
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,5020);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,5030);
        hello_keyboard_model_update(&model,6000);assert(!strcmp(model.text,expected));
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,6010);
        tap(&model,HELLO_KB_BUTTON_B,6100);assert(strlen(model.text)==strlen(expected)-1);
    }
}

static void test_cursor_step_api_preserves_gesture_and_graphemes(void) {
    const char* input="aé👨‍👩‍👧‍👦z";
    const size_t positions[]={0,1,strlen("aé"),strlen("aé👨‍👩‍👧‍👦"),strlen("aé👨‍👩‍👧‍👦z")};
    HelloKeyboardModel model;hello_keyboard_model_init(&model,input,strlen(input));model.upper=true;
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_update(&model,1350);assert(hello_keyboard_model_is_choosing_layer(&model));
    HelloKeyboardModel expected=model;
    for(int index=3;index>=0;index--) {
        assert(hello_keyboard_model_move_cursor(&model,-1)==-1&&model.cursor==positions[index]);
        expected.cursor=positions[index];assert(!memcmp(&model,&expected,sizeof(model)));
    }
    assert(hello_keyboard_model_move_cursor(&model,INT_MIN)==0);
    assert(hello_keyboard_model_move_cursor(&model,INT_MAX)==4&&model.cursor==positions[4]);
    expected.cursor=positions[4];assert(!memcmp(&model,&expected,sizeof(model)));
    assert(hello_keyboard_model_move_cursor(&model,INT_MAX)==0);
    assert(hello_keyboard_model_move_cursor(&model,0)==0);
    assert(hello_keyboard_model_move_cursor(&model,INT_MIN)==-4&&!model.cursor);
    expected.cursor=0;assert(!memcmp(&model,&expected,sizeof(model)));
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,1500);
    assert(model.upper&&!strcmp(model.text,input)&&!hello_keyboard_model_is_choosing_layer(&model));
}

static size_t model_family_text(char* output,size_t count) {
    const char* family="👨‍👩‍👧‍👦";const size_t bytes=strlen(family);
    assert(count*bytes<=HELLO_KEYBOARD_TEXT_BYTES);
    for(size_t i=0;i<count;i++)memcpy(output+i*bytes,family,bytes);
    output[count*bytes]=0;return count*bytes;
}
static void test_independent_text_bytes_and_grapheme_capacity(void) {
    const char* family="👨‍👩‍👧‍👦";
    char text[HELLO_KEYBOARD_TEXT_BYTES+2],entry[40];snprintf(entry,sizeof(entry),"%sa",family);
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={entry,NULL,NULL,NULL,NULL,NULL};
    HelloKeyboardModel model;
    model_family_text(text,159);hello_keyboard_model_init_with_groups(&model,text,strlen(text),groups);
    select_group(&model,1);tap(&model,HELLO_KB_BUTTON_A,1000);
    assert(!strcmp(model.pending_text,family));hello_keyboard_model_commit(&model,false);
    assert(strlen(model.text)==4000&&brb_grapheme_count(model.text)==160&&model.cursor==4000);
    select_group(&model,1);tap(&model,HELLO_KB_BUTTON_A,1100);assert(!strcmp(model.pending_text,family));
    hello_keyboard_model_commit(&model,false);assert(strlen(model.text)==4000&&model.input_rejected==HELLO_FIELD_TOO_LONG);
    // Opening through the wrapper validates; the low-level initializer preserves
    // bounded raw text so a later policy may use a larger grapheme count.
    model_family_text(text,161);hello_keyboard_model_init(&model,text,SIZE_MAX);
    assert(strlen(model.text)==4025&&brb_grapheme_count(model.text)==161);
    assert(hello_input_field_validate(&model.field,model.text,true,NULL)==HELLO_FIELD_TOO_LONG);
    HelloKeyboardFieldPolicy policy=model.field;policy.max_length=161;
    assert(hello_keyboard_model_set_field_policy(&model,&policy));
    assert(hello_input_field_validate(&model.field,model.text,true,NULL)==HELLO_FIELD_VALID);
    size_t at=model_family_text(text,159);text[at++]='a';
    for(int i=0;i<60;i++){memcpy(text+at,"́",2);at+=2;}text[at]=0;assert(at==4096);
    hello_keyboard_model_init(&model,text,SIZE_MAX);
    assert(strlen(model.text)==4096&&model.cursor==4096&&brb_grapheme_count(model.text)==160);
    at-=2;memcpy(text+at,"⃝",3);at+=3;text[at]=0;assert(at==4097);
    hello_keyboard_model_init(&model,text,SIZE_MAX);
    assert(strlen(model.text)==3975&&model.cursor==3975&&brb_grapheme_count(model.text)==159);
    // A two-byte combining insertion stays within the grapheme limit but
    // exceeds4096 bytes. Selection stays exact; commit rejects it atomically.
    at=model_family_text(text,158);text[at++]='a';
    for(int i=0;i<72;i++){memcpy(text+at,"́",2);at+=2;}text[at]=0;assert(at==4095);
    const char* combining[HELLO_KEYBOARD_GROUP_COUNT]={"́b",NULL,NULL,NULL,NULL,NULL};
    hello_keyboard_model_init_with_groups(&model,text,at,combining);select_group(&model,1);
    tap(&model,HELLO_KB_BUTTON_A,1200);assert(model.pending==0x301&&model.index==0&&!strcmp(model.text,text));
    hello_keyboard_model_commit(&model,false);assert(!strcmp(model.text,text)&&model.input_rejected==HELLO_FIELD_TOO_LONG);
    select_group(&model,1);tap(&model,HELLO_KB_BUTTON_B,1300);assert(model.pending=='b'&&model.index==1);
    hello_keyboard_model_commit(&model,false);assert(strlen(model.text)==4096&&brb_grapheme_count(model.text)==160);
}
static void test_large_context_transform_join_and_instance_policy(void) {
    char text[HELLO_KEYBOARD_TEXT_BYTES+1];model_family_text(text,159);
    const char* sharp[HELLO_KEYBOARD_GROUP_COUNT]={"ßa",NULL,NULL,NULL,NULL,"ßa"};
    HelloKeyboardModel first,second;
    hello_keyboard_model_init_with_groups(&first,text,strlen(text),sharp);select_group(&first,6);tap(&first,HELLO_KB_BUTTON_A,1000);
    assert(!strcmp(first.pending_text,"SS")&&brb_grapheme_count(first.text)==159); // SS would be161.
    hello_keyboard_model_commit(&first,false);assert(!strcmp(first.text,text)&&first.input_rejected==HELLO_FIELD_TOO_LONG);
    HelloKeyboardFieldPolicy policy=first.field;policy.max_length=161;policy.min_length=1;
    hello_keyboard_model_init_with_groups(&second,text,strlen(text),sharp);assert(hello_keyboard_model_set_field_policy(&second,&policy));
    select_group(&second,6);tap(&second,HELLO_KB_BUTTON_A,1000);assert(!strcmp(second.pending_text,"SS"));
    assert(first.field.max_length==160&&second.field.max_length==161);
    HelloKeyboardFieldPolicy before=second.field;policy.min_length=162;
    assert(!hello_keyboard_model_set_field_policy(&second,&policy));assert(!memcmp(&before,&second.field,sizeof(before)));
    assert(!strcmp(second.text,text)&&!strcmp(second.pending_text,"SS"));
    // Joining the existing last cluster is still legal at160 graphemes.
    strcat(text,"e");const char* accents[HELLO_KEYBOARD_GROUP_COUNT]={"́a",NULL,NULL,NULL,NULL,NULL};
    hello_keyboard_model_init_with_groups(&first,text,strlen(text),accents);select_group(&first,1);tap(&first,HELLO_KB_BUTTON_A,2000);
    assert(first.pending==0x301);hello_keyboard_model_commit(&first,false);
    assert(brb_grapheme_count(first.text)==160&&strlen(first.text)==3978&&!strcmp(first.text+3975,"é"));
}

static void prime_middle_selection(HelloKeyboardModel* model,int origin,bool upper,uint32_t start) {
    hello_keyboard_model_init(model,"",0);model->upper=upper;
    hello_keyboard_model_direction(model,HELLO_KB_RIGHT,start-1);
    if(origin==0)hello_keyboard_model_crank(model,5,start+240);
    else for(unsigned n=0;n<5;n++)tap(model,origin==1?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B,start+n*60);
    assert(model->index==4&&model->pending==(uint32_t)(upper?'Q':'q'));
}

static void test_button_navigation_scale_only_changes_button_repeat(void) {
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"",0);
    assert(model.button_navigation_scale==1.0f&&hello_keyboard_model_selection_hold_ms(&model)==350&&hello_keyboard_model_selection_repeat_ms(&model)==100);
    assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
    const float invalid[]={0,.249f,2.001f,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        assert(!hello_keyboard_model_set_button_navigation_scale(&model,invalid[i])&&model.button_navigation_scale==.5f);
    }
    for(int button=0;button<2;button++) {
        hello_keyboard_model_init(&model,"",0);assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
        hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,999);
        hello_keyboard_model_press(&model,(HelloKeyboardButton)button,1000);
        const int first=button==HELLO_KB_BUTTON_A?0:8,direction=button==HELLO_KB_BUTTON_A?1:-1;
        assert(model.index==first&&hello_keyboard_model_selection_hold_ms(&model)==175&&hello_keyboard_model_selection_repeat_ms(&model)==50);
        hello_keyboard_model_update(&model,1174);assert(model.index==first);
        hello_keyboard_model_update(&model,1175);assert(model.index==first+direction);
        hello_keyboard_model_update(&model,1224);assert(model.index==first+direction);
        hello_keyboard_model_update(&model,1225);assert(model.index==first+2*direction);
        hello_keyboard_model_update(&model,2225);assert(model.index==first+3*direction); // No catch-up burst.
        hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,2275);
        assert(!model.group&&!model.pending&&strlen(model.text)==1); // Release wins over a due repeat.
    }
    prime_middle_selection(&model,0,false,1000);assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
    tap(&model,HELLO_KB_BUTTON_B,1320);hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1400);
    hello_keyboard_model_update(&model,1574);assert(model.index==2&&model.pending=='m');
    hello_keyboard_model_update(&model,1575);assert(model.index==1&&model.pending=='o'&&!model.upper);
    hello_keyboard_model_update(&model,1625);assert(model.index==0&&model.pending=='n'&&!model.upper);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_B,1626);assert(model.index==0&&model.pending=='n');
    hello_keyboard_model_init(&model,"abcd",4);assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_B,1000);assert(!strcmp(model.text,"abc"));
    hello_keyboard_model_update(&model,1175);assert(!strcmp(model.text,"abc"));
    hello_keyboard_model_update(&model,1350);assert(!strcmp(model.text,"ab")); // Idle delete still350ms.
    hello_keyboard_model_init(&model,"",0);assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_update(&model,1175);assert(!hello_keyboard_model_is_choosing_layer(&model));
    hello_keyboard_model_update(&model,1350);assert(hello_keyboard_model_is_choosing_layer(&model));
    HelloKeyboardModel ordinary,scaled;hello_keyboard_model_init(&ordinary,"",0);hello_keyboard_model_init(&scaled,"",0);
    assert(hello_keyboard_model_set_button_navigation_scale(&scaled,.5f));
    hello_keyboard_model_direction(&ordinary,HELLO_KB_RIGHT,1);hello_keyboard_model_direction(&scaled,HELLO_KB_RIGHT,1);
    const int steps[]={5,-1,2,-3};for(unsigned i=0;i<4;i++) {
        hello_keyboard_model_crank(&ordinary,steps[i],100+i);hello_keyboard_model_crank(&scaled,steps[i],100+i);
        assert(ordinary.index==scaled.index&&ordinary.pending==scaled.pending&&!strcmp(ordinary.pending_text,scaled.pending_text));
    }
}

static void test_dpad_cursor_shares_scaled_selection_repeat(void) {
    const char* text="aé👨‍👩‍👧‍👦z";
    const size_t positions[]={0,1,strlen("aé"),strlen("aé👨‍👩‍👧‍👦"),strlen(text)};
    for(int scaled=0;scaled<2;scaled++)for(int right=0;right<2;right++)
    for(int upper=0;upper<2;upper++)for(int rollover=0;rollover<2;rollover++) {
        const uint32_t start=rollover?UINT32_MAX-100:1000;
        HelloKeyboardModel model;hello_keyboard_model_init(&model,text,positions[2]);model.upper=(bool)upper;
        assert(hello_keyboard_model_set_button_navigation_scale(&model,scaled?.5f:1.0f));
        const uint32_t delay=hello_keyboard_model_selection_hold_ms(&model),repeat=hello_keyboard_model_selection_repeat_ms(&model);
        assert(delay==(scaled?175u:350u)&&repeat==(scaled?50u:100u));
        const HelloKeyboardDirection direction=right?HELLO_KB_RIGHT:HELLO_KB_LEFT;
        const HelloKeyboardDirection reverse=right?HELLO_KB_LEFT:HELLO_KB_RIGHT;
        hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,start);
        hello_keyboard_model_direction(&model,direction,start+1);
        assert(model.cursor==positions[right?3:1]&&hello_keyboard_model_is_choosing_layer(&model));
        hello_keyboard_model_update(&model,start+delay);assert(model.cursor==positions[right?3:1]);
        hello_keyboard_model_update(&model,start+1+delay);assert(model.cursor==positions[right?4:0]);
        hello_keyboard_model_update(&model,start+10000);assert(model.cursor==positions[right?4:0]); // Stop, never wrap.
        hello_keyboard_model_direction(&model,reverse,start+10001);assert(model.cursor==positions[right?3:1]);
        hello_keyboard_model_direction_release(&model,reverse,start+10001+delay);
        hello_keyboard_model_update(&model,start+10001+delay);assert(model.cursor==positions[right?3:1]&&!model.cursor_hold);
        hello_keyboard_model_direction(&model,direction,start+11000);assert(model.cursor==positions[right?4:0]);
        hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,start+11000+delay);
        hello_keyboard_model_update(&model,start+11000+delay);
        assert(!model.cursor_hold&&model.upper==(bool)upper&&!model.group&&!model.pending&&!strcmp(model.text,text));
    }
    HelloKeyboardModel model;hello_keyboard_model_init(&model,"abcdefghijk",11);
    assert(hello_keyboard_model_set_button_navigation_scale(&model,.5f));
    cursor_direction(&model,HELLO_KB_LEFT,1000);assert(model.cursor==10);
    hello_keyboard_model_update(&model,5000);assert(model.cursor==9&&model.cursor_next_at==5050);
    hello_keyboard_model_update(&model,5000);assert(model.cursor==9);
    hello_keyboard_model_update(&model,5049);assert(model.cursor==9);
    hello_keyboard_model_update(&model,5050);assert(model.cursor==8);
    hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,5051);assert(model.cursor==9); // Reversal has no stale deadline.
    hello_keyboard_model_update(&model,5100);assert(model.cursor==9);
    hello_keyboard_model_update(&model,5226);assert(model.cursor==10);
}

static void test_tap_then_hold_repeats_without_case_changes(void) {
    const char* letters="nomkqjplr";
    for(int origin=0;origin<3;origin++)for(int upper=0;upper<2;upper++)for(int button=0;button<2;button++)for(int half=0;half<2;half++)for(int rollover=0;rollover<2;rollover++) {
        const uint32_t start=rollover?UINT32_MAX-500:1000;HelloKeyboardModel model;prime_middle_selection(&model,origin,upper,start);
        assert(hello_keyboard_model_set_button_navigation_scale(&model,half?.5f:1.0f));
        const HelloKeyboardButton face=(HelloKeyboardButton)button;const int direction=button==HELLO_KB_BUTTON_A?1:-1;
        tap(&model,face,start+320);assert(model.index==4+direction);
        hello_keyboard_model_press(&model,face,start+400);assert(model.index==4+2*direction&&model.hold[button].hold==HELLO_KB_HOLD_REPEAT);
        const uint32_t delay=half?175:350,repeat=half?50:100;
        hello_keyboard_model_update(&model,start+400+delay-1);assert(model.index==4+2*direction);
        hello_keyboard_model_update(&model,start+400+delay);assert(model.index==4+3*direction);
        const char expected=letters[model.index];assert(model.pending==(uint32_t)(upper?expected-'a'+'A':expected)&&model.upper==(bool)upper);
        hello_keyboard_model_update(&model,start+400+delay+repeat);assert(model.index==4+4*direction&&model.upper==(bool)upper);
        hello_keyboard_model_release(&model,face,start+401+delay+repeat);
        const int stopped=model.index;hello_keyboard_model_update(&model,start+30000);
        assert(model.index==stopped&&model.pending&&!model.text[0]&&model.upper==(bool)upper);
        char selected[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_pending_text(&model,selected,sizeof(selected));
        hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,start+30001);assert(!strcmp(model.text,selected)&&!model.group&&!model.pending);
    }
}

static void test_instance_views_and_emoji_picker_input(void) {
    const char* text="aé👨‍👩‍👧‍👦z";
    HelloKeyboardModel model,other;
    hello_keyboard_model_init(&model,text,strlen(text));
    hello_keyboard_model_init(&other,"other",5);
    assert(hello_keyboard_model_layer(&model)==1&&hello_keyboard_model_layer_count(&model)==1);
    assert(!hello_keyboard_model_set_view_count(&model,0)&&!hello_keyboard_model_set_view_count(&model,3));
    assert(hello_keyboard_model_set_view_count(&model,2));
    assert(hello_keyboard_model_layer_count(&model)==2&&hello_keyboard_model_layer_count(&other)==1);
    assert(!hello_keyboard_model_set_view(&model,(HelloKeyboardView)0));
    assert(!hello_keyboard_model_set_view(&model,(HelloKeyboardView)3));
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1000);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,1001);
    assert(hello_keyboard_model_layer(&model)==2&&hello_keyboard_model_is_choosing_layer(&model));
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,1002);
    assert(!model.upper&&!model.group&&!model.pending&&!model.view_confirm_requested);
    for(int direction=0;direction<4;direction++) {
        hello_keyboard_model_direction(&model,(HelloKeyboardDirection)direction,1010+(uint32_t)direction);
        hello_keyboard_model_direction_release(&model,(HelloKeyboardDirection)direction,1020+(uint32_t)direction);
    }
    hello_keyboard_model_crank(&model,10,1030);
    tap(&model,HELLO_KB_BUTTON_B,1040);
    assert(!strcmp(model.text,text)&&model.cursor==strlen(text)&&!model.group&&!model.pending);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1100);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,1116);
    assert(model.view_confirm_requested&&!model.upper&&!model.submit_requested&&!strcmp(model.text,text));
    hello_keyboard_model_reset_input(&model);
    assert(!model.view_confirm_requested&&hello_keyboard_model_layer(&model)==2&&hello_keyboard_model_layer_count(&model)==2);
    hello_keyboard_model_press(&model,HELLO_KB_BUTTON_A,1200);
    hello_keyboard_model_update(&model,1550);
    hello_keyboard_model_direction(&model,HELLO_KB_LEFT,1551);
    assert(model.cursor==strlen(text)-1&&hello_keyboard_model_is_choosing_layer(&model));
    hello_keyboard_model_direction_release(&model,HELLO_KB_LEFT,1552);
    hello_keyboard_model_direction(&model,HELLO_KB_DOWN,1553);
    assert(hello_keyboard_model_layer(&model)==1&&!model.group&&!model.pending);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,1554);
    assert(hello_keyboard_model_layer(&model)==2);
    hello_keyboard_model_release(&model,HELLO_KB_BUTTON_A,1555);
    assert(!model.upper&&!model.view_confirm_requested&&!strcmp(model.text,text));
    hello_keyboard_model_clear(&model);
    assert(!model.text[0]&&hello_keyboard_model_layer(&model)==2&&hello_keyboard_model_layer_count(&model)==2);
    assert(hello_keyboard_model_set_view_count(&model,1));
    assert(hello_keyboard_model_layer(&model)==1&&hello_keyboard_model_layer_count(&model)==1);
    tap(&model,HELLO_KB_BUTTON_A,1600);
    assert(model.upper&&!model.view_confirm_requested);
}

static void test_shifted_up_inventory_fallback(void) {
    HelloKeyboardModel model;
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"12!?_",NULL,NULL,NULL,NULL,"😀😎"};
    hello_keyboard_model_init_with_groups(&model,"",0,groups);
    model.upper=true;
    assert(hello_keyboard_model_direction_group(&model,HELLO_KB_UP)==HELLO_KEYBOARD_EMOJI_GROUP);
    model.shifted_up_enabled=false;
    hello_keyboard_model_direction(&model,HELLO_KB_UP,100);
    assert(model.group==1&&model.upper);
    const char* symbols="12!?_";
    for(int i=0;i<5;i++) {
        tap(&model,HELLO_KB_BUTTON_A,110+(uint32_t)i*20);
        assert(model.index==i&&model.pending==(uint32_t)symbols[i]);
    }
    tap(&model,HELLO_KB_BUTTON_B,220);assert(model.pending=='?');
    hello_keyboard_model_crank(&model,-2,240);assert(model.pending=='2');
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,260);
    assert(!strcmp(model.text,"2")&&!model.group&&model.upper);
    hello_keyboard_model_direction(&model,HELLO_KB_RIGHT,280);
    tap(&model,HELLO_KB_BUTTON_A,290);assert(model.pending=='N');
    hello_keyboard_model_direction_release(&model,HELLO_KB_RIGHT,310);
    assert(!strcmp(model.text,"2N")&&model.upper);
    hello_keyboard_model_clear(&model);model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_model_direction(&model,HELLO_KB_UP,330);
    hello_keyboard_model_crank(&model,3,340);assert(model.pending=='!');
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,350);
    assert(!model.text[0]&&model.input_rejected!=HELLO_FIELD_VALID);
    hello_keyboard_model_direction(&model,HELLO_KB_UP,370);
    tap(&model,HELLO_KB_BUTTON_A,380);
    hello_keyboard_model_direction_release(&model,HELLO_KB_UP,400);
    assert(!strcmp(model.text,"1")&&model.upper&&!model.shifted_up_enabled);
}

int main(void) {
    test_shifted_up_inventory_fallback();
    test_instance_views_and_emoji_picker_input();
    test_tap_then_hold_repeats_without_case_changes();
    test_dpad_cursor_shares_scaled_selection_repeat();
    test_button_navigation_scale_only_changes_button_repeat();
    test_independent_text_bytes_and_grapheme_capacity();
    test_large_context_transform_join_and_instance_policy();
    test_cursor_step_api_preserves_gesture_and_graphemes();
    test_idle_a_navigation_disables_b_delete();
    test_first_crank_index_override();
    test_first_selection_index_override();
    test_selection_hold_repeat_and_gate();
    test_return_is_submit_action();
    test_group_release_commit_contract();
    test_graphemes_and_variant_hook();
    test_emoji_layer_and_number_policy();
    // Selection never skips a multibyte item that would not fit; a later
    // opposite or crank step still follows the exact configured item order.
    HelloKeyboardModel mixed;char almost_full[159];memset(almost_full,'x',158);almost_full[158]=0;
    const char* mixed_groups[HELLO_KEYBOARD_GROUP_COUNT]={"a😀äb",NULL,NULL,NULL,NULL};
    hello_keyboard_model_init_with_groups(&mixed,almost_full,158,mixed_groups);mixed.field.max_bytes=160;
    hello_keyboard_model_direction(&mixed,HELLO_KB_UP,1);tap(&mixed,HELLO_KB_BUTTON_A,10);assert(mixed.pending=='a');
    tap(&mixed,HELLO_KB_BUTTON_A,80);assert(mixed.pending==0x1f600&&mixed.index==1);
    tap(&mixed,HELLO_KB_BUTTON_B,150);assert(mixed.pending=='a');
    hello_keyboard_model_crank(&mixed,-1,230);assert(mixed.pending=='b');
    hello_keyboard_model_commit(&mixed,false);assert(strlen(mixed.text)==159);
    HelloKeyboardModel limited;hello_keyboard_model_init(&limited,"12345",5);limited.field.max_length=6;limited.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_model_direction(&limited,HELLO_KB_UP,1);tap(&limited,HELLO_KB_BUTTON_A,10);hello_keyboard_model_commit(&limited,false);
    assert(strlen(limited.text)==6);char saved[7];strcpy(saved,limited.text);
    hello_keyboard_model_direction(&limited,HELLO_KB_UP,100);tap(&limited,HELLO_KB_BUTTON_A,110);
    assert(limited.pending=='1');hello_keyboard_model_crank(&limited,1,150);assert(limited.pending=='2');
    hello_keyboard_model_commit(&limited,false);tap(&limited,HELLO_KB_BUTTON_A,200);assert(!strcmp(limited.text,saved));
    limited.cursor=3;hello_keyboard_model_direction(&limited,HELLO_KB_UP,300);tap(&limited,HELLO_KB_BUTTON_B,310);
    hello_keyboard_model_commit(&limited,false);assert(!strcmp(limited.text,saved));
    tap(&limited,HELLO_KB_BUTTON_B,400);assert(strlen(limited.text)==5);
    hello_keyboard_model_clear(&limited);assert(limited.field.max_length==6);
    test_long_group_limits_and_navigation();
    test_multitap_and_direction_release();
    test_insert_delete_and_cursor();
    test_delete_starts_with_first_b_press();
    test_delete_feedback_requires_a_character();
    test_idle_a_case_and_layer_chooser();
    test_first_b_hold_deletes_without_switching_view();
    test_idle_b_hold_cannot_delete_new_group_commit();
    test_crank_and_limit();
    test_layer_chooser_cursor_holds();
    test_a_starts_left_and_b_starts_right();
    test_buttons_cross_halves_and_reverse();
    test_first_press_selects_immediately_and_hold_repeats();
    test_custom_groups_are_copied_and_used();
    test_invalid_custom_groups_use_defaults();
    test_default_punctuation_navigation();
    test_unicode_group_navigation();
    test_unicode_editing_and_limits();
    puts("keyboard model tests: OK");
    return 0;
}
