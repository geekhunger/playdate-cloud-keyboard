#include "keyboard.h"
#include "pill.h"
#include "float_motion.h"
#include "keyboard_crank.h"
#include "keyboard_geometry.h"
#include "popup.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define HELLO_KEYBOARD_CURSOR_GAP 2
#define HELLO_KEYBOARD_DEFAULT_FONT_SIZE 15

static const int kGlyphWidths[95] = {
    4,2,6,8,7,11,9,3,4,4,6,8,3,5,2,7,7,6,7,7,7,7,7,7,7,7,2,3,8,8,8,6,
    9,9,7,7,8,6,6,7,8,4,5,7,6,10,7,8,7,8,8,7,6,7,8,12,8,8,6,4,7,4,5,7,
    3,7,7,6,7,7,5,7,7,2,3,6,2,10,7,7,7,7,5,6,5,7,7,10,6,7,6,6,2,6,10
};

static int keyboard_utf8_decode(const char* text, uint32_t* codepoint) {
    const int bytes=hello_keyboard_utf8_decode(text,codepoint);
    if(bytes<0){*codepoint='?';return 1;}
    return bytes;
}

static void copy_utf8(char* output,size_t capacity,const char* text) {
    size_t at=0;const size_t length=text?strlen(text):0;
    while(at<length) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes||at+bytes>=capacity)break;
        memcpy(output+at,text+at,bytes);at+=bytes;
    }
    output[at]=0;
}

static const BrbEmojiSet* const* default_emoji_catalog(void) {
    static const BrbEmojiSet* sets[1];
    if(!sets[0])sets[0]=brb_emoji_everyday_set();
    return sets;
}

static bool emoji_set_usable(const BrbEmojiSet* set) {
    return set&&set->id&&set->glyphs&&set->count&&set->pixels&&set->keyboard_utf8&&
        set->keyboard_bytes<=HELLO_KEYBOARD_MAX_GROUP_BYTES&&
        hello_keyboard_model_group_is_valid(set->keyboard_utf8)&&
        strlen(set->keyboard_utf8)==set->keyboard_bytes&&brb_emoji_bitmap(set,&set->glyphs[0]);
}

const BrbEmojiSet* hello_keyboard_emoji_set(const HelloKeyboard* keyboard) {
    return keyboard?keyboard->selected_emoji_set:NULL;
}

static void sync_emoji_renderer(HelloKeyboard* keyboard) {
    if(keyboard->config.emoji_picker_enabled&&keyboard->config.unicode_font&&keyboard->selected_emoji_set&&
       brb_unicode_font_emoji_set(keyboard->config.unicode_font)!=keyboard->selected_emoji_set)
        brb_unicode_font_set_emoji_set(keyboard->config.unicode_font,keyboard->selected_emoji_set);
}

static bool use_emoji_set(HelloKeyboard* keyboard,const BrbEmojiSet* set,bool notify) {
    if(!emoji_set_usable(set))return false;
    const BrbEmojiSet* previous=keyboard->selected_emoji_set;
    memcpy(keyboard->group_storage[HELLO_KEYBOARD_EMOJI_GROUP-1],set->keyboard_utf8,set->keyboard_bytes+1);
    keyboard->config.groups[HELLO_KEYBOARD_EMOJI_GROUP-1]=keyboard->group_storage[HELLO_KEYBOARD_EMOJI_GROUP-1];
    memcpy(keyboard->model.groups[HELLO_KEYBOARD_EMOJI_GROUP],set->keyboard_utf8,set->keyboard_bytes+1);
    keyboard->selected_emoji_set=set;
    sync_emoji_renderer(keyboard);
    if(notify&&previous!=set&&keyboard->config.on_emoji_set_selected)
        keyboard->config.on_emoji_set_selected(keyboard->config.emoji_set_userdata,set);
    return true;
}

// ASCII UI art is loaded once. The optional Unicode fallback owns its bounded
// glyph cache separately; the image-table lookup itself never accesses storage.
static LCDBitmap* image(HelloKeyboard* keyboard, const char* name) {
    int table = -1, index = -1;
    int code, fill;
    char button;
    if (sscanf(name, "glyph-%d", &code) == 1 && code >= 32 && code <= 126) {
        table = 0;
        index = code - 32;
    } else if (sscanf(name, "footer-%c-%d", &button, &fill) == 2 &&
               (button == 'A' || button == 'B') && fill >= 0 && fill <= 21) {
        table = 1;
        index = (button == 'B' ? 22 : 0) + fill;
    } else {
        static const char* const directions[] = {
            "dpad-none", "dpad-up", "dpad-right", "dpad-down", "dpad-left"
        };
        for (int i = 0; i < 5; ++i) {
            if (strcmp(name, directions[i]) == 0) {
                table = 2;
                index = i;
                break;
            }
        }
    }
    if (table < 0 || keyboard->art[table] == NULL) return NULL;
    return keyboard->pd->graphics->getTableBitmap(keyboard->art[table], index);
}

static void draw_image(HelloKeyboard* keyboard, const char* name, int x, int y) {
    LCDBitmap* bitmap = image(keyboard, name);
    if (bitmap != NULL) keyboard->pd->graphics->drawBitmap(bitmap, x, y, kBitmapUnflipped);
}

void hello_keyboard_draw_hold_indicator(HelloKeyboard* keyboard, char button,
                                        int fill, int x, int y) {
    if (button != 'A' && button != 'B') return;
    if (fill < 0) fill = 0;
    if (fill > 21) fill = 21;
    char name[32];
    snprintf(name, sizeof(name), "footer-%c-%d", button, fill);
    LCDBitmap* bitmap = image(keyboard, name);
    if (bitmap != NULL) {
        keyboard->pd->graphics->drawBitmap(bitmap, x, y, kBitmapUnflipped);
    }
}

static int glyph_width(unsigned char code) {
    if (code < 32 || code > 126) code = '?';
    return kGlyphWidths[code - 32];
}

static int bitmap_text_width_n(const HelloKeyboard* keyboard,const char* text,size_t length) {
    int width = 0;
    for (size_t offset = 0; offset<length;) {
        uint32_t codepoint = 0;
        size_t bytes=brb_grapheme_next(text+offset,length-offset);if(!bytes)bytes=1;
        keyboard_utf8_decode(text+offset,&codepoint);
        if(codepoint==0x2013)width+=9;
        else if (keyboard->config.unicode_font && brb_text_has_unicode(text+offset,bytes)) {
            const BrbEmojiGlyph* art=brb_emoji_find(brb_unicode_font_emoji_set(keyboard->config.unicode_font),text+offset,bytes);
            width += art ? art->advance : 17;
        } else width += glyph_width(codepoint<=0x7f?(unsigned char)codepoint:(unsigned char)'?')+1;
        offset += bytes;
    }
    return width > 0 ? width - 1 : 0;
}

static int keyboard_font_size(const HelloKeyboard* keyboard) {
    return keyboard->config.font_size;
}

static int keyboard_text_height(const HelloKeyboard* keyboard,const char* text) {
    int height=keyboard_font_size(keyboard);
    if (!keyboard->config.unicode_font) return height;
    const BrbEmojiSet* set=brb_unicode_font_emoji_set(keyboard->config.unicode_font);
    const size_t length=strlen(text);
    for(size_t at=0;at<length;) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes)break;
        if(brb_text_has_unicode(text+at,bytes)) {
            const BrbEmojiGlyph* art=brb_emoji_find(set,text+at,bytes);
            const int glyph_height=art?set->cell_height:16;
            if(glyph_height>height)height=glyph_height;
        }
        at+=bytes;
    }
    return height;
}

static int keyboard_text_width_n(const HelloKeyboard* keyboard,const char* text,size_t length) {
    if(length==strlen("⏎")&&!memcmp(text,"⏎",length)){text="⎵";length=strlen(text);}
    if(!keyboard->config.font)return bitmap_text_width_n(keyboard,text,length);
    if(keyboard->config.unicode_font)
        return brb_text_width(keyboard->config.unicode_font,keyboard->config.font,text,length);
    return keyboard->pd->graphics->getTextWidth(keyboard->config.font,text,length,kUTF8Encoding,0);
}
static int keyboard_text_width(const HelloKeyboard* keyboard,const char* text) {
    return keyboard_text_width_n(keyboard,text,strlen(text));
}

static void draw_bitmap_text(HelloKeyboard* keyboard, const char* text, int x, int y, bool white) {
    keyboard->pd->graphics->setDrawMode(white ? kDrawModeFillWhite : kDrawModeCopy);
    const size_t length=strlen(text);
    for (size_t offset = 0; offset<length;) {
        uint32_t codepoint = 0;
        size_t bytes=brb_grapheme_next(text+offset,length-offset);if(!bytes)bytes=1;
        keyboard_utf8_decode(text+offset,&codepoint);
        if(codepoint==0x2013) {
            keyboard->pd->graphics->fillRect(x,y+keyboard_font_size(keyboard)/2,8,1,white?kColorWhite:kColorBlack);
            x+=9;offset+=bytes;continue;
        }
        if(keyboard->config.unicode_font&&brb_text_has_unicode(text+offset,bytes)) {
            const BrbEmojiGlyph* art=brb_emoji_find(brb_unicode_font_emoji_set(keyboard->config.unicode_font),text+offset,bytes);
            const int ink_width=art?art->width:16;
            if(!brb_unicode_font_draw(keyboard->config.unicode_font,text+offset,bytes,x,y,white))
                draw_image(keyboard,"glyph-63",x+(ink_width-glyph_width('?'))/2,y);
            // Keep the same advance as measurement even after a cold read or
            // allocation failure. Later characters and the cursor stay put.
            x+=art?art->advance:17;offset+=bytes;continue;
        }
        const unsigned char code = codepoint <= 0x7f ? (unsigned char)codepoint : (unsigned char)'?';
        char name[24];
        snprintf(name, sizeof(name), "glyph-%u", code >= 32 && code <= 126 ? code : (unsigned char)'?');
        draw_image(keyboard, name, x, y);
        x += glyph_width(code) + 1;
        offset += (size_t)bytes;
    }
    keyboard->pd->graphics->setDrawMode(kDrawModeCopy);
}

static void draw_keyboard_text(HelloKeyboard* keyboard, const char* text,
                               int x, int y, bool white) {
    if (keyboard->config.font == NULL) {
        draw_bitmap_text(keyboard, text, x, y, white);
        return;
    }
    if(keyboard->config.unicode_font) {
        brb_text_draw(keyboard->config.unicode_font,keyboard->config.font,text,strlen(text),x,y,white);
        return;
    }
    keyboard->pd->graphics->setFont(keyboard->config.font);
    keyboard->pd->graphics->setDrawMode(white ? kDrawModeFillWhite : kDrawModeCopy);
    keyboard->pd->graphics->drawText(text, strlen(text), kUTF8Encoding, x, y);
    keyboard->pd->graphics->setDrawMode(kDrawModeCopy);
}

static void reset_crank(HelloKeyboard* keyboard) {
    keyboard->crank_remainder = 0.0f;
    keyboard->crank_release_steps = 0;
    keyboard->crank_release_direction = 0;
    keyboard->crank_release_peak = 0;
    keyboard->crank_velocity = 0;
    keyboard->crank_speed_factor = 1;
    keyboard->crank_sampled_at = 0;
    keyboard->crank_sampled = false;
    keyboard->crank_wrapped = false;
    keyboard->crank_entry_breakaway = false;
    keyboard->crank_marker_rebase_group = 0;
    keyboard->crank_marker_pinned_group = 0;
}

static void reset_group_scrolls(HelloKeyboard* keyboard) {
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    memset(keyboard->group_scroll_motion,0,sizeof(keyboard->group_scroll_motion));
}

static void reset_selection_lock(HelloKeyboard* keyboard) {
    keyboard->selection_lock_group=0;
    keyboard->selection_lock_index=0;
    keyboard->selection_lock_crank=false;
}

static void clear_deferred_repeat(HelloKeyboard* keyboard) {
    keyboard->deferred_repeat_button=0;
    keyboard->deferred_repeat_group=keyboard->deferred_repeat_index=0;
    keyboard->deferred_repeat_at=0;
}

static void reset_dialog_choices(HelloKeyboard* keyboard) {
    keyboard->dialog_save_confirmed=false;
    keyboard->chord_held_faces=0;
    keyboard->chord_checkpoint.valid=false;
}

static void checkpoint_face_press(HelloKeyboard* keyboard,HelloKeyboardButton button,uint32_t now) {
    HelloKeyboardChordCheckpoint* saved=&keyboard->chord_checkpoint;
    const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;
    if(saved->valid||keyboard->cancel_dialog_active||keyboard->chord_held_faces!=face)return;
    const HelloKeyboardModel* model=&keyboard->model;
    memcpy(saved->text,model->text,strlen(model->text)+1);
    memcpy(saved->pending_text,model->pending_text,strlen(model->pending_text)+1);
    saved->cursor=model->cursor;saved->upper=model->upper;saved->view=model->view;
    saved->group=model->group;saved->index=model->index;saved->pending=model->pending;
    saved->pending_side=model->pending_side;saved->pending_at=model->pending_at;saved->group_at=model->group_at;
    saved->face=face;saved->at=now;saved->valid=true;
}

static void restore_chord_checkpoint(HelloKeyboard* keyboard) {
    const HelloKeyboardChordCheckpoint* saved=&keyboard->chord_checkpoint;
    if(!saved->valid)return;
    HelloKeyboardModel* model=&keyboard->model;
    memcpy(model->text,saved->text,strlen(saved->text)+1);
    memcpy(model->pending_text,saved->pending_text,strlen(saved->pending_text)+1);
    model->cursor=saved->cursor;model->upper=saved->upper;model->view=saved->view;
    model->group=saved->group;model->index=saved->index;model->pending=saved->pending;
    model->pending_side=saved->pending_side;model->pending_at=saved->pending_at;model->group_at=saved->group_at;
}

static void resume_selection_hold(HelloKeyboard* keyboard,HelloKeyboardButton button,uint32_t at,uint32_t next_at) {
    HelloKeyboardHold* hold=&keyboard->model.hold[button];
    memset(hold,0,sizeof(*hold));
    hold->active=true;hold->handled=true;hold->hold=HELLO_KB_HOLD_REPEAT;
    hold->group=keyboard->model.group;hold->at=at;hold->next_selection_at=next_at;
}

static bool selection_locked(HelloKeyboard* keyboard) {
    if(!keyboard->model.pending||keyboard->selection_lock_group!=keyboard->model.group||
       keyboard->selection_lock_index!=keyboard->model.index)reset_selection_lock(keyboard);
    return keyboard->selection_lock_group!=0;
}

static void lock_boundary_selection(HelloKeyboard* keyboard) {
    if(!keyboard->model.pending||!keyboard->model.group)return;
    keyboard->selection_lock_group=keyboard->model.group;
    keyboard->selection_lock_index=keyboard->model.index;
    keyboard->selection_lock_crank=true;
    keyboard->crank_marker_rebase_group=keyboard->model.group;
    keyboard->crank_marker_pinned_group=keyboard->model.group;
}

static bool crossed_selection_boundary(const HelloKeyboardModel* model,
    bool had_pending,int previous_index,int direction) {
    return had_pending&&model->pending&&
        (direction>0?model->index<previous_index:model->index>previous_index);
}

static bool group_has_overflow(const HelloKeyboard* keyboard,int group);
static bool group_selection_is_visible(const HelloKeyboard* keyboard);
static int group_last_visible_item(const HelloKeyboard* keyboard);

