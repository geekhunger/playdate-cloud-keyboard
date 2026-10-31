#include "../src/keyboard.c"
#include <assert.h>

static HelloKeyboard keyboard;
static void prepare(void) {
    memset(&keyboard,0,sizeof(keyboard));
    hello_keyboard_default_config(&keyboard.config);
    hello_keyboard_model_init(&keyboard.model,"secret",6);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_MASKEDTEXT);
    keyboard.visible=true;
}
static void test_direction_release_update_order(void) {
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonLeft|kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1150,kButtonLeft,0,kButtonA,0);
    hello_keyboard_update(&keyboard,31100,kButtonLeft,0,0,0);
    assert(keyboard.model.pending=='e'&&keyboard.model.group==4&&!keyboard.model.text[0]);
    hello_keyboard_update(&keyboard,31101,kButtonLeft,0,kButtonRight,0);
    assert(keyboard.model.pending=='e'&&!keyboard.model.text[0]);
    hello_keyboard_update(&keyboard,31102,0,0,kButtonLeft,0);
    assert(!strcmp(keyboard.model.text,"e")&&!keyboard.model.pending&&!keyboard.model.group);
    hello_keyboard_update(&keyboard,31103,0,0,kButtonLeft,0);
    assert(!strcmp(keyboard.model.text,"e"));

    for(unsigned elapsed=349;elapsed<=350;elapsed++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);
        hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
        hello_keyboard_update(&keyboard,1100,kButtonLeft|kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,1100+elapsed,0,0,kButtonLeft|kButtonA,0);
        assert(!strcmp(keyboard.model.text,"e"));
        assert(!keyboard.model.pending&&!keyboard.model.group&&!keyboard.model.hold[HELLO_KB_BUTTON_A].active);
    }
    // The final face press belongs to the selected group even when its D-pad
    // direction is released in this same frame; its later release adds nothing.
    for(int side=0;side<2;side++) {
        const PDButtons button=side?kButtonA:kButtonB;
        const HelloKeyboardButton model_button=side?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B;
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);
        hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
        hello_keyboard_update(&keyboard,1100,button,button,kButtonLeft,0);
        assert(!strcmp(keyboard.model.text,side?"e":"a"));
        assert(!keyboard.model.pending&&!keyboard.model.group);
        assert(!keyboard.model.hold[model_button].active);
        hello_keyboard_update(&keyboard,1150,0,0,button,0);
        assert(!strcmp(keyboard.model.text,side?"e":"a"));
        assert(!keyboard.model.upper&&!keyboard.model.alternate);
        assert(!keyboard.model.feedback_active[model_button]);
    }
    // Reopening the same group while the prior selection button remains held
    // starts cleanly: only a fresh press owns the new selection's hold clock.
    for(int side=0;side<2;side++) {
        const PDButtons old_button=side?kButtonA:kButtonB;
        const PDButtons next_button=side?kButtonB:kButtonA;
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);
        hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
        hello_keyboard_update(&keyboard,1020,kButtonLeft|old_button,old_button,0,0);
        hello_keyboard_update(&keyboard,1100,old_button,0,kButtonLeft,0);
        assert(!strcmp(keyboard.model.text,side?"e":"a"));
        hello_keyboard_update(&keyboard,1110,kButtonLeft|old_button,kButtonLeft,0,0);
        hello_keyboard_update(&keyboard,1120,kButtonLeft|next_button,next_button,old_button,0);
        hello_keyboard_update(&keyboard,1370,kButtonLeft|next_button,0,0,0);
        assert(keyboard.model.pending==(uint32_t)(side?'a':'e'));
        hello_keyboard_update(&keyboard,1380,kButtonLeft|next_button,0,old_button,0);
        assert(keyboard.model.pending==(uint32_t)(side?'a':'e'));
        hello_keyboard_update(&keyboard,1390,next_button,0,kButtonLeft,0);
        assert(!strcmp(keyboard.model.text,side?"ea":"ae"));
        hello_keyboard_update(&keyboard,1500,0,0,next_button,0);
        assert(!strcmp(keyboard.model.text,side?"ea":"ae"));
        assert(!keyboard.model.upper&&!keyboard.model.alternate);
    }

    // A final crank detent is consumed before direction release commits it.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1100,0,0,kButtonLeft,360);
    assert(keyboard.model.text[0]&&!keyboard.model.pending&&!keyboard.model.group);
    char committed[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];strcpy(committed,keyboard.model.text);
    assert(brb_grapheme_count(committed)==1);
    hello_keyboard_update(&keyboard,1150,0,0,0,0);
    assert(!strcmp(keyboard.model.text,committed));

    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1001,0,0,kButtonLeft,0);
    hello_keyboard_update(&keyboard,11001,0,0,0,0);
    assert(!keyboard.model.group&&!keyboard.model.pending);
}

