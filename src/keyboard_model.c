#include "keyboard_model.h"
#include "emoji_set.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

static const char* const kDefaultGroups[] = {
    "",
    "1234567890.,'?!-:/@_#~[<{\\}>]^=;*%$€&+)(",
    "nomkqjplr",
    "tswvxzyu⎵",
    "eidgbfcha",
    "1234567890"
};

static int positive_mod(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

int hello_keyboard_utf8_decode(const char* text, uint32_t* codepoint) {
    if(!text||!codepoint)return -1;
    if(!*text){*codepoint=0;return 0;}
    const size_t bytes=brb_utf8_decode(text,strlen(text),codepoint);
    return bytes?(int)bytes:-1;
}

int hello_keyboard_utf8_encode(uint32_t codepoint, char output[HELLO_KEYBOARD_MAX_UTF8_BYTES + 1]) {
    if(codepoint>0x10ffff||(codepoint>=0xd800&&codepoint<=0xdfff)){output[0]=0;return 0;}
    if (codepoint <= 0x7f) {
        output[0] = (char)codepoint;
        output[1] = '\0';
        return codepoint == 0 ? 0 : 1;
    }
    if (codepoint <= 0x7ff) {
        output[0] = (char)(0xc0 | (codepoint >> 6));
        output[1] = (char)(0x80 | (codepoint & 0x3f));
        output[2] = '\0';
        return 2;
    }
    if (codepoint <= 0xffff && !(codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        output[0] = (char)(0xe0 | (codepoint >> 12));
        output[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[2] = (char)(0x80 | (codepoint & 0x3f));
        output[3] = '\0';
        return 3;
    }
    if (codepoint <= 0x10ffff) {
        output[0] = (char)(0xf0 | (codepoint >> 18));
        output[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        output[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[3] = (char)(0x80 | (codepoint & 0x3f));
        output[4] = '\0';
        return 4;
    }
    output[0] = '\0';
    return 0;
}

size_t hello_keyboard_utf8_count(const char* text) {
    if(text==NULL)return SIZE_MAX;
    size_t count=0;
    while(*text) {
        uint32_t codepoint;const int bytes=hello_keyboard_utf8_decode(text,&codepoint);
        if(bytes<=0)return SIZE_MAX;
        text+=bytes;count++;
    }
    return count;
}


const char* hello_keyboard_default_group_chars(int group) {
    if (group == HELLO_KEYBOARD_EMOJI_GROUP) return brb_emoji_everyday_set()->keyboard_utf8;
    return group >= 1 && group <= HELLO_KEYBOARD_NUMBERS_GROUP ? kDefaultGroups[group] : "";
}

static bool item_text_valid(const char* text) {
    if(!text||!*text)return false;
    const size_t length=strlen(text);
    for(size_t at=0;at<length;) {
        uint32_t cp;const size_t bytes=brb_utf8_decode(text+at,length-at,&cp);
        if(!bytes||cp<0x20||(cp>=0x7f&&cp<=0x9f)||cp==0x2028||cp==0x2029)return false;
        at+=bytes;
    }
    return true;
}

bool hello_keyboard_model_group_is_valid(const char* characters) {
    if(!characters||strlen(characters)>HELLO_KEYBOARD_MAX_GROUP_BYTES||!item_text_valid(characters))return false;
    const size_t count=brb_grapheme_count(characters);
    if(count<2||count>HELLO_KEYBOARD_MAX_GROUP_CHARS)return false;
    const size_t length=strlen(characters);
    for(size_t at=0;at<length;) {
        const size_t bytes=brb_grapheme_next(characters+at,length-at);
        if(!bytes||bytes>HELLO_KEYBOARD_ITEM_BYTES)return false;
        at+=bytes;
    }
    return true;
}

const char* hello_keyboard_model_group_chars(const HelloKeyboardModel* model,int group) {
    return model&&group>=1&&group<=HELLO_KEYBOARD_GROUP_COUNT?model->groups[group]:"";
}

int hello_keyboard_model_group_length(const HelloKeyboardModel* model,int group) {
    const size_t count=brb_grapheme_count(hello_keyboard_model_group_chars(model,group));
    return count==SIZE_MAX?0:(int)count;
}

size_t hello_keyboard_model_group_text(const HelloKeyboardModel* model,
    int group,int index,char* output,size_t capacity) {
    if(!output||!capacity)return 0;
    output[0]=0;
    if(index<0)return 0;
    const char* characters=hello_keyboard_model_group_chars(model,group);
    const size_t length=strlen(characters);
    for(size_t at=0;at<length;--index) {
        const size_t bytes=brb_grapheme_next(characters+at,length-at);
        if(!bytes)return 0;
        if(index==0) {
            if(bytes>=capacity)return 0;
            memcpy(output,characters+at,bytes);output[bytes]=0;return bytes;
        }
        at+=bytes;
    }
    return 0;
}

uint32_t hello_keyboard_model_group_codepoint(const HelloKeyboardModel* model,int group,int index) {
    char text[HELLO_KEYBOARD_ITEM_BYTES+1];uint32_t cp=0;
    if(hello_keyboard_model_group_text(model,group,index,text,sizeof(text)))
        hello_keyboard_utf8_decode(text,&cp);
    return cp;
}

size_t hello_keyboard_model_pending_text(const HelloKeyboardModel* model,char* output,size_t capacity) {
    if(!output||!capacity)return 0;
    output[0]=0;if(!model->pending)return 0;
    uint32_t cp=0;
    if(model->pending_text[0]&&hello_keyboard_utf8_decode(model->pending_text,&cp)>0&&cp==model->pending) {
        const size_t bytes=strlen(model->pending_text);
        if(bytes>=capacity)return 0;
        memcpy(output,model->pending_text,bytes+1);return bytes;
    }
    char encoded[5];const int bytes=hello_keyboard_utf8_encode(model->pending,encoded);
    if(bytes<=0||(size_t)bytes>=capacity)return 0;
    memcpy(output,encoded,(size_t)bytes+1);return (size_t)bytes;
}

void hello_keyboard_model_cancel_gestures(HelloKeyboardModel* model) {
    memset(model->hold,0,sizeof(model->hold));
    memset(model->feedback_active,0,sizeof(model->feedback_active));
    model->cursor_hold=false;
    model->selection_repeated=model->selection_wrapped=false;
    model->view_confirm_requested=false;
}

static void reset_transient(HelloKeyboardModel* model) {
    model->group = 0;
    model->index = 0;
    model->pending = 0;
    model->pending_text[0] = 0;
    model->pending_side = 0;
    model->selection_repeated = model->selection_wrapped = model->submit_requested = false;
    model->view_confirm_requested=false;
    model->cursor_hold = false;
    model->input_rejected=HELLO_FIELD_VALID;
    memset(model->hold, 0, sizeof(model->hold));
    memset(model->feedback_active, 0, sizeof(model->feedback_active));
}

void hello_keyboard_model_reset_input(HelloKeyboardModel* model) {
    reset_transient(model);
}

void hello_keyboard_model_init_with_groups(
    HelloKeyboardModel* model,
    const char* text,
    size_t cursor,
    const char* const groups[HELLO_KEYBOARD_GROUP_COUNT]) {
    memset(model, 0, sizeof(*model));
    model->timing=brb_input_timing_defaults();
    model->button_navigation_scale=1.0f;
    model->view=HELLO_KB_VIEW_KEYBOARD;
    model->view_count=1;
    model->shifted_up_enabled=true;
    for (int index = 0; index < HELLO_KEYBOARD_GROUP_COUNT; ++index) {
        const char* value = groups != NULL ? groups[index] : NULL;
        if (!hello_keyboard_model_group_is_valid(value))
            value = hello_keyboard_default_group_chars(index + 1);
        strncpy(model->groups[index + 1], value, HELLO_KEYBOARD_MAX_GROUP_BYTES);
        model->groups[index + 1][HELLO_KEYBOARD_MAX_GROUP_BYTES] = '\0';
    }
    model->field = hello_input_field_default_policy();
    if(text!=NULL) {
        const size_t length=strlen(text);size_t at=0;
        while(at<length) {
            const size_t bytes=brb_grapheme_next(text+at,length-at);
            if(!bytes||at+bytes>HELLO_KEYBOARD_TEXT_BYTES)break;
            memcpy(model->text+at,text+at,bytes);at+=bytes;
        }
        model->text[at]=0;
    }
    model->cursor=brb_grapheme_floor(model->text,cursor);

}

void hello_keyboard_model_init(HelloKeyboardModel* model, const char* text, size_t cursor) {
    hello_keyboard_model_init_with_groups(model, text, cursor, NULL);
}

void hello_keyboard_model_clear(HelloKeyboardModel* model) {
    const bool upper = model->upper;
    const bool alternate = model->alternate;
    model->text[0] = '\0';
    model->cursor = 0;
    reset_transient(model);
    model->upper = upper;
    model->alternate = alternate;
}

bool hello_keyboard_model_set_button_navigation_scale(HelloKeyboardModel* model,float scale) {
    if(!model||!isfinite(scale)||scale<HELLO_KEYBOARD_BUTTON_NAVIGATION_SCALE_MIN||
       scale>HELLO_KEYBOARD_BUTTON_NAVIGATION_SCALE_MAX)return false;
    model->button_navigation_scale=scale;
    return true;
}
static uint32_t selection_duration(const HelloKeyboardModel* model,uint32_t duration) {
    if(model->button_navigation_scale==1.0f)return duration;
    const float scaled=(float)duration*model->button_navigation_scale;
    if(scaled>=(float)INT32_MAX)return INT32_MAX;
    const uint32_t rounded=(uint32_t)(scaled+0.5f);
    return rounded?rounded:1;
}
uint32_t hello_keyboard_model_selection_hold_ms(const HelloKeyboardModel* model) {
    return selection_duration(model,model->timing.hold_ms);
}
uint32_t hello_keyboard_model_selection_repeat_ms(const HelloKeyboardModel* model) {
    return selection_duration(model,model->timing.repeat_ms);
}

bool hello_keyboard_model_set_field_policy(HelloKeyboardModel* model,const HelloKeyboardFieldPolicy* policy) {
    if(!model||!hello_input_field_policy_valid(policy))return false;
    model->field=*policy;
    if(!model->field.max_bytes)model->field.max_bytes=HELLO_KEYBOARD_TEXT_BYTES;
    return true;
}

bool hello_keyboard_model_set_timing(HelloKeyboardModel* model,const BrbInputTimingConfig* timing) {
    if(!model||!brb_input_timing_valid(timing))return false;
    model->timing=*timing;return true;
}

static const char* committed_text(const char* text) {
    return !strcmp(text,"⎵")?" ":text;
}

static bool type_accepts_item(const HelloKeyboardModel* model,const char* text) {
    if(model->field.type!=HELLO_INPUT_CONTACT_NUMBER||!strcmp(text,HELLO_KEYBOARD_RETURN_TEXT))return true;
    text=committed_text(text);
    for(size_t i=0;text[i];++i)if(text[i]<'0'||text[i]>'9')return false;
    return true;
}
static HelloInputValidation insertion_status(HelloKeyboardModel* model,const char* text) {
    if(!strcmp(text,HELLO_KEYBOARD_RETURN_TEXT))return HELLO_FIELD_VALID;
    if(!type_accepts_item(model,text))return HELLO_FIELD_INVALID_TYPE;
    text=committed_text(text);
    const size_t bytes=strlen(text),length=strlen(model->text);
    if(!bytes||bytes+length>HELLO_KEYBOARD_TEXT_BYTES)return HELLO_FIELD_TOO_LONG;
    char* candidate=model->candidate;
    memcpy(candidate,model->text,model->cursor);
    memcpy(candidate+model->cursor,text,bytes);
    memcpy(candidate+model->cursor+bytes,model->text+model->cursor,length-model->cursor+1);
    return hello_input_field_validate(&model->field,candidate,false,NULL);
}
static void insert_text(HelloKeyboardModel* model,const char* text) {
    if(!strcmp(text,HELLO_KEYBOARD_RETURN_TEXT))return;
    const HelloInputValidation status=insertion_status(model,text);
    if(status!=HELLO_FIELD_VALID){model->input_rejected=status;return;}
    text=committed_text(text);
    const size_t bytes=strlen(text),length=strlen(model->text);
    memmove(model->text+model->cursor+bytes,model->text+model->cursor,length-model->cursor+1);
    memcpy(model->text+model->cursor,text,bytes);
    model->cursor=brb_grapheme_ceil(model->text,model->cursor+bytes);
}

static void transform_item(const HelloKeyboardModel* model,const char* input,
    HelloKeyboardTransformAction action,char* output,size_t capacity) {
    char transformed[HELLO_KEYBOARD_ITEM_BYTES+1]={0};bool accepted=false;
    bool handled=false;
    if(model->transform) {
        handled=model->transform(input,action,transformed,sizeof(transformed),model->transform_userdata);
        accepted=handled;
        // A callback must return a complete string. Never accept a silently
        // truncated variant merely because its first 160 bytes are valid UTF-8.
        if(memchr(transformed,0,sizeof(transformed))==NULL)accepted=false;
    }
    if(!handled&&action!=HELLO_KB_TRANSFORM_LOWER) {
        bool upper=action==HELLO_KB_TRANSFORM_UPPER;
        if(action==HELLO_KB_TRANSFORM_TOGGLE) {
            const size_t bytes=brb_unicode_case(input,true,transformed,sizeof(transformed));
            upper=bytes!=SIZE_MAX&&strcmp(input,transformed)!=0;
        }
        accepted=brb_unicode_case(input,upper,transformed,sizeof(transformed))!=SIZE_MAX;
    }
    const char* value=accepted&&item_text_valid(transformed)?transformed:input;
    const size_t bytes=strlen(value);
    if(bytes<capacity)memcpy(output,value,bytes+1);
    else output[0]=0;
}

static void selection_text(const HelloKeyboardModel* model,int index,char* output,size_t capacity) {
    char raw[HELLO_KEYBOARD_ITEM_BYTES+1];
    hello_keyboard_model_group_text(model,model->group,index,raw,sizeof(raw));
    if (model->upper && !strcmp(raw,"⎵")) {
        if (sizeof(HELLO_KEYBOARD_RETURN_TEXT) <= capacity) memcpy(output,HELLO_KEYBOARD_RETURN_TEXT,sizeof(HELLO_KEYBOARD_RETURN_TEXT));
        else output[0]=0;
        return;
    }
    if(model->upper||model->transform)
        transform_item(model,raw,model->upper?HELLO_KB_TRANSFORM_UPPER:HELLO_KB_TRANSFORM_LOWER,output,capacity);
    else if(strlen(raw)<capacity)memcpy(output,raw,strlen(raw)+1);
    else output[0]=0;
}

static bool delete_before_cursor(HelloKeyboardModel* model) {
    if (model->cursor == 0) return false;
    const size_t length = strlen(model->text);
    const size_t start = brb_grapheme_previous(model->text, model->cursor);
    memmove(model->text + start,
            model->text + model->cursor,
            length - model->cursor + 1);
    model->cursor = start;
    return true;
}

static void clear_selection_holds(HelloKeyboardModel* model,int group) {
    for (int button=0;button<2;button++) {
        if (model->hold[button].group==group) memset(&model->hold[button],0,sizeof(model->hold[button]));
        model->feedback_active[button]=false;
    }
}

void hello_keyboard_model_commit(HelloKeyboardModel* model, bool keep_group) {
    // A pending submit owns completion until the caller consumes it. Discard
    // selections from any further input in that same dispatch instead of typing.
    if(model->pending!=0 && !model->submit_requested) {
        char pending[HELLO_KEYBOARD_ITEM_BYTES+1];
        hello_keyboard_model_pending_text(model,pending,sizeof(pending));
        if (!strcmp(pending,HELLO_KEYBOARD_RETURN_TEXT)) {
            model->submit_requested=true;
        } else insert_text(model,pending);
    }
    if(model->submit_requested) {
        clear_selection_holds(model,model->group);
        keep_group=false;
    }
    model->pending = 0;
    model->pending_text[0] = 0;
    model->pending_side = 0;
    model->index = 0;
    if (!keep_group) model->group = 0;
}

bool hello_keyboard_model_set_view(HelloKeyboardModel* model,HelloKeyboardView view) {
    if(!model||view<HELLO_KB_VIEW_KEYBOARD||view>HELLO_KB_VIEW_EMOJI_SETS||
       (unsigned)view>model->view_count)return false;
    if(model->view!=view) {
        reset_transient(model);
        model->view=view;
    }
    return true;
}

bool hello_keyboard_model_set_view_count(HelloKeyboardModel* model,unsigned count) {
    if(!model||count<HELLO_KB_VIEW_KEYBOARD||count>HELLO_KB_VIEW_EMOJI_SETS)return false;
    if((unsigned)model->view>count)hello_keyboard_model_set_view(model,HELLO_KB_VIEW_KEYBOARD);
    model->view_count=count;
    return true;
}

unsigned hello_keyboard_model_layer(const HelloKeyboardModel* model) {
    return (unsigned)model->view;
}

unsigned hello_keyboard_model_layer_count(const HelloKeyboardModel* model) {
    return model->view_count;
}

bool hello_keyboard_model_is_choosing_layer(const HelloKeyboardModel* model) {
    const HelloKeyboardHold* hold = &model->hold[HELLO_KB_BUTTON_A];
    return hold->active && hold->group == 0 && hold->hold == HELLO_KB_HOLD_CASE && hold->handled;
}

static bool idle_a_held(const HelloKeyboardModel* model) {
    const HelloKeyboardHold* hold = &model->hold[HELLO_KB_BUTTON_A];
    return model->group == 0 && hold->active && hold->group == 0 && hold->hold == HELLO_KB_HOLD_CASE;
}

int hello_keyboard_model_direction_group(const HelloKeyboardModel* model,HelloKeyboardDirection direction) {
    return direction==HELLO_KB_UP&&model->upper&&model->shifted_up_enabled?
        HELLO_KEYBOARD_EMOJI_GROUP:(int)direction+1;
}

int hello_keyboard_model_move_cursor(HelloKeyboardModel* model,int steps) {
    const size_t length=strlen(model->text);
    int moved=0;
    for(unsigned budget=0;steps&&budget<HELLO_KEYBOARD_MAX_TEXT;++budget) {
        if(steps<0) {
            if(!model->cursor)break;
            model->cursor=brb_grapheme_previous(model->text,model->cursor);
            ++steps;--moved;
        } else {
            if(model->cursor>=length)break;
            model->cursor=brb_grapheme_following(model->text,model->cursor);
            --steps;++moved;
        }
    }
    return moved;
}

static void start_cursor_hold(HelloKeyboardModel* model, HelloKeyboardDirection direction, uint32_t now) {
    hello_keyboard_model_move_cursor(model,direction==HELLO_KB_LEFT?-1:1);
    model->cursor_hold = true;
    model->cursor_direction = direction;
    model->cursor_next_at = now + hello_keyboard_model_selection_hold_ms(model);
}

void hello_keyboard_model_direction(HelloKeyboardModel* model, HelloKeyboardDirection direction, uint32_t now) {
    model->cursor_hold = false;
    if (idle_a_held(model)) {
        // A chord enters held mode even before the hold threshold. Changing
        // view here retains that held gesture, so release cannot also Shift.
        model->hold[HELLO_KB_BUTTON_A].handled = true;
        model->feedback_active[HELLO_KB_BUTTON_A] = false;
        if (direction == HELLO_KB_LEFT || direction == HELLO_KB_RIGHT)
            start_cursor_hold(model, direction, now);
        else if(model->view_count>1) {
            model->view=model->view==HELLO_KB_VIEW_KEYBOARD?HELLO_KB_VIEW_EMOJI_SETS:HELLO_KB_VIEW_KEYBOARD;
            model->view_confirm_requested=false;
        }
        return;
    }
    if(model->view!=HELLO_KB_VIEW_KEYBOARD)return;
    const int group = hello_keyboard_model_direction_group(model, direction);
    if (!group) return;

    // An idle-origin B hold cannot follow a new selection gesture and delete
    // its committed item later. Group-owned holds retain their existing rules.
    HelloKeyboardHold* b_hold = &model->hold[HELLO_KB_BUTTON_B];
    if (b_hold->active && b_hold->group == 0) memset(b_hold, 0, sizeof(*b_hold));
    hello_keyboard_model_commit(model, false);
    model->group = group;
    model->group_at = now;
}

void hello_keyboard_model_direction_release(HelloKeyboardModel* model, HelloKeyboardDirection direction, uint32_t now) {
    if (model->cursor_hold && model->cursor_direction == direction) {
        // Stop immediately, including a release at the next repeat deadline.
        model->cursor_hold = false;
        return;
    }
    const int group = hello_keyboard_model_direction_group(model, direction);
    if (!group || model->group != group) return;
    (void)now; // Releasing the group commits without consuming a due repeat.
    // A still-held selection button belongs to this completed gesture. It must
    // not repeat a later selection if the same group is reopened before release.
    clear_selection_holds(model,group);
    // Empty/full-buffer gestures also close their group without inserting text.
    hello_keyboard_model_commit(model, false);
}

static void select_item(HelloKeyboardModel* model,int index,int side,uint32_t now) {
    char selected[HELLO_KEYBOARD_ITEM_BYTES+1];selection_text(model,index,selected,sizeof(selected));
    if(!selected[0])return;
    // Selection is independent of field validation. A full or numeric field
    // still presents the requested item; only committing may reject its value.
    model->index=index;
    memcpy(model->pending_text,selected,strlen(selected)+1);
    hello_keyboard_utf8_decode(selected,&model->pending);
    model->pending_side=side;model->pending_at=now;
}
static void tap_side(HelloKeyboardModel* model, int side, uint32_t now) {
    if (model->group == 0) return;
    const int length = hello_keyboard_model_group_length(model, model->group);
    if (length < 2) return;
    int index;
    if (model->pending == 0) {
        index = side == 1 ? 0 : length - 1;
    } else if (side == 1) {
        index = (model->index + 1) % length;
    } else {
        index = positive_mod(model->index - 1, length);
    }
    select_item(model,index,side,now);
}

void hello_keyboard_model_crank(HelloKeyboardModel* model, int steps, uint32_t now) {
    hello_keyboard_model_crank_from(model,steps,now,-1);
}

void hello_keyboard_model_crank_from(HelloKeyboardModel* model, int steps, uint32_t now, int first_index) {
    if (steps == 0 || model->group == 0) return;
    const int length = hello_keyboard_model_group_length(model, model->group);
    if (length < 2) return;
    const bool hinted=!model->pending&&first_index>=0&&first_index<length;
    const int from=hinted?first_index-(steps>0?1:-1):model->pending==0?(steps>0?-1:0):model->index;
    const int index=positive_mod(from+steps%length,length);
    select_item(model,index,3,now);
}

void hello_keyboard_model_press(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now) {
    hello_keyboard_model_press_from(model,button,now,-1);
}

void hello_keyboard_model_press_from(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now, int first_index) {
    if(button==HELLO_KB_BUTTON_B&&(idle_a_held(model)||model->view==HELLO_KB_VIEW_EMOJI_SETS)) {
        memset(&model->hold[button],0,sizeof(model->hold[button]));
        model->feedback_active[button]=false;
        return;
    }
    if(button==HELLO_KB_BUTTON_A&&model->group==0) {
        HelloKeyboardHold* b_hold=&model->hold[HELLO_KB_BUTTON_B];
        if(b_hold->active&&b_hold->group==0)memset(b_hold,0,sizeof(*b_hold));
        model->feedback_active[HELLO_KB_BUTTON_B]=false;
    }
    HelloKeyboardHold* hold=&model->hold[button];
    memset(hold,0,sizeof(*hold));hold->active=true;hold->at=now;hold->group=model->group;
    if(model->group==0) {
        if(button==HELLO_KB_BUTTON_A)hold->hold=HELLO_KB_HOLD_CASE;
        else {
            hold->hold=HELLO_KB_HOLD_DELETE;
            const bool deleted=delete_before_cursor(model);
            model->feedback_active[button]=deleted;
            if(deleted)model->feedback_at[button]=now;
        }
    } else {
        const int side=button==HELLO_KB_BUTTON_A?1:2;
        const int length=hello_keyboard_model_group_length(model,model->group);
        if(!model->pending&&first_index>=0&&first_index<length)
            select_item(model,first_index,side,now);
        else tap_side(model,side,now);
        hold->hold=HELLO_KB_HOLD_REPEAT;
        hold->next_selection_at=now+hello_keyboard_model_selection_hold_ms(model);
    }
}

void hello_keyboard_model_release(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now) {
    hello_keyboard_model_release_with_selection(model,button,now,true);
}

void hello_keyboard_model_release_with_selection(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t now, bool allowed) {
    HelloKeyboardHold press = model->hold[button];
    if (button == HELLO_KB_BUTTON_A && press.active && press.group == 0 &&
        press.hold == HELLO_KB_HOLD_CASE &&
        (press.handled || brb_input_elapsed(now, press.at) >= model->timing.hold_ms))
        // Stop before any threshold update can consume a due cursor repeat.
        model->cursor_hold = false;
    if (press.active && (press.hold == HELLO_KB_HOLD_REPEAT ||
        (press.hold != HELLO_KB_HOLD_NONE && !press.handled && brb_input_elapsed(now, press.at) >= model->timing.hold_ms))) {
        hello_keyboard_model_update_with_selection(model, now, allowed);
        press = model->hold[button];
    }
    memset(&model->hold[button], 0, sizeof(model->hold[button]));

    if (!press.active || press.handled) return;

    bool action_performed = false;
    if (press.group != 0) {
        action_performed = model->group == press.group;
    } else if (button == HELLO_KB_BUTTON_A) {
        if(model->view==HELLO_KB_VIEW_EMOJI_SETS)model->view_confirm_requested=true;
        else model->upper = !model->upper;
        action_performed = true;
    }
    if (action_performed) {
        model->feedback_active[button] = true;
        model->feedback_at[button] = now;
    }
}

void hello_keyboard_model_update(HelloKeyboardModel* model, uint32_t now) {
    hello_keyboard_model_update_with_selection(model,now,true);
}

void hello_keyboard_model_update_with_selection(HelloKeyboardModel* model, uint32_t now, bool allowed) {
    // Selection presents every accepted item; slow frames never catch up by
    // skipping ahead. D-pad cursor navigation shares this one-step policy;
    // deletion retains its independent four-step cap.
    int repeat_budget=1;
    bool wrapped_this_update=false;
    for (int button = 0; button < 2; ++button) {
        HelloKeyboardHold* hold = &model->hold[button];
        if (!hold->active) continue;
        if (hold->hold == HELLO_KB_HOLD_REPEAT) {
            if (model->group != hold->group || !model->pending) continue;
            if (!allowed || wrapped_this_update || !repeat_budget) {
                if (brb_input_due(now,hold->next_selection_at)) hold->next_selection_at=now+hello_keyboard_model_selection_repeat_ms(model);
                continue;
            }
            if (brb_input_due(now,hold->next_selection_at)) {
                int count=(int)((now-hold->next_selection_at)/hello_keyboard_model_selection_repeat_ms(model))+1;
                if (count>repeat_budget) count=repeat_budget;
                const int direction=button==HELLO_KB_BUTTON_A?1:-1;
                for (int n=0;n<count;n++) {
                    const int previous=model->index;
                    tap_side(model,direction>0?1:2,now);
                    repeat_budget--;
                    if (previous!=model->index) model->selection_repeated=true;
                    const bool wrapped=direction>0?model->index<previous:model->index>previous;
                    if (wrapped) {
                        model->selection_wrapped=true;
                        wrapped_this_update=true;
                        break;
                    }
                }
                hold->handled=true;
                hold->next_selection_at=now+hello_keyboard_model_selection_repeat_ms(model);
            }
            continue;
        }
        if (!hold->handled && brb_input_elapsed(now,hold->at)>=model->timing.hold_ms) {
            if (hold->hold == HELLO_KB_HOLD_CASE) {
                // Holding idle A enables layer selection without changing case.
                model->layer_at = 0;
                model->feedback_active[HELLO_KB_BUTTON_A] = false;
                hold->handled = true;
            } else if (hold->hold == HELLO_KB_HOLD_DELETE && model->group == 0) {
                // The initial deletion happened on press; this is the first
                // held repeat, followed by the established repeat interval.
                delete_before_cursor(model);
                hold->handled = true;
                hold->next_delete_at = now + model->timing.repeat_ms;
            }
        }
        if (hold->hold == HELLO_KB_HOLD_DELETE && hold->handled && hold->next_delete_at != 0 &&
            model->group == 0 && brb_input_due(now,hold->next_delete_at)) {
            int count = (int)((now - hold->next_delete_at) / model->timing.repeat_ms) + 1;
            if (count > 4) count = 4;
            for (int i = 0; i < count; ++i) delete_before_cursor(model);
            hold->next_delete_at = now + model->timing.repeat_ms;
        }
    }

    if (model->cursor_hold && hello_keyboard_model_is_choosing_layer(model) && model->group == 0 &&
        brb_input_due(now,model->cursor_next_at)) {
        hello_keyboard_model_move_cursor(model,model->cursor_direction==HELLO_KB_LEFT?-1:1);
        model->cursor_next_at = now + hello_keyboard_model_selection_repeat_ms(model);
    }
}