static PDButtons selected_direction_button(const HelloKeyboard* keyboard) {
    const int group=keyboard->model.group;
    return group==2?kButtonRight:group==3?kButtonDown:group==4?kButtonLeft:
        group?kButtonUp:0;
}

static bool selected_direction_released(const HelloKeyboard* keyboard,PDButtons released) {
    return (released&selected_direction_button(keyboard))!=0;
}

static void update_deferred_repeat(HelloKeyboard* keyboard,uint32_t now,PDButtons current,
    PDButtons pushed,PDButtons released) {
    const PDButtons faces=kButtonA|kButtonB;
    const PDButtons fresh=pushed&faces;
    if(!keyboard->deferred_repeat_button&&!fresh)return;
    const bool eligible=keyboard->model.view==HELLO_KB_VIEW_KEYBOARD&&keyboard->model.group&&
        keyboard->model.pending&&(current&selected_direction_button(keyboard))&&
        !(released&selected_direction_button(keyboard))&&(current&faces)!=faces;
    if(!eligible||!(current&keyboard->deferred_repeat_button)||
       keyboard->deferred_repeat_group!=keyboard->model.group||
       keyboard->deferred_repeat_index!=keyboard->model.index)clear_deferred_repeat(keyboard);
    if(fresh) {
        clear_deferred_repeat(keyboard);
        if(eligible&&selection_locked(keyboard)) {
            const PDButtons face=(fresh&kButtonB)?kButtonB:kButtonA;
            if(current&face) {
                keyboard->deferred_repeat_button=face;
                keyboard->deferred_repeat_group=keyboard->model.group;
                keyboard->deferred_repeat_index=keyboard->model.index;
                keyboard->deferred_repeat_at=now;
            }
        }
    }
    if(keyboard->deferred_repeat_button&&!selection_locked(keyboard)) {
        const PDButtons face=keyboard->deferred_repeat_button;
        const uint32_t at=keyboard->deferred_repeat_at;
        uint32_t next_at=at+hello_keyboard_model_selection_hold_ms(&keyboard->model);
        if(brb_input_due(now,next_at))next_at=now;
        resume_selection_hold(keyboard,face==kButtonA?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B,at,next_at);
        keyboard->rejected_face_buttons&=~face;
        clear_deferred_repeat(keyboard);
    }
}

static bool selection_repeats_allowed(HelloKeyboard* keyboard,PDButtons released) {
    return !selection_locked(keyboard)&&!selected_direction_released(keyboard,released)&&
        keyboard->requested_action!=HELLO_KEYBOARD_DONE&&!keyboard->model.submit_requested;
}

static void consume_selection_repeat(HelloKeyboard* keyboard) {
    if(keyboard->model.selection_repeated)reset_crank(keyboard);
    if(keyboard->model.selection_wrapped&&keyboard->model.group&&group_has_overflow(keyboard,keyboard->model.group))
        lock_boundary_selection(keyboard);
    keyboard->model.selection_repeated=false;
    keyboard->model.selection_wrapped=false;
}


static void press_selection(HelloKeyboard* keyboard,HelloKeyboardButton button,uint32_t now) {
    checkpoint_face_press(keyboard,button,now);
    const bool had_pending=keyboard->model.pending!=0;
    const int previous_index=keyboard->model.index;
    const int first_index=button==HELLO_KB_BUTTON_B&&!had_pending&&keyboard->model.group?
        group_last_visible_item(keyboard):-1;
    hello_keyboard_model_press_from(&keyboard->model,button,now,first_index);
    // Face buttons need a boundary gate only when the opposite endpoint must
    // scroll into view. Fully visible groups retain immediate tap selection;
    // the independent crank boundary policy below remains unchanged.
    if(keyboard->model.group&&group_has_overflow(keyboard,keyboard->model.group)) {
        const bool wrapped=crossed_selection_boundary(&keyboard->model,had_pending,previous_index,
            button==HELLO_KB_BUTTON_A?1:-1);
        if(wrapped||(!had_pending&&keyboard->model.pending&&!group_selection_is_visible(keyboard))) {
            lock_boundary_selection(keyboard);
            if(!wrapped)keyboard->selection_lock_crank=false;
        }
    }
}

static void clear_callback(void* userdata) {
    HelloKeyboard* keyboard = userdata;
    keyboard->chord_checkpoint.valid=false;
    hello_keyboard_model_clear(&keyboard->model);
    reset_group_scrolls(keyboard);
    reset_crank(keyboard);
    reset_selection_lock(keyboard);
    clear_deferred_repeat(keyboard);
    keyboard->rejected_face_buttons=0;
    keyboard->selection_marker.active = false;
}

static void done_callback(void* userdata) {
    HelloKeyboard* keyboard = userdata;
    keyboard->requested_action = HELLO_KEYBOARD_DONE;
}

static void cancel_callback(void* userdata) {
    HelloKeyboard* keyboard = userdata;
    keyboard->requested_action = HELLO_KEYBOARD_CANCELLED;
}

static void hide_callback(void* userdata) {
    HelloKeyboard* keyboard=userdata;
    if(keyboard->hide_item&&hello_input_field_can_toggle_mask(&keyboard->input_field))
        keyboard->input_field.masked=keyboard->pd->system->getMenuItemValue(keyboard->hide_item)!=0;
}

void hello_keyboard_default_config(HelloKeyboardConfig* config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(*config));
    for (int index = 0; index < HELLO_KEYBOARD_GROUP_COUNT; ++index)
        config->groups[index] = hello_keyboard_default_group_chars(index + 1);
    config->font_size = HELLO_KEYBOARD_DEFAULT_FONT_SIZE;
    config->field=hello_input_field_default_policy();
    config->menu_actions=HELLO_KEYBOARD_MENU_ALL;
    config->title = "";
    config->timing=brb_input_timing_defaults();
    config->button_navigation_scale=1.0f;
    config->emoji_input_enabled=true;
    config->emoji_picker_enabled=true;
    config->emoji_sets=default_emoji_catalog();config->emoji_set_count=1;
    config->labels=hello_keyboard_default_labels();
    config->feedback.duration_ms=HELLO_KEYBOARD_DEFAULT_FEEDBACK_MS;
}

bool hello_keyboard_set_labels(HelloKeyboard* keyboard, const HelloKeyboardLabels* labels) {
    if (!keyboard) return false;
    if (keyboard->visible && (keyboard->clear_item||keyboard->done_item||keyboard->cancel_item||keyboard->hide_item)&&
        !keyboard->pd->system->setMenuItemTitle) return false;
    // Stage the complete update: sources may refer to this instance's previous
    // storage, including swapping captions between fields. Menu ownership and
    // action callbacks remain untouched throughout a visible rename.
    HelloKeyboardLabelStorage owned;
    hello_keyboard_labels_store(&owned, labels);
    keyboard->label_storage=owned;
    keyboard->config.labels=hello_keyboard_stored_labels(&keyboard->label_storage);
    if (keyboard->visible) {
        if(keyboard->clear_item)keyboard->pd->system->setMenuItemTitle(keyboard->clear_item,keyboard->config.labels.clear);
        if(keyboard->done_item)keyboard->pd->system->setMenuItemTitle(keyboard->done_item,keyboard->config.labels.done);
        if(keyboard->cancel_item)keyboard->pd->system->setMenuItemTitle(keyboard->cancel_item,keyboard->config.labels.cancel);
        if(keyboard->hide_item)keyboard->pd->system->setMenuItemTitle(keyboard->hide_item,keyboard->config.labels.hide);
    }
    return true;
}

void hello_keyboard_set_feedback_config(HelloKeyboard* keyboard,
                                        const HelloKeyboardFeedbackConfig* feedback) {
    if (!keyboard) return;
    keyboard->config.feedback=feedback?*feedback:(HelloKeyboardFeedbackConfig){0};
    if (!keyboard->config.feedback.duration_ms)
        keyboard->config.feedback.duration_ms=HELLO_KEYBOARD_DEFAULT_FEEDBACK_MS;
}

void hello_keyboard_show_feedback(HelloKeyboard* keyboard, const char* code,
                                  const char* fallback, uint32_t now) {
    if (!keyboard) return;
    if (!fallback) fallback="";
    const HelloKeyboardFeedbackConfig* feedback=&keyboard->config.feedback;
    const char* message=feedback->resolver?feedback->resolver(feedback->userdata,code,fallback):NULL;
    // Keep caller/server text outside the generic keyboard; retain only the
    // bounded message copy already used by the title/error renderer.
    hello_keyboard_show_error(keyboard,message?message:fallback,now,feedback->duration_ms);
}

void hello_keyboard_set_title(HelloKeyboard* keyboard, const char* title) {
    if (title == NULL) title = "";
    copy_utf8(keyboard->title_storage,sizeof(keyboard->title_storage),title);
    keyboard->config.title = keyboard->title_storage;
    keyboard->title_started_at = keyboard->pd->system->getCurrentTimeMilliseconds();
}

void hello_keyboard_clear_error(HelloKeyboard* keyboard) {
    keyboard->error_storage[0] = '\0';
    keyboard->error_started = 0;
    keyboard->error_until = 0;
}

void hello_keyboard_show_error(HelloKeyboard* keyboard, const char* error,
                               uint32_t now, uint32_t duration_ms) {
    if (error == NULL) error = "";
    copy_utf8(keyboard->error_storage,sizeof(keyboard->error_storage),error);
    keyboard->error_started = now;
    keyboard->error_until = now + duration_ms;
}

void hello_keyboard_set_initial_layer(HelloKeyboard* keyboard, bool alternate) {
    keyboard->initial_alternate = alternate;
}

static void configure_model_emoji_policy(HelloKeyboard* keyboard) {
    keyboard->model.shifted_up_enabled=keyboard->config.emoji_input_enabled;
    hello_keyboard_model_set_view_count(&keyboard->model,
        keyboard->config.emoji_input_enabled&&keyboard->config.emoji_picker_enabled?2:1);
}

bool hello_keyboard_set_emoji_input_enabled(HelloKeyboard* keyboard,bool enabled) {
    if(!keyboard||keyboard->visible)return false;
    keyboard->config.emoji_input_enabled=enabled;
    configure_model_emoji_policy(keyboard);
    return true;
}
bool hello_keyboard_set_field_policy(HelloKeyboard* keyboard,const HelloKeyboardFieldPolicy* policy) {
    if(!keyboard||!hello_input_field_policy_valid(policy))return false;
    HelloKeyboardFieldPolicy resolved=*policy;
    if(!resolved.max_bytes)resolved.max_bytes=HELLO_KEYBOARD_TEXT_BYTES;
    keyboard->config.field=resolved;
    if(keyboard->visible) {
        hello_keyboard_model_set_field_policy(&keyboard->model,&resolved);
        hello_input_field_init(&keyboard->input_field,resolved.type);
    }
    return true;
}
void hello_keyboard_set_max_length(HelloKeyboard* keyboard,size_t maximum) {
    if(!keyboard)return;
    HelloKeyboardFieldPolicy policy=keyboard->visible?keyboard->model.field:keyboard->config.field;
    policy.max_length=maximum>HELLO_KEYBOARD_TEXT_BYTES?HELLO_KEYBOARD_TEXT_BYTES:maximum;
    hello_keyboard_set_field_policy(keyboard,&policy);
}

void hello_keyboard_set_group_height(HelloKeyboard* keyboard,int pixels) {
    keyboard->config.group_height=pixels>=26?pixels:0;
}

void hello_keyboard_set_visible_prefix(HelloKeyboard* keyboard,int group,unsigned count) {
    if(!keyboard||group<1||group>HELLO_KEYBOARD_GROUP_COUNT)return;
    keyboard->config.visible_prefix_items[group-1]=count>HELLO_KEYBOARD_MAX_GROUP_CHARS?
        HELLO_KEYBOARD_MAX_GROUP_CHARS:count;
}

void hello_keyboard_init_with_config(HelloKeyboard* keyboard, PlaydateAPI* pd,
                                     const HelloKeyboardConfig* config) {
    memset(keyboard, 0, sizeof(*keyboard));
    keyboard->pd = pd;
    static const char* const table_paths[] = {
        "keyboard/glyphs", "keyboard/footer", "keyboard/dpad"
    };
    for (int i = 0; i < 3; ++i) {
        const char* error = NULL;
        keyboard->art[i] = pd->graphics->loadBitmapTable(table_paths[i], &error);
        if (keyboard->art[i] == NULL)
            pd->system->logToConsole("keyboard table missing: %s (%s)",
                table_paths[i], error != NULL ? error : "unknown");
    }
    HelloKeyboardConfig defaults;
    hello_keyboard_default_config(&defaults);
    const HelloKeyboardFieldPolicy* field=config?&config->field:&defaults.field;
    const bool unspecified=config&&!field->min_length&&!field->max_length&&!field->max_bytes&&
        !field->trim_for_length&&field->type==HELLO_INPUT_PLAINTEXT&&field->length_unit==HELLO_KEYBOARD_LENGTH_GRAPHEMES;
    if(!hello_keyboard_set_field_policy(keyboard,unspecified?&defaults.field:field))
        hello_keyboard_set_field_policy(keyboard,&defaults.field);
    keyboard->config.menu_actions=config&&config->menu_actions?
        config->menu_actions&(HELLO_KEYBOARD_MENU_ALL|HELLO_KEYBOARD_MENU_HIDE):HELLO_KEYBOARD_MENU_ALL;
    keyboard->config.font = config != NULL ? config->font : defaults.font;
    keyboard->config.transform = config != NULL ? config->transform : NULL;
    keyboard->config.transform_userdata = config != NULL ? config->transform_userdata : NULL;
    keyboard->config.unicode_font = config != NULL ? config->unicode_font : NULL;
    keyboard->config.emoji_input_enabled=config?config->emoji_input_enabled:defaults.emoji_input_enabled;
    keyboard->config.emoji_picker_enabled=config?config->emoji_picker_enabled:defaults.emoji_picker_enabled;
    keyboard->config.emoji_sets=config&&config->emoji_sets&&config->emoji_set_count?
        config->emoji_sets:defaults.emoji_sets;
    keyboard->config.emoji_set_count=config&&config->emoji_sets&&config->emoji_set_count?
        config->emoji_set_count:defaults.emoji_set_count;
    keyboard->config.on_emoji_set_selected=config?config->on_emoji_set_selected:NULL;
    keyboard->config.emoji_set_userdata=config?config->emoji_set_userdata:NULL;
    if(!emoji_set_usable(keyboard->config.emoji_sets[0])) {
        keyboard->config.emoji_sets=default_emoji_catalog();keyboard->config.emoji_set_count=1;
    }
    keyboard->selected_emoji_set=keyboard->config.emoji_sets[0];
    if (keyboard->config.font != NULL && config != NULL && config->font_size > 0)
        keyboard->config.font_size = config->font_size;
    else if (keyboard->config.font != NULL)
        keyboard->config.font_size = pd->graphics->getFontHeight(keyboard->config.font);
    else
        keyboard->config.font_size = defaults.font_size;
    hello_keyboard_set_group_height(keyboard,config?config->group_height:0);
    // Init copies captions into owned buffers. Only feedback's callback context
    // retains caller ownership; no label pointer survives this initialization.
    hello_keyboard_set_labels(keyboard,config?&config->labels:NULL);
    hello_keyboard_set_feedback_config(keyboard,config?&config->feedback:NULL);

    for (int index = 0; index < HELLO_KEYBOARD_GROUP_COUNT; ++index) {
        hello_keyboard_set_visible_prefix(keyboard,index+1,config?config->visible_prefix_items[index]:0);
        const char* value = config != NULL ? config->groups[index] : NULL;
        if (!hello_keyboard_model_group_is_valid(value))
            value = defaults.groups[index];
        strncpy(keyboard->group_storage[index], value, HELLO_KEYBOARD_MAX_GROUP_BYTES);
        keyboard->group_storage[index][HELLO_KEYBOARD_MAX_GROUP_BYTES] = '\0';
        keyboard->config.groups[index] = keyboard->group_storage[index];
    }
    if(keyboard->config.emoji_picker_enabled)use_emoji_set(keyboard,keyboard->selected_emoji_set,false);
    hello_keyboard_set_title(
        keyboard, config != NULL && config->title != NULL ? config->title : defaults.title);
    hello_keyboard_set_timing(keyboard,config&&brb_input_timing_valid(&config->timing)?&config->timing:&defaults.timing);
    if(!hello_keyboard_set_button_navigation_scale(keyboard,config?config->button_navigation_scale:1.0f))
        hello_keyboard_set_button_navigation_scale(keyboard,1.0f);
    configure_model_emoji_policy(keyboard);
    hello_keyboard_model_set_view(&keyboard->model,HELLO_KB_VIEW_KEYBOARD);
}