static void test_idle_a_layer_chord_order(void) {
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1100,0,0,kButtonA,0);
    assert(keyboard.model.upper&&!keyboard.model.text[0]);
    hello_keyboard_update(&keyboard,1101,0,0,kButtonA,0);
    assert(keyboard.model.upper);

    // Simultaneous fresh D-pad+A keeps direction-then-selection for fast typing.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonA|kButtonLeft,kButtonA|kButtonLeft,0,0);
    assert(!hello_keyboard_model_is_choosing_layer(&keyboard.model));
    assert(keyboard.model.group==4&&keyboard.model.pending=='e');
    hello_keyboard_update(&keyboard,1001,0,0,kButtonA|kButtonLeft,0);
    assert(!strcmp(keyboard.model.text,"e")&&!keyboard.model.upper);

    // A held from the previous frame owns a chord even before 350 ms.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1001,kButtonA|kButtonUp,kButtonUp,0,0);
    assert(hello_keyboard_model_is_choosing_layer(&keyboard.model));
    assert(!keyboard.model.alternate&&!keyboard.model.group&&!keyboard.model.pending);
    hello_keyboard_update(&keyboard,1010,kButtonA,0,kButtonUp,0);
    hello_keyboard_update(&keyboard,1020,kButtonA|kButtonDown,kButtonDown,0,0);
    assert(!keyboard.model.alternate&&!keyboard.model.group);
    hello_keyboard_update(&keyboard,1030,kButtonA,0,kButtonDown,0);
    hello_keyboard_update(&keyboard,1040,0,0,kButtonA,0);
    assert(!keyboard.model.upper&&!hello_keyboard_model_is_choosing_layer(&keyboard.model));
    hello_keyboard_update(&keyboard,1056,kButtonLeft|kButtonB,kButtonLeft|kButtonB,0,0);
    assert(keyboard.model.group==4&&keyboard.model.pending=='a');
    hello_keyboard_update(&keyboard,1060,0,0,kButtonLeft|kButtonB,0);
    assert(!strcmp(keyboard.model.text,"a"));

    // Releasing A in the same snapshot as fresh Up uses the shared lower page
    // for digit-type fields; it does not enter a historical alternate page.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);keyboard.model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1350,kButtonA,0,0,0);
    assert(hello_keyboard_model_is_choosing_layer(&keyboard.model)&&!keyboard.model.upper);
    hello_keyboard_update(&keyboard,1360,kButtonA|kButtonUp,kButtonUp,0,0);
    hello_keyboard_update(&keyboard,1361,kButtonA,0,kButtonUp,0);
    hello_keyboard_update(&keyboard,1370,kButtonUp|kButtonB,kButtonUp|kButtonB,kButtonA,0);
    assert(!hello_keyboard_model_is_choosing_layer(&keyboard.model));
    assert(keyboard.model.group==1&&keyboard.model.pending=='0');
    hello_keyboard_update(&keyboard,1380,0,0,kButtonUp|kButtonB,0);
    assert(!strcmp(keyboard.model.text,"0")&&!keyboard.model.upper);

    // Left/right while A owns the chooser move the cursor without opening a group.
    prepare();hello_keyboard_model_init(&keyboard.model,"abc",3);
    hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1350,kButtonA,0,0,0);
    hello_keyboard_update(&keyboard,1360,kButtonA|kButtonLeft,kButtonLeft,0,0);
    assert(keyboard.model.cursor==2&&keyboard.model.cursor_hold);
    hello_keyboard_update(&keyboard,1370,kButtonA|kButtonRight,kButtonRight,kButtonLeft,0);
    assert(!keyboard.model.group&&!keyboard.model.pending&&keyboard.model.cursor_hold&&keyboard.model.cursor==3);
    hello_keyboard_update(&keyboard,1380,0,0,kButtonA|kButtonRight,0);
    assert(!strcmp(keyboard.model.text,"abc")&&!keyboard.model.upper&&!keyboard.model.cursor_hold);
}

static void test_chooser_cursor_release_order(void) {
    for(int layer=0;layer<2;layer++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"aé👨‍👩‍👧‍👦z",strlen("aé👨‍👩‍👧‍👦z"));
        keyboard.model.alternate=layer;
        const size_t end=keyboard.model.cursor;
        hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,1001,kButtonA|kButtonLeft,kButtonLeft,0,0);
        assert(hello_keyboard_model_is_choosing_layer(&keyboard.model));
        assert(keyboard.model.cursor==end-1&&!keyboard.model.group);
        hello_keyboard_update(&keyboard,1350,kButtonA|kButtonLeft,0,0,0);
        assert(keyboard.model.cursor==end-1);
        hello_keyboard_update(&keyboard,1351,kButtonA|kButtonLeft,0,0,0);
        assert(keyboard.model.cursor==strlen("aé"));
        // D-pad release wins over its next repeat deadline.
        hello_keyboard_update(&keyboard,1451,kButtonA,0,kButtonLeft,0);
        assert(keyboard.model.cursor==strlen("aé")&&!keyboard.model.cursor_hold);
        hello_keyboard_update(&keyboard,1500,kButtonA|kButtonRight,kButtonRight,0,0);
        assert(keyboard.model.cursor==end-1);
        // A release also stops the chooser cursor while Right remains down.
        hello_keyboard_update(&keyboard,1850,kButtonRight,0,kButtonA,0);
        assert(keyboard.model.cursor==end-1&&!keyboard.model.cursor_hold);
        assert(!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        hello_keyboard_update(&keyboard,2000,kButtonRight,0,0,0);
        assert(keyboard.model.cursor==end-1&&!keyboard.model.upper&&keyboard.model.alternate==(bool)layer);
        hello_keyboard_update(&keyboard,2001,0,0,kButtonRight,0);
        assert(!strcmp(keyboard.model.text,"aé👨‍👩‍👧‍👦z"));
    }
}

static void test_stale_idle_b_hold_selection(void) {
    for(int type=0;type<3;type++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);
        keyboard.model.upper=type==1;keyboard.model.field.type=(type==2)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
        const PDButtons direction=type==2?kButtonUp:kButtonLeft;
        const char* expected=type==2?"1":type==1?"E":"e";
        hello_keyboard_update(&keyboard,1000,kButtonB,kButtonB,0,0);
        hello_keyboard_update(&keyboard,1010,kButtonB|direction,direction,0,0);
        assert(!keyboard.model.hold[HELLO_KB_BUTTON_B].active&&!keyboard.model.pending);
        hello_keyboard_update(&keyboard,1020,kButtonB|direction,0,0,50.0f);
        assert(keyboard.model.pending==(uint32_t)expected[0]);
        hello_keyboard_update(&keyboard,1400,kButtonB,0,direction,0);
        assert(!strcmp(keyboard.model.text,expected));
        hello_keyboard_update(&keyboard,1410,0,0,kButtonB,0);
        assert(!strcmp(keyboard.model.text,expected));
    }
}

static void timing_accept_editing_save(uint32_t now) {
    assert(keyboard.cancel_dialog_active);
    const PDButtons opening=keyboard.footer_buttons&(kButtonA|kButtonB);
    assert(hello_keyboard_update(&keyboard,now,opening,0,0,0)==HELLO_KEYBOARD_NONE);
    hello_keyboard_update(&keyboard,now+1,0,0,opening,0);assert(keyboard.cancel_dialog_ready);
    assert(hello_keyboard_update(&keyboard,now+2,kButtonA,kButtonA,0,0)==HELLO_KEYBOARD_DONE);
}