bool hello_keyboard_set_button_navigation_scale(HelloKeyboard* keyboard,float scale) {
    if(!keyboard||!hello_keyboard_model_set_button_navigation_scale(&keyboard->model,scale))return false;
    keyboard->config.button_navigation_scale=scale;
    return true;
}

bool hello_keyboard_set_timing(HelloKeyboard* keyboard,const BrbInputTimingConfig* timing) {
    if(!keyboard||!hello_keyboard_model_set_timing(&keyboard->model,timing))return false;
    keyboard->config.timing=*timing;return true;
}

void hello_keyboard_init(HelloKeyboard* keyboard, PlaydateAPI* pd) {
    hello_keyboard_init_with_config(keyboard, pd, NULL);
}

static bool open_with_policy(HelloKeyboard* keyboard,const char* text,const HelloKeyboardFieldPolicy* policy) {
    if(!keyboard||keyboard->visible||!hello_input_field_policy_valid(policy))return false;
    if(!text)text="";
    if(hello_input_field_validate(policy,text,false,NULL)!=HELLO_FIELD_VALID)return false;
    strcpy(keyboard->initial_text,text);
    hello_keyboard_model_init_with_groups(&keyboard->model,text,strlen(text),keyboard->config.groups);
    hello_keyboard_model_set_timing(&keyboard->model,&keyboard->config.timing);
    hello_keyboard_model_set_button_navigation_scale(&keyboard->model,keyboard->config.button_navigation_scale);
    configure_model_emoji_policy(keyboard);
    if(keyboard->config.emoji_picker_enabled)use_emoji_set(keyboard,keyboard->selected_emoji_set,false);
    keyboard->emoji_picker_index=0;
    for(size_t i=0;i<keyboard->config.emoji_set_count;i++)
        if(keyboard->config.emoji_sets[i]==keyboard->selected_emoji_set){keyboard->emoji_picker_index=i;break;}
    hello_keyboard_model_set_field_policy(&keyboard->model,policy);
    keyboard->model.transform=keyboard->config.transform;
    keyboard->model.transform_userdata=keyboard->config.transform_userdata;
    keyboard->model.alternate=false;
    keyboard->requested_action = HELLO_KEYBOARD_NONE;
    clear_deferred_repeat(keyboard);
    hello_input_field_init(&keyboard->input_field, policy->type);
    hello_keyboard_cursor_blink_reset(&keyboard->cursor_blink);
    keyboard->cancel_dialog_active=keyboard->cancel_dialog_ready=false;
    reset_dialog_choices(keyboard);
    keyboard->cancel_dialog_started_at=0;
    keyboard->cancel_dialog_kind=HELLO_KEYBOARD_DIALOG_CANCEL;
    reset_crank(keyboard);
    const unsigned actions=keyboard->config.menu_actions;
    if(actions&HELLO_KEYBOARD_MENU_CLEAR)
        keyboard->clear_item=keyboard->pd->system->addMenuItem(keyboard->config.labels.clear,clear_callback,keyboard);
    if(actions&HELLO_KEYBOARD_MENU_DONE)
        keyboard->done_item=keyboard->pd->system->addMenuItem(keyboard->config.labels.done,done_callback,keyboard);
    if(actions&HELLO_KEYBOARD_MENU_CANCEL)
        keyboard->cancel_item=keyboard->pd->system->addMenuItem(keyboard->config.labels.cancel,cancel_callback,keyboard);
    const bool hide=(actions&HELLO_KEYBOARD_MENU_HIDE)&&hello_input_field_can_toggle_mask(&keyboard->input_field);
    if(hide&&keyboard->pd->system->addOptionsMenuItem&&keyboard->pd->system->getMenuItemValue&&keyboard->pd->system->setMenuItemValue) {
        hello_keyboard_label_copy(keyboard->hide_option_storage[0],keyboard->config.labels.no);
        hello_keyboard_label_copy(keyboard->hide_option_storage[1],keyboard->config.labels.yes);
        keyboard->hide_options[0]=keyboard->hide_option_storage[0];keyboard->hide_options[1]=keyboard->hide_option_storage[1];
        keyboard->hide_item=keyboard->pd->system->addOptionsMenuItem(keyboard->config.labels.hide,keyboard->hide_options,2,hide_callback,keyboard);
        if(keyboard->hide_item)keyboard->pd->system->setMenuItemValue(keyboard->hide_item,0);
    }
    if(((actions&HELLO_KEYBOARD_MENU_CLEAR)&&!keyboard->clear_item)||
       ((actions&HELLO_KEYBOARD_MENU_DONE)&&!keyboard->done_item)||
       ((actions&HELLO_KEYBOARD_MENU_CANCEL)&&!keyboard->cancel_item)||(hide&&!keyboard->hide_item)) {
        hello_keyboard_close(keyboard);
        return false;
    }
    keyboard->visible = true;
    keyboard->footer_dpad_buttons=0;
    keyboard->footer_buttons=0;
    reset_group_scrolls(keyboard);
    reset_selection_lock(keyboard);
    keyboard->rejected_face_buttons=0;
    keyboard->selection_marker.active = false;
    return true;
}

bool hello_keyboard_open_configured(HelloKeyboard* keyboard,const char* text,const HelloKeyboardOpenOptions* options) {
    if(!keyboard||keyboard->visible)return false;
    const HelloKeyboardFieldPolicy policy=options&&options->field?*options->field:keyboard->config.field;
    if(!hello_input_field_policy_valid(&policy)||
       hello_input_field_validate(&policy,text?text:"",false,NULL)!=HELLO_FIELD_VALID)return false;
    hello_keyboard_set_labels(keyboard,options?options->labels:NULL);
    hello_keyboard_set_feedback_config(keyboard,options?options->feedback:NULL);
    return open_with_policy(keyboard,text,&policy);
}

static HelloKeyboardFieldPolicy legacy_field_policy(const HelloKeyboard* keyboard,HelloInputType type) {
    HelloKeyboardFieldPolicy policy=keyboard->config.field;
    policy.type=type;
    const size_t cap=hello_input_field_max_length(type);
    if(cap&&policy.max_length>cap)policy.max_length=cap;
    return policy;
}
bool hello_keyboard_open_field(HelloKeyboard* keyboard,const char* text,HelloInputType type) {
    if(!keyboard)return false;
    const HelloKeyboardFieldPolicy policy=legacy_field_policy(keyboard,type);
    return open_with_policy(keyboard,text,&policy);
}
bool hello_keyboard_open(HelloKeyboard* keyboard,const char* text) {
    return hello_keyboard_open_field(keyboard,text,HELLO_INPUT_PLAINTEXT);
}
bool hello_keyboard_open_field_with_options(HelloKeyboard* keyboard,const char* text,
    HelloInputType type,const HelloKeyboardOpenOptions* options) {
    if(!keyboard)return false;
    HelloKeyboardFieldPolicy policy=options&&options->field?*options->field:legacy_field_policy(keyboard,type);
    policy.type=type;
    HelloKeyboardOpenOptions resolved=options?*options:(HelloKeyboardOpenOptions){0};
    resolved.field=&policy;
    return hello_keyboard_open_configured(keyboard,text,&resolved);
}

void hello_keyboard_close(HelloKeyboard* keyboard) {
    clear_deferred_repeat(keyboard);
    if (keyboard->clear_item != NULL) keyboard->pd->system->removeMenuItem(keyboard->clear_item);
    if (keyboard->done_item != NULL) keyboard->pd->system->removeMenuItem(keyboard->done_item);
    if (keyboard->cancel_item != NULL) keyboard->pd->system->removeMenuItem(keyboard->cancel_item);
    if (keyboard->hide_item != NULL) keyboard->pd->system->removeMenuItem(keyboard->hide_item);
    keyboard->clear_item = NULL;
    keyboard->done_item = NULL;
    keyboard->cancel_item = NULL;
    keyboard->hide_item = NULL;
    keyboard->cancel_dialog_active=keyboard->cancel_dialog_ready=false;
    reset_dialog_choices(keyboard);
    keyboard->cancel_dialog_started_at=0;
    keyboard->cancel_dialog_kind=HELLO_KEYBOARD_DIALOG_CANCEL;
    keyboard->visible = false;
    hello_keyboard_cursor_blink_reset(&keyboard->cursor_blink);
    keyboard->footer_dpad_buttons=0;
    keyboard->footer_buttons=0;
    reset_selection_lock(keyboard);
    keyboard->rejected_face_buttons=0;
    memset(keyboard->group_scroll_motion,0,sizeof(keyboard->group_scroll_motion));
    keyboard->selection_marker.active = false;
    hello_input_field_init(&keyboard->input_field, HELLO_INPUT_PLAINTEXT);
    reset_crank(keyboard);
}

static void direction_down(HelloKeyboard* keyboard, PDButtons pushed, uint32_t now) {
    if (pushed & kButtonUp) hello_keyboard_model_direction(&keyboard->model, HELLO_KB_UP, now);
    if (pushed & kButtonRight) hello_keyboard_model_direction(&keyboard->model, HELLO_KB_RIGHT, now);
    if (pushed & kButtonDown) hello_keyboard_model_direction(&keyboard->model, HELLO_KB_DOWN, now);
    if (pushed & kButtonLeft) hello_keyboard_model_direction(&keyboard->model, HELLO_KB_LEFT, now);
}

static void direction_up(HelloKeyboard* keyboard, PDButtons released, uint32_t now) {
    if (released & kButtonUp) hello_keyboard_model_direction_release(&keyboard->model, HELLO_KB_UP, now);
    if (released & kButtonRight) hello_keyboard_model_direction_release(&keyboard->model, HELLO_KB_RIGHT, now);
    if (released & kButtonDown) hello_keyboard_model_direction_release(&keyboard->model, HELLO_KB_DOWN, now);
    if (released & kButtonLeft) hello_keyboard_model_direction_release(&keyboard->model, HELLO_KB_LEFT, now);
}

static HelloKeyboardCrankDetent crank_detent(const HelloKeyboard* keyboard) {
    return (HelloKeyboardCrankDetent){.remainder=keyboard->crank_remainder,
        .release_steps=keyboard->crank_release_steps,.direction=keyboard->crank_release_direction,
        .release_peak=keyboard->crank_release_peak,.velocity=keyboard->crank_velocity,
        .speed_factor=keyboard->crank_speed_factor,.sampled_at=keyboard->crank_sampled_at,
        .sampled=keyboard->crank_sampled,.wrapped=keyboard->crank_wrapped,
        .entry_breakaway=keyboard->crank_entry_breakaway};
}
static void store_crank_detent(HelloKeyboard* keyboard,const HelloKeyboardCrankDetent* detent) {
    keyboard->crank_remainder=detent->remainder;
    keyboard->crank_release_steps=detent->release_steps;
    keyboard->crank_release_direction=detent->direction;
    keyboard->crank_release_peak=detent->release_peak;
    keyboard->crank_velocity=detent->velocity;
    keyboard->crank_speed_factor=detent->speed_factor;
    keyboard->crank_sampled_at=detent->sampled_at;
    keyboard->crank_sampled=detent->sampled;
    keyboard->crank_wrapped=detent->wrapped;
    keyboard->crank_entry_breakaway=detent->entry_breakaway;
}
static void crank_navigation(HelloKeyboard* keyboard,float change,uint32_t now,bool picker) {
    HelloKeyboardModel* model=&keyboard->model;
    HelloKeyboardCrankDetent detent=crank_detent(keyboard);
    if(!hello_keyboard_crank_add(&detent,change)){reset_crank(keyboard);return;}
    hello_keyboard_crank_sample_velocity(&detent,change,now);
    bool exhausted=true;
    for(int budget=0;budget<32;++budget) {
        const bool blocked=picker?(detent.remainder<0?keyboard->emoji_picker_index==0:
            detent.remainder>0&&keyboard->emoji_picker_index>=keyboard->config.emoji_set_count-1):
            (detent.remainder<0?model->cursor==0:detent.remainder>0&&model->cursor>=strlen(model->text));
        if(blocked) {
            // Discard outward pressure at a text boundary, keeping the last
            // consumed direction and its ordinary reversal resistance.
            detent.remainder=0;
            hello_keyboard_crank_velocity_reset(&detent);
            exhausted=false;break;
        }
        const int step=hello_keyboard_crank_take(&detent,false);
        if(!step){exhausted=false;break;}
        if(picker) {
            if(step<0)--keyboard->emoji_picker_index;
            else ++keyboard->emoji_picker_index;
        } else hello_keyboard_model_move_cursor(model,step);
    }
    if(exhausted)hello_keyboard_crank_reset(&detent);
    store_crank_detent(keyboard,&detent);
}
static void crank_selection(HelloKeyboard* keyboard,float change,uint32_t now) {
    HelloKeyboardModel* model=&keyboard->model;
    const HelloKeyboardHold* a=&model->hold[HELLO_KB_BUTTON_A];
    if(model->group==0&&a->active&&a->group==0&&a->hold==HELLO_KB_HOLD_CASE&&
       !a->handled&&brb_input_reached(now,a->at,model->timing.hold_ms))
        hello_keyboard_model_update_with_selection(model,now,false);
    if(hello_keyboard_model_is_choosing_layer(model)) {
        crank_navigation(keyboard,change,now,false);
        return;
    }
    if(model->view==HELLO_KB_VIEW_EMOJI_SETS) {
        crank_navigation(keyboard,change,now,true);
        return;
    }
    if(selection_locked(keyboard)&&keyboard->selection_lock_crank) {
        // Gated travel is discarded, including its velocity history. It must
        // never become angular pressure or a burst of resistance on unlock.
        keyboard->crank_velocity=0;keyboard->crank_speed_factor=1;
        keyboard->crank_sampled_at=0;keyboard->crank_sampled=false;
        return;
    }
    const int length=hello_keyboard_model_group_length(model,model->group);
    if(model->group!=0&&length>=2&&!model->pending&&isfinite(change)) {
        if(fabsf(change)<=0.01f)return;
        const int direction=change<0?-1:1;
        const int first_index=direction<0?group_last_visible_item(keyboard):-1;
        hello_keyboard_model_crank_from(model,direction,now,first_index);
        // Real movement accepts one visible entry immediately. Its sample owns
        // no subsequent travel; the next movement still owes the initial well.
        HelloKeyboardCrankDetent entry={.direction=direction,.entry_breakaway=true};
        hello_keyboard_crank_sample_velocity(&entry,change,now);
        if(model->pending)store_crank_detent(keyboard,&entry);
        else reset_crank(keyboard);
        return;
    }
    HelloKeyboardCrankDetent detent=crank_detent(keyboard);
    if(model->group==0||length<2||!hello_keyboard_crank_add(&detent,change)) {
        reset_crank(keyboard);return;
    }
    hello_keyboard_crank_sample_velocity(&detent,change,now);
    if(fabsf(change)>0.01f) {
        if(model->pending)model->pending_at=now;
        else model->group_at=now;
    }
    bool exhausted=true;
    // Real angular input only; keep bounded work for invalid SDK-sized deltas.
    for(int n=0;n<32;n++) {
        const int direction=detent.remainder<0?-1:1;
        const bool wraps=model->pending && (direction>0?model->index==length-1:model->index==0);
        const int step=hello_keyboard_crank_take(&detent,wraps);
        if(!step){exhausted=false;break;}
        if(selection_locked(keyboard)&&!keyboard->selection_lock_crank) {
            // Only a real detent supersedes a button-entry presentation gate.
            reset_selection_lock(keyboard);
            keyboard->crank_marker_rebase_group=0;
            keyboard->crank_marker_pinned_group=0;
        }
        const bool had_pending=model->pending!=0;
        const int previous_index=model->index;
        const int first_index=!had_pending&&step<0?group_last_visible_item(keyboard):-1;
        hello_keyboard_model_crank_from(model,step,now,first_index);
        if(!had_pending&&model->pending) {
            // Present the accepted entry item before further physical travel.
            // Excess from this first sample is discarded, never replayed idle.
            detent.remainder=0;
            exhausted=false;
            break;
        }
        if(crossed_selection_boundary(model,had_pending,previous_index,step)) {
            // The endpoint must be presented before another detent can move
            // away. Excess travel from this sample is discarded, never queued.
            lock_boundary_selection(keyboard);
            detent.remainder=0;
            hello_keyboard_crank_velocity_reset(&detent);
            exhausted=false;
            break;
        }
    }
    if(exhausted)hello_keyboard_crank_reset(&detent);
    store_crank_detent(keyboard,&detent);
}