static void test_selection_repeat_dispatch_and_return(void) {
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonLeft|kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1449,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='e');
    keyboard.crank_wrapped=true;keyboard.crank_remainder=10;
    hello_keyboard_update(&keyboard,1450,kButtonLeft|kButtonA,0,0,0);
    assert(keyboard.model.pending=='i'&&!keyboard.crank_wrapped&&keyboard.crank_remainder==0);
    hello_keyboard_update(&keyboard,1550,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='d');
    hello_keyboard_update(&keyboard,3000,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='g');
    // D-pad release commits that current item even at the next repeat deadline.
    hello_keyboard_update(&keyboard,3100,0,0,kButtonLeft|kButtonA,0);
    assert(!strcmp(keyboard.model.text,"g")&&!keyboard.model.pending);

    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,1000,kButtonLeft,kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonLeft|kButtonA,kButtonA,0,0);
    keyboard.selection_lock_group=4;keyboard.selection_lock_index=0;keyboard.selection_lock_crank=false;
    hello_keyboard_update(&keyboard,1450,kButtonLeft|kButtonA,0,0,0);
    hello_keyboard_update(&keyboard,5000,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='e');
    reset_selection_lock(&keyboard);
    hello_keyboard_update(&keyboard,5001,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='e');
    hello_keyboard_update(&keyboard,5100,kButtonLeft|kButtonA,0,0,0);assert(keyboard.model.pending=='i');

    // Return committed by a group change also finishes once, discarding any
    // fresh selection delivered later in that same SDK snapshot.
    prepare();hello_keyboard_model_init(&keyboard.model,"abc",3);keyboard.model.upper=true;
    hello_keyboard_update(&keyboard,1000,kButtonDown,kButtonDown,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonDown|kButtonB,kButtonB,0,0);
    assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);
    assert(hello_keyboard_update(&keyboard,1200,kButtonRight|kButtonA,kButtonRight|kButtonA,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);
    timing_accept_editing_save(5000);
    assert(!strcmp(keyboard.model.text,"abc")&&!keyboard.model.submit_requested&&!keyboard.model.pending&&!keyboard.model.group);
    assert(!keyboard.model.hold[HELLO_KB_BUTTON_A].active);

    char full[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];memset(full,'x',sizeof(full)-1);full[sizeof(full)-1]=0;
    const char* texts[]={"",full};
    for(size_t run=0;run<2;run++) {
        prepare();hello_keyboard_model_init(&keyboard.model,texts[run],strlen(texts[run]));
        keyboard.model.field.max_length=strlen(texts[run]);keyboard.model.upper=true;
        hello_keyboard_update(&keyboard,1000,kButtonDown,kButtonDown,0,0);
        hello_keyboard_update(&keyboard,1100,kButtonDown|kButtonB,kButtonB,0,0);
        assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);
        assert(hello_keyboard_update(&keyboard,1450,0,0,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);timing_accept_editing_save(1451);
        assert(!strcmp(keyboard.model.text,texts[run])&&!keyboard.model.submit_requested&&!keyboard.model.pending);
        assert(hello_keyboard_update(&keyboard,2000,0,0,0,0)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,2050,0,0,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);
        assert(!strcmp(keyboard.model.text,texts[run]));
    }
}

static void test_offscreen_entry_gate_is_button_only(void) {
    // B first selects the final complete cell currently visible, rather than
    // jumping to the far end of the overflowing group.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    hello_keyboard_update(&keyboard,1000,kButtonUp,kButtonUp,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonUp|kButtonB,kButtonB,0,0);
    assert(keyboard.model.index==9&&keyboard.model.pending=='0'&&!selection_locked(&keyboard));
    hello_keyboard_update(&keyboard,1110,kButtonUp,0,kButtonB,0);
    hello_keyboard_update(&keyboard,1200,kButtonUp|kButtonA,kButtonA,0,0);
    assert(keyboard.model.index==10&&keyboard.model.pending=='.');

    // A retains its logical first item even if a previously scrolled row has
    // not yet homed; that offscreen entry keeps its button-only gate.
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    hello_keyboard_update(&keyboard,1000,kButtonUp,kButtonUp,0,0);
    keyboard.group_scroll[1].initialized=true;keyboard.group_scroll[1].position=200;
    hello_keyboard_update(&keyboard,1100,kButtonUp|kButtonA,kButtonA,0,0);
    assert(keyboard.model.index==0&&selection_locked(&keyboard)&&!keyboard.selection_lock_crank);
    hello_keyboard_update(&keyboard,1200,kButtonUp|kButtonA,0,0,1);
    assert(keyboard.model.index==0&&selection_locked(&keyboard));
    hello_keyboard_update(&keyboard,1201,kButtonUp|kButtonA,0,0,50);
    assert(keyboard.model.index==1&&!selection_locked(&keyboard));

    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    hello_keyboard_update(&keyboard,1000,kButtonUp,kButtonUp,0,0);
    keyboard.group_scroll[1].initialized=true;keyboard.group_scroll[1].position=200;
    hello_keyboard_update(&keyboard,1100,kButtonUp|kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,1110,kButtonUp,0,kButtonA,0); // Keep this a single-button gate test.
    hello_keyboard_update(&keyboard,1200,kButtonUp|kButtonB,kButtonB,0,0);
    assert(keyboard.model.index==0&&!keyboard.model.hold[HELLO_KB_BUTTON_B].active);
    hello_keyboard_update(&keyboard,5000,kButtonUp|kButtonB,0,0,0);assert(keyboard.model.index==0);
    reset_selection_lock(&keyboard); // Isolate dispatch after the actual-render gate clears.
    hello_keyboard_update(&keyboard,5001,kButtonUp|kButtonB,0,0,0);
    assert(keyboard.model.index==39&&keyboard.model.hold[HELLO_KB_BUTTON_B].active&&keyboard.selection_lock_crank);
    hello_keyboard_update(&keyboard,5100,kButtonUp|kButtonB,0,0,0);
    assert(keyboard.model.index==39&&!keyboard.model.text[0]); // A true wrap still owns its new gate.
    hello_keyboard_update(&keyboard,5101,kButtonUp,0,kButtonB,0);
    assert(!keyboard.model.hold[HELLO_KB_BUTTON_B].active);
    hello_keyboard_update(&keyboard,5102,0,0,kButtonUp,0);
    assert(!keyboard.model.pending&&!keyboard.model.group&&!strcmp(keyboard.model.text,"("));
}

static void test_first_negative_crank_uses_visible_entry(void) {
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_keyboard_update(&keyboard,1000,kButtonUp,kButtonUp,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonUp,0,0,-0.02f);
    assert(keyboard.model.index==9&&keyboard.model.pending=='0'&&keyboard.model.pending_side==3);
    assert(!keyboard.crank_wrapped&&!selection_locked(&keyboard)&&keyboard.crank_entry_breakaway);
    assert(keyboard.crank_remainder==0);
    // Drain only the real pressure needed for the next positive detent.
    HelloKeyboardCrankDetent detent=crank_detent(&keyboard);detent.remainder=1;
    const float positive=hello_keyboard_crank_threshold(&detent,false)-keyboard.crank_remainder+0.001f;
    hello_keyboard_update(&keyboard,2100,kButtonUp,0,0,positive);
    assert(keyboard.model.index==10&&keyboard.model.pending=='.'&&!keyboard.crank_wrapped&&!selection_locked(&keyboard));
    assert(!keyboard.crank_entry_breakaway);
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_set_visible_prefix(&keyboard,1,10);
    hello_keyboard_update(&keyboard,1000,kButtonUp,kButtonUp,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonUp,0,0,0.02f);
    assert(keyboard.model.index==0&&keyboard.model.pending=='1'&&!keyboard.crank_wrapped);
    assert(keyboard.crank_entry_breakaway&&keyboard.crank_remainder==0);
    detent=crank_detent(&keyboard);detent.remainder=-1;
    const float negative=-hello_keyboard_crank_threshold(&detent,true)-keyboard.crank_remainder-0.001f;
    hello_keyboard_update(&keyboard,2100,kButtonUp,0,0,negative);
    assert(keyboard.model.index==39&&keyboard.model.pending=='('&&keyboard.crank_wrapped);
    assert(selection_locked(&keyboard)&&keyboard.selection_lock_crank);
}

static void test_done_ends_all_input_lifetimes(void) {
    prepare();hello_keyboard_model_init(&keyboard.model,"abc",3);keyboard.model.upper=true;
    hello_keyboard_update(&keyboard,1000,kButtonDown,kButtonDown,0,0);
    hello_keyboard_update(&keyboard,1100,kButtonDown|kButtonB,kButtonB,0,0);
    keyboard.requested_action=HELLO_KEYBOARD_DONE;
    assert(hello_keyboard_update(&keyboard,1200,kButtonLeft|kButtonA,kButtonLeft|kButtonA,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);
    assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"abc"));
    timing_accept_editing_save(1201);
    assert(keyboard.visible&&!keyboard.model.hold[0].active&&!keyboard.model.hold[1].active);
    assert(!keyboard.model.cursor_hold&&!keyboard.model.feedback_active[0]&&!keyboard.model.feedback_active[1]);
    assert(!keyboard.model.group&&!keyboard.model.pending&&!keyboard.model.submit_requested&&keyboard.model.upper);
    hello_keyboard_update(&keyboard,1600,kButtonA,0,0,0);assert(!strcmp(keyboard.model.text,"abc"));
    hello_keyboard_update(&keyboard,1601,0,0,kButtonA,0);
    hello_keyboard_update(&keyboard,1610,kButtonLeft|kButtonA,kButtonLeft|kButtonA,0,0);
    hello_keyboard_update(&keyboard,1620,0,0,kButtonLeft|kButtonA,0);assert(!strcmp(keyboard.model.text,"abcE"));

    prepare();hello_keyboard_model_init(&keyboard.model,"abcdef",6);
    hello_keyboard_update(&keyboard,1000,kButtonB,kButtonB,0,0);
    keyboard.requested_action=HELLO_KEYBOARD_DONE;
    assert(hello_keyboard_update(&keyboard,1100,kButtonB,0,0,0)==HELLO_KEYBOARD_NONE);
    assert(keyboard.cancel_dialog_active);
    hello_keyboard_update(&keyboard,5000,kButtonB,0,0,0);assert(!strcmp(keyboard.model.text,"abcde"));
    timing_accept_editing_save(5001);
    assert(!keyboard.model.hold[HELLO_KB_BUTTON_B].active&&!strcmp(keyboard.model.text,"abcde"));
}

static void test_editing_decision_discard_and_fresh_save(void) {
    for(int type=0;type<3;type++)for(int rollover=0;rollover<2;rollover++) {
        const uint32_t start=rollover?UINT32_MAX-100:1000;
        const char* original=type==HELLO_INPUT_CONTACT_NUMBER?"12345":"original";
        const char* edited=type==HELLO_INPUT_CONTACT_NUMBER?"6789":"edited";
        prepare();hello_keyboard_model_init(&keyboard.model,edited,2);strcpy(keyboard.initial_text,original);
        hello_input_field_init(&keyboard.input_field,(HelloInputType)type);keyboard.model.field.type=(HelloInputType)type;keyboard.model.upper=true;
        hello_keyboard_update(&keyboard,start,kButtonA|kButtonB,kButtonA|kButtonB,0,360);
        hello_keyboard_update(&keyboard,start+349,kButtonA|kButtonB,0,0,0);assert(keyboard.cancel_dialog_active);
        hello_keyboard_update(&keyboard,start+350,kButtonA|kButtonB,0,0,0);
        assert(keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,start+5000,kButtonA|kButtonB,0,0,360)==HELLO_KEYBOARD_NONE&&keyboard.visible);
        hello_keyboard_update(&keyboard,start+5010,kButtonA,0,kButtonB,0);
        assert(hello_keyboard_update(&keyboard,start+10000,kButtonA,0,0,0)==HELLO_KEYBOARD_NONE&&!keyboard.cancel_dialog_ready);
        assert(!strcmp(keyboard.model.text,edited)&&keyboard.model.cursor==2&&keyboard.model.upper);
        hello_keyboard_update(&keyboard,start+10010,0,0,kButtonA,0);assert(keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,start+10020,kButtonB,kButtonB,0,0)==HELLO_KEYBOARD_CANCELLED);
        assert(!keyboard.visible&&!strcmp(keyboard.model.text,original)&&!keyboard.model.pending&&!keyboard.cancel_dialog_active);
        assert(hello_keyboard_update(&keyboard,start+10380,kButtonB,0,0,0)==HELLO_KEYBOARD_NONE&&!strcmp(keyboard.model.text,original));
        assert(hello_keyboard_update(&keyboard,start+10381,0,0,kButtonB,0)==HELLO_KEYBOARD_NONE);

        prepare();hello_keyboard_model_init(&keyboard.model,edited,strlen(edited));strcpy(keyboard.initial_text,original);
        hello_input_field_init(&keyboard.input_field,(HelloInputType)type);keyboard.model.field.type=(HelloInputType)type;
        hello_keyboard_update(&keyboard,start,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,start+350,kButtonA|kButtonB,0,0,0);
        hello_keyboard_update(&keyboard,start+351,0,0,kButtonA|kButtonB,0);assert(keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,start+360,kButtonA,kButtonA,0,0)==HELLO_KEYBOARD_DONE);
        assert(keyboard.visible&&!strcmp(keyboard.model.text,edited)&&!keyboard.cancel_dialog_active);
        assert(hello_keyboard_update(&keyboard,start+711,kButtonA,0,0,0)==HELLO_KEYBOARD_NONE);
    }
    // Discard cancels a selected item even if its associated direction remains
    // physically held; no delayed commit or accepted repeat survives closing.
    for(int released=0;released<2;released++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"hi",2);strcpy(keyboard.initial_text,"hi");
        hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,900,kButtonRight,kButtonRight,0,0);hello_keyboard_model_crank(&keyboard.model,5,901);
        hello_keyboard_update(&keyboard,1000,kButtonRight|kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,1350,kButtonRight|kButtonA|kButtonB,0,0,0);
        const PDButtons held=released?0:kButtonRight;
        hello_keyboard_update(&keyboard,1351,held,0,kButtonA|kButtonB|(released?kButtonRight:0),0);
        assert(hello_keyboard_update(&keyboard,1360,held|kButtonB,kButtonB,0,0)==HELLO_KEYBOARD_CANCELLED);
        assert(!keyboard.visible&&!keyboard.model.pending&&!keyboard.model.group&&!strcmp(keyboard.model.text,"hi"));
        hello_keyboard_update(&keyboard,30000,held,0,kButtonB,0);assert(!strcmp(keyboard.model.text,"hi"));
    }
}