static void home_inactive_group_scrolls(HelloKeyboard* keyboard,uint32_t now) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        HelloScrollFollow* follow=&keyboard->group_scroll[group];
        if(follow->initialized && (keyboard->model.group!=group || !keyboard->model.pending))
            hello_keyboard_scroll_follow_update(follow,&keyboard->group_scroll_motion[group],0,now,true);
    }
}

static void field_feedback(HelloKeyboard* keyboard,HelloInputValidation status,uint32_t now) {
    const char* code="FIELD_INVALID";const char* fallback="Invalid field value";
    if(status==HELLO_FIELD_REQUIRED){code="FIELD_REQUIRED";fallback="This field is required";}
    else if(status==HELLO_FIELD_TOO_SHORT){code="FIELD_TOO_SHORT";fallback="Text is too short";}
    else if(status==HELLO_FIELD_TOO_LONG){code="FIELD_TOO_LONG";fallback="Text is too long";}
    else if(status==HELLO_FIELD_INVALID_TYPE){code="FIELD_INVALID_TYPE";fallback="Only digits are allowed";}
    hello_keyboard_show_feedback(keyboard,code,fallback,now);
}

static void open_edit_decision(HelloKeyboard* keyboard,uint32_t now,PDButtons current);

static HelloKeyboardResult finish_keyboard_action(HelloKeyboard* keyboard,uint32_t now,PDButtons current) {
    const PDButtons face_buttons=kButtonA|kButtonB;
    if (keyboard->requested_action == HELLO_KEYBOARD_DONE) {
        if(!keyboard->dialog_save_confirmed) {
            open_edit_decision(keyboard,now,current);
            return HELLO_KEYBOARD_NONE;
        }
        keyboard->dialog_save_confirmed=false;
        clear_deferred_repeat(keyboard);
        const size_t draft_cursor=keyboard->model.cursor;
        memcpy(keyboard->preview.raw,keyboard->model.text,strlen(keyboard->model.text)+1);
        hello_keyboard_model_commit(&keyboard->model, false);
        HelloInputFieldValue value;
        HelloInputValidation status=keyboard->model.input_rejected;
        if(status==HELLO_FIELD_VALID)
            status=hello_input_field_validate(&keyboard->model.field,keyboard->model.text,true,&value);
        if(status!=HELLO_FIELD_VALID) {
            // A pending item may have been accepted before completion failed.
            // Roll back only the draft transaction; the instance scratch keeps
            // large text off the device's small stack and draw overwrites it.
            memcpy(keyboard->model.text,keyboard->preview.raw,strlen(keyboard->preview.raw)+1);
            keyboard->model.cursor=draft_cursor;
        }
        if(status==HELLO_FIELD_VALID&&keyboard->model.field.trim_for_length) {
            size_t cursor=keyboard->model.cursor>value.offset?keyboard->model.cursor-value.offset:0;
            memmove(keyboard->model.text,keyboard->model.text+value.offset,value.bytes);
            keyboard->model.text[value.bytes]=0;
            if(cursor>value.bytes)cursor=value.bytes;
            keyboard->model.cursor=brb_grapheme_floor(keyboard->model.text,cursor);
        }
        hello_keyboard_model_reset_input(&keyboard->model);
        reset_crank(keyboard);
        reset_selection_lock(keyboard);
        keyboard->selection_marker.active=false;
        // Validation may leave the host's input field open. A button still
        // physically held at completion must be released before a new gesture.
        keyboard->rejected_face_buttons|=current&face_buttons;
        keyboard->cancel_dialog_active=keyboard->cancel_dialog_ready=false;
        reset_dialog_choices(keyboard);
        keyboard->cancel_dialog_started_at=0;
        keyboard->cancel_dialog_kind=HELLO_KEYBOARD_DIALOG_CANCEL;
        home_inactive_group_scrolls(keyboard,now);
        keyboard->requested_action = HELLO_KEYBOARD_NONE;
        if(status!=HELLO_FIELD_VALID){field_feedback(keyboard,status,now);return HELLO_KEYBOARD_NONE;}
        return HELLO_KEYBOARD_DONE;
    }
    if(keyboard->requested_action==HELLO_KEYBOARD_NONE&&keyboard->model.input_rejected!=HELLO_FIELD_VALID) {
        field_feedback(keyboard,keyboard->model.input_rejected,now);
        keyboard->model.input_rejected=HELLO_FIELD_VALID;
    }
    if (keyboard->requested_action == HELLO_KEYBOARD_CANCELLED) {
        hello_keyboard_model_init_with_groups(
            &keyboard->model,
            keyboard->initial_text,
            strlen(keyboard->initial_text),
            keyboard->config.groups);
        hello_keyboard_model_set_timing(&keyboard->model,&keyboard->config.timing);
        hello_keyboard_model_set_button_navigation_scale(&keyboard->model,keyboard->config.button_navigation_scale);
        configure_model_emoji_policy(keyboard);
        hello_keyboard_close(keyboard);
        keyboard->requested_action = HELLO_KEYBOARD_NONE;
        return HELLO_KEYBOARD_CANCELLED;
    }
    return HELLO_KEYBOARD_NONE;
}

static bool update_cancel_prompt(HelloKeyboard* keyboard,uint32_t now,PDButtons current,
    PDButtons pushed,PDButtons released) {
    const PDButtons faces=kButtonA|kButtonB;
    if(keyboard->cancel_dialog_active) {
        if(!keyboard->cancel_dialog_ready) {
            if(!(current&faces))keyboard->cancel_dialog_ready=true;
            return true;
        }
        if(((current|pushed)&faces)==faces) {
            // Opposed choices are neutral, including a coalesced tap while
            // the other face button is held. Require a fresh all-up boundary.
            reset_dialog_choices(keyboard);
            keyboard->cancel_dialog_ready=false;
            return true;
        }
        // A choice is a fresh press, never a hold timer. Also accept a short
        // press/release pair reported in the same SDK sample exactly once.
        if((pushed&kButtonB)&&((current|released)&kButtonB)) {
            keyboard->requested_action=HELLO_KEYBOARD_CANCELLED;
        } else if((pushed&kButtonA)&&((current|released)&kButtonA)) {
            keyboard->dialog_save_confirmed=true;
            keyboard->requested_action=HELLO_KEYBOARD_DONE;
        }
        return true;
    }
    // Open on the first fresh overlap, before either individual action runs.
    // current|pushed also covers a full A+B tap coalesced into one SDK sample;
    // released must confirm any newly pushed face that is already up. A plain
    // release of the old face plus a new opposite press is not an overlap.
    const PDButtons fresh=pushed&faces&~keyboard->rejected_face_buttons;
    const PDButtons owned=((keyboard->chord_held_faces&current)|
        (fresh&(current|released)))&~keyboard->rejected_face_buttons;
    keyboard->chord_held_faces=owned&current;
    if(fresh&&(owned&faces)==faces) {
        restore_chord_checkpoint(keyboard);
        open_edit_decision(keyboard,now,current);
        return true;
    }
    return false;
}

static void open_edit_decision(HelloKeyboard* keyboard,uint32_t now,PDButtons current) {
    clear_deferred_repeat(keyboard);
    hello_keyboard_model_cancel_gestures(&keyboard->model);
    keyboard->model.submit_requested=false;
    keyboard->model.view_confirm_requested=false;
    reset_crank(keyboard);reset_selection_lock(keyboard);keyboard->selection_marker.active=false;
    keyboard->cancel_dialog_active=true;keyboard->cancel_dialog_kind=HELLO_KEYBOARD_DIALOG_CANCEL;
    keyboard->cancel_dialog_started_at=now;
    keyboard->cancel_dialog_ready=(current&(kButtonA|kButtonB))==0;
    reset_dialog_choices(keyboard);
    keyboard->requested_action=HELLO_KEYBOARD_NONE;
}