static void test_editing_popup_opposed_presses_wait_for_release(void) {
    for(int first=0;first<2;first++)for(int coalesced=0;coalesced<4;coalesced++)for(int rollover=0;rollover<2;rollover++) {
        const uint32_t start=rollover?UINT32_MAX-100:1000;
        prepare();hello_keyboard_model_init(&keyboard.model,"draft",3);strcpy(keyboard.initial_text,"original");
        const PDButtons face=first?kButtonB:kButtonA,other=first?kButtonA:kButtonB;
        hello_keyboard_update(&keyboard,start,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,start+350,kButtonA|kButtonB,0,0,0);
        hello_keyboard_update(&keyboard,start+351,0,0,kButtonA|kButtonB,0);
        if(coalesced==3) {
            // The SDK may coalesce the other button's full tap while one
            // button is physically down without a new press edge.
            assert(hello_keyboard_update(&keyboard,start+355,face,0,0,0)==HELLO_KEYBOARD_NONE);
            assert(keyboard.cancel_dialog_ready);
        }
        const PDButtons current=coalesced==0?face|other:coalesced==2?0:face;
        const PDButtons pushed=coalesced==3?other:face|other;
        const PDButtons released=coalesced==0?0:coalesced==2?face|other:other;
        assert(hello_keyboard_update(&keyboard,start+360,current,pushed,released,0)==HELLO_KEYBOARD_NONE);
        assert(keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready&&!strcmp(keyboard.model.text,"draft"));
        if(current) {
            assert(hello_keyboard_update(&keyboard,start+1000,current,0,0,0)==HELLO_KEYBOARD_NONE);
            hello_keyboard_update(&keyboard,start+1010,face,0,other,0);
            assert(hello_keyboard_update(&keyboard,start+2000,face,0,0,0)==HELLO_KEYBOARD_NONE);
            assert(!keyboard.cancel_dialog_ready);
        }
        assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"draft")&&keyboard.model.cursor==3);
        hello_keyboard_update(&keyboard,start+2010,0,0,face,0);assert(keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,start+2020,kButtonB,kButtonB,0,0)==HELLO_KEYBOARD_CANCELLED);
        assert(!keyboard.visible&&!strcmp(keyboard.model.text,"original"));
        assert(hello_keyboard_update(&keyboard,start+2021,0,0,kButtonB,0)==HELLO_KEYBOARD_NONE);
    }
}

static void test_editing_popup_quick_taps_emit_once(void) {
    for(int save=0;save<2;save++)for(int coalesced=0;coalesced<2;coalesced++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"draft",3);strcpy(keyboard.initial_text,"original");
        hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,1000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,1350,kButtonA|kButtonB,0,0,0);
        hello_keyboard_update(&keyboard,1351,0,0,kButtonA|kButtonB,0);
        const PDButtons face=save?kButtonA:kButtonB;
        assert(hello_keyboard_update(&keyboard,1360,coalesced?0:face,face,coalesced?face:0,0)==(save?HELLO_KEYBOARD_DONE:HELLO_KEYBOARD_CANCELLED));
        const char* expected=save?"draft":"original";
        assert(!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,expected));
        assert(hello_keyboard_update(&keyboard,1361,0,0,face,0)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,30000,0,0,0,0)==HELLO_KEYBOARD_NONE);
        assert(!strcmp(keyboard.model.text,expected)&&keyboard.model.cursor==(save?3:8)&&!keyboard.model.upper);
    }
}

static void test_editing_opening_tap_boundaries(void) {
    const PDButtons faces=kButtonA|kButtonB;
    for(int rollover=0;rollover<2;rollover++)for(int sample=0;sample<4;sample++) {
        const uint32_t start=rollover?UINT32_MAX-100:1000;
        const PDButtons current=sample==0?faces:sample==1?kButtonA:sample==2?kButtonB:0;
        prepare();strcpy(keyboard.initial_text,"original");
        assert(hello_keyboard_update(&keyboard,start,current,faces,faces&~current,360)==HELLO_KEYBOARD_NONE);
        assert(keyboard.cancel_dialog_active&&keyboard.cancel_dialog_started_at==start);
        assert(keyboard.cancel_dialog_ready==(current==0));
        assert(!strcmp(keyboard.model.text,"secret")&&!keyboard.model.pending&&!keyboard.model.upper);
        assert(!keyboard.model.hold[0].active&&!keyboard.model.hold[1].active);
        assert(!hold_fill(&keyboard,HELLO_KB_BUTTON_A,start)&&!hold_fill(&keyboard,HELLO_KB_BUTTON_B,start));
        // Opening consumes its entire SDK sample, even when both full taps
        // are already up. No timer or continued hold may become a choice.
        assert(hello_keyboard_update(&keyboard,start+1,current,0,0,360)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,start+10000,current,0,0,0)==HELLO_KEYBOARD_NONE);
        hello_keyboard_update(&keyboard,start+10001,0,0,current,0);assert(keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,start+10002,0,kButtonB,kButtonB,0)==HELLO_KEYBOARD_CANCELLED);
        assert(!strcmp(keyboard.model.text,"original")&&!keyboard.visible);
    }
    const uint32_t delays[]={1,349,350,5000};
    for(int first_b=0;first_b<2;first_b++)for(int coalesced=0;coalesced<2;coalesced++)
    for(size_t i=0;i<sizeof(delays)/sizeof(delays[0]);i++) {
        const PDButtons first=first_b?kButtonB:kButtonA,second=first_b?kButtonA:kButtonB;
        const uint32_t now=1000+delays[i];
        prepare();hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,1000,first,first,0,0);
        assert(!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,first_b?"secre":"secret"));
        hello_keyboard_update(&keyboard,now,first|(coalesced?0:second),second,coalesced?second:0,0);
        const bool opens=first_b||delays[i]<keyboard.model.timing.hold_ms;
        assert(keyboard.cancel_dialog_active==opens);
        assert(!strcmp(keyboard.model.text,first_b&&delays[i]>=keyboard.model.timing.hold_ms?"secre":"secret"));
        if(opens)assert(keyboard.cancel_dialog_started_at==now&&!keyboard.cancel_dialog_ready);
        else assert(hello_keyboard_model_is_choosing_layer(&keyboard.model));
    }
    for(int first_b=0;first_b<2;first_b++) {
        const PDButtons first=first_b?kButtonB:kButtonA,second=first_b?kButtonA:kButtonB;
        prepare();hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,1000,first,first,0,0);
        // An old release plus a new opposite press has no observed overlap.
        hello_keyboard_update(&keyboard,1010,second,second,first,0);
        hello_keyboard_update(&keyboard,1020,0,0,second,0);
        assert(!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"secre")&&keyboard.model.upper);
    }
    for(int rejected=0;rejected<3;rejected++) {
        prepare();
        keyboard.rejected_face_buttons=rejected==1?kButtonA:rejected==2?kButtonB:0;
        const PDButtons pushed=rejected?faces:0;
        hello_keyboard_update(&keyboard,1000,faces,pushed,0,0);
        hello_keyboard_update(&keyboard,10000,faces,0,0,0);
        assert(!keyboard.cancel_dialog_active); // Held bits and rejected faces never arm later.
        hello_keyboard_update(&keyboard,10001,0,0,faces,0);
        hello_keyboard_update(&keyboard,10002,faces,faces,0,0);
        assert(keyboard.cancel_dialog_active);
        hello_keyboard_close(&keyboard);
        assert(!keyboard.visible&&!keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready);
        assert(hello_keyboard_update(&keyboard,20000,faces,faces,0,360)==HELLO_KEYBOARD_NONE);
        assert(!keyboard.cancel_dialog_active); // A closed lifecycle consumes no events.
    }
    for(int stale_a=0;stale_a<2;stale_a++) {
        const PDButtons stale=stale_a?kButtonA:kButtonB,fresh=stale_a?kButtonB:kButtonA;
        prepare();
        hello_keyboard_update(&keyboard,1000,stale,0,0,0);
        hello_keyboard_update(&keyboard,1001,faces,fresh,0,0);
        assert(!keyboard.cancel_dialog_active); // An unowned held bit is never half a chord.
        hello_keyboard_update(&keyboard,1002,0,0,faces,0);
        hello_keyboard_update(&keyboard,1003,faces,faces,0,0);
        assert(keyboard.cancel_dialog_active);
    }
}

static void test_idle_b_deletes_on_press(void) {
    const uint32_t starts[]={1000,UINT32_MAX-100};
    for(size_t run=0;run<sizeof(starts)/sizeof(starts[0]);++run) {
        const uint32_t start=starts[run];
        prepare();hello_keyboard_model_init(&keyboard.model,"abcdef",6);
        hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(&keyboard,start,kButtonB,kButtonB,0,0);
        assert(!strcmp(keyboard.model.text,"abcde"));
        hello_keyboard_update(&keyboard,start+349,kButtonB,0,0,0);
        assert(!strcmp(keyboard.model.text,"abcde"));
        hello_keyboard_update(&keyboard,start+350,kButtonB,0,0,0);
        assert(!strcmp(keyboard.model.text,"abcd"));
        hello_keyboard_update(&keyboard,start+449,kButtonB,0,0,0);
        assert(!strcmp(keyboard.model.text,"abcd"));
        hello_keyboard_update(&keyboard,start+450,kButtonB,0,0,0);
        assert(!strcmp(keyboard.model.text,"abc"));
        hello_keyboard_update(&keyboard,start+451,0,0,kButtonB,0);
        hello_keyboard_update(&keyboard,start+1000,0,0,0,0);
        assert(!strcmp(keyboard.model.text,"abc"));
        hello_keyboard_update(&keyboard,start+1010,kButtonB,kButtonB,0,0);
        assert(!strcmp(keyboard.model.text,"ab"));
        hello_keyboard_update(&keyboard,start+1020,0,0,kButtonB,0);
        assert(!strcmp(keyboard.model.text,"ab"));
    }
}

static void test_same_button_tap_then_hold(void) {
    const uint32_t starts[]={1000,UINT32_MAX-100};const char* letters="eidgbfcha";
    for(unsigned run=0;run<2;run++)for(int button=0;button<2;button++)for(int half=0;half<2;half++)for(int held=0;held<2;held++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        assert(hello_keyboard_set_button_navigation_scale(&keyboard,half?.5f:1.0f));
        const uint32_t start=starts[run],delay=half?175:350,repeat=half?50:100;
        const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;const int first=button==HELLO_KB_BUTTON_A?0:8,direction=button==HELLO_KB_BUTTON_A?1:-1;
        hello_keyboard_update(&keyboard,start,kButtonLeft,kButtonLeft,0,0);
        hello_keyboard_update(&keyboard,start+10,kButtonLeft|face,face,0,0);assert(keyboard.model.index==first);
        hello_keyboard_update(&keyboard,start+20,kButtonLeft,0,face,0);assert(keyboard.model.index==first);
        hello_keyboard_update(&keyboard,start+40,kButtonLeft|face,face,0,0);assert(keyboard.model.index==first+direction);
        hello_keyboard_update(&keyboard,start+40+delay-1,kButtonLeft|face,0,0,0);assert(keyboard.model.index==first+direction);
        int expected=first+direction;uint32_t stopped=start+40+delay;
        if(held) {
            hello_keyboard_update(&keyboard,start+40+delay,kButtonLeft|face,0,0,0);assert(keyboard.model.index==first+2*direction);
            hello_keyboard_update(&keyboard,start+40+delay+repeat,kButtonLeft|face,0,0,0);expected=first+3*direction;assert(keyboard.model.index==expected);
            stopped=start+41+delay+repeat;
        }
        hello_keyboard_update(&keyboard,held?stopped:start+40+delay-1,kButtonLeft,0,face,0);
        assert(keyboard.model.index==expected&&keyboard.model.pending==(uint32_t)letters[expected]&&!keyboard.model.upper);
        hello_keyboard_update(&keyboard,start+30000,kButtonLeft,0,0,0);assert(keyboard.model.index==expected&&!keyboard.model.text[0]);
        hello_keyboard_update(&keyboard,start+30001,0,0,kButtonLeft,0);assert(keyboard.model.text[0]==letters[expected]&&!keyboard.model.text[1]);
    }
}