HelloKeyboardResult hello_keyboard_update(HelloKeyboard* keyboard,
                                           uint32_t now,
                                           PDButtons current,
                                           PDButtons pushed,
                                           PDButtons released,
                                           float crank_change) {
    keyboard->footer_buttons=current&(kButtonUp|kButtonRight|kButtonDown|kButtonLeft|kButtonA|kButtonB);
    if (!keyboard->visible) return HELLO_KEYBOARD_NONE;
    sync_emoji_renderer(keyboard);
    if(keyboard->requested_action!=HELLO_KEYBOARD_NONE) {
        hello_keyboard_cursor_blink_observe(&keyboard->cursor_blink,now,false);
        return finish_keyboard_action(keyboard,now,current);
    }
    const size_t previous_cursor=keyboard->model.cursor;
    const uint32_t previous_pending_at=keyboard->model.pending_at;
    const float physical_crank_change=crank_change;
    const PDButtons physical_current=current;
    HelloKeyboardChordCheckpoint* checkpoint=&keyboard->chord_checkpoint;
    if(checkpoint->valid) {
        const PDButtons faces=kButtonA|kButtonB;
        const bool overlapping=(current&checkpoint->face)&&
            (pushed&faces&~checkpoint->face&~keyboard->rejected_face_buttons);
        const uint32_t threshold=checkpoint->group?hello_keyboard_model_selection_hold_ms(&keyboard->model):
            keyboard->model.timing.hold_ms;
        // Only the initial, unconsumed face action is reversible. Explicit
        // direction/crank work and accepted hold repeats keep their history.
        if(!(current&checkpoint->face)||(released&checkpoint->face)||brb_input_reached(now,checkpoint->at,threshold)||
           (!overlapping&&(((pushed|released)&(kButtonUp|kButtonRight|kButtonDown|kButtonLeft))||
             (isfinite(crank_change)&&fabsf(crank_change)>0.01f))))checkpoint->valid=false;
    }
    keyboard->footer_dpad_buttons=current&(kButtonUp|kButtonRight|kButtonDown|kButtonLeft);
    const HelloKeyboardHold* navigation_a=&keyboard->model.hold[HELLO_KB_BUTTON_A];
    const bool navigation_chord=(pushed&(kButtonUp|kButtonRight|kButtonDown|kButtonLeft))!=0;
    if((current&kButtonA)&&keyboard->model.group==0&&navigation_a->active&&navigation_a->group==0&&
       navigation_a->hold==HELLO_KB_HOLD_CASE&&(navigation_a->handled||navigation_chord||
       brb_input_reached(now,navigation_a->at,keyboard->model.timing.hold_ms))) {
        // B is unmapped once held A owns navigation. Keep its physical latch
        // until release, even though this update consumes its logical bits.
        if(physical_current&kButtonB)keyboard->rejected_face_buttons|=kButtonB;
        current&=~kButtonB;pushed&=~kButtonB;released&=~kButtonB;
    }
    // The physical chord owns this frame before group changes, commit, delete,
    // selection or crank dispatch. Its opening tap cannot choose an action.
    if(update_cancel_prompt(keyboard,now,current,pushed,released)) {
        hello_keyboard_cursor_blink_observe(&keyboard->cursor_blink,now,false);
        return finish_keyboard_action(keyboard,now,current);
    }
    if(keyboard->model.view==HELLO_KB_VIEW_EMOJI_SETS) {
        // A fresh A+B chord was handled above; B alone has no picker action.
        current&=~kButtonB;pushed&=~kButtonB;released&=~kButtonB;
        const HelloKeyboardHold* a=&keyboard->model.hold[HELLO_KB_BUTTON_A];
        const bool held_a=(current&kButtonA)&&a->active&&a->group==0&&a->hold==HELLO_KB_HOLD_CASE;
        if(!held_a) {
            if((pushed&kButtonUp)&&keyboard->emoji_picker_index) --keyboard->emoji_picker_index;
            if((pushed&kButtonDown)&&keyboard->emoji_picker_index<keyboard->config.emoji_set_count-1)
                ++keyboard->emoji_picker_index;
        }
    }
    const PDButtons input_buttons =
        kButtonUp | kButtonRight | kButtonDown | kButtonLeft | kButtonA | kButtonB;
    const PDButtons face_buttons = kButtonA | kButtonB;
    const int previous_group=keyboard->model.group;
    const int previous_index=keyboard->model.index;
    const uint32_t previous_pending=keyboard->model.pending;
    const HelloKeyboardView previous_view=keyboard->model.view;
    // A reported up in this snapshot finishes first, so a fresh D-pad gesture
    // can type immediately. Only A held from an earlier update owns a layer
    // chord; simultaneous fresh D-pad+A presses keep direction-then-selection.
    const bool masking_chord=(current&face_buttons)==face_buttons;
    if(keyboard->model.group==0 && !masking_chord &&
       (released&kButtonA)&&!(current&kButtonA)) {
        const HelloKeyboardHold* a=&keyboard->model.hold[HELLO_KB_BUTTON_A];
        const bool navigation=a->active&&a->group==0&&a->hold==HELLO_KB_HOLD_CASE&&
            (a->handled||brb_input_reached(now,a->at,keyboard->model.timing.hold_ms));
        hello_keyboard_model_release(&keyboard->model,HELLO_KB_BUTTON_A,now);
        if(navigation){reset_crank(keyboard);crank_change=0;}
    }
    direction_down(keyboard, pushed, now);
    // A cursor release stops before face-button hold processing can repeat it.
    // Group commits wait for this frame's accepted face/crank selection below.
    if(keyboard->model.cursor_hold)direction_up(keyboard, released, now);
    if(previous_view!=keyboard->model.view) {
        clear_deferred_repeat(keyboard);
        reset_selection_lock(keyboard);keyboard->selection_marker.active=false;
    }
    if(previous_view!=keyboard->model.view||previous_group!=keyboard->model.group||previous_index!=keyboard->model.index||
       previous_pending!=keyboard->model.pending)reset_crank(keyboard);
    update_deferred_repeat(keyboard,now,physical_current,pushed,released);
    const PDButtons ignored_releases=released&keyboard->rejected_face_buttons;
    keyboard->rejected_face_buttons&=physical_current;
    if(selection_locked(keyboard))keyboard->rejected_face_buttons|=pushed&face_buttons;
    pushed&=~keyboard->rejected_face_buttons;
    released&=~ignored_releases;
    current&=~keyboard->rejected_face_buttons;
    if (pushed & face_buttons) reset_crank(keyboard);
    if (keyboard->error_storage[0] != '\0' &&
        ((pushed & input_buttons) != 0 || fabsf(crank_change) > 0.01f))
        hello_keyboard_clear_error(keyboard);
    {
        // An up and a fresh down for one physical button may share an SDK
        // snapshot. Finish the old gesture before installing its new hold.
        for(int button=0;button<2;button++) {
            const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;
            if((pushed&released&current&face)&&keyboard->model.hold[button].active) {
                hello_keyboard_model_release_with_selection(&keyboard->model,(HelloKeyboardButton)button,now,
                    selection_repeats_allowed(keyboard,released));
                consume_selection_repeat(keyboard);
                released&=~face;
            }
        }
        if(selection_locked(keyboard)) {
            keyboard->rejected_face_buttons|=pushed&face_buttons;
            pushed&=~keyboard->rejected_face_buttons;
            current&=~keyboard->rejected_face_buttons;
        }
        if (pushed & kButtonA) {
            press_selection(keyboard, HELLO_KB_BUTTON_A, now);
        }
        if (pushed & kButtonB) {
            if(selection_locked(keyboard))keyboard->rejected_face_buttons|=kButtonB;
            else {
                press_selection(keyboard, HELLO_KB_BUTTON_B, now);
            }
        }
        if (released & kButtonA) {
            hello_keyboard_model_release_with_selection(&keyboard->model,HELLO_KB_BUTTON_A,now,
                selection_repeats_allowed(keyboard,released));
            consume_selection_repeat(keyboard);
        }
        if (released & kButtonB) {
            hello_keyboard_model_release_with_selection(&keyboard->model,HELLO_KB_BUTTON_B,now,
                selection_repeats_allowed(keyboard,released));
            consume_selection_repeat(keyboard);
        }
    }

    crank_selection(keyboard, crank_change, now);
    const int release_group=keyboard->model.group;
    direction_up(keyboard, released, now);
    if(release_group!=keyboard->model.group) {
        keyboard->chord_checkpoint.valid=false;
        // Acceptance writes the bounded RAM text buffer synchronously. Retire
        // all selection visuals/gates in this update, before the next gesture;
        // drawing consumes the latest buffer separately in the same frame.
        reset_crank(keyboard);
        reset_selection_lock(keyboard);
        keyboard->selection_marker.active=false;
    }
    hello_keyboard_model_update_with_selection(&keyboard->model,now,selection_repeats_allowed(keyboard,released));
    consume_selection_repeat(keyboard);
    selection_locked(keyboard);
    if(!keyboard->model.pending) {
        keyboard->crank_marker_rebase_group=0;
        keyboard->crank_marker_pinned_group=0;
    } else {
        if(keyboard->crank_marker_rebase_group!=keyboard->model.group)
            keyboard->crank_marker_rebase_group=0;
        if(keyboard->crank_marker_pinned_group!=keyboard->model.group)
            keyboard->crank_marker_pinned_group=0;
    }
    home_inactive_group_scrolls(keyboard,now);

    if(keyboard->model.view_confirm_requested) {
        keyboard->chord_checkpoint.valid=false;
        keyboard->model.view_confirm_requested=false;
        if(keyboard->model.view==HELLO_KB_VIEW_EMOJI_SETS&&
           keyboard->emoji_picker_index<keyboard->config.emoji_set_count&&
           use_emoji_set(keyboard,keyboard->config.emoji_sets[keyboard->emoji_picker_index],true)) {
            hello_keyboard_model_set_view(&keyboard->model,HELLO_KB_VIEW_KEYBOARD);
            reset_crank(keyboard);reset_selection_lock(keyboard);reset_group_scrolls(keyboard);
            keyboard->selection_marker.active=false;
        }
    }

    if(keyboard->model.submit_requested&&keyboard->requested_action!=HELLO_KEYBOARD_CANCELLED) {
        // Return may be committed by a group change before another selection
        // arrives in this SDK snapshot. That later selection is not the draft.
        hello_keyboard_model_reset_input(&keyboard->model);
        open_edit_decision(keyboard,now,physical_current);
    }
    const bool choosing=hello_keyboard_model_is_choosing_layer(&keyboard->model);
    if(isfinite(physical_crank_change)&&fabsf(physical_crank_change)>0.01f)
        keyboard->chord_checkpoint.valid=false;
    const bool cursor_moving=keyboard->model.cursor!=previous_cursor||keyboard->model.pending_at!=previous_pending_at||
        (keyboard->model.cursor_hold&&choosing)||
        (choosing&&isfinite(physical_crank_change)&&fabsf(physical_crank_change)>0.01f);
    hello_keyboard_cursor_blink_observe(&keyboard->cursor_blink,now,cursor_moving);
    return finish_keyboard_action(keyboard,now,current);
}

static int hold_fill(const HelloKeyboard* keyboard, HelloKeyboardButton button, uint32_t now) {
    const HelloKeyboardHold* hold = &keyboard->model.hold[button];
    if (hold->active) {
        if (hold->hold == HELLO_KB_HOLD_NONE) return 0;
        if (hold->hold == HELLO_KB_HOLD_DELETE && keyboard->model.cursor == 0) return 0;
        const uint32_t duration = now - hold->at;
        const uint32_t threshold=hold->hold==HELLO_KB_HOLD_REPEAT?
            hello_keyboard_model_selection_hold_ms(&keyboard->model):keyboard->model.timing.hold_ms;
        const int fill = (int)brb_input_progress(duration,threshold,21);
        return fill > 21 ? 21 : fill;
    }
    if (keyboard->model.feedback_active[button] &&
        now - keyboard->model.feedback_at[button] < 130) return 21;
    return 0;
}

static void draw_centered_item(HelloKeyboard* keyboard, const char* item,
                                int center_x, int center_y, bool white) {
    if(!strcmp(item,"⏎")) {
        const LCDColor color=white?kColorWhite:kColorBlack;
        keyboard->pd->graphics->drawLine(center_x+5,center_y-4,center_x+5,center_y+2,1,color);
        keyboard->pd->graphics->drawLine(center_x+5,center_y+2,center_x-5,center_y+2,1,color);
        keyboard->pd->graphics->drawLine(center_x-5,center_y+2,center_x-2,center_y-1,1,color);
        keyboard->pd->graphics->drawLine(center_x-5,center_y+2,center_x-2,center_y+5,1,color);
        return;
    }
    if (!strcmp(item,"⎵")) {
        const LCDColor color = white ? kColorWhite : kColorBlack;
        keyboard->pd->graphics->drawLine(center_x - 5, center_y + 3,
                                         center_x + 5, center_y + 3, 1, color);
        keyboard->pd->graphics->drawLine(center_x - 5, center_y,
                                         center_x - 5, center_y + 3, 1, color);
        keyboard->pd->graphics->drawLine(center_x + 5, center_y,
                                         center_x + 5, center_y + 3, 1, color);
        return;
    }
    draw_keyboard_text(keyboard, item,
                       center_x - keyboard_text_width(keyboard, item) / 2,
                       center_y - keyboard_text_height(keyboard,item) / 2, white);
}

// A selection seam changes cyclic coordinates rather than traversing every
// intervening glyph. Translate the existing spring shape without resetting its
// velocity. Ordinary selection changes continue through the accepted springs.
static void rebase_selection_marker(BrbElasticMarker* marker,float target,float minimum,float maximum) {
    float delta=target-marker->target;
    const float left=fminf(marker->left,marker->right),right=fmaxf(marker->left,marker->right);
    if(right-left<=maximum-minimum)
        delta=fminf(maximum-right,fmaxf(minimum-left,delta));
    marker->left+=delta;marker->right+=delta;marker->target+=delta;
}

// Boundary presentation runs beside row travel, even before its glyph is
// visible. Integrate in endpoint coordinates and clip only the final paint:
// clipping the spring itself would cancel overshoot early at viewport edges.
static void update_boundary_marker(BrbElasticMarker* marker,int group,float target,
    float minimum,float maximum,uint32_t now,bool enter,float time_scale) {
    const BrbElasticMarkerMotion configured=hello_keyboard_button_marker_motion(time_scale);
    const BrbElasticMarkerMotion* motion=&configured;
    target=fminf(maximum,fmaxf(minimum,target));
    const uint32_t elapsed=now-marker->at;
    if(enter||!marker->active||marker->tag!=group) {
        *marker=(BrbElasticMarker){.left=target,.right=target,.target=target,
            .at=now,.tag=group,.active=true,.direction=1};
        if(enter)hello_keyboard_marker_enter(marker,minimum,maximum);
        return;
    }
    const float delta=target-marker->target;
    marker->left+=delta;marker->right+=delta;marker->target=target;
    if(elapsed>250) {
        marker->left=marker->right=target;
        marker->left_velocity=marker->right_velocity=0;
    } else if(elapsed) {
        const bool left_head=marker->direction<0;
        const float seconds=(float)elapsed*0.001f;
        const float low=target-motion->maximum_tail,high=target+motion->maximum_tail;
        brb_elastic_spring_configured(&marker->left,&marker->left_velocity,target,seconds,
            left_head?motion->head_omega:motion->tail_omega,
            left_head?motion->head_damping:motion->tail_damping,low,high,true);
        brb_elastic_spring_configured(&marker->right,&marker->right_velocity,target,seconds,
            left_head?motion->tail_omega:motion->head_omega,
            left_head?motion->tail_damping:motion->head_damping,low,high,true);
    }
    marker->at=now;
}

// Use the same labels for measuring and drawing, including pending variants.
static void group_display_text(const HelloKeyboardModel* model, int group, int index,
                               const char* raw, size_t bytes, bool pending,
                               char output[HELLO_KEYBOARD_ITEM_BYTES + 1]) {
    memcpy(output, raw, bytes); output[bytes] = '\0';
    if (pending && model->group == group && model->pending != 0 && model->index == index) {
        hello_keyboard_model_pending_text(model, output, HELLO_KEYBOARD_ITEM_BYTES + 1);
    } else if (model->upper) {
        if(!strcmp(output,"⎵")){strcpy(output,"⏎");return;}
        char upper[HELLO_KEYBOARD_ITEM_BYTES + 1];
        if (brb_unicode_case(output, true, upper, sizeof(upper)) != SIZE_MAX)
            memcpy(output, upper, strlen(upper) + 1);
    }
}