static void test_coalesced_same_face_release_precedes_new_hold(void) {
    for(int masked=0;masked<2;masked++)for(int button=0;button<2;button++)for(int half=0;half<2;half++) {
        prepare();hello_keyboard_model_init(&keyboard.model,"",0);
        hello_input_field_init(&keyboard.input_field,masked?HELLO_INPUT_MASKEDTEXT:HELLO_INPUT_PLAINTEXT);
        assert(hello_keyboard_set_button_navigation_scale(&keyboard,half?.5f:1.0f));
        hello_keyboard_update(&keyboard,900,kButtonRight,kButtonRight,0,0);
        hello_keyboard_model_crank(&keyboard.model,5,901);assert(keyboard.model.pending=='q'&&keyboard.model.index==4);
        const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;const int neighbor=button==HELLO_KB_BUTTON_A?5:3;
        hello_keyboard_update(&keyboard,1000,kButtonRight|face,face,0,0);assert(keyboard.model.index==neighbor);
        // Settle the old short tap before starting the newly reported hold.
        hello_keyboard_update(&keyboard,1016,kButtonRight|face,face,face,0);
        const int preview=neighbor+(button==HELLO_KB_BUTTON_A?1:-1);
        assert(keyboard.model.index==preview&&keyboard.model.hold[button].active&&keyboard.model.hold[button].at==1016);
        assert(keyboard.model.hold[button].hold==HELLO_KB_HOLD_REPEAT);
        const uint32_t delay=half?175:350;
        hello_keyboard_update(&keyboard,1016+delay-1,kButtonRight|face,0,0,0);assert(keyboard.model.index==preview);
        hello_keyboard_update(&keyboard,1016+delay,kButtonRight|face,0,0,0);
        const char* expected=button==HELLO_KB_BUTTON_A?"l":"o";
        assert(keyboard.model.index==preview+(button==HELLO_KB_BUTTON_A?1:-1)&&keyboard.model.pending==(uint32_t)expected[0]&&!keyboard.model.upper);
        hello_keyboard_update(&keyboard,1017+delay,face,0,kButtonRight,0);assert(!strcmp(keyboard.model.text,expected));
    }
    prepare();hello_keyboard_model_init(&keyboard.model,"",0);
    hello_keyboard_update(&keyboard,900,kButtonRight,kButtonRight,0,0);hello_keyboard_model_crank(&keyboard.model,5,901);
    hello_keyboard_update(&keyboard,1000,kButtonRight|kButtonA,kButtonA,0,0);assert(keyboard.model.index==5);
    hello_keyboard_update(&keyboard,1016,kButtonRight|kButtonA|kButtonB,kButtonA|kButtonB,kButtonA,0);
    assert(keyboard.cancel_dialog_active&&!keyboard.model.hold[0].active&&!keyboard.model.hold[1].active);
    hello_keyboard_update(&keyboard,1366,kButtonRight|kButtonA|kButtonB,0,0,0);
    assert(!hello_input_field_is_masked(&keyboard.input_field)&&keyboard.cancel_dialog_active&&keyboard.model.index==5&&keyboard.model.pending=='j');
    assert(!keyboard.model.upper&&!keyboard.model.text[0]);
}

static void test_navigation_modifier_unmaps_b_until_fresh_release(void) {
    for(int type=0;type<3;type++)for(int entry=0;entry<3;entry++) {
        prepare();const char* text=type==HELLO_INPUT_CONTACT_NUMBER?"123456":"abcdef";
        hello_keyboard_model_init(&keyboard.model,text,3);keyboard.model.field.type=(HelloInputType)type;
        hello_input_field_init(&keyboard.input_field,(HelloInputType)type);assert(hello_keyboard_set_button_navigation_scale(&keyboard,.5f));
        hello_keyboard_update(&keyboard,1000,kButtonA,kButtonA,0,0);uint32_t left;
        if(entry==0) {
            hello_keyboard_update(&keyboard,1350,kButtonA,0,0,0);
            hello_keyboard_update(&keyboard,1360,kButtonA|kButtonLeft,kButtonLeft,0,0);left=1360;
            hello_keyboard_update(&keyboard,1370,kButtonA|kButtonB|kButtonLeft,kButtonB,0,0);
        } else if(entry==1) {
            hello_keyboard_update(&keyboard,1350,kButtonA|kButtonB,kButtonB,0,0);
            assert(hello_keyboard_model_is_choosing_layer(&keyboard.model));
            hello_keyboard_update(&keyboard,1351,kButtonA|kButtonB|kButtonLeft,kButtonLeft,0,0);left=1351;
        } else {
            hello_keyboard_update(&keyboard,1001,kButtonA|kButtonB|kButtonLeft,kButtonLeft|kButtonB,0,0);left=1001;
        }
        assert(hello_keyboard_model_is_choosing_layer(&keyboard.model)&&keyboard.model.cursor_hold&&keyboard.model.cursor==2);
        assert(!keyboard.cancel_dialog_active&&!keyboard.model.hold[HELLO_KB_BUTTON_B].active&&(keyboard.rejected_face_buttons&kButtonB));
        hello_keyboard_update(&keyboard,left+20,kButtonA|kButtonB|kButtonLeft,kButtonB,kButtonB,0);
        assert(keyboard.model.cursor_hold&&!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,text));
        hello_keyboard_update(&keyboard,left+175,kButtonA|kButtonB|kButtonLeft,0,0,0);
        assert(keyboard.model.cursor==1&&keyboard.model.cursor_hold&&!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,text));
        hello_keyboard_update(&keyboard,left+176,kButtonB|kButtonLeft,0,kButtonA,0);
        hello_keyboard_update(&keyboard,left+1000,kButtonB|kButtonLeft,kButtonB,0,0);
        assert(!keyboard.model.cursor_hold&&!hello_keyboard_model_is_choosing_layer(&keyboard.model)&&keyboard.model.cursor==1&&!keyboard.model.upper&&!strcmp(keyboard.model.text,text));
        hello_keyboard_update(&keyboard,left+1001,kButtonLeft,0,kButtonB,0);
        hello_keyboard_update(&keyboard,left+1002,kButtonB|kButtonLeft,kButtonB,0,0);
        assert(!strcmp(keyboard.model.text,text+1)&&keyboard.model.cursor==0);
    }
    prepare();hello_keyboard_model_init(&keyboard.model,"abcdef",6);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    hello_keyboard_update(&keyboard,1000,kButtonA|kButtonB|kButtonLeft,kButtonA|kButtonB|kButtonLeft,0,0);
    hello_keyboard_update(&keyboard,1350,kButtonA|kButtonB|kButtonLeft,0,0,0);
    assert(keyboard.cancel_dialog_active&&!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,"abcdef"));
}

int main(int argc,char** argv) {
    test_editing_opening_tap_boundaries();
    test_navigation_modifier_unmaps_b_until_fresh_release();
    test_coalesced_same_face_release_precedes_new_hold();
    test_idle_b_deletes_on_press();
    test_same_button_tap_then_hold();
    test_editing_decision_discard_and_fresh_save();
    test_editing_popup_opposed_presses_wait_for_release();
    test_editing_popup_quick_taps_emit_once();
    test_done_ends_all_input_lifetimes();
    test_first_negative_crank_uses_visible_entry();
    test_offscreen_entry_gate_is_button_only();
    test_selection_repeat_dispatch_and_return();
    test_stale_idle_b_hold_selection();
    test_chooser_cursor_release_order();
    test_idle_a_layer_chord_order();
    test_direction_release_update_order();
    FILE* actual=tmpfile();assert(actual);
    const uint32_t starts[]={1000,UINT32_MAX-100};
    for(size_t run=0;run<2;run++) {
        prepare();strcpy(keyboard.initial_text,"original");const uint32_t start=starts[run];
        const unsigned times[]={0,1,174,349,350,351,700};
        for(size_t i=0;i<sizeof(times)/sizeof(times[0]);i++) {
            hello_keyboard_update(&keyboard,start+times[i],kButtonA|kButtonB,i?0:kButtonA|kButtonB,0,0);
            assert(!keyboard.input_field.masked&&keyboard.cancel_dialog_active);
            const int a_fill=keyboard.cancel_dialog_active?0:hold_fill(&keyboard,HELLO_KB_BUTTON_A,start+times[i]);
            const int b_fill=keyboard.cancel_dialog_active?0:hold_fill(&keyboard,HELLO_KB_BUTTON_B,start+times[i]);
            fprintf(actual,"combo %zu +%u mask=%d dialog=%d ready=%d fills=%d,%d text=%s\n",run,times[i],
                keyboard.input_field.masked,keyboard.cancel_dialog_active,keyboard.cancel_dialog_ready,a_fill,b_fill,keyboard.model.text);
        }
        hello_keyboard_update(&keyboard,start+701,kButtonA,0,kButtonB,0);
        hello_keyboard_update(&keyboard,start+702,0,0,kButtonA,0);
        assert(keyboard.cancel_dialog_active&&keyboard.cancel_dialog_ready);
        fprintf(actual,"released %zu dialog=%d ready=%d mask=%d text=%s\n",run,keyboard.cancel_dialog_active,keyboard.cancel_dialog_ready,keyboard.input_field.masked,keyboard.model.text);
        const PDButtons choice=run?kButtonB:kButtonA;
        const HelloKeyboardResult result=hello_keyboard_update(&keyboard,start+703,choice,choice,0,0);
        assert(result==(run?HELLO_KEYBOARD_CANCELLED:HELLO_KEYBOARD_DONE));
        fprintf(actual,"tap %zu +703 choice=%c result=%s dialog=%d visible=%d text=%s\n",run,run?'B':'A',run?"cancelled":"done",keyboard.cancel_dialog_active,keyboard.visible,keyboard.model.text);
        assert(hello_keyboard_update(&keyboard,start+704,0,0,choice,0)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,start+20000,0,0,0,0)==HELLO_KEYBOARD_NONE);
        fprintf(actual,"settled %zu +20000 dialog=%d visible=%d text=%s\n",run,keyboard.cancel_dialog_active,keyboard.visible,keyboard.model.text);
    }
    rewind(actual);
    if(argc==2&&!strcmp(argv[1],"--record")){int c;while((c=fgetc(actual))!=EOF)putchar(c);}
    else {
        FILE* expected=fopen("tests/fixtures/input-timing-keyboard-editing-decision.trace","rb");assert(expected);
        int a,e;do{a=fgetc(actual);e=fgetc(expected);assert(a==e);}while(a!=EOF);
        fclose(expected);puts("keyboard timing: editing decision, fresh choice taps, progress clocks and selection passed");
    }
    fclose(actual);
    prepare();BrbInputTimingConfig custom=brb_input_timing_defaults();custom.hold_ms=500;
    assert(hello_keyboard_set_timing(&keyboard,&custom));
    assert(keyboard.config.timing.hold_ms==500&&keyboard.model.timing.hold_ms==500);
    hello_keyboard_update(&keyboard,1000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    hello_keyboard_update(&keyboard,1499,kButtonA|kButtonB,0,0,0);
    assert(!keyboard.input_field.masked&&keyboard.cancel_dialog_active&&hold_fill(&keyboard,HELLO_KB_BUTTON_A,1499)==0);
    assert(hold_fill(&keyboard,HELLO_KB_BUTTON_B,1499)==0);
    hello_keyboard_update(&keyboard,1500,kButtonA|kButtonB,0,0,0);
    assert(!keyboard.input_field.masked&&keyboard.cancel_dialog_active);
    hello_keyboard_update(&keyboard,1501,0,0,kButtonA|kButtonB,0);
    assert(hello_keyboard_update(&keyboard,1502,kButtonA,kButtonA,0,0)==HELLO_KEYBOARD_DONE);
    assert(!keyboard.cancel_dialog_active);
    hello_keyboard_model_clear(&keyboard.model);assert(keyboard.model.timing.hold_ms==500);
    hello_keyboard_model_press(&keyboard.model,HELLO_KB_BUTTON_A,2000);
    hello_keyboard_model_update(&keyboard.model,2499);assert(!keyboard.model.upper);
    hello_keyboard_model_update(&keyboard.model,2500);assert(!keyboard.model.upper&&hello_keyboard_model_is_choosing_layer(&keyboard.model));
    custom.hold_ms=0;assert(!hello_keyboard_set_timing(&keyboard,&custom));
    assert(keyboard.config.timing.hold_ms==500&&keyboard.model.timing.hold_ms==500);
}