static int group_item_slot(const HelloKeyboard* keyboard,int group,const char* item) {
    const bool side=group==2||group==4;
    int radius=(keyboard_text_height(keyboard,item)+5)/2;
    if(radius<7)radius=7;
    if(radius>(side?9:11))radius=side?9:11;
    const int minimum=side?18:radius*2+3;
    const int width=keyboard_text_width(keyboard,item)+8;
    return width>minimum?width:minimum;
}
typedef struct {
    int total,prefix,widest,initial,selected_offset,selected_slot;
} KeyboardGroupMetrics;
static KeyboardGroupMetrics group_metrics(const HelloKeyboard* keyboard,int group,int available,bool pending) {
    KeyboardGroupMetrics result={0};
    const HelloKeyboardModel* model=&keyboard->model;
    const char* raw=hello_keyboard_model_group_chars(model,group);
    const size_t length=strlen(raw);size_t at=0;int index=0;bool full=false;
    const unsigned initial_count=keyboard->config.visible_prefix_items[group-1];
    while(at<length) {
        const size_t bytes=brb_grapheme_next(raw+at,length-at);if(!bytes)break;
        char item[HELLO_KEYBOARD_ITEM_BYTES+1];
        group_display_text(model,group,index,raw+at,bytes,pending,item);at+=bytes;
        const int slot=group_item_slot(keyboard,group,item);
        if(index==model->index){result.selected_offset=result.total;result.selected_slot=slot;}
        if((unsigned)index<initial_count)result.initial+=slot;
        ++index;
        result.total+=slot;if(slot>result.widest)result.widest=slot;
        if(!full){if(result.prefix+slot<=available)result.prefix+=slot;else full=true;}
    }
    return result;
}
static KeyboardGroupMetrics group_base_metrics(const HelloKeyboard* keyboard,int group,int available) {
    return group_metrics(keyboard,group,available,false);
}
typedef struct { int natural, minimum; } KeyboardPairMeasure;
static int group_partner(const HelloKeyboard* keyboard,int group) {
    const int top=hello_keyboard_model_direction_group(&keyboard->model,HELLO_KB_UP);
    return group==2?4:group==4?2:group==3?top:
        (group==1||group==HELLO_KEYBOARD_EMOJI_GROUP)?3:0;
}
static KeyboardPairMeasure group_pair_measure(const HelloKeyboard* keyboard,int group) {
    const int partner=group_partner(keyboard,group);
    const KeyboardGroupMetrics own=group_base_metrics(keyboard,group,0);
    const KeyboardGroupMetrics other=partner?group_base_metrics(keyboard,partner,0):(KeyboardGroupMetrics){0};
    int natural=own.total;
    if(other.total>0&&other.total<natural)natural=other.total;
    const int widest=own.widest>other.widest?own.widest:other.widest;
    const int padding=2*HELLO_KEYBOARD_GROUP_PADDING;
    KeyboardPairMeasure result={natural+padding,widest+padding};
    if(result.natural<result.minimum)result.natural=result.minimum;
    return result;
}
static HelloKeyboardPillBounds group_bounds(const HelloKeyboard* keyboard,int group) {
    const bool side=group==2||group==4;
    const int maximum=side?
        hello_keyboard_full_row_side_bounds(HELLO_KEYBOARD_SCREEN_WIDTH,false).width:
        HELLO_KEYBOARD_VERTICAL_MAX_WIDTH;
    const int partner=group_partner(keyboard,group);
    const bool paired=partner&&hello_keyboard_model_group_length(&keyboard->model,group)==
        hello_keyboard_model_group_length(&keyboard->model,partner);
    int width;
    if(keyboard->config.visible_prefix_items[group-1]) {
        const KeyboardGroupMetrics own=group_base_metrics(keyboard,group,0);
        width=own.initial+2*HELLO_KEYBOARD_GROUP_PADDING;
        if(width>maximum)width=maximum;
    } else if(paired) {
        // Equal-count opposites retain the established shared natural/minimum
        // width, including their different measured glyph widths.
        const KeyboardPairMeasure measure=group_pair_measure(keyboard,group);
        width=measure.natural>measure.minimum?measure.natural:measure.minimum;
        if(width>maximum)width=maximum;
        if(!side&&width%2)width+=width<maximum?1:-1;
    } else {
        // Unequal or unpaired rows own their full axis budget. Snap the outer
        // pill to whole initial cells plus padding rather than empty remainder.
        const int padding=2*HELLO_KEYBOARD_GROUP_PADDING;
        const int available=maximum-padding;
        const KeyboardGroupMetrics own=group_base_metrics(keyboard,group,available);
        int content=own.total<=available?own.total:own.prefix;
        if(content<own.widest)content=own.widest;
        width=content+padding;
        if(width>maximum)width=maximum;
    }
    if(width<HELLO_KEYBOARD_SIDE_MIN_WIDTH)width=HELLO_KEYBOARD_SIDE_MIN_WIDTH;
    if(side)return hello_keyboard_full_row_side_bounds(width,group==2);
    return (HelloKeyboardPillBounds){(HELLO_KEYBOARD_SCREEN_WIDTH-width)/2,width};
}
typedef struct { int width,prefix_shift; } KeyboardGroupViewport;
static KeyboardGroupViewport group_viewport(KeyboardGroupMetrics base,int available,
    int content_width,int selected_width) {
    int width=base.prefix>0?base.prefix:available;
    const int widest=selected_width>base.widest?selected_width:base.widest;
    if(width<widest&&widest<=available)width=widest;
    if(content_width<width)width=content_width;
    return (KeyboardGroupViewport){width,base.prefix>0&&base.prefix<width?(width-base.prefix)/2:0};
}
static bool group_selection_is_visible(const HelloKeyboard* keyboard) {
    const HelloKeyboardModel* model=&keyboard->model;
    if(!model->pending||model->group<1||model->group>HELLO_KEYBOARD_GROUP_COUNT)return false;
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,model->group);
    const int available=bounds.width-2*HELLO_KEYBOARD_GROUP_PADDING;
    const KeyboardGroupMetrics base=group_base_metrics(keyboard,model->group,available);
    const KeyboardGroupMetrics content=group_metrics(keyboard,model->group,available,true);
    if(!content.selected_slot)return false;
    const KeyboardGroupViewport viewport=group_viewport(base,available,content.total,content.selected_slot);
    const HelloScrollFollow* follow=&keyboard->group_scroll[model->group];
    const float maximum=(float)(content.total+viewport.prefix_shift-viewport.width);
    const float scroll=maximum>0&&follow->initialized?fminf(maximum,fmaxf(0,follow->position)):0;
    const float left=(float)(viewport.prefix_shift+content.selected_offset)-scroll;
    return left>=-0.01f&&left+(float)content.selected_slot<=(float)viewport.width+0.01f;
}
static int group_last_visible_item(const HelloKeyboard* keyboard) {
    const HelloKeyboardModel* model=&keyboard->model;
    if(model->pending||model->group<1||model->group>HELLO_KEYBOARD_GROUP_COUNT)return -1;
    const int group=model->group;
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
    const int available=bounds.width-2*HELLO_KEYBOARD_GROUP_PADDING;
    const KeyboardGroupMetrics base=group_base_metrics(keyboard,group,available);
    const KeyboardGroupViewport viewport=group_viewport(base,available,base.total,0);
    const HelloScrollFollow* follow=&keyboard->group_scroll[group];
    const float maximum=(float)(base.total+viewport.prefix_shift-viewport.width);
    const float scroll=maximum>0&&follow->initialized?fminf(maximum,fmaxf(0,follow->position)):0;
    float left=(float)viewport.prefix_shift-scroll;
    const char* raw=hello_keyboard_model_group_chars(model,group);
    const size_t length=strlen(raw);size_t at=0;int last=-1;
    // Entry follows the row currently on screen, including an in-flight return
    // to home. Partially clipped endpoint cells are never selectable entries.
    for(int index=0;at<length&&index<HELLO_KEYBOARD_MAX_GROUP_CHARS;++index) {
        const size_t bytes=brb_grapheme_next(raw+at,length-at);
        if(!bytes)break;
        char item[HELLO_KEYBOARD_ITEM_BYTES+1];
        group_display_text(model,group,index,raw+at,bytes,false,item);at+=bytes;
        const int slot=group_item_slot(keyboard,group,item);
        if(left>=-0.01f&&left+(float)slot<=(float)viewport.width+0.01f)last=index;
        left+=(float)slot;
        if(left>(float)viewport.width+0.01f)break;
    }
    return last;
}
static bool group_has_overflow(const HelloKeyboard* keyboard,int group) {
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
    const int available=bounds.width-2*HELLO_KEYBOARD_GROUP_PADDING;
    const KeyboardGroupMetrics base=group_base_metrics(keyboard,group,available);
    const KeyboardGroupMetrics content=group_metrics(keyboard,group,available,true);
    const KeyboardGroupViewport viewport=group_viewport(base,available,content.total,content.widest);
    return content.total+viewport.prefix_shift>viewport.width;
}
static void draw_group(HelloKeyboard* keyboard, int group, int x, int y, uint32_t now) {
    if(group==HELLO_KEYBOARD_EMOJI_GROUP&&!keyboard->config.emoji_input_enabled)return;
    const HelloKeyboardModel* model = &keyboard->model;
    const int count = hello_keyboard_model_group_length(model, group);
    const bool side = group == 2 || group == 4;
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
    const int width=bounds.width;x=bounds.x;
    const int original_height=side?38:36;
    const int height=keyboard->config.group_height?keyboard->config.group_height:original_height;
    const bool active = model->group == group;
    const int selected_index=model->pending?model->index:0;
    const bool selected=active&&model->pending&&selected_index>=0&&selected_index<count;
    const bool button_selection=selected&&(model->pending_side==1||model->pending_side==2);
    const float time_scale=button_selection?model->button_navigation_scale:1.0f;
    const bool boundary_presentation=selected&&keyboard->selection_lock_crank&&
        keyboard->selection_lock_group==group&&keyboard->selection_lock_index==selected_index;
    if(!selected) {
        if(keyboard->crank_marker_rebase_group==group)keyboard->crank_marker_rebase_group=0;
        if(keyboard->crank_marker_pinned_group==group)keyboard->crank_marker_pinned_group=0;
    }
    const int vertical_offset = hello_floating_offset(active?model->group_at:now, (float)(group - 1) * 0.45f);
    const int pill_y = y + vertical_offset+(original_height-height)/2;
    if (count < 1 || count > HELLO_KEYBOARD_MAX_GROUP_CHARS) return;

    // Keep the established spacing where it fits. Otherwise give every item its
    // measured width plus breathing room, rather than squeezing or dropping it.
    float centers[HELLO_KEYBOARD_MAX_GROUP_CHARS];
    int slots[HELLO_KEYBOARD_MAX_GROUP_CHARS];
    uint16_t offsets[HELLO_KEYBOARD_MAX_GROUP_CHARS + 1];
    const char* raw = hello_keyboard_model_group_chars(model, group);
    const size_t raw_length = strlen(raw);
    size_t raw_at = 0;
    float content_width = 0;
    for (int index = 0; index < count; ++index) {
        char item[HELLO_KEYBOARD_ITEM_BYTES + 1];
        const size_t bytes = brb_grapheme_next(raw + raw_at, raw_length - raw_at);
        offsets[index] = (uint16_t)raw_at;
        raw_at += bytes;
        group_display_text(model, group, index, raw + offsets[index], bytes, true, item);
        slots[index]=group_item_slot(keyboard,group,item);
        content_width += (float)slots[index];
    }
    offsets[count] = (uint16_t)raw_at;
    const int available=width-2*HELLO_KEYBOARD_GROUP_PADDING;
    const KeyboardGroupMetrics base=group_base_metrics(keyboard,group,available);
    const KeyboardGroupViewport viewport=group_viewport(base,available,(int)content_width,
        selected?slots[selected_index]:0);
    const int viewport_width=viewport.width,prefix_shift=viewport.prefix_shift;
    const int inset=(width-viewport_width)/2;
    const int clip_right=inset+viewport_width;
    hello_pill_draw(keyboard->pd,x,pill_y,width,height,active?kColorBlack:kColorWhite,kColorBlack,2);
    content_width+=(float)prefix_shift;
    const bool overflow=content_width>viewport_width;
    float offset=(float)(inset+prefix_shift);
    for(int index=0;index<count;index++) {
        centers[index]=offset+(float)(slots[index]/2);
        offset+=(float)slots[index];
    }
    HelloScrollFollow* follow = &keyboard->group_scroll[group];
    const float previous_scroll = follow->initialized ? follow->position : 0;
    float scroll = 0;
    if (overflow) {
        const float maximum = content_width - viewport_width;
        float target = 0;
        if (selected) {
            target = centers[selected_index] - ((float)inset+(float)viewport_width/2);
            target = fminf(maximum, fmaxf(0, target));
        }
        if (!follow->initialized) {
            keyboard->group_scroll_motion[group]=(HelloKeyboardScrollMotion){0};
            hello_scroll_follow_update(follow,0,now,false);
        }
        scroll = hello_keyboard_scroll_follow_update_scaled(follow,&keyboard->group_scroll_motion[group],
            target,now,!selected||model->pending_side==2||
                (keyboard->selection_lock_crank&&keyboard->selection_lock_group==group),time_scale);
        follow->position = scroll = fminf(maximum, fmaxf(0, scroll));
    } else {
        hello_scroll_follow_update(follow,0,now,false);
        keyboard->group_scroll_motion[group]=(HelloKeyboardScrollMotion){0};
    }

    const int center_y = pill_y + height / 2;
    // Text and XOR marker share one viewport, never touching the pill border or
    // its neighbours. Full redraws prevent trails during fast retargeting.
    const int clip_inset = inset;
    keyboard->pd->graphics->setClipRect(x+clip_inset,pill_y+2,viewport_width,height-4);
    float selected_center=selected?centers[selected_index]-scroll:0;
    bool selected_pinned=false;
    // A enters from the last character: let offscreen content actually travel
    // into view on the same follow curve used to return home. Pinning this
    // glyph would visually teleport the endpoint before its row arrives.
    // A crank wrap presents its accepted endpoint for the whole boundary
    // animation, like a fitted group. Button entry/wrap still follows its row.
    const bool crank_boundary=boundary_presentation&&model->pending_side==3;
    const bool content_entry=selected&&(model->pending_side==2||
        (keyboard->selection_lock_crank&&keyboard->selection_lock_group==group&&!crank_boundary));
    if(selected && !content_entry && slots[selected_index]<=viewport_width) {
        const float left=(float)(slots[selected_index]/2);
        const float right=(float)(slots[selected_index]-slots[selected_index]/2);
        const float bounded=fminf(clip_right-right,fmaxf(clip_inset+left,selected_center));
        selected_pinned=bounded!=selected_center;selected_center=bounded;
    }
    const bool selected_visible=selected&&(!content_entry||(
        selected_center-(float)(slots[selected_index]/2)>=(float)clip_inset-0.01f&&
        selected_center+(float)(slots[selected_index]-slots[selected_index]/2)<=(float)clip_right+0.01f));
    if(!selected_visible&&!boundary_presentation&&keyboard->selection_marker.tag==group) {
        keyboard->selection_marker.active=false;
    }
    char selected_item[HELLO_KEYBOARD_ITEM_BYTES+1];
    if(selected_visible||boundary_presentation) {
        if(model->pending)hello_keyboard_model_pending_text(model,selected_item,sizeof(selected_item));
        else group_display_text(model,group,selected_index,raw+offsets[selected_index],
            (size_t)(offsets[selected_index+1]-offsets[selected_index]),false,selected_item);
    }
    for (int index = 0; index < count; ++index) {
        const bool is_selected=selected&&selected_index==index;
        const float local_center=is_selected?selected_center:centers[index]-scroll;
        // During catch-up the selected item stays at the corresponding edge.
        // Reserve its whole cell so no neighbour is drawn underneath it.
        if(selected_pinned && !is_selected &&
            fabsf(local_center-selected_center)<(float)(slots[index]+slots[selected_index])/2)continue;
        const float visible_center = (float)x + local_center;
        const int center_x = (int)lroundf(visible_center);
        // Only whole cells are drawn at the viewport edges, including while
        // easing. Never leave a cropped half-emoji as the first/last label.
        if (visible_center - (float)(slots[index]/2) < (float)(x + clip_inset) - 0.01f ||
            visible_center + (float)(slots[index]-slots[index]/2) > (float)(x + clip_right) + 0.01f) continue;
        char item[HELLO_KEYBOARD_ITEM_BYTES + 1];
        group_display_text(model, group, index, raw + offsets[index],
            (size_t)(offsets[index + 1] - offsets[index]), true, item);
        if(is_selected&&selected_visible)
            draw_centered_item(keyboard,selected_item,center_x,center_y,active);
        else draw_centered_item(keyboard, item, center_x, center_y, active);
    }
    if (selected_visible||boundary_presentation) {
        const char* item=selected_item;
        int radius = (keyboard_text_height(keyboard, item) + 5) / 2;
        if (radius < 7) radius = 7;
        if (radius > (side ? 9 : 11)) radius = side ? 9 : 11;
        if(radius>(height-5)/2)radius=(height-5)/2;
        BrbElasticMarker* marker = &keyboard->selection_marker;
        const bool seam=keyboard->crank_marker_rebase_group==group;
        const bool seam_follow=keyboard->crank_marker_pinned_group==group;
        // After a seam, the glyph stays pinned while its row catches up. Its
        // marker must share that viewport coordinate instead of being dragged
        // across the row by content-scroll compensation on subsequent frames.
        if (marker->active && marker->tag == group && !boundary_presentation && !seam && !seam_follow) {
            const float delta = previous_scroll - scroll;
            marker->left += delta; marker->right += delta; marker->target += delta;
        }
        if(seam_follow&&!selected_pinned)keyboard->crank_marker_pinned_group=0;
        int extra = keyboard_text_width(keyboard,item) + 4 - (radius * 2 + 1);
        if (extra < 0) extra = 0;
        const int maximum_extra = viewport_width - (radius * 2 + 1);
        if (extra > maximum_extra) extra = maximum_extra;
        const float minimum = (float)(clip_inset + radius + extra / 2);
        const float maximum = (float)(clip_right - radius - (extra + 1) / 2 - 1);
        if(seam&&!boundary_presentation) {
            if(marker->active&&marker->tag==group) {
                rebase_selection_marker(marker,fminf(maximum,fmaxf(minimum,selected_center)),minimum,maximum);
            }
        }
        if (marker->active && marker->tag == group && !boundary_presentation) {
            marker->left = fminf(maximum, fmaxf(minimum, marker->left));
            marker->right = fminf(maximum, fmaxf(minimum, marker->right));
            marker->target = fminf(maximum, fmaxf(minimum, marker->target));
        }
        float left_rest=marker->target,right_rest=marker->target;
        if(boundary_presentation) {
            const float endpoint=centers[selected_index]-keyboard->group_scroll_motion[group].target;
            update_boundary_marker(marker,group,endpoint,minimum,maximum,now,seam,time_scale);
            left_rest=right_rest=marker->target;
            keyboard->crank_marker_rebase_group=0;
        } else if(keyboard->crank_release_direction!=0||fabsf(keyboard->crank_remainder)>0.001f) {
            const HelloKeyboardCrankDetent detent=crank_detent(keyboard);
            const int direction=detent.remainder<0?-1:1;
            const bool wraps=model->pending && (direction>0?selected_index==count-1:selected_index==0);
            const float pull=hello_keyboard_crank_preload(&detent,wraps)*fminf(6.0f,(float)slots[selected_index]*0.3f);
            hello_keyboard_crank_marker_update(marker,group,selected_center,pull,minimum,maximum,now);
            left_rest=fminf(maximum,fmaxf(minimum,marker->target+fminf(0,pull)));
            right_rest=fminf(maximum,fmaxf(minimum,marker->target+fmaxf(0,pull)));
        } else {
            if(button_selection)
                hello_keyboard_button_marker_update(marker,group,selected_center,minimum,maximum,now,time_scale);
            else hello_keyboard_marker_update(marker,group,selected_center,0,minimum,maximum,now);
            left_rest=right_rest=marker->target;
        }
        if(seam&&!boundary_presentation) {
            // A cyclic endpoint can spend the whole row-follow offscreen. Apply
            // its arrival after initialization, exactly once when visible; an
            // inactive old marker must not swallow this bounded inward stretch.
            hello_keyboard_marker_enter(marker,minimum,maximum);
            keyboard->crank_marker_rebase_group=0;
        }
        int left = (int)lroundf(fminf(selected_center,fminf(marker->left, marker->right))) - radius - extra / 2;
        int right = (int)lroundf(fmaxf(selected_center,fmaxf(marker->left, marker->right))) + radius + (extra + 1) / 2;
        if (left < clip_inset) left = clip_inset;
        if (right >= clip_right) right = clip_right - 1;
        if (selected_visible&&right >= left) hello_pill_fill(keyboard->pd, x + left, pill_y + 2,
            right - left + 1, height - 4, kColorXOR);
        if(selected_visible&&keyboard->selection_lock_group==group&&keyboard->selection_lock_index==selected_index&&
           !selected_pinned&&keyboard->group_scroll_motion[group].remaining_ms==0&&
           fabsf(follow->position-keyboard->group_scroll_motion[group].target)<0.04f&&
           marker->tag==group&&brb_elastic_marker_settled(marker,left_rest,right_rest))
            reset_selection_lock(keyboard);
    }
    keyboard->pd->graphics->clearClipRect();
}

static void draw_input_icon(HelloKeyboard* keyboard, HelloInputIcon icon) {
    if (icon == HELLO_INPUT_ICON_NONE) return;
    PlaydateAPI* pd = keyboard->pd;
    pd->graphics->drawEllipse(360, 39, 23, 14, 2,
                              0.0f, 360.0f, kColorWhite);
    pd->graphics->fillEllipse(369, 43, 5, 6,
                              0.0f, 360.0f, kColorWhite);
    if (icon == HELLO_INPUT_ICON_EYE_OFF)
        pd->graphics->drawLine(357, 37, 384, 55, 2, kColorWhite);
}

static void draw_title(HelloKeyboard* keyboard, uint32_t now) {
    const bool show_error = keyboard->error_storage[0] != '\0' &&
        (int32_t)(keyboard->error_until - now) > 0;
    const char* title = show_error ? keyboard->error_storage : keyboard->config.title;
    if (title == NULL || title[0] == '\0') return;
    const int width = keyboard_text_width(keyboard, title);
    const int title_x = 23;
    const int available = 353;
    const int y = (27 - keyboard_font_size(keyboard)) / 2;
    keyboard->pd->graphics->setClipRect(title_x, 0, available, 27);
    if (width <= available) {
        draw_keyboard_text(keyboard, title, title_x, y, false);
    } else {
        const int stride = width + 40;
        const uint32_t animation_time = show_error
            ? now - keyboard->error_started : now - keyboard->title_started_at;
        const int offset = (int)((animation_time / 40) % (uint32_t)stride);
        draw_keyboard_text(keyboard, title, title_x - offset, y, false);
        draw_keyboard_text(keyboard, title, title_x - offset + stride, y, false);
    }
    keyboard->pd->graphics->clearClipRect();
}

static void draw_input_field(HelloKeyboard* keyboard,uint32_t now) {
    PlaydateAPI* pd=keyboard->pd;
    const HelloKeyboardModel* model=&keyboard->model;
    char* raw=keyboard->preview.raw;
    const size_t length = strlen(model->text);
    memcpy(raw, model->text, model->cursor);
    size_t raw_cursor = model->cursor;
    if(model->pending!=0) {
        char pending[HELLO_KEYBOARD_ITEM_BYTES+1];
        hello_keyboard_model_pending_text(model,pending,sizeof(pending));
        const char* value=!strcmp(pending,"⏎")?"":!strcmp(pending,"⎵")?" ":pending;
        const size_t bytes=strlen(value);
        memcpy(raw+raw_cursor,value,bytes);raw_cursor+=bytes;
    }
    memcpy(raw+raw_cursor,model->text+model->cursor,length-model->cursor+1);
    raw_cursor=brb_grapheme_ceil(raw,raw_cursor);
    char* shown=keyboard->preview.shown;size_t shown_cursor=0,shown_length=0;
    const size_t raw_length=strlen(raw);
    for(size_t offset=0;offset<raw_length;) {
        if(offset==raw_cursor)shown_cursor=shown_length;
        const size_t bytes=brb_grapheme_next(raw+offset,raw_length-offset);
        if(!bytes)break;
        if(hello_input_field_is_masked(&keyboard->input_field))shown[shown_length++]='*';
        else {memcpy(shown+shown_length,raw+offset,bytes);shown_length+=bytes;}
        offset+=bytes;
        if(offset==raw_length&&raw_cursor==offset)shown_cursor=shown_length;
    }
    shown[shown_length]=0;
    const HelloInputIcon input_icon = hello_input_field_icon(&keyboard->input_field);
    const bool has_input_icon = input_icon != HELLO_INPUT_ICON_NONE;
    const int prefix_width=keyboard_text_width_n(keyboard,shown,shown_cursor);
    const bool next = shown[shown_cursor]!=0;
    const size_t next_bytes=next?brb_grapheme_next(shown+shown_cursor,shown_length-shown_cursor):0;
    const int glyph_width=next?keyboard_text_width_n(keyboard,shown+shown_cursor,next_bytes):0;
    int cursor_width=glyph_width;
    int cursor_position=prefix_width;
    if(next) {
        if(!keyboard->config.font) {
            // Bitmap widths omit the final tracking pixel; the next glyph
            // starts after it, while the cursor leaves that gap untouched.
            if(shown_cursor)cursor_position++;
        } else {
            // Contextual prefix widths retain native kerning and Unicode-run
            // advances. Stop before the following glyph's actual origin.
            const size_t through=shown_cursor+next_bytes;
            cursor_position=keyboard_text_width_n(keyboard,shown,through)-glyph_width;
            if(through<shown_length) {
                const size_t following=brb_grapheme_next(shown+through,shown_length-through);
                const int next_start=keyboard_text_width_n(keyboard,shown,through+following)-
                    keyboard_text_width_n(keyboard,shown+through,following);
                if(next_start-cursor_position<cursor_width)cursor_width=next_start-cursor_position;
            }
        }
        if(cursor_width<1)cursor_width=1;
    } else {
        cursor_position+=HELLO_KEYBOARD_CURSOR_GAP;
        cursor_width=keyboard_font_size(keyboard)*3/5;
        if(cursor_width<6)cursor_width=6;
    }
    const int cursor_limit = (has_input_icon ? 352 : 376)-23-cursor_width-2;
    const int offset = cursor_position > cursor_limit ? cursor_position - cursor_limit : 0;
    const int rendered_height=keyboard_text_height(keyboard,shown);
    const int text_y = 45 - rendered_height / 2;
    int cursor_height = rendered_height + 3;
    if (cursor_height < 12) cursor_height = 12;
    if (cursor_height > 27) cursor_height = 27;
    const int cursor_y = 45 - cursor_height / 2;
    pd->graphics->setClipRect(22, 32, has_input_icon ? 330 : 354, 27);
    const int x = 23 + cursor_position - offset;
    const bool cursor_visible=hello_keyboard_cursor_blink_visible(&keyboard->cursor_blink,now,0);
    draw_keyboard_text(keyboard,shown,23-offset,text_y,true);
    if (cursor_visible) {
        pd->graphics->fillRect(x, cursor_y, cursor_width, cursor_height, kColorXOR);
    }
    pd->graphics->clearClipRect();
    draw_input_icon(keyboard, input_icon);
}

// Library-owned footer geometry keeps native-size controls on one shared row;
// caption configuration changes text, never the physical control positions.
enum {
    HELLO_KEYBOARD_FOOTER_TOP=HELLO_POPUP_FOOTER_TOP,
    HELLO_KEYBOARD_FOOTER_ICON_Y=HELLO_POPUP_FOOTER_ICON_Y,
    HELLO_KEYBOARD_FOOTER_ICON_SIZE=HELLO_POPUP_FOOTER_ICON_SIZE,
    HELLO_KEYBOARD_FOOTER_BORDER=2,
    HELLO_KEYBOARD_FOOTER_DPAD_ARM=8,
    HELLO_KEYBOARD_FOOTER_GAP=HELLO_POPUP_FOOTER_GAP,
    HELLO_KEYBOARD_FOOTER_LEFT=HELLO_POPUP_FOOTER_LEFT,
    HELLO_KEYBOARD_FOOTER_LEFT_END=185,
    HELLO_KEYBOARD_FOOTER_RIGHT=215,
    HELLO_KEYBOARD_FOOTER_RIGHT_END=HELLO_POPUP_FOOTER_RIGHT
};


static void draw_dpad_light_arm(HelloKeyboard* keyboard,HelloKeyboardDpadState state,int x,int y) {
    PlaydateAPI* pd=keyboard->pd;
    const int size=HELLO_KEYBOARD_FOOTER_ICON_SIZE,border=HELLO_KEYBOARD_FOOTER_BORDER;
    const int arm=HELLO_KEYBOARD_FOOTER_DPAD_ARM,inset=(size-arm)/2,inner_arm=arm-2*border;
    if(state==HELLO_KEYBOARD_DPAD_UP)
        pd->graphics->fillRect(x+inset+border,y+border,inner_arm,inset,kColorWhite);
    else if(state==HELLO_KEYBOARD_DPAD_RIGHT)
        pd->graphics->fillRect(x+inset+arm-border,y+inset+border,inset,inner_arm,kColorWhite);
    else if(state==HELLO_KEYBOARD_DPAD_DOWN)
        pd->graphics->fillRect(x+inset+border,y+inset+arm-border,inner_arm,inset,kColorWhite);
    else if(state==HELLO_KEYBOARD_DPAD_LEFT)
        pd->graphics->fillRect(x+border,y+inset+border,inset,inner_arm,kColorWhite);
}

void hello_keyboard_draw_dpad(HelloKeyboard* keyboard,HelloKeyboardDpadState state,int x,int y) {
    PlaydateAPI* pd=keyboard->pd;
    const int size=HELLO_KEYBOARD_FOOTER_ICON_SIZE,border=HELLO_KEYBOARD_FOOTER_BORDER;
    const int arm=HELLO_KEYBOARD_FOOTER_DPAD_ARM,inset=(size-arm)/2;
    const int inner_arm=arm-2*border,inner_size=size-2*border;
    // Union the stepped outer arms first. All-light uses one continuous inset
    // union, so there are no outline seams crossing through its center.
    pd->graphics->fillRect(x+inset+1,y,arm-2,size,kColorBlack);
    pd->graphics->fillRect(x+inset,y+1,arm,size-2,kColorBlack);
    pd->graphics->fillRect(x+1,y+inset,size-2,arm,kColorBlack);
    pd->graphics->fillRect(x,y+inset+1,size,arm-2,kColorBlack);
    if(state==HELLO_KEYBOARD_DPAD_ALL_LIGHT) {
        pd->graphics->fillRect(x+inset+border,y+border,inner_arm,inner_size,kColorWhite);
        pd->graphics->fillRect(x+border,y+inset+border,inner_size,inner_arm,kColorWhite);
    } else draw_dpad_light_arm(keyboard,state,x,y);
}

static void draw_footer_dpad_at(HelloKeyboard* keyboard,int x) {
    const int y=HELLO_KEYBOARD_FOOTER_ICON_Y;
    PDButtons held=keyboard->footer_dpad_buttons;
    if(keyboard->model.group==0&&hello_keyboard_model_is_choosing_layer(&keyboard->model)&&
       hello_keyboard_model_layer_count(&keyboard->model)==1)held&=~(kButtonUp|kButtonDown);
    hello_keyboard_draw_dpad(keyboard,HELLO_KEYBOARD_DPAD_BLACK_NEUTRAL,x,y);
    // Each physical direction is independent; releasing every direction leaves
    // a black cross even while a selection or navigation mode remains active.
    if(held&kButtonUp)draw_dpad_light_arm(keyboard,HELLO_KEYBOARD_DPAD_UP,x,y);
    if(held&kButtonRight)draw_dpad_light_arm(keyboard,HELLO_KEYBOARD_DPAD_RIGHT,x,y);
    if(held&kButtonDown)draw_dpad_light_arm(keyboard,HELLO_KEYBOARD_DPAD_DOWN,x,y);
    if(held&kButtonLeft)draw_dpad_light_arm(keyboard,HELLO_KEYBOARD_DPAD_LEFT,x,y);
}


static int footer_text_y(const HelloKeyboard* keyboard,const char* text) {
    return HELLO_KEYBOARD_FOOTER_ICON_Y+
        (HELLO_KEYBOARD_FOOTER_ICON_SIZE-keyboard_text_height(keyboard,text))/2;
}

// Captions are always dynamic. Each side owns its clip so caller-supplied text
// cannot cover the central D-pad or change any physical-control geometry.
static void draw_footer_caption(HelloKeyboard* keyboard,const char* text,int x,int width) {
    if(!text[0]||width<=0)return;
    keyboard->pd->graphics->setClipRect(x,HELLO_KEYBOARD_FOOTER_TOP+1,width,28);
    draw_keyboard_text(keyboard,text,x,footer_text_y(keyboard,text),false);
    keyboard->pd->graphics->clearClipRect();
}

void hello_keyboard_draw_footer_indicator(HelloKeyboard* keyboard,char button,int fill,int x,int y) {
    if(!keyboard||(button!='A'&&button!='B'))return;
    if(fill<0)fill=0;
    if(fill>21)fill=21;
    PlaydateAPI* pd=keyboard->pd;
    const int size=HELLO_KEYBOARD_FOOTER_ICON_SIZE;
    const int border=HELLO_KEYBOARD_FOOTER_BORDER,inner_size=size-2*border;
    pd->graphics->fillEllipse(x,y,size,size,0,360,kColorBlack);
    // Hold progress and accepted-tap feedback share the existing 0..21 value.
    // Fill the interior from bottom to top; the outer black border stays intact.
    if(fill>0) {
        const int filled_height=(inner_size*fill+20)/21;
        pd->graphics->setClipRect(x+border,y+border+inner_size-filled_height,inner_size,filled_height);
        pd->graphics->fillEllipse(x+border,y+border,inner_size,inner_size,0,360,kColorWhite);
        pd->graphics->clearClipRect();
    }
    const char glyph[2]={button,0};
    const int glyph_x=x+(size-keyboard_text_width(keyboard,glyph))/2;
    const int glyph_y=y+(size-keyboard_text_height(keyboard,glyph))/2;
    pd->graphics->setClipRect(x+border,y+border,inner_size,inner_size);
    // The physical letter is ASCII in either font path. NXOR flips only its
    // black ink, keeping it white on black and black on the advancing white fill.
    pd->graphics->setDrawMode(kDrawModeNXOR);
    if(keyboard->config.font) {
        pd->graphics->setFont(keyboard->config.font);
        pd->graphics->drawText(glyph,1,kUTF8Encoding,glyph_x,glyph_y);
    } else draw_image(keyboard,button=='A'?"glyph-65":"glyph-66",glyph_x,glyph_y);
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->clearClipRect();
}

static void draw_footer_button(HelloKeyboard* keyboard,char button,int fill,int x) {
    hello_keyboard_draw_footer_indicator(keyboard,button,fill,x,HELLO_KEYBOARD_FOOTER_ICON_Y);
}

static void draw_idle_footer_labels(HelloKeyboard* keyboard) {
    const HelloKeyboardLabels* labels=&keyboard->config.labels;
    const int x=HELLO_KEYBOARD_FOOTER_LEFT+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP;
    draw_footer_caption(keyboard,labels->delete_text,x,HELLO_KEYBOARD_FOOTER_LEFT_END-x);
}

static void draw_footer_dpad_caption(HelloKeyboard* keyboard,const char* caption) {
    const int text_x=HELLO_KEYBOARD_FOOTER_LEFT+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP;
    draw_footer_dpad_at(keyboard,HELLO_KEYBOARD_FOOTER_LEFT);
    draw_footer_caption(keyboard,caption,text_x,HELLO_KEYBOARD_FOOTER_LEFT_END-text_x);
}

static const char* held_navigation_caption(const HelloKeyboard* keyboard) {
    const HelloKeyboardLabels* labels=&keyboard->config.labels;
    const char* caption=labels->navigation_text;
    if(strcmp(caption,"Cursor / Layer"))return caption;
    const bool horizontal=(keyboard->footer_dpad_buttons&(kButtonLeft|kButtonRight))!=0;
    const bool vertical=(keyboard->footer_dpad_buttons&(kButtonUp|kButtonDown))!=0;
    if(hello_keyboard_model_layer_count(&keyboard->model)==1||(horizontal&&!vertical))return "Cursor";
    return vertical&&!horizontal?labels->layer_text:caption;
}

static void draw_picker_footer(HelloKeyboard* keyboard,uint32_t now) {
    const bool choosing=hello_keyboard_model_is_choosing_layer(&keyboard->model);
    const HelloKeyboardLabels* labels=&keyboard->config.labels;
    draw_footer_dpad_caption(keyboard,choosing?held_navigation_caption(keyboard):labels->picker_select);
    char generated[HELLO_KEYBOARD_MAX_LABEL+1];
    const char* caption=labels->use_set;
    if(choosing) {
        hello_keyboard_layer_caption(generated,sizeof(generated),labels->layer_text,
            hello_keyboard_model_layer(&keyboard->model),hello_keyboard_model_layer_count(&keyboard->model));
        caption=generated;
    }
    const int maximum=HELLO_KEYBOARD_FOOTER_RIGHT_END-HELLO_KEYBOARD_FOOTER_RIGHT-
        HELLO_KEYBOARD_FOOTER_ICON_SIZE-HELLO_KEYBOARD_FOOTER_GAP;
    const int natural=keyboard_text_width(keyboard,caption),width=natural<maximum?natural:maximum;
    const int x=HELLO_KEYBOARD_FOOTER_RIGHT_END-width-
        (width?HELLO_KEYBOARD_FOOTER_GAP:0)-HELLO_KEYBOARD_FOOTER_ICON_SIZE;
    draw_footer_button(keyboard,'A',hold_fill(keyboard,HELLO_KB_BUTTON_A,now),x);
    draw_footer_caption(keyboard,caption,x+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP,width);
}

static void draw_footer(HelloKeyboard* keyboard,uint32_t now) {
    PlaydateAPI* pd=keyboard->pd;
    const HelloKeyboardLabels* labels=&keyboard->config.labels;
    pd->graphics->fillRect(0,HELLO_KEYBOARD_FOOTER_TOP,400,29,kColorWhite);
    pd->graphics->drawLine(3,HELLO_KEYBOARD_FOOTER_TOP,396,HELLO_KEYBOARD_FOOTER_TOP,1,kColorBlack);
    if(keyboard->model.view==HELLO_KB_VIEW_EMOJI_SETS) {
        draw_picker_footer(keyboard,now);
        return;
    }
    if(keyboard->model.group!=0) {
        draw_footer_dpad_caption(keyboard,labels->select_text);
        // The same clear gap separates both physical controls and their caption.
        const int pair_width=HELLO_KEYBOARD_FOOTER_ICON_SIZE*2+HELLO_KEYBOARD_FOOTER_GAP;
        const int max_label=HELLO_KEYBOARD_FOOTER_RIGHT_END-HELLO_KEYBOARD_FOOTER_RIGHT-pair_width-HELLO_KEYBOARD_FOOTER_GAP;
        const int width=(int)fminf((float)max_label,(float)keyboard_text_width(keyboard,labels->selection_text));
        const int icon_x=HELLO_KEYBOARD_FOOTER_RIGHT_END-width-HELLO_KEYBOARD_FOOTER_GAP-pair_width;
        draw_footer_button(keyboard,'A',hold_fill(keyboard,HELLO_KB_BUTTON_A,now),icon_x);
        draw_footer_button(keyboard,'B',hold_fill(keyboard,HELLO_KB_BUTTON_B,now),
            icon_x+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP);
        draw_footer_caption(keyboard,labels->selection_text,icon_x+pair_width+HELLO_KEYBOARD_FOOTER_GAP,width);
    } else {
        const bool choosing=hello_keyboard_model_is_choosing_layer(&keyboard->model);
        if(choosing) {
            draw_footer_dpad_caption(keyboard,held_navigation_caption(keyboard));
        }
        else {
            draw_footer_button(keyboard,'B',hold_fill(keyboard,HELLO_KB_BUTTON_B,now),HELLO_KEYBOARD_FOOTER_LEFT);
            draw_idle_footer_labels(keyboard);
        }
        const char* right=keyboard->model.upper?labels->hold_upper:labels->hold_lower;
        char layer_caption[HELLO_KEYBOARD_MAX_LABEL+1];
        if(choosing&&keyboard->model.view!=HELLO_KB_VIEW_KEYBOARD&&hello_keyboard_model_layer_count(&keyboard->model)>1) {
            hello_keyboard_layer_caption(layer_caption,sizeof(layer_caption),labels->layer_text,
                hello_keyboard_model_layer(&keyboard->model),hello_keyboard_model_layer_count(&keyboard->model));
            right=layer_caption;
        }
        const int max_label=HELLO_KEYBOARD_FOOTER_RIGHT_END-HELLO_KEYBOARD_FOOTER_RIGHT-
            HELLO_KEYBOARD_FOOTER_ICON_SIZE-HELLO_KEYBOARD_FOOTER_GAP;
        const int width=(int)fminf((float)max_label,(float)keyboard_text_width(keyboard,right));
        const int icon_x=HELLO_KEYBOARD_FOOTER_RIGHT_END-width-
            (width?HELLO_KEYBOARD_FOOTER_GAP:0)-HELLO_KEYBOARD_FOOTER_ICON_SIZE;
        draw_footer_button(keyboard,'A',hold_fill(keyboard,HELLO_KB_BUTTON_A,now),icon_x);
        draw_footer_caption(keyboard,right,icon_x+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP,width);
        if(!keyboard->footer_buttons&&labels->escape_text[0]) {
            const int delete_max=HELLO_KEYBOARD_FOOTER_LEFT_END-HELLO_KEYBOARD_FOOTER_LEFT-
                HELLO_KEYBOARD_FOOTER_ICON_SIZE-HELLO_KEYBOARD_FOOTER_GAP;
            const int delete_width=keyboard_text_width(keyboard,labels->delete_text);
            const int left=HELLO_KEYBOARD_FOOTER_LEFT+HELLO_KEYBOARD_FOOTER_ICON_SIZE+
                2*HELLO_KEYBOARD_FOOTER_GAP+(delete_width<delete_max?delete_width:delete_max);
            const int available=icon_x-HELLO_KEYBOARD_FOOTER_GAP-left;
            if(available>0) {
                const int escape_width=keyboard_text_width(keyboard,labels->escape_text);
                pd->graphics->setClipRect(left,HELLO_KEYBOARD_FOOTER_TOP+1,available,28);
                draw_keyboard_text(keyboard,labels->escape_text,(400-escape_width)/2,
                    footer_text_y(keyboard,labels->escape_text),false);
                pd->graphics->clearClipRect();
            }
        }
    }
}

static void draw_emoji_picker(HelloKeyboard* keyboard) {
    enum { ROWS=5,ROW_HEIGHT=22,ROW_TOP=96 };
    const char* title=keyboard->config.labels.picker_title;
    keyboard->pd->graphics->setClipRect(8,68,384,24);
    draw_keyboard_text(keyboard,title,(400-keyboard_text_width(keyboard,title))/2,70,false);
    keyboard->pd->graphics->clearClipRect();
    const size_t first=keyboard->emoji_picker_index>=ROWS?keyboard->emoji_picker_index-(ROWS-1):0;
    for(size_t row=0;row<ROWS&&row<keyboard->config.emoji_set_count-first;++row) {
        const size_t index=first+row;
        const BrbEmojiSet* set=keyboard->config.emoji_sets[index];
        if(!set||!set->id)continue;
        const bool selected=index==keyboard->emoji_picker_index;
        const int y=ROW_TOP+(int)row*ROW_HEIGHT;
        hello_pill_draw(keyboard->pd,8,y,384,ROW_HEIGHT,selected?kColorBlack:kColorWhite,kColorBlack,1);
        unsigned previews=0;int preview_width=0;
        while(set->glyphs&&previews<3&&previews<set->count) {
            const BrbEmojiGlyph* glyph=&set->glyphs[previews];
            if(!brb_emoji_bitmap(set,glyph))break;
            const int next=preview_width+(previews?4:0)+glyph->width;
            if(next>96)break;
            preview_width=next;++previews;
        }
        char name[HELLO_KEYBOARD_MAX_LABEL+1];
        const char* real_name=set==brb_emoji_everyday_set()?"Everyday":set->id;
        if(!hello_keyboard_label_copy(name,real_name))name[0]=0;
        keyboard->pd->graphics->setClipRect(20,y+1,348-preview_width,ROW_HEIGHT-2);
        draw_keyboard_text(keyboard,name,20,y+(ROW_HEIGHT-keyboard_text_height(keyboard,name))/2,selected);
        keyboard->pd->graphics->clearClipRect();
        int x=380-preview_width;
        for(unsigned glyph=0;glyph<previews;glyph++) {
            brb_emoji_glyph_draw(keyboard->pd,set,&set->glyphs[glyph],x,
                y+(ROW_HEIGHT-set->cell_height)/2,selected);
            x+=set->glyphs[glyph].width+4;
        }
    }
}

static void draw_cancel_prompt(HelloKeyboard* keyboard,uint32_t now) {
    PlaydateAPI* pd=keyboard->pd;
    const HelloKeyboardLabels* labels=&keyboard->config.labels;
    const char* question=labels->cancel_question;
    int line_height=keyboard_text_height(keyboard,"A");
    if(keyboard->config.font&&pd->graphics->getFontHeight) {
        const int native_height=pd->graphics->getFontHeight(keyboard->config.font);
        if(native_height>0)line_height=native_height;
    }
    const int padding=keyboard->config.group_height>line_height?
        (keyboard->config.group_height-line_height)/2:HELLO_POPUP_PADDING;
    const HelloPopupBounds popup=hello_popup_layout(now,line_height,padding,true);
    hello_popup_draw_shell(pd,popup);
    const int viewport=popup.width-2*HELLO_POPUP_TEXT_INSET;
    const int question_width=keyboard_text_width(keyboard,question);
    const int text_y=popup.y+(popup.height-line_height)/2;
    pd->graphics->setClipRect(popup.x+HELLO_POPUP_TEXT_INSET,popup.y+padding,viewport,popup.height-2*padding);
    if(question_width<=viewport)
        draw_keyboard_text(keyboard,question,popup.x+(popup.width-question_width)/2,text_y,false);
    else {
        const int offset=hello_popup_marquee_offset(now,keyboard->cancel_dialog_started_at,question_width,viewport);
        draw_keyboard_text(keyboard,question,popup.x+HELLO_POPUP_TEXT_INSET-offset,text_y,false);
        draw_keyboard_text(keyboard,question,popup.x+HELLO_POPUP_TEXT_INSET-offset+question_width+32,text_y,false);
    }
    pd->graphics->clearClipRect();
    hello_popup_draw_footer(pd);
    for(int button=0;button<2;button++) {
        const bool yes=button==HELLO_KB_BUTTON_A;
        const char* label=yes?labels->save:labels->discard;
        const int maximum=yes?HELLO_KEYBOARD_FOOTER_RIGHT_END-HELLO_KEYBOARD_FOOTER_RIGHT-
            HELLO_KEYBOARD_FOOTER_ICON_SIZE-HELLO_KEYBOARD_FOOTER_GAP:
            HELLO_KEYBOARD_FOOTER_LEFT_END-HELLO_KEYBOARD_FOOTER_LEFT-
            HELLO_KEYBOARD_FOOTER_ICON_SIZE-HELLO_KEYBOARD_FOOTER_GAP;
        const int natural=keyboard_text_width(keyboard,label),width=natural<maximum?natural:maximum;
        const int icon_x=yes?HELLO_KEYBOARD_FOOTER_RIGHT_END-width-
            HELLO_KEYBOARD_FOOTER_GAP-HELLO_KEYBOARD_FOOTER_ICON_SIZE:HELLO_KEYBOARD_FOOTER_LEFT;
        draw_footer_button(keyboard,yes?'A':'B',0,icon_x);
        draw_footer_caption(keyboard,label,icon_x+HELLO_KEYBOARD_FOOTER_ICON_SIZE+HELLO_KEYBOARD_FOOTER_GAP,width);
    }
}

void hello_keyboard_draw(HelloKeyboard* keyboard, uint32_t now) {
    if (!keyboard->visible) return;
    sync_emoji_renderer(keyboard);
    PlaydateAPI* pd = keyboard->pd;
    const HelloKeyboardModel* model = &keyboard->model;
    if (model->pending==0||model->group==0)
        keyboard->selection_marker.active=false;
    pd->graphics->fillRect(0,0,400,240,kColorWhite);
    draw_title(keyboard, now);
    const int input_top=27,input_height=37;
    hello_pill_fill(pd,9,input_top,382,input_height,kColorBlack);

    // Center the whole group block in the free region, preserving each row's
    // nominal height, internal spacing and independent floating phase.
    const int center=(input_top+input_height+HELLO_KEYBOARD_FOOTER_TOP)/2;
    const int row_spacing=44,vertical_half=36/2,side_half=38/2;

    if(model->view==HELLO_KB_VIEW_EMOJI_SETS)draw_emoji_picker(keyboard);
    else {
        draw_group(keyboard,hello_keyboard_model_direction_group(model,HELLO_KB_UP),105,center-row_spacing-vertical_half,now);
        draw_group(keyboard,3,105,center+row_spacing-vertical_half,now);
        draw_group(keyboard,2,236,center-side_half,now);
        draw_group(keyboard,4,8,center-side_half,now);
    }

    if(!keyboard->cancel_dialog_active)draw_footer(keyboard,now);

    draw_input_field(keyboard,now);
    if(keyboard->cancel_dialog_active)draw_cancel_prompt(keyboard,now);
}

const char* hello_keyboard_text(const HelloKeyboard* keyboard) {
    return keyboard->model.text;
}
