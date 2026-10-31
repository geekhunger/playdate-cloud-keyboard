#include "../src/keyboard.c"
#include <assert.h>
#include <stdlib.h>

static HelloKeyboardResult tap_editing_choice(HelloKeyboard* keyboard,uint32_t now,PDButtons choice) {
    assert(keyboard->cancel_dialog_active);
    const PDButtons opening=keyboard->footer_buttons&(kButtonA|kButtonB);
    assert(hello_keyboard_update(keyboard,now,opening,0,0,0)==HELLO_KEYBOARD_NONE);
    hello_keyboard_update(keyboard,now+1,0,0,opening,0);assert(keyboard->cancel_dialog_ready);
    return hello_keyboard_update(keyboard,now+2,choice,choice,0,0);
}
static HelloKeyboardResult save_editing_popup(HelloKeyboard* keyboard,uint32_t now) {
    return tap_editing_choice(keyboard,now,kButtonA);
}
static HelloKeyboardResult discard_editing_popup(HelloKeyboard* keyboard,uint32_t now) {
    return tap_editing_choice(keyboard,now,kButtonB);
}

static int test_group_pair_width(const HelloKeyboard* keyboard,int group) {
    return group_bounds(keyboard,group).width;
}

static const char kBuild68UpGroup[]=".,'?!-:/@_#[<{\\}>]^=;*%$€&+)(";
static const char* const kBuild68Groups[HELLO_KEYBOARD_GROUP_COUNT]={kBuild68UpGroup,NULL,NULL,NULL,NULL,"❤️😊😂🙏👍😢🤩🤗😘😉😮🤔😌💪🎉😅😠😤😟😴🖕"};
static const char kOverflowGroup[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?";
static const char* const kOverflowGroups[HELLO_KEYBOARD_GROUP_COUNT]={
    kOverflowGroup,kOverflowGroup,kOverflowGroup,kOverflowGroup,
    "1234567890123456789012345678901234567890123456789012345678901234",kOverflowGroup};

static int tables[3], bitmap, loads, expected_table, expected_frame;
static int text_draws,text_x,cursor_draws,cursor_x,cursor_width;
static int text_y,cursor_y,cursor_height;
static char rendered[HELLO_KEYBOARD_TEXT_BYTES+HELLO_KEYBOARD_ITEM_BYTES+1];
static bool capture_bitmap_text;
static bool wide_test_w,capture_initial_digits,capture_return_icon,cursor_font_metrics;
static int initial_digit_count,return_line_count;
static char initial_digits[11];
static int return_lines[4][4];
static bool capture_base_page,capture_group_block;
static int block_count,block_y[4],block_h[4],input_pill_count;
static bool base_page_upper;
static bool base_page_letters[26];
static int base_page_top_unicode,base_page_top_ascii;
static bool capture_group_view, group_clip_active;
static int group_clip_x,group_clip_y,group_clip_w,group_clip_h;
static int group_pill_x,group_pill_y,group_pill_w;
static bool capture_pack_draw,capture_cursor_geometry;
static int pack_rects;
static int group_draw_count,group_item_x[80],group_item_width[80];
static int drawn_glyphs,last_glyph_frame,glyph_frames[128],glyph_x[128],glyph_y[128];
static int marker_draws,marker_x,marker_y,marker_w,marker_h;
static bool capture_cancel_popup,capture_escape_dash;
static int escape_dash_lines;
static int cancel_popup_fills,cancel_popup_frames[8],cancel_popup_frame_count,cancel_popup_x,cancel_popup_y,cancel_popup_h,popup_footer_white;
static bool capture_selected_ink;
static const char* expected_selected_item;
static int expected_item_draws,expected_item_x,expected_item_y,expected_item_draw_index;
static bool capture_group_ink,expected_group_active;
static int group_ink_texts,group_ink_rects,group_ink_lines;
static LCDColor group_ink_fill;
static int selected_ink_draws,selected_ink_x,selected_ink_y;
static int group_pill_h;
static bool capture_footer_labels;
static bool capture_footer_canvas;
static int canvas_white_fills,footer_dividers,footer_circles,footer_white_fills;
static float footer_fill_angle;
static int footer_circle_x[2];
static bool active_clip;
static int active_clip_x,active_clip_y,active_clip_w,active_clip_h;
static int footer_progress_clip_y,footer_progress_clip_h;
static LCDBitmapDrawMode active_draw_mode,footer_caption_modes[8];
static int footer_caption_x[8],footer_caption_y[8],footer_caption_clip_w[8];
static int footer_label_clips,footer_label_fills,footer_clip_x,footer_clip_y,footer_clip_w,footer_clip_h;
static char footer_captions[8][64];
static int footer_caption_count;
static int footer_dpad_draws;
static bool capture_native_dpad;
static int native_dpad_x,native_dpad_rects;
static unsigned char native_dpad_pixels[20][20];
static void native_dpad_begin(int x) {
    capture_native_dpad=true;native_dpad_x=x;native_dpad_rects=0;
    memset(native_dpad_pixels,0,sizeof(native_dpad_pixels));
}
static void native_dpad_check_lights(unsigned lights,bool all_light) {
    int arms=0;for(unsigned bit=0;bit<4;bit++)if(lights&(1u<<bit))arms++;
    assert(native_dpad_rects==4+(all_light?2:arms));
    for(int y=0;y<20;y++)for(int x=0;x<20;x++) {
        const bool outer=(x>=7&&x<13)||(x>=6&&x<14&&y>=1&&y<19)||
            (x>=1&&x<19&&y>=6&&y<14)||(y>=7&&y<13);
        const bool inner=(x>=8&&x<12&&y>=2&&y<18)||(y>=8&&y<12&&x>=2&&x<18);
        const bool arm=((lights&1)&&x>=8&&x<12&&y>=2&&y<8)||
            ((lights&2)&&x>=12&&x<18&&y>=8&&y<12)||
            ((lights&4)&&x>=8&&x<12&&y>=12&&y<18)||
            ((lights&8)&&x>=2&&x<8&&y>=8&&y<12);
        assert(native_dpad_pixels[y][x]==(unsigned char)(outer&&!(all_light?inner:arm)));
    }
    capture_native_dpad=false;
}
static void native_dpad_check(int state) {
    native_dpad_check_lights(state>=1&&state<=4?1u<<(state-1):0,state==5);
}
static void native_dpad_check_buttons(PDButtons buttons) {
    const unsigned lights=(buttons&kButtonUp?1u:0)|(buttons&kButtonRight?2u:0)|
        (buttons&kButtonDown?4u:0)|(buttons&kButtonLeft?8u:0);
    native_dpad_check_lights(lights,false);
}
static void scaled_bitmap(LCDBitmap* value,int x,int y,float sx,float sy) {
    (void)value;(void)x;(void)y;(void)sx;(void)sy;
    assert(!"keyboard glyphs and D-pad must render at native size");
}
static void rounded_fill(int x,int y,int w,int h,int radius,LCDColor color) {
    if(capture_cancel_popup&&w==272) {assert(radius==12&&h>=24&&x>=0&&y>=0&&x+w<=400&&y+h<=211&&color==kColorWhite);cancel_popup_fills++;cancel_popup_x=x;cancel_popup_y=y;cancel_popup_h=h;}
    else assert(w>0&&h>0&&radius==(w<h?w:h)/2);
    if(capture_group_block&&color!=kColorXOR) {
        if(x==9&&y==27&&w==382&&h==37){assert(color==kColorBlack);input_pill_count++;}
        if(y>=64&&y<211){assert(block_count<4);block_y[block_count]=y;block_h[block_count++]=h;}
    }
    if(capture_group_ink&&color!=kColorXOR)group_ink_fill=color;
    if(color==kColorXOR){marker_draws++;marker_x=x;marker_y=y;marker_w=w;marker_h=h;}
}
static void rounded_outline(int x,int y,int w,int h,int radius,int stroke,LCDColor color) {
    if(capture_group_view||capture_cursor_geometry){group_pill_x=x;group_pill_y=y;group_pill_w=w;group_pill_h=h;}
    (void)x;(void)y;(void)w;(void)h;(void)radius;(void)stroke;(void)color;
}
static void set_font(LCDFont* value){(void)value;}
static LCDBitmapDrawMode draw_mode(LCDBitmapDrawMode value){active_draw_mode=value;return value;}
static void clip(int x,int y,int w,int h){
    active_clip=true;active_clip_x=x;active_clip_y=y;active_clip_w=w;active_clip_h=h;
    if(capture_footer_labels) {
        assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=400&&y+h<=240);
        footer_label_clips++;footer_clip_x=x;footer_clip_y=y;footer_clip_w=w;footer_clip_h=h;
    }
    if(capture_group_view){assert(!group_clip_active);group_clip_active=true;
        group_clip_x=x;group_clip_y=y;group_clip_w=w;group_clip_h=h;}
}
static void clear_clip(void){active_clip=false;if(capture_group_view){assert(group_clip_active);group_clip_active=false;}}
static int measure(LCDFont* font,const void* text,size_t length,PDStringEncoding encoding,int tracking) {
    (void)font;(void)tracking;assert(encoding==kUTF8Encoding);
    const char* at=text;int width=0;
    for(size_t offset=0;offset<length;) {
        uint32_t codepoint;const int bytes=hello_keyboard_utf8_decode(at+offset,&codepoint);
        assert(bytes>0&&offset+(size_t)bytes<=length);
        width+=cursor_font_metrics&&(codepoint=='l'||codepoint=='1'||codepoint=='.')?1:wide_test_w&&codepoint=='W'?20:codepoint=='i'?2:codepoint==0xe9?4:8;offset+=(size_t)bytes;
    }
    if(cursor_font_metrics)for(size_t at=0;at+1<length;at++)if((((const char*)text)[at]=='W'&&((const char*)text)[at+1]=='i')||(((const char*)text)[at]=='i'&&((const char*)text)[at+1]=='W'))width--;
    return width;
}
static int text(const void* value,size_t length,PDStringEncoding encoding,int x,int y) {
    assert(encoding==kUTF8Encoding&&length<sizeof(rendered));
    memcpy(rendered,value,length);rendered[length]=0;text_x=x;text_y=y;text_draws++;
    if(capture_initial_digits) {
        assert(length==1&&initial_digit_count<10);initial_digits[initial_digit_count++]=rendered[0];
    }
    if(capture_footer_canvas&&length==1&&(rendered[0]=='A'||rendered[0]=='B')&&active_draw_mode==kDrawModeNXOR)
        assert(active_clip&&active_clip_w==16&&active_clip_h==16&&active_clip_y==217);
    if(capture_base_page&&y>=69&&y<211) {
        if(length==1&&isalpha((unsigned char)rendered[0])) {
            const unsigned char character=(unsigned char)rendered[0];
            assert((bool)(isupper(character)!=0)==base_page_upper);
            base_page_letters[tolower(character)-'a']=true;
        }
        if(y<110) {
            if((unsigned char)rendered[0]>=0x80)base_page_top_unicode++;
            else base_page_top_ascii++;
        }
    }
    if(capture_footer_labels) {
        assert(footer_caption_count<8&&length<64);
        footer_caption_modes[footer_caption_count]=active_draw_mode;footer_caption_x[footer_caption_count]=x;footer_caption_y[footer_caption_count]=y;footer_caption_clip_w[footer_caption_count]=active_clip_w;
        memcpy(footer_captions[footer_caption_count],value,length);footer_captions[footer_caption_count++][length]=0;
    }
    if(expected_selected_item&&strlen(expected_selected_item)==length&&!memcmp(value,expected_selected_item,length)) {
        expected_item_draws++;expected_item_x=x;expected_item_y=y;if(capture_group_view)expected_item_draw_index=group_draw_count;
    }
    if(capture_group_ink) {
        assert(active_draw_mode==(expected_group_active?kDrawModeFillWhite:kDrawModeCopy));group_ink_texts++;
    }
    if(capture_selected_ink&&length==2&&!memcmp(value,"SS",2)) {
        selected_ink_draws++;selected_ink_x=x;selected_ink_y=y;
    }
    if(capture_group_view) {
        assert(group_clip_active && group_draw_count<80);
        group_item_x[group_draw_count]=x;
        group_item_width[group_draw_count++]=measure(NULL,value,length,encoding,0);
        assert(y>=group_clip_y && y+10<=group_clip_y+group_clip_h);
    }
    if(capture_cursor_geometry)assert(x>=group_pill_x+12&&x+measure(NULL,value,length,encoding,0)<=group_pill_x+group_pill_w-12);
    return 0;
}
static void fill(int x,int y,int w,int h,LCDColor color) {
    if(capture_escape_dash&&w==8&&h==1&&y>=212&&y<240&&color==kColorBlack)escape_dash_lines++;
    if(capture_group_ink&&color!=kColorXOR) {
        assert(color==(expected_group_active?kColorWhite:kColorBlack));group_ink_rects++;
    }
    if(y==215&&w==6&&h==20&&color==kColorBlack)footer_dpad_draws++;
    if(capture_native_dpad) {
        if(x>=native_dpad_x&&x+w<=native_dpad_x+20&&y>=215&&y+h<=235)native_dpad_rects++;
        for(int row=0;row<20;row++)for(int col=0;col<20;col++)
            if(native_dpad_x+col>=x&&native_dpad_x+col<x+w&&215+row>=y&&215+row<y+h)
                native_dpad_pixels[row][col]=(unsigned char)(color==kColorBlack);
    }
    if(capture_footer_canvas&&x==0&&y==0&&w==400&&h==240) {
        if(!capture_cancel_popup)assert(color==kColorWhite);if(color==kColorWhite)canvas_white_fills++;
    }
    if(capture_cancel_popup&&x==0&&y==211&&w==400&&h==29){assert(color==kColorWhite);popup_footer_white++;}
    if(capture_footer_labels&&!(capture_cancel_popup&&x==0&&y==0&&w==400&&h==240)) {
        assert((color==kColorWhite||color==kColorBlack)&&x>=0&&y>=0&&x+w<=400&&y+h<=240);footer_label_fills++;
    }
    if(color==kColorXOR){cursor_draws++;cursor_x=x;cursor_y=y;cursor_width=w;cursor_height=h;}
    else if(capture_cursor_geometry){assert(x>=group_pill_x+12&&x+w<=group_pill_x+group_pill_w-12);}
    else if(capture_pack_draw){assert(group_clip_active);assert(x>=group_clip_x&&x+w<=group_clip_x+group_clip_w);
        assert(y>=group_clip_y&&y+h<=group_clip_y+group_clip_h);pack_rects++;}
}
static void draw_bitmap(LCDBitmap* value,int x,int y,LCDBitmapFlip flip) {
    assert(value==(LCDBitmap*)&bitmap&&flip==kBitmapUnflipped);
    if(capture_bitmap_text) {
        assert(drawn_glyphs<128);glyph_frames[drawn_glyphs]=last_glyph_frame;
        glyph_x[drawn_glyphs]=x;glyph_y[drawn_glyphs]=y;drawn_glyphs++;
    }
}
static void outline_rect(int x,int y,int w,int h,LCDColor color) {
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=400&&y+h<=240&&color==kColorBlack);
}
static void triangle(int x,int y,int xx,int yy,int xxx,int yyy,LCDColor color) {
    (void)y;(void)yy;(void)yyy;(void)color;
    if(capture_cursor_geometry){assert(x>=group_pill_x+12&&x<group_pill_x+group_pill_w-12);
        assert(xx>=group_pill_x+12&&xx<group_pill_x+group_pill_w-12);
        assert(xxx>=group_pill_x+12&&xxx<group_pill_x+group_pill_w-12);}
}
static void eye(int x,int y,int w,int h,int line,float start,float end,LCDColor color) {
    if(capture_footer_canvas&&y==216) {
        assert(!"keyboard footer must fill its interior instead of drawing a progress outline");
    }
    (void)x;(void)y;(void)w;(void)h;(void)line;(void)start;(void)end;(void)color;
}
static void pupil(int x,int y,int w,int h,float start,float end,LCDColor color) {
    if(capture_footer_canvas&&y==215) {
        assert(x>=3&&x+w<=396&&w==20&&h==20&&start==0&&end==360&&color==kColorBlack);
        assert(footer_circles<2);footer_circle_x[footer_circles++]=x;
    } else if(capture_footer_canvas&&y==217) {
        assert(x>=5&&x+w<=394&&w==16&&h==16&&start==0&&end==360&&color==kColorWhite);
        assert(active_clip&&active_clip_x==x&&active_clip_w==16&&active_clip_h>0&&active_clip_h<=16);
        assert(active_clip_y>=217&&active_clip_y+active_clip_h==233);
        footer_progress_clip_y=active_clip_y;footer_progress_clip_h=active_clip_h;
        footer_white_fills++;footer_fill_angle=end;
    }
    (void)x;(void)y;(void)w;(void)h;(void)start;(void)end;(void)color;
}
static void line(int x,int y,int xx,int yy,int w,LCDColor color) {
    if(capture_return_icon) {
        assert(return_line_count<4&&w==1&&color==kColorWhite);
        return_lines[return_line_count][0]=x;return_lines[return_line_count][1]=y;
        return_lines[return_line_count][2]=xx;return_lines[return_line_count++][3]=yy;
    }
    if(capture_group_ink) {
        assert(color==(expected_group_active?kColorWhite:kColorBlack));group_ink_lines++;
    }
    if(capture_footer_canvas&&y==211) {
        assert(x==3&&xx==396&&yy==211&&w==1&&color==kColorBlack);footer_dividers++;
    }
    (void)x;(void)y;(void)xx;(void)yy;(void)w;(void)color;
}

static bool capture_menu_labels;
static int options_menu,options_adds,options_removes,options_value;
static char options_title[HELLO_KEYBOARD_MAX_LABEL+1],options_labels[2][HELLO_KEYBOARD_MAX_LABEL+1];
static PDMenuItemCallbackFunction* options_callback;
static void* options_userdata;
static int menu_items[3],menu_label_adds,menu_label_removes,menu_label_renames;
static char menu_titles[3][HELLO_KEYBOARD_MAX_LABEL+1];
static PDMenuItemCallbackFunction* menu_callbacks[3];
static void* menu_userdata[3];
static PDMenuItem* add_menu(const char* title,PDMenuItemCallbackFunction* callback,void* userdata) {
    if(capture_menu_labels) {
        assert(menu_label_adds<3);const int item=menu_label_adds++;
        assert(strlen(title)<sizeof(menu_titles[item]));strcpy(menu_titles[item],title);
        menu_callbacks[item]=callback;menu_userdata[item]=userdata;
        return (PDMenuItem*)&menu_items[item];
    }
    return (PDMenuItem*)&bitmap;
}
static void remove_menu(PDMenuItem* item) {
    if(item==(PDMenuItem*)&options_menu){options_removes++;return;}
    if(capture_menu_labels) {
        assert(item==(PDMenuItem*)&menu_items[0]||item==(PDMenuItem*)&menu_items[1]||item==(PDMenuItem*)&menu_items[2]);
        menu_label_removes++;return;
    }
    assert(item==(PDMenuItem*)&bitmap);
}
static void rename_menu(PDMenuItem* item,const char* title) {
    int index=-1;for(int i=0;i<3;i++)if(item==(PDMenuItem*)&menu_items[i])index=i;
    assert(capture_menu_labels&&index>=0&&strlen(title)<sizeof(menu_titles[index]));
    strcpy(menu_titles[index],title);menu_label_renames++;
}

static PDMenuItem* add_options_menu(const char* title,const char** titles,int count,PDMenuItemCallbackFunction* callback,void* userdata) {
    assert(count==2);options_adds++;strcpy(options_title,title);strcpy(options_labels[0],titles[0]);strcpy(options_labels[1],titles[1]);
    options_callback=callback;options_userdata=userdata;return (PDMenuItem*)&options_menu;
}
static int get_options_value(PDMenuItem* item){assert(item==(PDMenuItem*)&options_menu);return options_value;}
static void set_options_value(PDMenuItem* item,int value){assert(item==(PDMenuItem*)&options_menu&&value>=0&&value<=1);options_value=value;}

static uint32_t fake_time(void) { return 0; }

static LCDBitmapTable* load_table(const char* path, const char** error) {
    static const char* const paths[] = {
        "keyboard/glyphs", "keyboard/footer", "keyboard/dpad"
    };
    (void)error;
    assert(loads < 3 && strcmp(path, paths[loads]) == 0);
    return (LCDBitmapTable*)&tables[loads++];
}

static LCDBitmap* get_frame(LCDBitmapTable* table, int frame) {
    if(capture_cancel_popup){assert(table==(LCDBitmapTable*)&tables[1]&&frame>=0&&frame<44&&cancel_popup_frame_count<8);cancel_popup_frames[cancel_popup_frame_count++]=frame;return (LCDBitmap*)&bitmap;}
    if(capture_bitmap_text) {
        assert(table==(LCDBitmapTable*)&tables[0]&&frame>=0&&frame<95);
        last_glyph_frame=frame;return (LCDBitmap*)&bitmap;
    }
    assert(table == (LCDBitmapTable*)&tables[expected_table]);
    assert(frame == expected_frame);
    return (LCDBitmap*)&bitmap;
}

static void check(HelloKeyboard* keyboard, const char* name, int table, int frame) {
    expected_table = table;
    expected_frame = frame;
    assert(image(keyboard, name) == (LCDBitmap*)&bitmap);
    assert(loads == 3);
}

static float crank_test_threshold(const HelloKeyboard* keyboard,int direction) {
    HelloKeyboardCrankDetent state=crank_detent(keyboard);
    state.remainder=(float)direction;
    const int length=hello_keyboard_model_group_length(&keyboard->model,keyboard->model.group);
    const bool wraps=keyboard->model.pending &&
        (direction>0?keyboard->model.index==length-1:keyboard->model.index==0);
    return hello_keyboard_crank_threshold(&state,wraps);
}
// Isolate the angular mechanism from the independent velocity sampler.
static void crank_test_selection(HelloKeyboard* keyboard,float change,uint32_t now) {
    keyboard->crank_sampled=false;crank_selection(keyboard,change,now);
}
static void test_crank_global_sensitivity(void) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    assert(tick>29.06f&&tick<29.07f);
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        HelloKeyboard keyboard={0};hello_keyboard_model_init(&keyboard.model,"",0);
        keyboard.model.group=group;keyboard.model.alternate=false;keyboard.model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;keyboard.model.upper=group==6;
        keyboard.model.index=0;
        keyboard.model.pending=hello_keyboard_model_group_codepoint(&keyboard.model,group,0);
        crank_test_selection(&keyboard,65,10);
        assert(keyboard.model.index==1); // Build 63 skipped directly to index 2.
        crank_test_selection(&keyboard,10,11);assert(keyboard.model.index==1);
        crank_test_selection(&keyboard,5,12);assert(keyboard.model.index==2);
        const float running=crank_test_threshold(&keyboard,1);
        crank_test_selection(&keyboard,running-0.2f,13);assert(keyboard.model.index==2);
        crank_test_selection(&keyboard,0.2f,14);assert(keyboard.model.index==3);
        const float pressure=keyboard.crank_remainder;
        for(int i=0;i<1000;i++) {
            crank_test_selection(&keyboard,0.25f,15);crank_test_selection(&keyboard,-0.25f,15);
            assert(keyboard.model.index==3);
        }
        assert(fabsf(keyboard.crank_remainder-pressure)<0.0001f);
        crank_test_selection(&keyboard,-pressure,16);
        const float reverse=crank_test_threshold(&keyboard,-1);
        assert(reverse>running);
        crank_test_selection(&keyboard,-reverse*0.99f,17);assert(keyboard.model.index==3);
        crank_test_selection(&keyboard,-reverse*0.02f,18);assert(keyboard.model.index==2);
        for(int i=0;i<1000;i++)crank_test_selection(&keyboard,0,20+(uint32_t)i);
        assert(keyboard.model.index==2); // Stopping never schedules catch-up.
        // Entry is immediate; its first sample cannot pay the next angular well.
        hello_keyboard_model_init(&keyboard.model,"",0);keyboard.model.group=group;reset_crank(&keyboard);
        crank_test_selection(&keyboard,0.011f,2000);assert(keyboard.model.pending&&keyboard.model.index==0);
        assert(keyboard.crank_entry_breakaway&&!keyboard.crank_remainder);
        crank_test_selection(&keyboard,tick*1.55f-0.1f,2001);assert(keyboard.model.index==0);
        crank_test_selection(&keyboard,0.101f,2002);assert(keyboard.model.index==1&&!keyboard.crank_entry_breakaway);
    }
}
static void crank_test_prepare(HelloKeyboard* keyboard,const HelloKeyboard* reference,int group,
    int direction,const char* const* groups,uint32_t now) {
    memset(keyboard,0,sizeof(*keyboard));keyboard->pd=reference->pd;keyboard->config=reference->config;
    hello_keyboard_model_init_with_groups(&keyboard->model,"",0,groups);
    hello_input_field_init(&keyboard->input_field,HELLO_INPUT_PLAINTEXT);
    keyboard->visible=true;keyboard->model.alternate=false;keyboard->model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;keyboard->model.upper=group==6;
    keyboard->model.group=group;
    const int count=hello_keyboard_model_group_length(&keyboard->model,group);
    keyboard->model.index=direction>0?count-1:0;keyboard->model.pending_side=direction>0?2:3;
    hello_keyboard_model_group_text(&keyboard->model,group,keyboard->model.index,
        keyboard->model.pending_text,sizeof(keyboard->model.pending_text));
    hello_keyboard_utf8_decode(keyboard->model.pending_text,&keyboard->model.pending);
    for(uint32_t elapsed=0;elapsed<=600;elapsed+=20)draw_group(keyboard,group,105,69,now-600+elapsed);
    keyboard->model.pending_at=now;
}
static uint32_t crank_test_finish_wrap(HelloKeyboard* keyboard,uint32_t now) {
    const int group=keyboard->model.group,index=keyboard->model.index;
    uint32_t first_visible=UINT32_MAX;
    for(uint32_t elapsed=0;elapsed<=1240;elapsed+=20) {
        hello_keyboard_update(keyboard,now+elapsed,0,0,0,0);
        marker_draws=0;draw_group(keyboard,group,105,69,now+elapsed);
        if(marker_draws&&first_visible==UINT32_MAX)first_visible=elapsed;
        assert(keyboard->model.pending&&keyboard->model.index==index&&!keyboard->model.text[0]);
        if(!keyboard->selection_lock_group) {
            assert(first_visible!=UINT32_MAX&&elapsed-first_visible<=650);return now+elapsed;
        }
    }
    assert(!"wrap failed to reach its rendered endpoint and jelly rest within 1250 ms");return now;
}
static void test_crank_edge_release(const HelloKeyboard* reference) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    const float factors[]={2.2888889f,2.2386406f,1.9294705f,1.4104015f,1.20f,1.20f};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,NULL,10000);
        const int length=hello_keyboard_model_group_length(&keyboard.model,group);
        const int edge=keyboard.model.index;
        const float threshold=crank_test_threshold(&keyboard,direction);
        crank_test_selection(&keyboard,(float)direction*(threshold-0.1f),10010);
        assert(keyboard.model.index==edge);
        crank_test_selection(&keyboard,(float)direction*0.101f,10020);
        int index=direction>0?0:length-1;
        assert(keyboard.model.index==index&&keyboard.crank_release_steps==5);
        assert(keyboard.selection_lock_group==group);
        const uint32_t resumed=crank_test_finish_wrap(&keyboard,10020);
        for(int step=0;step<6;step++) {
            const float distance=tick*factors[step];
            crank_test_selection(&keyboard,(float)direction*(distance-0.1f),resumed+10+(uint32_t)step*2);
            assert(keyboard.model.index==index);
            crank_test_selection(&keyboard,(float)direction*0.101f,resumed+11+(uint32_t)step*2);
            index+=direction;assert(keyboard.model.index==index);
        }
        keyboard.visible=true;keyboard.crank_release_steps=5;
        keyboard.crank_remainder=tick;keyboard.model.index=length-1;
        keyboard.model.pending=hello_keyboard_model_group_codepoint(&keyboard.model,group,length-1);
        hello_keyboard_update(&keyboard,resumed+120,kButtonA,kButtonA,0,0);
        assert(keyboard.model.index==0&&!keyboard.crank_remainder&&!keyboard.crank_release_steps);
        const uint32_t face_resumed=crank_test_finish_wrap(&keyboard,resumed+120);
        keyboard.crank_remainder=tick;crank_test_selection(&keyboard,NAN,face_resumed+10);
        assert(!keyboard.crank_remainder);
    }
}
// Drive the public update/render boundary at a normal 100 degrees/second.
// Splitting the angle here keeps speed assistance out of threshold assertions.
static uint32_t crank_render_travel(HelloKeyboard* keyboard,int direction,float distance,uint32_t now) {
    while(distance>0) {
        const float sample=fminf(2.0f,distance);
        now+=20;hello_keyboard_update(keyboard,now,0,0,0,(float)direction*sample);
        draw_group(keyboard,keyboard->model.group,105,69,now);
        distance-=sample;
        assert(keyboard->model.pending&&!keyboard->model.text[0]);
    }
    return now;
}
static uint32_t crank_render_entry(HelloKeyboard* keyboard,const HelloKeyboard* reference,int group,bool last,uint32_t now) {
    crank_test_prepare(keyboard,reference,group,last?1:-1,NULL,now);
    keyboard->model.pending=0;keyboard->model.pending_text[0]=0;
    if(!last)reset_group_scrolls(keyboard);keyboard->selection_marker.active=false;
    const PDButtons face=last?kButtonB:kButtonA;
    hello_keyboard_update(keyboard,now,face,face,0,0);
    const int count=hello_keyboard_model_group_length(&keyboard->model,group);
    assert(keyboard->model.index==(last?count-1:0)&&keyboard->model.pending);
    hello_keyboard_update(keyboard,now+8,0,0,face,0);
    if(keyboard->selection_lock_group)return crank_test_finish_wrap(keyboard,now+8);
    for(uint32_t elapsed=20;elapsed<=600;elapsed+=20) {
        hello_keyboard_update(keyboard,now+elapsed,0,0,0,0);
        draw_group(keyboard,group,105,69,now+elapsed);
        assert(!keyboard->selection_lock_group&&!keyboard->model.text[0]);
    }
    return now+600;
}
static void test_crank_rendered_five_character_release(const HelloKeyboard* reference) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    const float factors[]={2.2888889f,2.2386406f,1.9294705f,1.4104015f,1.20f};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int last=0;last<2;last++) {
        HelloKeyboard keyboard;uint32_t now=crank_render_entry(&keyboard,reference,group,last,90000);
        const int direction=last?1:-1;
        const int edge=keyboard.model.index;
        now=crank_render_travel(&keyboard,direction,tick*2.64f-0.05f,now);
        assert(keyboard.model.index==edge&&!keyboard.selection_lock_group);
        now=crank_render_travel(&keyboard,direction,0.051f,now);
        const int count=hello_keyboard_model_group_length(&keyboard.model,group);
        int index=direction>0?0:count-1;
        assert(keyboard.model.index==index&&keyboard.crank_release_steps==5);
        now=crank_test_finish_wrap(&keyboard,now);
        for(int step=0;step<5;step++) {
            assert(fabsf(crank_test_threshold(&keyboard,direction)-tick*factors[step])<0.001f);
            now=crank_render_travel(&keyboard,direction,tick*factors[step]-0.05f,now);
            assert(keyboard.model.index==index&&keyboard.crank_release_steps==5-step);
            now=crank_render_travel(&keyboard,direction,0.051f,now);
            index+=direction;
            assert(keyboard.model.index==index&&keyboard.crank_release_steps==4-step);
            assert(!keyboard.selection_lock_group);
            // Reverse at every phase of the existing wrap taper. Its current
            // well and normal reversal breakaway combine by their maximum,
            // so changing direction cannot stack two sticky barriers.
            HelloKeyboard reversed=keyboard;
            const float peak=fmaxf(factors[step==4?4:step+1],HELLO_KEYBOARD_CRANK_REVERSAL_FACTOR);
            const float leftover=reversed.crank_remainder;
            uint32_t reverse_now=crank_render_travel(&reversed,-direction,fabsf(leftover),now);
            assert(fabsf(reversed.crank_remainder)<0.002f);
            assert(fabsf(crank_test_threshold(&reversed,-direction)-tick*peak)<0.001f);
            reverse_now=crank_render_travel(&reversed,-direction,tick*peak-0.05f,reverse_now);
            assert(reversed.model.index==index&&reversed.crank_release_steps==4-step);
            reverse_now=crank_render_travel(&reversed,-direction,0.051f,reverse_now);
            assert(reversed.model.index==index-direction&&reversed.crank_release_steps==4);
            assert(fabsf(reversed.crank_release_peak-peak)<0.001f&&!reversed.selection_lock_group);
            const float first_phase=1-0.25f*0.25f*0.25f;
            const float next=1.20f+(peak-1.20f)*first_phase*first_phase*first_phase;
            // At the endpoint the next outward move is the unchanged group
            // barrier; elsewhere it is exactly the next OutCubic release well.
            if(step>0)assert(fabsf(crank_test_threshold(&reversed,-direction)-tick*next)<0.001f);
            if(step==4) {
                for(int back=1;back<5;back++) {
                    const float phase=(float)back/4;
                    const float remaining=1-phase*phase*phase;
                    const float distance=tick*(1.20f+(peak-1.20f)*remaining*remaining*remaining);
                    assert(fabsf(crank_test_threshold(&reversed,-direction)-distance)<0.001f);
                    reverse_now=crank_render_travel(&reversed,-direction,distance-0.05f,reverse_now);
                    assert(reversed.model.index==index-back*direction);
                    reverse_now=crank_render_travel(&reversed,-direction,0.051f,reverse_now);
                    assert(reversed.model.index==index-(back+1)*direction&&reversed.crank_release_steps==4-back);
                }
            }
        }
        assert(fabsf(crank_test_threshold(&keyboard,direction)-tick*1.20f)<0.001f);
        // No timer or rendering callback may manufacture another character.
        for(int frame=0;frame<10;frame++) {
            now+=20;hello_keyboard_update(&keyboard,now,0,0,0,0);
            draw_group(&keyboard,group,105,69,now);
            assert(keyboard.model.index==index&&!keyboard.model.text[0]);
        }
    }
}
static void test_first_crank_wrap_softer_then_full_boundary(const HelloKeyboard* reference) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,NULL,270000);
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),edge=keyboard.model.index;
        const float first=crank_test_threshold(&keyboard,direction);
        assert(!keyboard.crank_wrapped&&fabsf(first-tick*2.64f)<0.001f);
        uint32_t now=crank_render_travel(&keyboard,direction,first-0.05f,270000);
        assert(keyboard.model.index==edge&&!keyboard.crank_wrapped);
        now=crank_render_travel(&keyboard,direction,0.051f,now);
        const int endpoint=direction>0?0:count-1;
        assert(keyboard.model.index==endpoint&&keyboard.crank_wrapped&&keyboard.selection_lock_crank);
        // Discarded input during the seam preserves the one-time wrap state.
        now+=16;hello_keyboard_update(&keyboard,now,0,0,0,(float)direction*10000);
        assert(keyboard.model.index==endpoint&&!keyboard.crank_remainder&&keyboard.crank_wrapped);
        now=crank_test_finish_wrap(&keyboard,now);
        const float later=crank_test_threshold(&keyboard,-direction);
        assert(fabsf(later-tick*3.30f)<0.001f&&fabsf(first/later-0.80f)<0.00001f);
        now=crank_render_travel(&keyboard,-direction,later-0.05f,now);
        assert(keyboard.model.index==endpoint&&keyboard.crank_wrapped);
        now=crank_render_travel(&keyboard,-direction,0.051f,now);
        assert(keyboard.model.index==edge&&keyboard.selection_lock_crank&&keyboard.crank_release_steps==5);
        now=crank_test_finish_wrap(&keyboard,now);
        assert(keyboard.crank_wrapped&&!keyboard.crank_remainder);
        for(unsigned frame=0;frame<8;frame++) {
            now+=20;hello_keyboard_update(&keyboard,now,0,0,0,0);draw_group(&keyboard,group,105,69,now);
            assert(keyboard.model.index==edge&&keyboard.crank_wrapped&&!keyboard.model.text[0]);
        }
    }
}

static void test_crank_rendered_velocity_and_idle(const HelloKeyboard* reference) {
    const int frames_per_second[]={25,30,50};
    const float speeds[]={90,360,361,540,720,900,1080,1440};
    float final_velocity[3]={0};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(unsigned speed=0;speed<sizeof(speeds)/sizeof(speeds[0]);speed++)
    for(int cadence=0;cadence<3;cadence++) {
        const int fps=frames_per_second[cadence];
        HelloKeyboard forward,reverse;
        crank_test_prepare(&forward,reference,group,1,NULL,120000);
        crank_test_prepare(&reverse,reference,group,-1,NULL,120000);
        uint32_t now=crank_render_travel(&forward,1,crank_test_threshold(&forward,1)+0.001f,120000);
        const uint32_t other=crank_render_travel(&reverse,-1,crank_test_threshold(&reverse,-1)+0.001f,120000);
        assert(now==other);
        const uint32_t first_ready=crank_test_finish_wrap(&forward,now);
        const uint32_t last_ready=crank_test_finish_wrap(&reverse,now);
        now=first_ready>last_ready?first_ready:last_ready;
        hello_keyboard_update(&forward,now,0,0,0,0);hello_keyboard_update(&reverse,now,0,0,0,0);
        const int count=hello_keyboard_model_group_length(&forward.model,group);
        assert(forward.model.index==0&&reverse.model.index==count-1);
        const uint32_t begin=now;
        float previous=1;
        for(int frame=1;frame<=fps;frame++) {
            const uint32_t next=begin+(uint32_t)lroundf(1000.0f*(float)frame/(float)fps);
            // Equal physical speed at each cadence. Alternating loaded motion
            // keeps the same well so wrap gating cannot hide the speed curve.
            const float delta=speeds[speed]*(float)(next-now)*0.001f*(frame%2?1:-1);
            hello_keyboard_update(&forward,next,0,0,0,delta);
            hello_keyboard_update(&reverse,next,0,0,0,-delta);
            draw_group(&forward,group,105,69,next);draw_group(&reverse,group,105,69,next);
            assert(forward.model.index==0&&reverse.model.index==count-1);
            assert(forward.crank_release_steps==5&&reverse.crank_release_steps==5);
            assert(fabsf(forward.crank_remainder+reverse.crank_remainder)<0.002f);
            assert(fabsf(forward.crank_speed_factor-reverse.crank_speed_factor)<0.000001f);
            const float factor=forward.crank_speed_factor;
            assert(factor>=1&&factor<=1.100001f&&factor>=previous);
            assert(factor-previous<0.09f);
            if(speeds[speed]<=360)assert(factor==1);
            previous=factor;now=next;
        }
        if(speeds[speed]>=1080)assert(previous>1.099f);
        if(speeds[speed]==720)assert(previous>1.049f&&previous<1.051f);
        final_velocity[cadence]=forward.crank_velocity;
        if(cadence==2) {
            assert(fabsf(final_velocity[0]-final_velocity[1])<0.002f);
            assert(fabsf(final_velocity[0]-final_velocity[2])<0.002f);
        }
        const float pressure=forward.crank_remainder;
        // Sparse idle and a long suspension must never release queued travel.
        for(int frame=1;frame<=8;frame++) {
            now+=40;hello_keyboard_update(&forward,now,0,0,0,0);draw_group(&forward,group,105,69,now);
            assert(forward.model.index==0&&forward.crank_remainder==pressure);
        }
        now+=300;hello_keyboard_update(&forward,now,0,0,0,0);draw_group(&forward,group,105,69,now);
        assert(forward.model.index==0&&forward.crank_remainder==pressure);
        now+=20;hello_keyboard_update(&forward,now,0,0,0,1);draw_group(&forward,group,105,69,now);
        assert(forward.model.index==0&&forward.crank_speed_factor==1);
        assert(fabsf(forward.crank_remainder-pressure-1)<0.002f);
    }
}
static void test_crank_reversal_during_wrap_release(const HelloKeyboard* reference) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2)
    for(int advance=0;advance<=5;advance++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,NULL,10000);
        const int length=hello_keyboard_model_group_length(&keyboard.model,group);
        crank_test_selection(&keyboard,(float)direction*(crank_test_threshold(&keyboard,direction)+0.001f),10010);
        assert(keyboard.crank_release_steps==5);
        const uint32_t resumed=crank_test_finish_wrap(&keyboard,10010);
        for(int i=0;i<advance;i++)
            crank_test_selection(&keyboard,(float)direction*(crank_test_threshold(&keyboard,direction)+0.001f),resumed+20+(uint32_t)i);
        const int index=keyboard.model.index,remaining=keyboard.crank_release_steps;
        crank_test_selection(&keyboard,(float)direction*crank_test_threshold(&keyboard,direction)*0.4f,resumed+30);
        const float loaded=keyboard.crank_remainder;
        for(int i=0;i<100;i++) {
            crank_test_selection(&keyboard,-(float)direction*0.1f,resumed+31);
            crank_test_selection(&keyboard,(float)direction*0.1f,resumed+31);
        }
        assert(keyboard.model.index==index&&keyboard.crank_release_steps==remaining);
        assert(fabsf(keyboard.crank_remainder-loaded)<0.0001f);
        // Reverse through the exact old pressure, then load the opposite well.
        crank_test_selection(&keyboard,-loaded,resumed+32);
        assert(fabsf(keyboard.crank_remainder)<0.0001f);
        const float threshold=crank_test_threshold(&keyboard,-direction);
        crank_test_selection(&keyboard,-(float)direction*threshold*0.99f,resumed+34);
        assert(keyboard.model.index==index&&keyboard.crank_release_steps==remaining);
        crank_test_selection(&keyboard,-(float)direction*threshold*0.02f,resumed+35);
        assert(keyboard.model.index==(index-direction+length)%length);
        const bool wrapped=direction>0?index==0:index==length-1;
        assert(keyboard.crank_release_steps==(wrapped?5:4));
    }
    HelloKeyboard batched={0};hello_keyboard_model_init(&batched.model,"",0);
    batched.model.group=1;batched.model.index=9;batched.model.pending='!';
    HelloKeyboard sliced=batched;
    const float changes[]={64,31,-13,-47,18,-65,-41,52,8,-32,99,-48,250};
    for(size_t i=0;i<sizeof(changes)/sizeof(changes[0]);i++) {
        crank_test_selection(&batched,changes[i],100+(uint32_t)i);
        for(int j=0;j<100;j++)crank_test_selection(&sliced,changes[i]/100,100+(uint32_t)i);
        assert(batched.model.index==sliced.model.index);
        assert(batched.crank_release_steps==sliced.crank_release_steps);
        assert(batched.crank_release_direction==sliced.crank_release_direction);
        assert(batched.crank_wrapped==sliced.crank_wrapped);
        assert(fabsf(batched.crank_remainder-sliced.crank_remainder)<0.002f);
        assert(batched.selection_lock_crank==sliced.selection_lock_crank);
        if(batched.selection_lock_crank)assert(!batched.crank_remainder&&!sliced.crank_remainder);
    }
}

static void crank_update_enter(HelloKeyboard* keyboard,int group,bool last,int layer) {
    memset(keyboard,0,sizeof(*keyboard));
    hello_keyboard_default_config(&keyboard->config);
    hello_keyboard_model_init(&keyboard->model,"",0);
    keyboard->visible=true;
    keyboard->model.alternate=false;
    keyboard->model.upper=group==6||layer==1;
    if(group==HELLO_KEYBOARD_EMOJI_GROUP&&layer==1)keyboard->model.alternate=false;
    keyboard->model.field.type=(group==5||layer==2)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    const PDButtons direction=directions[group],face=last?kButtonB:kButtonA;
    hello_keyboard_update(keyboard,100,direction,direction,0,0);
    hello_keyboard_update(keyboard,101,direction,0,0,0);
    if(last) {
        const int available=group_bounds(keyboard,group).width-24;
        const KeyboardGroupMetrics content=group_base_metrics(keyboard,group,available);
        const KeyboardGroupViewport viewport=group_viewport(content,available,content.total,0);
        keyboard->group_scroll[group]=(HelloScrollFollow){.position=(float)(content.total+viewport.prefix_shift-viewport.width),.updated_at=101,.initialized=true};
    }
    hello_keyboard_update(keyboard,102,direction|face,face,0,0);
    hello_keyboard_update(keyboard,103,direction,0,face,0);
    assert(keyboard->model.group==group&&keyboard->model.pending);
}

static void test_crank_update_mirrors(void) {
    // Enter through the real update path: A selects the first item and B the
    // last visible item. Negating travel and mirroring the index must match before a
    // first detent, after a wrap, and through rapid loaded reversals.
    const float changes[]={0.005f,-0.005f,65,10,5,0,-0.125f,0.125f,38,44,
        -0.005f,0.005f,-12,-105,0.25f,-0.25f,-42,-66,18,72,250,-250,135,-80};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int layer=0;layer<3;layer++)
    for(int direction=-1;direction<=1;direction+=2) {
        if(layer==1&&group==1)continue; // Upper base page exposes emoji on Up.
        HelloKeyboard first,last;
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        crank_update_enter(&first,group,false,layer);
        crank_update_enter(&last,group,true,layer);
        const int count=hello_keyboard_model_group_length(&first.model,group);
        assert(first.model.index==0&&last.model.index==count-1);
        for(unsigned n=0;n<sizeof(changes)/sizeof(changes[0]);n++) {
            const float change=(float)direction*changes[n];
            hello_keyboard_update(&first,104+n,0,0,0,change);
            hello_keyboard_update(&last,104+n,0,0,0,-change);
            assert(first.model.index+last.model.index==count-1);
            assert(first.model.pending&&last.model.pending);
            assert(first.crank_release_steps==last.crank_release_steps);
            assert(first.crank_release_direction==-last.crank_release_direction);
            assert(first.crank_wrapped==last.crank_wrapped);
            assert(fabsf(first.crank_remainder+last.crank_remainder)<0.003f);
            assert(first.selection_lock_crank==last.selection_lock_crank);
            if(first.selection_lock_crank)assert(!first.crank_remainder&&!last.crank_remainder);
        }
        const int a=first.model.index,b=last.model.index;
        for(unsigned n=0;n<400;n++) {
            const float change=n%2?0.005f:-0.005f;
            hello_keyboard_update(&first,128+n,0,0,0,change);
            hello_keyboard_update(&last,128+n,0,0,0,-change);
            assert(first.model.index==a&&last.model.index==b);
        }
    }
}

static void select_group_item(HelloKeyboard* keyboard,int group,int index) {
    keyboard->model.group=group;keyboard->model.index=index;
    hello_keyboard_model_group_text(&keyboard->model,group,index,keyboard->model.pending_text,sizeof(keyboard->model.pending_text));
    hello_keyboard_utf8_decode(keyboard->model.pending_text,&keyboard->model.pending);
}
static void check_group_view(HelloKeyboard* keyboard,int group,uint32_t now) {
    group_draw_count=0;marker_draws=0;
    draw_group(keyboard,group,105,100,now);
    assert(!group_clip_active && group_draw_count>0);
    assert(group_pill_w==test_group_pair_width(keyboard,group));
    assert(group_clip_w<=group_pill_w-24);
    const int left_margin=group_clip_x-group_pill_x;
    const int right_margin=group_pill_x+group_pill_w-group_clip_x-group_clip_w;
    assert(abs(left_margin-right_margin)<=1);
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
    assert(group_pill_x==bounds.x);
    if(group==2||group==4) {
        assert(group_pill_w<=188&&group_pill_x>=8&&group_pill_x+group_pill_w<=392);
        if(group==4)assert(group_pill_x+group_pill_w<=196);
        else assert(group_pill_x>=204);
    }
    for(int i=1;i<group_draw_count;i++)
        assert(group_item_x[i]>=group_item_x[i-1]+group_item_width[i-1]+4);
    if(keyboard->model.group==group&&keyboard->model.pending) {
        assert(marker_draws==1 && marker_x>=group_clip_x && marker_x+marker_w<=group_clip_x+group_clip_w);
        assert(marker_y>=group_clip_y && marker_y+marker_h<=group_clip_y+group_clip_h);
    }
}
static void test_group_viewport(HelloKeyboard* keyboard) {
    // Normal short layers keep their existing centres and never start scrolling.
    hello_keyboard_model_init(&keyboard->model,"",0);
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    keyboard->config.font_size=15;
    for(int group=1;group<HELLO_KEYBOARD_EMOJI_GROUP;group++) {
        select_group_item(keyboard,group,0);draw_group(keyboard,group,105,100,1);
        assert(keyboard->group_scroll[group].position==0);
        const int length=hello_keyboard_model_group_length(&keyboard->model,group);
        select_group_item(keyboard,group,length-1);draw_group(keyboard,group,105,100,2);
        assert(keyboard->group_scroll[group].position>=0);
        const int partner=group==1?3:group==3?1:group==2?4:group==4?2:0;
        if(partner&&length==hello_keyboard_model_group_length(&keyboard->model,partner))
            assert(test_group_pair_width(keyboard,group)==test_group_pair_width(keyboard,partner));
    }
    keyboard->config.font_size=10;
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?",
        "ab🧡c👍🏽d🇩🇪e👨‍👩‍👧‍👦fghijklmnopqrstuvwxyz",NULL,NULL,
        "0123456789012345678901234567890123456789012345678901234567890123"};
    hello_keyboard_model_init_with_groups(&keyboard->model,"",0,groups);
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    keyboard->selection_marker.active=false;capture_group_view=true;
    select_group_item(keyboard,1,0);check_group_view(keyboard,1,1000);
    assert(keyboard->group_scroll[1].position==0 && group_draw_count<64);
    select_group_item(keyboard,1,20);check_group_view(keyboard,1,1016);
    const float first=keyboard->group_scroll[1].position;
    assert(first>0 && first<20*16-70); // Visible immediately, not yet centred.
    check_group_view(keyboard,1,1032);assert(keyboard->group_scroll[1].position>first);
    for(uint32_t t=1048;t<2000;t+=16)check_group_view(keyboard,1,t);
    const float middle=keyboard->group_scroll[1].position;
    assert(fabsf(middle-(20*17+8-(float)group_clip_w/2))<1);
    // Immediate reversals and wrap never leak marker/text into adjacent pills.
    for(int i=0;i<80;i++) {
        select_group_item(keyboard,1,i%3==0?63:i%3==1?0:31);
        check_group_view(keyboard,1,2000+(uint32_t)i*16);
        assert(isfinite(keyboard->group_scroll[1].position));
    }
    // Inactive groups begin a smooth return rather than snapping to zero.
    const float old_scroll=keyboard->group_scroll[1].position;
    select_group_item(keyboard,2,4);check_group_view(keyboard,1,3400);
    assert(keyboard->group_scroll[1].position>0&&keyboard->group_scroll[1].position<old_scroll);
    check_group_view(keyboard,2,3400);
    select_group_item(keyboard,2,hello_keyboard_model_group_length(&keyboard->model,2)-1);check_group_view(keyboard,2,3416);
    assert(keyboard->group_scroll[2].position>0);
    strcpy(keyboard->model.pending_text,"WIDE");keyboard->model.pending='W';
    check_group_view(keyboard,2,3432); // Variant wider than its configured item.
    select_group_item(keyboard,5,63);check_group_view(keyboard,5,3450);
    check_group_view(keyboard,5,3460);
    assert(keyboard->group_scroll[5].position>0);
    hello_keyboard_model_commit(&keyboard->model,false);
    assert(!strcmp(keyboard->model.text,"3")); // Draw/viewport never changes input.
    const float committed_scroll=keyboard->group_scroll[5].position;
    check_group_view(keyboard,5,3466);
    assert(keyboard->group_scroll[5].position>0&&keyboard->group_scroll[5].position<committed_scroll);
    // The first/last selected cap must remain fully round, including with
    // taller native fonts. No flat clipping at either end of the viewport.
    keyboard->config.font_size=15;
    for(int endpoint=0;endpoint<2;endpoint++) {
        select_group_item(keyboard,1,endpoint?63:0);
        for(uint32_t t=4000;t<=5400;t+=20)check_group_view(keyboard,1,t+(uint32_t)endpoint*1600);
        assert(marker_w>=21&&marker_h==group_clip_h&&marker_y==group_clip_y);
        assert(marker_x>=group_clip_x && marker_x+marker_w<=group_clip_x+group_clip_w);
    }
    keyboard->config.font_size=10;
    capture_group_view=false;
}

static void check_sizing_fixture(HelloKeyboard* keyboard,int group,int expected_width,bool snapped,uint32_t now) {
    const int count=hello_keyboard_model_group_length(&keyboard->model,group);
    const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
    assert(bounds.width==expected_width);
    if(group==2)assert(bounds.x+bounds.width==392);
    else if(group==4)assert(bounds.x==8);
    else assert(abs(2*bounds.x+bounds.width-400)<=1);
    capture_group_view=true;
    for(int endpoint=0;endpoint<2;endpoint++) {
        select_group_item(keyboard,group,endpoint?count-1:0);
        for(unsigned frame=0;frame<=30;frame++) {
            check_group_view(keyboard,group,now);
            if(snapped)assert(group_clip_x==bounds.x+12&&group_clip_w==expected_width-24);
            for(int glyph=0;glyph<group_draw_count;glyph++)assert(group_item_x[glyph]>=group_clip_x&&
                group_item_x[glyph]+group_item_width[glyph]<=group_clip_x+group_clip_w);
            now+=20;
        }
    }
    capture_group_view=false;
}
static void test_equal_counts_and_independent_snapped_group_widths(const HelloKeyboard* reference) {
    HelloKeyboard keyboard={0};keyboard.pd=reference->pd;keyboard.config=reference->config;
    keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
    const char* equal[HELLO_KEYBOARD_GROUP_COUNT]={"WWWWWWWW","WWWWWWWW","iiiiiiii","iiiiiiii",NULL,NULL};
    hello_keyboard_model_init_with_groups(&keyboard.model,"",0,equal);wide_test_w=true;
    assert(group_bounds(&keyboard,1).width==group_bounds(&keyboard,3).width);
    assert(group_bounds(&keyboard,2).width==group_bounds(&keyboard,4).width);
    check_sizing_fixture(&keyboard,1,192,false,300000);check_sizing_fixture(&keyboard,3,192,false,302000);
    check_sizing_fixture(&keyboard,2,168,false,304000);check_sizing_fixture(&keyboard,4,168,false,306000);
    wide_test_w=false;hello_keyboard_model_init(&keyboard.model,"",0);reset_group_scrolls(&keyboard);
    assert(hello_keyboard_model_group_length(&keyboard.model,1)==40&&hello_keyboard_model_group_length(&keyboard.model,3)==9);
    check_sizing_fixture(&keyboard,1,297,true,308000);check_sizing_fixture(&keyboard,3,213,true,310000);
    keyboard.model.upper=true;
    assert(hello_keyboard_model_group_length(&keyboard.model,6)==21);
    check_sizing_fixture(&keyboard,6,300,true,312000);check_sizing_fixture(&keyboard,3,213,true,314000);
    keyboard.model.upper=false;keyboard.model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    check_sizing_fixture(&keyboard,5,234,true,316000);
    const char* unequal[HELLO_KEYBOARD_GROUP_COUNT]={NULL,"abcdefghi",NULL,"abcdefghijkl",NULL,NULL};
    hello_keyboard_model_init_with_groups(&keyboard.model,"",0,unequal);reset_group_scrolls(&keyboard);
    assert(!group_has_overflow(&keyboard,2)&&group_has_overflow(&keyboard,4));
    check_sizing_fixture(&keyboard,2,186,true,318000);check_sizing_fixture(&keyboard,4,186,true,320000);
    unequal[1]="WWWWWWWWW";unequal[3]="iiiiiiiiiiii";
    hello_keyboard_model_init_with_groups(&keyboard.model,"",0,unequal);reset_group_scrolls(&keyboard);wide_test_w=true;
    check_sizing_fixture(&keyboard,2,164,true,322000);check_sizing_fixture(&keyboard,4,186,true,324000);
    wide_test_w=false;
}

static void test_approved_emoji_pack_geometry(HelloKeyboard* keyboard) {
    HelloKeyboardModel* model=&keyboard->model;
    hello_keyboard_model_init(model,"",0);model->upper=true;
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    BrbUnicodeFont pack={0};pack.pd=keyboard->pd;
    keyboard->config.unicode_font=&pack;keyboard->config.font=NULL;keyboard->config.font_size=9;
    capture_group_view=true;capture_pack_draw=true;
    const BrbEmojiSet* set=brb_emoji_everyday_set();assert(set->count==21);
    for(unsigned index=0;index<set->count;index++) {
        select_group_item(keyboard,HELLO_KEYBOARD_EMOJI_GROUP,(int)index);
        group_draw_count=marker_draws=pack_rects=0;
        draw_group(keyboard,HELLO_KEYBOARD_EMOJI_GROUP,105,69,8000+index*16);
        assert(!group_clip_active&&group_pill_w==test_group_pair_width(keyboard,6)&&abs(2*group_pill_x+group_pill_w-400)<=1);
        assert(pack_rects>0&&marker_draws==1);
        assert(marker_w>=set->glyphs[index].width+4);
        assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
        assert(keyboard_text_height(keyboard,set->glyphs[index].utf8)==14);
        assert(keyboard_text_width(keyboard,set->glyphs[index].utf8)==set->glyphs[index].advance-1);
    }
    capture_pack_draw=false;keyboard->config.unicode_font=NULL;
    keyboard->config.font=(LCDFont*)&bitmap;keyboard->config.font_size=10;
    // A medium top group grows exactly to its complete natural cell budget,
    // stays centred, and does not scroll prematurely below the 300px cap.
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={"abcdefghijklmn",NULL,"abcdefghijklmn",NULL,NULL,NULL};
    hello_keyboard_model_init_with_groups(model,"",0,groups);select_group_item(keyboard,1,13);
    group_draw_count=0;draw_group(keyboard,1,105,69,9000);
    assert(group_pill_w==262&&group_pill_x==69&&group_draw_count==14);
    assert(keyboard->group_scroll[1].position==0);
    capture_group_view=false;
}

static void test_commit_homing_and_early_reactivation(HelloKeyboard* keyboard) {
    for(int button=0;button<2;button++) {
        hello_keyboard_model_init(&keyboard->model,"",0);keyboard->model.upper=true;
        memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
        hello_input_field_init(&keyboard->input_field,HELLO_INPUT_PLAINTEXT);
        keyboard->visible=true;keyboard->requested_action=HELLO_KEYBOARD_NONE;
        keyboard->selection_marker.active=false;
        const uint32_t begin=10000+(uint32_t)button*5000;
        hello_keyboard_update(keyboard,begin,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(keyboard,begin+1,kButtonUp,0,0,HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM*12);
        // The first SDK sample now presents its entry item only; this fresh
        // second turn moves far enough to exercise the existing homing path.
        hello_keyboard_update(keyboard,begin+17,kButtonUp,0,0,HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM*12);
        hello_keyboard_update(keyboard,begin+33,kButtonUp,0,0,HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM*12);
        assert(keyboard->model.group==HELLO_KEYBOARD_EMOJI_GROUP&&keyboard->model.pending);
        for(uint32_t t=begin+33;t<begin+600;t+=16)draw_group(keyboard,6,105,69,t);
        const float before=keyboard->group_scroll[6].position;assert(before>20);
        // Matching D-pad release commits and starts homing before the next draw.
        hello_keyboard_update(keyboard,begin+800,0,0,kButtonUp,0);
        assert(!keyboard->model.pending&&!keyboard->model.group);
        float returning=keyboard->group_scroll[6].position;
        assert(returning>0&&returning<before);
        for(uint32_t t=begin+816;t<begin+880;t+=16) {
            hello_keyboard_update(keyboard,t,0,0,0,0);draw_group(keyboard,6,105,69,t);
            assert(keyboard->group_scroll[6].position>0&&keyboard->group_scroll[6].position<returning);
            returning=keyboard->group_scroll[6].position;
        }
        hello_keyboard_update(keyboard,begin+880,kButtonUp,kButtonUp,0,0);
        const PDButtons face=button==0?kButtonA:kButtonB;
        const int expected_index=button==0?0:group_last_visible_item(keyboard);
        hello_keyboard_update(keyboard,begin+896,kButtonUp|face,face,0,0);
        assert(keyboard->model.index==expected_index);
        const float before_draw=keyboard->group_scroll[6].position;
        capture_group_view=true;group_draw_count=0;marker_draws=0;
        draw_group(keyboard,6,105,69,begin+896);
        assert(!group_clip_active&&group_draw_count>0);
        assert(marker_draws==1); // First RTL selection uses the currently visible endpoint.
        assert(keyboard->group_scroll[6].position>0); // No hard reset during homing.
        if(button==0)assert(keyboard->group_scroll[6].position<before_draw);
        else assert(keyboard->group_scroll[6].position>before_draw);
        assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
        capture_group_view=false;
        // Confirmed Save retains the validation view and begins the same homing path.
        keyboard->requested_action=HELLO_KEYBOARD_DONE;
        const float done_before=keyboard->group_scroll[6].position;
        assert(hello_keyboard_update(keyboard,begin+912,0,0,face,0)==HELLO_KEYBOARD_NONE);
        assert(save_editing_popup(keyboard,begin+913)==HELLO_KEYBOARD_DONE);
        assert(keyboard->group_scroll[6].position>=0&&keyboard->group_scroll[6].position<=done_before);
        // A case change does not suspend return of the now-hidden emoji group.
        keyboard->model.upper=false;
        hello_keyboard_model_direction(&keyboard->model,HELLO_KB_LEFT,begin+1280);
        float old=keyboard->group_scroll[6].position;
        for(uint32_t t=begin+1280;t<begin+1652;t+=16) {
            hello_keyboard_update(keyboard,t,0,0,0,0);
            assert(keyboard->group_scroll[6].position<=old);old=keyboard->group_scroll[6].position;
        }
        for(uint32_t t=begin+1652;t<begin+3852;t+=16)hello_keyboard_update(keyboard,t,0,0,0,0);
        assert(keyboard->group_scroll[6].position==0);
    }
    keyboard->visible=false;
}
static void test_last_entry_matches_home_curve(HelloKeyboard* keyboard) {
    static HelloKeyboard home;
    const int groups[]={HELLO_KEYBOARD_EMOJI_GROUP,1};
    for(unsigned variant=0;variant<sizeof(groups)/sizeof(groups[0]);variant++) {
        const int group=groups[variant];const uint32_t begin=50000+variant*10000;
        hello_keyboard_model_init(&keyboard->model,"",0);reset_crank(keyboard);
        keyboard->config.font_size=variant==0?10:15;
        keyboard->model.upper=group==HELLO_KEYBOARD_EMOJI_GROUP;
        keyboard->visible=true;keyboard->requested_action=HELLO_KEYBOARD_NONE;
        hello_input_field_init(&keyboard->input_field,HELLO_INPUT_PLAINTEXT);
        memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
        keyboard->selection_marker.active=false;
        hello_keyboard_update(keyboard,begin,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(keyboard,begin+16,kButtonUp,0,0,0);
        draw_group(keyboard,group,105,69,begin+16);
        const HelloScrollFollow initial=keyboard->group_scroll[group];
        assert(initial.position==0);
        hello_keyboard_model_press(&keyboard->model,HELLO_KB_BUTTON_B,begin+32);
        const int last=hello_keyboard_model_group_length(&keyboard->model,group)-1;
        assert(keyboard->model.index==last&&keyboard->model.pending_side==2);
        const uint32_t selected=keyboard->model.pending;
        // Measure the endpoint using the real layout without changing selection.
        for(uint32_t elapsed=32;elapsed<3000;elapsed+=16)
            draw_group(keyboard,group,105,69,begin+elapsed);
        const float maximum=keyboard->group_scroll[group].position;assert(maximum>20);
        keyboard->group_scroll[group]=initial;keyboard->selection_marker.active=false;
        home=*keyboard;home.group_scroll[group].position=maximum;
        home.model.group=0;home.model.pending=0;memset(home.model.hold,0,sizeof(home.model.hold));
        bool saw_entered_marker=false;
        capture_group_view=true;
        for(uint32_t elapsed=32;elapsed<2500;elapsed+=16) {
            const uint32_t now=begin+elapsed;
            group_draw_count=marker_draws=0;draw_group(keyboard,group,105,69,now);
            assert(!group_clip_active&&group_draw_count>0);
            if(elapsed==32)assert(!marker_draws); // No endpoint teleported onto the right edge.
            if(marker_draws) {
                saw_entered_marker=true;
                assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
            }
            for(int item=0;item<group_draw_count;item++)
                assert(group_item_x[item]>=group_clip_x&&
                    group_item_x[item]+group_item_width[item]<=group_clip_x+group_clip_w);
            assert(keyboard->model.index==last&&keyboard->model.pending==selected&&keyboard->model.pending_side==2);
            hello_keyboard_update(&home,now,0,0,0,0);
            group_draw_count=0;draw_group(&home,group,105,69,now);
            // Entry and home share the same finite profile, including the
            // endpoint and duplicate update/draw visits at one timestamp.
            const float entry=keyboard->group_scroll[group].position;
            const float returning=home.group_scroll[group].position;
            assert(fabsf(entry+returning-maximum)<0.002f);
        }
        assert(saw_entered_marker&&keyboard->group_scroll[group].position==maximum&&home.group_scroll[group].position==0);
        capture_group_view=false;
        // Re-enter while the previous return still runs: retarget current
        // position on the same follow instance, with no reset or direction flip.
        hello_keyboard_model_commit(&keyboard->model,false);
        hello_keyboard_update(keyboard,begin+2600,0,0,kButtonB,0);
        const float returning=keyboard->group_scroll[group].position;assert(returning>0&&returning<maximum);
        hello_keyboard_update(keyboard,begin+2616,kButtonUp,kButtonUp,0,0);
        const HelloScrollFollow before=keyboard->group_scroll[group];
        hello_keyboard_model_press(&keyboard->model,HELLO_KB_BUTTON_B,begin+2632);
        assert(keyboard->group_scroll[group].position==before.position);
        HelloScrollFollow expected=before;
        HelloKeyboardScrollMotion expected_motion=keyboard->group_scroll_motion[group];
        hello_keyboard_scroll_follow_update(&expected,&expected_motion,maximum,begin+2632,true);
        draw_group(keyboard,group,105,69,begin+2632);
        assert(fabsf(keyboard->group_scroll[group].position-expected.position)<0.002f);
        assert(keyboard->model.index==last&&keyboard->model.pending==selected&&keyboard->model.pending_side==2);
    }
    keyboard->config.font_size=10;keyboard->visible=false;
}

static void test_non_endpoint_follow_unchanged(void) {
    HelloScrollFollow control={0},keyboard={0};HelloKeyboardScrollMotion motion={0};
    uint32_t now=UINT32_MAX-500;
    for(unsigned frame=0;frame<300;frame++) {
        const float target=(float)((frame/7)%5)*81;
        now+=frame%3==0?20:frame%3==1?40:80;
        const float before=hello_scroll_follow_update(&control,target,now,true);
        const float after=hello_keyboard_scroll_follow_update(&keyboard,&motion,target,now,false);
        assert(before==after&&control.updated_at==keyboard.updated_at&&control.initialized==keyboard.initialized);
        assert(motion.remaining_ms==0);
    }
    const float position=keyboard.position;
    hello_keyboard_scroll_follow_update(&keyboard,&motion,400,now,true);
    assert(keyboard.position==position); // Same-time endpoint retarget cannot jump.
    hello_keyboard_scroll_follow_update(&keyboard,&motion,400,now+540,true);
    assert(keyboard.position==400&&motion.remaining_ms==0);
}

static void test_visible_rtl_entry_waits_for_direction_release(HelloKeyboard* keyboard) {
    static BrbUnicodeFont native_font;
    native_font.pd=keyboard->pd;
    const int groups[]={1,HELLO_KEYBOARD_EMOJI_GROUP};
    for(unsigned group_at=0;group_at<2;group_at++)
    for(uint32_t frame_ms=20;frame_ms<=40;frame_ms+=20) {
        const int group=groups[group_at];const uint32_t begin=90000+group_at*10000+frame_ms*100;
        hello_keyboard_model_init(&keyboard->model,"",0);reset_crank(keyboard);
        keyboard->model.upper=group==HELLO_KEYBOARD_EMOJI_GROUP;
        keyboard->config.font=NULL;keyboard->config.font_size=15;keyboard->config.unicode_font=&native_font;
        keyboard->visible=true;keyboard->requested_action=HELLO_KEYBOARD_NONE;
        memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));keyboard->selection_marker.active=false;
        hello_input_field_init(&keyboard->input_field,HELLO_INPUT_PLAINTEXT);
        hello_keyboard_update(keyboard,begin-frame_ms*2,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(keyboard,begin-frame_ms,kButtonUp,0,0,0);
        capture_group_view=true;capture_pack_draw=group==HELLO_KEYBOARD_EMOJI_GROUP;
        capture_bitmap_text=true;drawn_glyphs=0;draw_group(keyboard,group,105,69,begin-frame_ms);
        if(group==1) {
            const char* prefix="123456789";
            assert(drawn_glyphs>=9&&drawn_glyphs<35); // Euro follows the digit-prefixed punctuation.
            for(int i=0;i<9;i++)assert(glyph_frames[i]==prefix[i]-32);
            for(int i=0;i<drawn_glyphs;i++) {
                assert(glyph_x[i]>=group_clip_x);
                assert(glyph_x[i]+glyph_width((unsigned char)(glyph_frames[i]+32))<=group_clip_x+group_clip_w);
            }
            assert(keyboard_text_width(keyboard,"€")==16); // One Unicode cell, not three byte cells.
        }
        uint32_t visible_at=UINT32_MAX;
        char selected[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1]={0};
        for(uint32_t elapsed=0;elapsed<=1800;elapsed+=frame_ms) {
            hello_keyboard_update(keyboard,begin+elapsed,kButtonUp|(elapsed==0?kButtonB:0),
                elapsed==0?kButtonB:0,elapsed==frame_ms?kButtonB:0,0);
            if(elapsed==0)hello_keyboard_model_pending_text(&keyboard->model,selected,sizeof(selected));
            drawn_glyphs=marker_draws=0;draw_group(keyboard,group,105,69,begin+elapsed);
            assert(!keyboard->selection_lock_group||keyboard->selection_lock_group==group); // Offscreen initial entry waits for rendering.
            if(keyboard->model.pending&&marker_draws) {
                assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
                if(visible_at==UINT32_MAX)visible_at=elapsed;
            }
            assert(keyboard->model.pending&&!keyboard->model.text[0]);
        }
        capture_bitmap_text=false;capture_group_view=false;capture_pack_draw=false;
        assert(visible_at==0);
        hello_keyboard_update(keyboard,begin+100000,kButtonUp,0,kButtonDown,0);
        assert(keyboard->model.pending&&!keyboard->model.text[0]&&keyboard->model.group==group);
        hello_keyboard_update(keyboard,begin+100001,0,0,kButtonUp,0);
        assert(!keyboard->model.pending&&!keyboard->model.group&&!strcmp(keyboard->model.text,selected));
        hello_keyboard_update(keyboard,begin+100002,0,0,kButtonUp,0);
        assert(!strcmp(keyboard->model.text,selected));
    }
    keyboard->config.font=(LCDFont*)&bitmap;keyboard->config.unicode_font=NULL;
    keyboard->config.font_size=10;keyboard->visible=false;
}

static void test_scroll_session_lifecycle(HelloKeyboard* keyboard) {
    for(int session=0;session<3;session++) {
        const uint32_t begin=150000+(uint32_t)session*10000;
        keyboard->visible=false;keyboard->config.font_size=session==1?10:15;
        keyboard->config.font=(LCDFont*)&bitmap;keyboard->config.unicode_font=NULL;
        keyboard->group_scroll[1]=(HelloScrollFollow){.position=100,.updated_at=begin-20,.initialized=true};
        keyboard->group_scroll_motion[1]=(HelloKeyboardScrollMotion){.target=500,.remaining_ms=300};
        assert(hello_keyboard_open(keyboard,""));
        for(int group=0;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
            assert(!keyboard->group_scroll[group].initialized&&!keyboard->group_scroll_motion[group].remaining_ms&&
                keyboard->group_scroll_motion[group].target==0);
        hello_keyboard_update(keyboard,begin,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(keyboard,begin+20,kButtonUp,0,0,0);
        draw_group(keyboard,1,105,69,begin+20);
        hello_keyboard_update(keyboard,begin+40,kButtonB,kButtonB,0,0);
        draw_group(keyboard,1,105,69,begin+40);
        assert(keyboard->group_scroll_motion[1].remaining_ms==HELLO_KEYBOARD_SCROLL_ENDPOINT_MS-20);
        clear_callback(keyboard);
        assert(!keyboard->group_scroll[1].initialized&&!keyboard->group_scroll_motion[1].remaining_ms);
        hello_keyboard_close(keyboard);
        assert(!keyboard->group_scroll_motion[1].remaining_ms);
    }
    keyboard->config.font_size=10;
}

static bool count_selection_transform(const char* input,HelloKeyboardTransformAction action,
    char* output,size_t capacity,void* userdata) {
    (void)action;(*(int*)userdata)++;assert(strlen(input)<capacity);strcpy(output,input);return true;
}
static void test_immediate_crank_entry_and_face_button_isolation(HelloKeyboard* keyboard) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)for(int sign=-1;sign<=1;sign+=2) {
        hello_keyboard_model_init(&keyboard->model,"",0);
        keyboard->model.group=group;keyboard->model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;keyboard->model.upper=group==6;
        int transforms=0;keyboard->model.transform=count_selection_transform;keyboard->model.transform_userdata=&transforms;
        keyboard->visible=true;keyboard->requested_action=HELLO_KEYBOARD_NONE;
        reset_group_scrolls(keyboard);reset_crank(keyboard);keyboard->selection_marker.active=false;
        marker_draws=0;draw_group(keyboard,group,105,69,20000);assert(!marker_draws);
        const int expected=sign<0?group_last_visible_item(keyboard):0;
        hello_keyboard_update(keyboard,20016,0,0,0,(float)sign*0.01f);
        assert(!keyboard->model.pending&&!transforms);
        hello_keyboard_update(keyboard,20032,0,0,0,(float)sign*360);
        assert(keyboard->model.pending&&keyboard->model.index==expected&&transforms==1);
        assert(keyboard->crank_entry_breakaway&&keyboard->crank_release_direction==sign&&!keyboard->crank_remainder&&!keyboard->crank_wrapped);
        marker_draws=0;draw_group(keyboard,group,105,69,20032);assert(marker_draws==1);
        for(uint32_t now=20048;now<=20512;now+=16) {
            hello_keyboard_update(keyboard,now,0,0,0,0);draw_group(keyboard,group,105,69,now);
            assert(keyboard->model.index==expected&&!keyboard->model.text[0]&&transforms==1);
        }
        const PDButtons face=sign>0?kButtonA:kButtonB;
        hello_keyboard_update(keyboard,20528,face,face,0,0);
        assert(keyboard->model.index==expected+sign&&transforms==2);
        assert(!keyboard->crank_entry_breakaway&&!keyboard->crank_remainder&&!keyboard->crank_release_direction);
        hello_keyboard_update(keyboard,20536,0,0,face,0);
    }
    hello_keyboard_model_init(&keyboard->model,"123456",6);
    keyboard->model.field.max_length=6;keyboard->model.field.type=HELLO_INPUT_CONTACT_NUMBER;keyboard->model.group=1;
    reset_crank(keyboard);keyboard->selection_marker.active=false;
    hello_keyboard_update(keyboard,22000,0,0,0,-360);
    marker_draws=0;draw_group(keyboard,1,105,69,22000);
    assert(keyboard->model.pending&&marker_draws==1&&keyboard->crank_entry_breakaway&&!strcmp(keyboard->model.text,"123456"));
    hello_keyboard_update(keyboard,22016,0,0,kButtonUp,0);
    assert(!keyboard->model.pending&&!strcmp(keyboard->model.text,"123456"));
    keyboard->visible=false;reset_crank(keyboard);
}

static void test_no_cross_layout_and_alphabet_reachability(HelloKeyboard* keyboard) {
    const char* compact[HELLO_KEYBOARD_GROUP_COUNT]={"ab","ab🇩🇪cd","cd","abcdefghijk", "12", "❤️😊"};
    const char* broad[HELLO_KEYBOARD_GROUP_COUNT]={"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?",NULL,
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?",NULL,NULL,NULL};
    for(int variant=0;variant<3;variant++) {
        hello_keyboard_model_init_with_groups(&keyboard->model,"",0,variant==0?NULL:variant==1?compact:broad);
        for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
            const HelloKeyboardPillBounds bounds=group_bounds(keyboard,group);
            capture_group_view=true;group_draw_count=0;keyboard->model.group=0;
            draw_group(keyboard,group,105,100,30000+(uint32_t)group);
            assert(!group_clip_active&&group_draw_count>0);
            if(group==2||group==4) {
                assert(group_pill_w<=188&&group_pill_x>=8&&group_pill_x+group_pill_w<=392);
                if(group==4)assert(group_pill_x+group_pill_w<=196);
                else assert(group_pill_x>=204);
            } else assert(group_pill_x==bounds.x&&group_pill_w==bounds.width&&group_pill_w<=300);
            capture_group_view=false;
        }
    }
    for(int method=0;method<3;method++) {
        bool seen[26]={0};
        for(int group=2;group<=4;group++) {
            hello_keyboard_model_init(&keyboard->model,"",0);keyboard->model.group=group;reset_crank(keyboard);
            const int count=hello_keyboard_model_group_length(&keyboard->model,group);
            for(int n=0;n<count;n++) {
                if(method<2) {
                    const HelloKeyboardButton button=method==0?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B;
                    hello_keyboard_model_press(&keyboard->model,button,31000+(uint32_t)n*50);
                    hello_keyboard_model_release(&keyboard->model,button,31010+(uint32_t)n*50);
                } else crank_test_selection(keyboard,crank_test_threshold(keyboard,1)+0.001f,31000+(uint32_t)n*50);
                const uint32_t cp=keyboard->model.pending;
                if(cp>='a'&&cp<='z')seen[cp-'a']=true;
            }
        }
        for(int letter=0;letter<26;letter++)assert(seen[letter]);
    }
}

static void test_complete_default_side_groups_and_long_overflow(HelloKeyboard* keyboard) {
    hello_keyboard_model_init(&keyboard->model,"",0);reset_crank(keyboard);
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    capture_group_view=true;
    for(int group=2;group<=4;group+=2) {
        group_draw_count=0;draw_group(keyboard,group,105,100,40000);
        assert(group_draw_count==9); // Every default letter is fully rendered, not clipped.
        assert(group_pill_w==186&&group_pill_x==(group==4?8:206));
        assert(keyboard->group_scroll[group].position==0);
        for(int i=0;i<group_draw_count;i++) {
            assert(group_item_x[i]>=group_clip_x);
            assert(group_item_x[i]+group_item_width[i]<=group_clip_x+group_clip_w);
        }
    }
    const char* groups[HELLO_KEYBOARD_GROUP_COUNT]={NULL,"abcdefghijklmnopqrstuvwxyz",NULL,"abcdefghijklmnopqrstuvwxyz",NULL,NULL};
    hello_keyboard_model_init_with_groups(&keyboard->model,"",0,groups);
    memset(keyboard->group_scroll,0,sizeof(keyboard->group_scroll));
    for(int group=2;group<=4;group+=2) {
        select_group_item(keyboard,group,25);group_draw_count=marker_draws=0;
        draw_group(keyboard,group,105,100,40016);
        assert(group_pill_w==188&&group_pill_x==(group==4?8:204));
        assert(group_draw_count<26&&group_draw_count>0&&marker_draws==1);
        assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
        draw_group(keyboard,group,105,100,40032);
        assert(keyboard->group_scroll[group].position>0);
    }
    capture_group_view=false;
}

static uint8_t native_font_height(LCDFont* font) {(void)font;return 14;}
static LCDBitmap* unexpected_selection_bitmap(int width,int height,LCDColor color) {
    (void)width;(void)height;(void)color;assert(!"keyboard allocated a selected-item bitmap");return NULL;
}
static void test_selection_native_size_and_height(PlaydateAPI* pd) {
    struct playdate_graphics* graphics=(struct playdate_graphics*)pd->graphics;
    graphics->newBitmap=unexpected_selection_bitmap;
    graphics->drawScaledBitmap=scaled_bitmap;graphics->getFontHeight=native_font_height;
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;config.group_height=30;
    BrbUnicodeFont unicode={.pd=pd};config.unicode_font=&unicode;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,pd,&config);
    capture_group_view=true;
    uint32_t item_time=50000;
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        hello_keyboard_model_init(&keyboard.model,"",0);
        const int count=hello_keyboard_model_group_length(&keyboard.model,group);
        for(int index=0;index<count;index++,item_time+=1000) {
            select_group_item(&keyboard,group,index);keyboard.model.pending_side=3;
            reset_group_scrolls(&keyboard);keyboard.selection_marker.active=false;
            int native_height=0;
            for(uint32_t elapsed=0;elapsed<=640;elapsed+=80) {
                group_draw_count=marker_draws=0;draw_group(&keyboard,group,105,69,item_time+elapsed);
                assert(group_pill_h==30&&group_clip_h==26);
                assert(marker_draws==1&&marker_h==26&&marker_y==group_clip_y);
                if(!elapsed)native_height=marker_h;
                assert(marker_h==native_height);
                assert(marker_h==group_clip_h);
                assert(marker_y>=group_clip_y&&marker_y+marker_h<=group_clip_y+group_clip_h);
                assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
            }
        }
    }
    // Variants and complete graphemes use the same native text path immediately.
    const char* variants[]={"a","A","SS","⎵","👍🏽","👨‍👩‍👧‍👦"};
    for(unsigned index=0;index<sizeof(variants)/sizeof(variants[0]);index++) {
        select_group_item(&keyboard,6,1);strcpy(keyboard.model.pending_text,variants[index]);
        hello_keyboard_utf8_decode(variants[index],&keyboard.model.pending);
        keyboard.selection_marker.active=false;
        for(uint32_t elapsed=0;elapsed<=320;elapsed+=80) {
            group_draw_count=marker_draws=0;draw_group(&keyboard,6,105,69,item_time+elapsed);
            assert(marker_draws==1&&marker_h==26&&marker_y==group_clip_y);
        }
        item_time+=1000;
    }
    capture_group_view=false;
    graphics->newBitmap=NULL;
}

static void jelly_render_check(HelloKeyboard* keyboard,uint32_t now) {
    strcpy(keyboard->model.pending_text,"SS");keyboard->model.pending='S';
    capture_group_view=capture_selected_ink=true;
    group_draw_count=marker_draws=selected_ink_draws=0;
    draw_group(keyboard,keyboard->model.group,105,69,now);
    // Last-item entry content can still be travelling into view. Its glyph and marker
    // must become visible together, without teleporting an offscreen endpoint.
    if(!selected_ink_draws) {
        assert(!marker_draws);capture_group_view=capture_selected_ink=false;return;
    }
    assert(marker_draws==1&&selected_ink_draws==1&&marker_h==group_clip_h&&marker_y==group_clip_y);
    // The native 16x14 test glyph stays inside the black rendered envelope,
    // including the first retarget frame while the independent caps lag behind.
    assert(marker_x<=selected_ink_x&&marker_x+marker_w>=selected_ink_x+16);
    assert(marker_y<=selected_ink_y&&marker_y+marker_h>=selected_ink_y+14);
    assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
    assert(marker_y>=group_clip_y&&marker_y+marker_h<=group_clip_y+group_clip_h);
    const BrbElasticMarker* marker=&keyboard->selection_marker;
    assert(isfinite(marker->left)&&isfinite(marker->right)&&isfinite(marker->left_velocity)&&isfinite(marker->right_velocity));
    assert(fabsf(marker->left-marker->target)<=42.001f&&fabsf(marker->right-marker->target)<=42.001f);
    capture_group_view=capture_selected_ink=false;
}
static void test_jelly_both_inputs_and_group_entries(const HelloKeyboard* reference) {
    const int signs[]={1,1,-1,-1};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int crank=0;crank<2;crank++)
    for(uint32_t frame_ms=20;frame_ms<=40;frame_ms+=20) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,-1,kBuild68Groups,150000);
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group);
        int index=count/2;select_group_item(&keyboard,group,index);
        keyboard.model.pending_side=3;reset_group_scrolls(&keyboard);keyboard.selection_marker.active=false;
        jelly_render_check(&keyboard,150000);
        uint32_t now=150000;
        bool stretched=false;
        for(unsigned step=0;step<sizeof(signs)/sizeof(signs[0]);step++) {
            const int direction=signs[step];now+=16;
            if(crank) {
                const float load=crank_test_threshold(&keyboard,direction)*1.11f-
                    (float)direction*keyboard.crank_remainder+0.002f;
                hello_keyboard_update(&keyboard,now,0,0,0,(float)direction*load);
            } else {
                const PDButtons face=direction>0?kButtonA:kButtonB;
                hello_keyboard_update(&keyboard,now,face,face,0,0);
                hello_keyboard_update(&keyboard,now+8,0,0,face,0);
            }
            index+=direction;
            assert(keyboard.model.index==index&&!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons);
            jelly_render_check(&keyboard,now);
            if(marker_draws&&marker_w>20)stretched=true;
        }
        assert(stretched);
        if(crank) {
            now+=16;hello_keyboard_update(&keyboard,now,0,0,0,-keyboard.crank_remainder);
            jelly_render_check(&keyboard,now);assert(keyboard.model.index==index);
        }
        // Rendering and cap settling never manufacture additional input.
        for(uint32_t elapsed=frame_ms;elapsed<=700;elapsed+=frame_ms) {
            hello_keyboard_update(&keyboard,now+elapsed,0,0,0,0);
            jelly_render_check(&keyboard,now+elapsed);
            assert(keyboard.model.index==index&&!keyboard.selection_lock_group&&!keyboard.model.text[0]);
        }
        assert(brb_elastic_marker_settled(&keyboard.selection_marker,
            keyboard.selection_marker.target,keyboard.selection_marker.target));
    }
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        const int previous=group==6?2:(group==1?2:1);
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,previous,-1,kBuild68Groups,160000);
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        keyboard.model.upper=group==6;keyboard.model.field.type=HELLO_INPUT_PLAINTEXT;
        jelly_render_check(&keyboard,160000);
        const PDButtons direction=directions[group];
        hello_keyboard_update(&keyboard,160016,direction,direction,0,0);
        hello_keyboard_update(&keyboard,160024,direction,0,0,0);
        hello_keyboard_update(&keyboard,160032,direction|kButtonA,kButtonA,0,0);
        assert(keyboard.model.group==group&&keyboard.model.index==0&&keyboard.model.pending);
        assert(!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons);
        jelly_render_check(&keyboard,160032);
        assert(marker_w>20); // A bounded inward tail makes the new group visible.
        hello_keyboard_update(&keyboard,160040,direction,0,kButtonA,0);
        for(uint32_t elapsed=20;elapsed<=600;elapsed+=20)jelly_render_check(&keyboard,160032+elapsed);
        assert(brb_elastic_marker_settled(&keyboard.selection_marker,
            keyboard.selection_marker.target,keyboard.selection_marker.target));
    }
}

static void test_jelly_wrap_both_inputs(const HelloKeyboard* reference) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2)
    for(int crank=0;crank<2;crank++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,crank?NULL:kOverflowGroups,170000);
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        for(uint32_t now=170000;now<=170600;now+=20)draw_group(&keyboard,group,105,69,now);
        const uint32_t begin=170620;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),endpoint=direction>0?0:count-1;
        const PDButtons face=direction>0?kButtonA:kButtonB;
        hello_keyboard_update(&keyboard,begin,crank?0:face,crank?0:face,0,
            crank?(float)direction*(crank_test_threshold(&keyboard,direction)+0.001f):0);
        assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==group);
        bool visible=false;uint32_t arrived=UINT32_MAX,first_visible=UINT32_MAX;
        for(uint32_t elapsed=0;elapsed<=1240;elapsed+=20) {
            hello_keyboard_update(&keyboard,begin+elapsed,0,0,!elapsed&&!crank?face:0,0);
            jelly_render_check(&keyboard,begin+elapsed);
            if(!elapsed)assert(fabsf(keyboard.selection_marker.right-keyboard.selection_marker.left)==18);
            if(marker_draws){visible=true;if(first_visible==UINT32_MAX)first_visible=elapsed;}
            assert(keyboard.model.index==endpoint&&!keyboard.model.text[0]);
            if(!keyboard.selection_lock_group){arrived=elapsed;break;}
        }
        assert(visible&&arrived<=650&&arrived-first_visible<=650);
        hello_keyboard_update(&keyboard,begin+arrived+20,face,face,0,0);
        assert(keyboard.model.index==endpoint+direction&&!keyboard.selection_lock_group);
    }
}

static void test_parallel_wrap_presentation(const HelloKeyboard* reference) {
    for(uint32_t frame_ms=20;frame_ms<=40;frame_ms+=20) {
        uint32_t shortest=UINT32_MAX,longest=0;
        for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;++group) {
            uint32_t duration[2]={0};
            for(int direction=-1;direction<=1;direction+=2) {
                HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,NULL,500000);
                keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
                if(group==1)hello_keyboard_set_visible_prefix(&keyboard,1,10);
                for(uint32_t now=500020;now<=500620;now+=20)draw_group(&keyboard,group,105,69,now);
                keyboard.selection_marker.left-=4;keyboard.selection_marker.right+=4;
                keyboard.selection_marker.left_velocity=500;keyboard.selection_marker.right_velocity=-700;
                const uint32_t begin=500640;
                hello_keyboard_update(&keyboard,begin,0,0,0,(float)direction*(crank_test_threshold(&keyboard,direction)+0.001f));
                const int endpoint=keyboard.model.index;
                bool painted=false;uint32_t arrived=UINT32_MAX;
                capture_group_view=true;
                for(uint32_t elapsed=0;elapsed<=600;elapsed+=frame_ms) {
                    hello_keyboard_update(&keyboard,begin+elapsed,0,0,0,0);
                    group_draw_count=marker_draws=0;draw_group(&keyboard,group,105,69,begin+elapsed);
                    assert(keyboard.model.index==endpoint&&keyboard.selection_marker.active);
                    if(!elapsed) {
                        assert(fabsf(keyboard.selection_marker.right-keyboard.selection_marker.left)==18);
                        assert(keyboard.selection_marker.left_velocity==0&&keyboard.selection_marker.right_velocity==0);
                    }
                    if(marker_draws)painted=true;
                    if(!keyboard.selection_lock_group) {
                        assert(painted&&!keyboard.group_scroll_motion[group].remaining_ms);
                        assert(brb_elastic_marker_settled(&keyboard.selection_marker,
                            keyboard.selection_marker.target,keyboard.selection_marker.target));
                        arrived=elapsed;break;
                    }
                }
                capture_group_view=false;
                assert(arrived>=500&&arrived<=600);
                duration[direction>0]=arrived;
                if(arrived<shortest)shortest=arrived;
                if(arrived>longest)longest=arrived;
            }
            assert(abs((int)duration[0]-(int)duration[1])<=(int)frame_ms);
        }
        assert(longest-shortest<=frame_ms);
    }
}

static void test_default_up_digits_tilde_and_scrolled_items(const HelloKeyboard* reference) {
    static const char expected[]="1234567890.,'?!-:/@_#~[<{\\}>]^=;*%$€&+)(";
    HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,-1,NULL,230000);
    keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
    const char* group=hello_keyboard_model_group_chars(&keyboard.model,1);
    assert(!strcmp(group,expected)&&strlen(group)==42&&hello_keyboard_model_group_length(&keyboard.model,1)==40);
    bool digits[10]={0};int tildes=0;uint32_t now=230000;
    capture_group_view=true;
    for(int index=0;index<40;index++) {
        char item[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];hello_keyboard_model_group_text(&keyboard.model,1,index,item,sizeof(item));
        if(index<10){assert(item[1]==0&&item[0]=="1234567890"[index]);digits[index]=true;}
        if(!strcmp(item,"~")){assert(index==21);tildes++;}
        if(index==39)assert(!strcmp(item,"("));
        select_group_item(&keyboard,1,index);keyboard.model.pending_side=3;
        expected_selected_item=item;
        for(unsigned frame=0;frame<=30;frame++) {
            group_draw_count=marker_draws=expected_item_draws=0;draw_group(&keyboard,1,105,69,now);
            assert(expected_item_draws==1&&marker_draws==1);
            const int width=measure(NULL,item,strlen(item),kUTF8Encoding,0);
            assert(expected_item_x>=group_clip_x&&expected_item_x+width<=group_clip_x+group_clip_w);
            assert(marker_x<=expected_item_x&&marker_x+marker_w>=expected_item_x+width);
            for(int visible=0;visible<group_draw_count;visible++)assert(group_item_x[visible]>=group_clip_x&&
                group_item_x[visible]+group_item_width[visible]<=group_clip_x+group_clip_w);
            now+=20;
        }
    }
    expected_selected_item=NULL;capture_group_view=false;
    for(int digit=0;digit<10;digit++)assert(digits[digit]);
    assert(tildes==1&&keyboard.group_scroll[1].position>0);
    // The new 40-item default also completes real A/B boundary travel and
    // jelly rest in both directions within the existing gate budget.
    for(int direction=1;direction>=-1;direction-=2) {
        const PDButtons face=direction>0?kButtonA:kButtonB;
        const uint32_t began=now;
        hello_keyboard_update(&keyboard,now,kButtonUp|face,face,0,0);
        assert(keyboard.selection_lock_group==1&&keyboard.model.index==(direction>0?0:39));
        hello_keyboard_update(&keyboard,now+8,kButtonUp,0,face,0);
        now=crank_test_finish_wrap(&keyboard,now+8);
        assert(now-began<=1250&&brb_elastic_marker_settled(&keyboard.selection_marker,
            keyboard.selection_marker.target,keyboard.selection_marker.target));
        now+=20;
    }
}
static void check_group_ink(HelloKeyboard* keyboard,int group,bool active,uint32_t now) {
    capture_group_view=capture_group_ink=true;expected_group_active=active;
    group_ink_texts=group_ink_rects=group_ink_lines=group_draw_count=marker_draws=0;
    draw_group(keyboard,group,105,69,now);
    assert(group_ink_fill==(active?kColorBlack:kColorWhite));
    if(group==HELLO_KEYBOARD_EMOJI_GROUP)assert(group_ink_rects>0);
    else assert(group_ink_texts>0);
    if(group==3)assert(group_ink_lines==3);
    capture_group_view=capture_group_ink=false;
}
static void test_held_group_ink_and_immediate_release(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int pending=0;pending<2;pending++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard={0};keyboard.pd=reference->pd;keyboard.config=reference->config;
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        BrbUnicodeFont pack={.pd=reference->pd};
        if(group==HELLO_KEYBOARD_EMOJI_GROUP){keyboard.config.font=NULL;keyboard.config.unicode_font=&pack;}
        hello_keyboard_model_init(&keyboard.model,"",0);hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        keyboard.visible=true;keyboard.model.alternate=false;keyboard.model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;keyboard.model.upper=group==6;
        const PDButtons dpad=directions[group];
        hello_keyboard_update(&keyboard,260000,dpad,dpad,0,0);
        check_group_ink(&keyboard,group,true,260000);
        assert(!marker_draws);
        if(pending) {
            hello_keyboard_update(&keyboard,260008,dpad|kButtonA,kButtonA,0,0);
            check_group_ink(&keyboard,group,true,260008);assert(marker_draws==1);
            hello_keyboard_update(&keyboard,260016,dpad,0,kButtonA,0);
        }
        hello_keyboard_update(&keyboard,260024,0,0,dpad,0);
        assert(!keyboard.model.group&&!keyboard.model.pending);
        check_group_ink(&keyboard,group,false,260024);assert(!marker_draws);
        assert((keyboard.model.text[0]!=0)==(pending!=0));
    }
}

static void test_white_canvas_and_dynamic_footer(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    hello_keyboard_model_init(&keyboard.model,"a",1);keyboard.visible=true;
    keyboard.model.hold[HELLO_KB_BUTTON_B]=(HelloKeyboardHold){.active=true,.at=1000,.hold=HELLO_KB_HOLD_DELETE};
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    expected_table=2;expected_frame=0;capture_footer_canvas=true;
    for(unsigned frame=0;frame<26;frame++) {
        const uint32_t now=1000+frame*16;
        canvas_white_fills=footer_dividers=footer_circles=footer_white_fills=0;
        const int dpads=footer_dpad_draws;
        hello_keyboard_draw(&keyboard,now);
        assert(canvas_white_fills==1&&footer_dividers==1&&footer_circles==2&&footer_dpad_draws==dpads);
        const int progress=hold_fill(&keyboard,HELLO_KB_BUTTON_B,now);
        assert(footer_white_fills==(progress>0?1:0));
        if(progress) {
            const int height=(16*progress+20)/21;
            assert(footer_fill_angle==360&&footer_progress_clip_h==height&&footer_progress_clip_y==233-height);
        }
    }
    capture_footer_canvas=false;
}

static void check_base_page(HelloKeyboard* keyboard,uint32_t now) {
    capture_base_page=capture_group_block=true;base_page_upper=keyboard->model.upper;
    block_count=input_pill_count=0;
    memset(base_page_letters,0,sizeof(base_page_letters));
    base_page_top_unicode=base_page_top_ascii=0;
    hello_keyboard_draw(keyboard,now);
    for(int letter=0;letter<26;letter++)assert(base_page_letters[letter]);
    if(base_page_upper)assert(base_page_top_unicode>0&&!base_page_top_ascii);
    else assert(base_page_top_ascii>0&&!base_page_top_unicode);
    assert(block_count==4&&input_pill_count==1);
    const int top=block_y[0]-hello_floating_offset(now,(float)((base_page_upper?6:1)-1)*0.45f);
    const int bottom=block_y[1]-hello_floating_offset(now,0.9f)+block_h[1];
    assert(abs((top-64)-(211-bottom))<=1);
    for(int group=0;group<4;group++)assert(block_y[group]>64&&block_y[group]+block_h[group]<211);
    capture_base_page=capture_group_block=false;
}
static void check_idle_caption(HelloKeyboard* keyboard,const char* expected,uint32_t now) {
    capture_footer_canvas=capture_footer_labels=true;
    footer_circles=footer_white_fills=footer_caption_count=footer_label_clips=footer_label_fills=0;
    const int dpads=footer_dpad_draws;
    const bool navigation=hello_keyboard_model_is_choosing_layer(&keyboard->model);
    const bool shown=navigation;
    if(shown)native_dpad_begin(3);
    draw_footer(keyboard,now);
    if(navigation) {
        if(shown)native_dpad_check_buttons(keyboard->footer_dpad_buttons&(kButtonLeft|kButtonRight));
        const char* text=!strcmp(keyboard->config.labels.navigation_text,"Cursor / Layer")?"Cursor":keyboard->config.labels.navigation_text;
        const bool caption=shown&&text[0]!=0;
        const char* right=keyboard->model.upper?keyboard->config.labels.hold_upper:keyboard->config.labels.hold_lower;
        const int right_width=keyboard_text_width(keyboard,right);
        assert(footer_dpad_draws==dpads+(shown?1:0)&&footer_circles==1&&footer_circle_x[0]==396-right_width-24);
        assert(footer_caption_count==(caption?3:2));
        if(caption)assert(!strcmp(footer_captions[0],text)&&footer_caption_x[0]==27&&footer_caption_clip_w[0]==158);
        assert(!strcmp(footer_captions[footer_caption_count-2],"A")&&!strcmp(footer_captions[footer_caption_count-1],right));
    } else {
        bool found=false;for(int i=0;i<footer_caption_count;i++)if(!strcmp(footer_captions[i],expected))found=true;
        assert(footer_dpad_draws==dpads&&footer_circles==2&&found);
    }
    capture_footer_canvas=capture_footer_labels=false;
}
static void test_idle_a_case_layer_chooser_and_base_pages(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.emoji_picker_enabled=false; // Explicit standalone, one-view integration.
    config.font=(LCDFont*)&bitmap;config.font_size=14;config.group_height=30;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(!strcmp(keyboard.config.labels.navigation_text,"Cursor / Layer"));
    hello_keyboard_set_initial_layer(&keyboard,true); // Legacy flag cannot create a second view.
    assert(hello_keyboard_open(&keyboard,"seed"));
    uint32_t now=210000;
    check_base_page(&keyboard,now);check_idle_caption(&keyboard,"Shift",now);
    for(int tap=0;tap<2;tap++) {
        now+=160;hello_keyboard_update(&keyboard,now,kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,now+16,0,0,kButtonA,0);
        assert(keyboard.model.upper==(tap==0)&&!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,"seed"));
        check_base_page(&keyboard,now+16);check_idle_caption(&keyboard,tap==0?"shift":"Shift",now+16);
        assert(footer_white_fills==1&&footer_progress_clip_y==217&&footer_progress_clip_h==16);
    }
    for(int upper=0;upper<2;upper++) {
        if(keyboard.model.upper!=(bool)upper) {
            now+=160;hello_keyboard_update(&keyboard,now,kButtonA,kButtonA,0,0);
            hello_keyboard_update(&keyboard,now+16,0,0,kButtonA,0);
        }
        check_base_page(&keyboard,now+16);
        const int group=upper?HELLO_KEYBOARD_EMOJI_GROUP:1;
        now+=200;hello_keyboard_update(&keyboard,now,kButtonUp,kButtonUp,0,0);assert(keyboard.model.group==group);
        hello_keyboard_update(&keyboard,now+8,kButtonUp|kButtonA,kButtonA,0,0);
        char selected[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1],before[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];
        strcpy(before,keyboard.model.text);hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        hello_keyboard_update(&keyboard,now+16,kButtonUp,0,kButtonA,0);
        assert(keyboard.model.pending&&!strcmp(keyboard.model.text,before));
        hello_keyboard_update(&keyboard,now+24,0,0,kButtonUp,0);
        assert(!keyboard.model.group&&!keyboard.model.pending&&strlen(keyboard.model.text)==strlen(before)+strlen(selected));
        now+=200;hello_keyboard_update(&keyboard,now,kButtonA,kButtonA,0,0);
        now+=keyboard.model.timing.hold_ms;hello_keyboard_update(&keyboard,now,kButtonA,0,0,0);
        assert(hello_keyboard_model_is_choosing_layer(&keyboard.model)&&hello_keyboard_model_layer_count(&keyboard.model)==1&&hello_keyboard_model_layer(&keyboard.model)==1);
        strcpy(before,keyboard.model.text);check_idle_caption(&keyboard,"",now);
        const size_t cursor_before_b=keyboard.model.cursor;
        now+=16;hello_keyboard_update(&keyboard,now,kButtonA|kButtonB,kButtonB,0,0);
        now+=keyboard.model.timing.hold_ms+keyboard.model.timing.repeat_ms;
        hello_keyboard_update(&keyboard,now,kButtonA|kButtonB,0,0,0);
        assert(!keyboard.model.hold[HELLO_KB_BUTTON_B].active&&!strcmp(keyboard.model.text,before)&&keyboard.model.cursor==cursor_before_b);
        assert(!keyboard.cancel_dialog_active&&hello_keyboard_model_is_choosing_layer(&keyboard.model));
        check_idle_caption(&keyboard,"",now);hello_keyboard_update(&keyboard,now+8,kButtonA,0,kButtonB,0);
        const PDButtons noop[]={kButtonUp,kButtonDown};
        for(unsigned step=0;step<2;step++) {
            now+=16;hello_keyboard_update(&keyboard,now,kButtonA|noop[step],noop[step],0,0);
            assert(hello_keyboard_model_layer(&keyboard.model)==1&&!keyboard.model.group&&!keyboard.model.pending&&keyboard.model.upper==(bool)upper);
            assert(!strcmp(keyboard.model.text,before));check_idle_caption(&keyboard,"",now);
            hello_keyboard_update(&keyboard,now+8,kButtonA,0,noop[step],0);
        }
        now+=16;const size_t original=keyboard.model.cursor,left=brb_grapheme_previous(before,original);
        hello_keyboard_cursor_blink_reset(&keyboard.cursor_blink); // Isolate this coordinate check from blink phase.
        draw_input_field(&keyboard,0);const int old_x=cursor_x;
        hello_keyboard_update(&keyboard,now,kButtonA|kButtonLeft,kButtonLeft,0,0);
        assert(keyboard.model.cursor==left&&keyboard.model.cursor_hold&&!strcmp(keyboard.model.text,before));
        draw_input_field(&keyboard,0);assert(cursor_x<old_x&&!strcmp(rendered,before));
        check_idle_caption(&keyboard,"",now);
        hello_keyboard_update(&keyboard,now+hello_keyboard_model_selection_hold_ms(&keyboard.model)-1,kButtonA|kButtonLeft,0,0,0);assert(keyboard.model.cursor==left);
        now+=hello_keyboard_model_selection_hold_ms(&keyboard.model);hello_keyboard_update(&keyboard,now,kButtonA|kButtonLeft,0,0,0);
        assert(keyboard.model.cursor==brb_grapheme_previous(before,left));
        hello_keyboard_update(&keyboard,now+8,kButtonA,0,kButtonLeft,0);
        now+=16;hello_keyboard_update(&keyboard,now,kButtonA|kButtonRight,kButtonRight,0,0);assert(keyboard.model.cursor==left);
        check_idle_caption(&keyboard,"",now);
        hello_keyboard_update(&keyboard,now+8,kButtonRight,0,kButtonA,0);
        now+=1000;hello_keyboard_update(&keyboard,now,kButtonRight,0,0,0);
        assert(!hello_keyboard_model_is_choosing_layer(&keyboard.model)&&!keyboard.model.cursor_hold&&!keyboard.model.group);
        assert(keyboard.model.cursor==left&&keyboard.model.upper==(bool)upper&&!strcmp(keyboard.model.text,before));
        check_idle_caption(&keyboard,upper?"shift":"Shift",now);hello_keyboard_update(&keyboard,now+8,0,0,kButtonRight,0);
        const size_t at=keyboard.model.cursor;strcpy(before,keyboard.model.text);
        hello_keyboard_update(&keyboard,now+40,kButtonRight|kButtonA,kButtonRight|kButtonA,0,0);
        assert(keyboard.model.group==2&&keyboard.model.pending&&!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        hello_keyboard_update(&keyboard,now+48,kButtonRight,0,kButtonA,0);
        hello_keyboard_update(&keyboard,now+56,0,0,kButtonRight,0);
        assert(!keyboard.model.group&&!strncmp(keyboard.model.text,before,at)&&!strcmp(keyboard.model.text+at+strlen(selected),before+at));
        now+=160;
    }
    hello_keyboard_close(&keyboard);config.labels.layer_text="Page %s";
    char custom_navigation[]="Move %s";config.labels.navigation_text=custom_navigation;
    loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,""));
    custom_navigation[0]='x';assert(!strcmp(keyboard.config.labels.navigation_text,"Move %s"));
    hello_keyboard_update(&keyboard,220000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,220000+keyboard.model.timing.hold_ms,kButtonA,0,0,0);
    check_idle_caption(&keyboard,"",220000+keyboard.model.timing.hold_ms);hello_keyboard_close(&keyboard);
}
static int picker_changes;
static const BrbEmojiSet* last_picker_set;
static void picker_changed(void* userdata,const BrbEmojiSet* set) {
    assert(userdata==&picker_changes);picker_changes++;last_picker_set=set;
}
static const unsigned char kTinyEmojiPixels[12]={255,255,255,255,255,252,255,255,255,255,255,252};
static const BrbEmojiGlyph kTinyEmojiA[]={
    {.id="a0",.utf8="😀",.utf8_length=4,.width=3,.advance=4,.bitmap_offset=0},
    {.id="a1",.utf8="😃",.utf8_length=4,.width=3,.advance=4,.bitmap_offset=6}};
static const BrbEmojiGlyph kTinyEmojiB[]={
    {.id="b0",.utf8="😎",.utf8_length=4,.width=3,.advance=4,.bitmap_offset=0},
    {.id="b1",.utf8="🤔",.utf8_length=4,.width=3,.advance=4,.bitmap_offset=6}};
static const BrbEmojiSet kTinyEmojiSets[]={
    {.id="tiny-a",.version=1,.cell_height=14,.baseline=12,.glyphs=kTinyEmojiA,.count=2,
     .pixels=kTinyEmojiPixels,.pixels_size=sizeof(kTinyEmojiPixels),.keyboard_utf8="😀😃",.keyboard_bytes=8},
    {.id="tiny-b",.version=1,.cell_height=14,.baseline=12,.glyphs=kTinyEmojiB,.count=2,
     .pixels=kTinyEmojiPixels,.pixels_size=sizeof(kTinyEmojiPixels),.keyboard_utf8="😎🤔",.keyboard_bytes=8}};
static void enter_picker_view(HelloKeyboard* keyboard,uint32_t now) {
    hello_keyboard_update(keyboard,now,kButtonA,kButtonA,0,0);
    hello_keyboard_update(keyboard,now+1,kButtonA|kButtonUp,kButtonUp,0,0);
    assert(hello_keyboard_model_layer(&keyboard->model)==2&&!keyboard->model.group&&!keyboard->model.pending);
    // Releasing the modifier while Up stays physically held cannot become
    // a fresh group/picker action in the destination view.
    hello_keyboard_update(keyboard,now+2,kButtonUp,0,kButtonA,0);
    assert(!keyboard->model.group&&!keyboard->model.pending);
    hello_keyboard_update(keyboard,now+3,0,0,kButtonUp,0);
    assert(!hello_keyboard_model_is_choosing_layer(&keyboard->model));
}
static void test_emoji_picker_views_selection_and_shared_font(const HelloKeyboard* reference) {
    const BrbEmojiSet* sets[]={&kTinyEmojiSets[0],&kTinyEmojiSets[1]};
    static BrbUnicodeFont shared;memset(&shared,0,sizeof(shared));shared.pd=reference->pd;
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    assert(config.emoji_picker_enabled);config.font=(LCDFont*)&bitmap;config.font_size=14;
    config.unicode_font=&shared;config.emoji_sets=sets;config.emoji_set_count=2;
    config.on_emoji_set_selected=picker_changed;config.emoji_set_userdata=&picker_changes;
    HelloKeyboard keyboard,other;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    picker_changes=0;last_picker_set=NULL;
    assert(hello_keyboard_open(&keyboard,"seed"));
    assert(hello_keyboard_model_layer_count(&keyboard.model)==2&&hello_keyboard_emoji_set(&keyboard)==sets[0]);
    assert(!strcmp(keyboard.model.groups[6],sets[0]->keyboard_utf8));
    enter_picker_view(&keyboard,510000);
    for(int row=0;row<2;row++) {
        expected_selected_item=sets[row]->id;expected_item_draws=0;hello_keyboard_draw(&keyboard,510004);
        assert(expected_item_draws>0&&expected_item_y>=64&&expected_item_y+14<=211);
    }
    expected_selected_item=NULL;
    capture_footer_labels=capture_footer_canvas=true;footer_caption_count=footer_circles=0;draw_footer(&keyboard,510005);
    bool choose=false,use=false;
    for(int i=0;i<footer_caption_count;i++) {
        choose|=!strcmp(footer_captions[i],keyboard.config.labels.picker_select);
        use|=!strcmp(footer_captions[i],keyboard.config.labels.use_set);
        assert(strcmp(footer_captions[i],keyboard.config.labels.delete_text)&&strcmp(footer_captions[i],keyboard.config.labels.escape_text));
    }
    assert(choose&&use&&footer_circles==1);capture_footer_labels=capture_footer_canvas=false;
    hello_keyboard_update(&keyboard,510010,kButtonDown,kButtonDown,0,0);assert(keyboard.emoji_picker_index==1);
    hello_keyboard_update(&keyboard,510011,0,0,kButtonDown,0);
    hello_keyboard_update(&keyboard,510012,kButtonDown,kButtonDown,0,0);assert(keyboard.emoji_picker_index==1);
    hello_keyboard_update(&keyboard,510013,0,0,kButtonDown,0);
    hello_keyboard_update(&keyboard,510020,0,0,0,10000);assert(keyboard.emoji_picker_index==1);
    hello_keyboard_update(&keyboard,510030,0,0,0,-10000);assert(keyboard.emoji_picker_index==0);
    hello_keyboard_update(&keyboard,510040,0,0,0,10000);assert(keyboard.emoji_picker_index==1);
    hello_keyboard_update(&keyboard,510050,kButtonB,kButtonB,0,0);
    hello_keyboard_update(&keyboard,510066,0,0,kButtonB,0);assert(!strcmp(keyboard.model.text,"seed"));
    assert(hello_keyboard_update(&keyboard,510080,kButtonA,kButtonA,0,0)==HELLO_KEYBOARD_NONE);
    assert(hello_keyboard_model_layer(&keyboard.model)==2);
    assert(hello_keyboard_update(&keyboard,510096,0,0,kButtonA,0)==HELLO_KEYBOARD_NONE);
    assert(hello_keyboard_model_layer(&keyboard.model)==1&&hello_keyboard_emoji_set(&keyboard)==sets[1]);
    assert(!strcmp(keyboard.model.groups[6],sets[1]->keyboard_utf8)&&!strcmp(keyboard.model.text,"seed"));
    assert(!keyboard.model.pending&&!keyboard.model.group&&!keyboard.model.upper&&!keyboard.cancel_dialog_active&&picker_changes==1&&last_picker_set==sets[1]);
    // The selecting release cannot become an idle Shift or a hidden character.
    hello_keyboard_update(&keyboard,510097,0,0,kButtonA,0);assert(!keyboard.model.upper&&!keyboard.model.pending);
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"saved"));
    assert(hello_keyboard_emoji_set(&keyboard)==sets[1]&&!strcmp(keyboard.model.groups[6],sets[1]->keyboard_utf8));
    loads=0;hello_keyboard_init_with_config(&other,reference->pd,&config);assert(hello_keyboard_open(&other,"other"));
    assert(hello_keyboard_emoji_set(&other)==sets[0]);
    hello_keyboard_draw(&other,511000);assert(brb_unicode_font_emoji_set(&shared)==sets[0]);
    hello_keyboard_draw(&keyboard,511001);assert(brb_unicode_font_emoji_set(&shared)==sets[1]);
    hello_keyboard_draw(&other,511002);assert(brb_unicode_font_emoji_set(&shared)==sets[0]);
    assert(!strcmp(keyboard.model.groups[6],sets[1]->keyboard_utf8)&&!strcmp(other.model.groups[6],sets[0]->keyboard_utf8));
    keyboard.model.upper=true;hello_keyboard_update(&keyboard,511100,kButtonUp,kButtonUp,0,0);
    hello_keyboard_update(&keyboard,511116,kButtonUp|kButtonA,kButtonA,0,0);
    char pending[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_pending_text(&keyboard.model,pending,sizeof(pending));
    assert(!strcmp(pending,sets[1]->glyphs[0].utf8));
    hello_keyboard_update(&keyboard,511124,kButtonUp,0,kButtonA,0);
    hello_keyboard_update(&keyboard,511132,0,0,kButtonUp,0);assert(!strcmp(keyboard.model.text,"saved😎"));
    enter_picker_view(&keyboard,511200);
    const size_t cursor_before=keyboard.model.cursor;
    hello_keyboard_update(&keyboard,511300,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,511650,kButtonA,0,0,0);
    capture_footer_labels=true;footer_caption_count=0;draw_footer(&keyboard,511650);
    bool layer_caption=false;for(int i=0;i<footer_caption_count;i++)if(!strcmp(footer_captions[i],"lY"))layer_caption=true;
    assert(layer_caption);capture_footer_labels=false;
    hello_keyboard_update(&keyboard,511651,kButtonA|kButtonLeft,kButtonLeft,0,0);
    assert(keyboard.model.cursor==brb_grapheme_previous(keyboard.model.text,cursor_before)&&!keyboard.model.group);
    hello_keyboard_update(&keyboard,511652,kButtonA|kButtonRight,kButtonRight,kButtonLeft,0);
    assert(keyboard.model.cursor==cursor_before);
    hello_keyboard_update(&keyboard,511653,kButtonA|kButtonUp,kButtonUp,kButtonRight,0);assert(hello_keyboard_model_layer(&keyboard.model)==1);
    hello_keyboard_update(&keyboard,511654,kButtonA|kButtonDown,kButtonDown,kButtonUp,0);assert(hello_keyboard_model_layer(&keyboard.model)==2);
    hello_keyboard_update(&keyboard,511655,kButtonA|kButtonB,kButtonB,kButtonDown,0);
    hello_keyboard_update(&keyboard,512005,kButtonA|kButtonB,0,0,0);
    assert(!keyboard.cancel_dialog_active&&keyboard.model.upper&&!strcmp(keyboard.model.text,"saved😎"));
    hello_keyboard_update(&keyboard,512006,kButtonA,0,kButtonB,0);
    hello_keyboard_update(&keyboard,512007,0,0,kButtonA,0);
    assert(hello_keyboard_model_layer(&keyboard.model)==2&&keyboard.model.upper&&!keyboard.model.pending);
    hello_keyboard_close(&other);hello_keyboard_close(&keyboard);
    config.emoji_sets=NULL;config.emoji_set_count=0;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(hello_keyboard_open(&keyboard,""));assert(hello_keyboard_emoji_set(&keyboard)==brb_emoji_everyday_set());
    enter_picker_view(&keyboard,513000);
    hello_keyboard_update(&keyboard,513010,kButtonDown,kButtonDown,0,10000);
    assert(keyboard.emoji_picker_index==0);hello_keyboard_update(&keyboard,513011,0,0,kButtonDown,0);
    hello_keyboard_update(&keyboard,513020,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    hello_keyboard_update(&keyboard,513370,kButtonA|kButtonB,0,0,0);assert(keyboard.cancel_dialog_active);
    hello_keyboard_update(&keyboard,513371,0,0,kButtonA|kButtonB,0);
    assert(discard_editing_popup(&keyboard,513380)==HELLO_KEYBOARD_CANCELLED);
    assert(!keyboard.visible&&!keyboard.cancel_dialog_active&&!keyboard.model.text[0]&&!keyboard.model.pending);
    hello_keyboard_close(&keyboard);
}

static void held_cursor_crank_tick(HelloKeyboard* keyboard,int direction,uint32_t now) {
    HelloKeyboardCrankDetent detent=crank_detent(keyboard);detent.remainder=(float)direction;
    const float threshold=hello_keyboard_crank_threshold(&detent,false);
    const float travel=threshold*1.101f-(float)direction*keyboard->crank_remainder;
    hello_keyboard_update(keyboard,now,kButtonA,0,0,(float)direction*travel);
    assert(!keyboard->model.group&&!keyboard->model.pending&&!keyboard->selection_lock_group);
}
static void test_held_a_crank_cursor_graphemes_and_lifecycle(const HelloKeyboard* reference) {
    const char* input="aé👨‍👩‍👧‍👦z";
    const size_t positions[]={0,1,strlen("aé"),strlen("aé👨‍👩‍👧‍👦"),strlen("aé👨‍👩‍👧‍👦z")};
    for(int upper=0;upper<2;upper++)for(int rollover=0;rollover<2;rollover++) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        assert(hello_keyboard_open(&keyboard,input));keyboard.model.upper=upper;
        const uint32_t start=rollover?UINT32_MAX-100:330000;
        hello_keyboard_update(&keyboard,start,kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,start+349,kButtonA,0,0,-360);
        assert(keyboard.model.cursor==positions[4]&&!keyboard.crank_remainder&&!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        // The exact hold boundary activates navigation before this frame's crank.
        hello_keyboard_update(&keyboard,start+350,kButtonA,0,0,-60);
        assert(keyboard.model.cursor==positions[3]&&hello_keyboard_model_is_choosing_layer(&keyboard.model));
        uint32_t now=start+366;
        for(int index=2;index>=0;index--,now+=16) {
            held_cursor_crank_tick(&keyboard,-1,now);draw_input_field(&keyboard,now);
            assert(keyboard.model.cursor==positions[index]&&!strcmp(rendered,input));
        }
        hello_keyboard_update(&keyboard,now,kButtonA,0,0,-360);now+=16;
        assert(!keyboard.model.cursor&&!keyboard.crank_remainder);
        for(int frame=0;frame<10;frame++,now+=16) {
            hello_keyboard_update(&keyboard,now,kButtonA,0,0,0);assert(!keyboard.model.cursor&&!keyboard.crank_remainder);
        }
        for(unsigned index=1;index<5;index++,now+=16) {
            held_cursor_crank_tick(&keyboard,1,now);draw_input_field(&keyboard,now);
            assert(keyboard.model.cursor==positions[index]&&!strcmp(rendered,input));
        }
        hello_keyboard_update(&keyboard,now,kButtonA,0,0,360);now+=16;
        assert(keyboard.model.cursor==positions[4]&&!keyboard.crank_remainder);
        hello_keyboard_update(&keyboard,now,kButtonA|kButtonB,kButtonB,0,0);now+=16;
        hello_keyboard_update(&keyboard,now,kButtonA,0,kButtonB,0);now+=16;
        assert(!strcmp(keyboard.model.text,input)&&keyboard.model.upper==(bool)upper&&!keyboard.model.hold[HELLO_KB_BUTTON_B].active);
        // Release discards this frame's crank and all previous angular pressure.
        hello_keyboard_update(&keyboard,now,0,0,kButtonA,-360);now+=16;
        assert(keyboard.model.cursor==positions[4]&&!keyboard.crank_remainder&&!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        assert(!strcmp(keyboard.model.text,input)&&keyboard.model.upper==(bool)upper);
        hello_keyboard_update(&keyboard,now,kButtonRight,kButtonRight,0,0);now+=16;
        assert(keyboard.model.group==2&&!keyboard.model.pending&&!keyboard.crank_remainder);
        hello_keyboard_update(&keyboard,now,kButtonRight,0,0,0);assert(!keyboard.model.pending);
        hello_keyboard_close(&keyboard);
    }
    // A genuinely short A press still changes case; its crank never moves text.
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);HelloKeyboard keyboard;
    config.emoji_picker_enabled=false; // Legacy one-view chord-to-typing handoff.
    loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,input));
    hello_keyboard_update(&keyboard,340000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,340100,kButtonA,0,0,-360);
    hello_keyboard_update(&keyboard,340200,0,0,kButtonA,0);
    assert(keyboard.model.upper&&keyboard.model.cursor==positions[4]&&!strcmp(keyboard.model.text,input));
    // A prior-frame A+Dpad chord enables the same cursor route before350ms.
    hello_keyboard_update(&keyboard,341000,kButtonA,kButtonA,0,0);
    hello_keyboard_update(&keyboard,341001,kButtonA|kButtonUp,kButtonUp,0,0);
    assert(hello_keyboard_model_is_choosing_layer(&keyboard.model)&&!keyboard.model.group);
    hello_keyboard_update(&keyboard,341002,kButtonA|kButtonUp,0,0,-60);
    assert(keyboard.model.cursor==positions[3]&&!strcmp(keyboard.model.text,input));
    hello_keyboard_update(&keyboard,341003,kButtonRight,kButtonRight,kButtonA|kButtonUp,360);
    assert(keyboard.model.cursor==positions[3]&&keyboard.model.upper&&!keyboard.crank_remainder);
    assert(keyboard.model.group==2&&!keyboard.model.pending&&!strcmp(keyboard.model.text,input));
    hello_keyboard_update(&keyboard,341019,kButtonRight,0,0,0);assert(!keyboard.model.pending);
    hello_keyboard_close(&keyboard);
}

static void test_dpad_cursor_uses_configured_button_repeat(const HelloKeyboard* reference) {
    const char* family="aé👨‍👩‍👧‍👦z";
    for(int type=0;type<3;type++)for(int half=0;half<2;half++)for(int rollover=0;rollover<2;rollover++) {
        const char* initial=type==HELLO_INPUT_CONTACT_NUMBER?"1234":family;
        const size_t positions[]={0,1,type==HELLO_INPUT_CONTACT_NUMBER?2:strlen("aé"),type==HELLO_INPUT_CONTACT_NUMBER?3:strlen("aé👨‍👩‍👧‍👦"),strlen(initial)};
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;config.button_navigation_scale=half?.5f:1.0f;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        assert(hello_keyboard_open_field(&keyboard,initial,(HelloInputType)type));
        assert(keyboard.model.button_navigation_scale==config.button_navigation_scale);
        const uint32_t start=rollover?UINT32_MAX-100:540000;
        const uint32_t delay=half?175:350,repeat=half?50:100;
        hello_keyboard_update(&keyboard,start,kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,start+349,kButtonA,0,0,0);assert(!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        hello_keyboard_update(&keyboard,start+350,kButtonA,0,0,0);assert(hello_keyboard_model_is_choosing_layer(&keyboard.model));
        const uint32_t left=start+360;
        hello_keyboard_update(&keyboard,left,kButtonA|kButtonLeft,kButtonLeft,0,0);assert(keyboard.model.cursor==positions[3]);
        hello_keyboard_update(&keyboard,left+delay-1,kButtonA|kButtonLeft,0,0,0);assert(keyboard.model.cursor==positions[3]);
        hello_keyboard_update(&keyboard,left+delay,kButtonA|kButtonLeft,0,0,0);assert(keyboard.model.cursor==positions[2]);
        hello_keyboard_update(&keyboard,left+delay+repeat-1,kButtonA|kButtonLeft,0,0,0);assert(keyboard.model.cursor==positions[2]);
        hello_keyboard_update(&keyboard,left+delay+repeat,kButtonA|kButtonLeft,0,0,0);assert(keyboard.model.cursor==positions[1]);
        const uint32_t right=left+delay+repeat+1;
        hello_keyboard_update(&keyboard,right,kButtonA|kButtonRight,kButtonRight,kButtonLeft,0);assert(keyboard.model.cursor==positions[2]);
        hello_keyboard_update(&keyboard,right+1000,kButtonA|kButtonRight,0,0,0);assert(keyboard.model.cursor==positions[3]); // One step despite lag.
        hello_keyboard_update(&keyboard,right+1000+repeat-1,kButtonA|kButtonRight,0,0,0);assert(keyboard.model.cursor==positions[3]);
        hello_keyboard_update(&keyboard,right+1000+repeat,kButtonA|kButtonRight,0,0,0);
        assert(keyboard.model.cursor==positions[4]);
        hello_keyboard_update(&keyboard,right+1000+2*repeat,kButtonA|kButtonRight,0,0,0);assert(keyboard.model.cursor==positions[4]);
        const uint32_t next=right+1020+2*repeat;
        hello_keyboard_update(&keyboard,next,kButtonA|kButtonLeft,kButtonLeft,kButtonRight,0);assert(keyboard.model.cursor==positions[3]);
        hello_keyboard_update(&keyboard,next+delay,kButtonA,0,kButtonLeft,0);assert(keyboard.model.cursor==positions[3]&&!keyboard.model.cursor_hold);
        hello_keyboard_update(&keyboard,next+delay+1,kButtonA|kButtonLeft,kButtonLeft,0,0);assert(keyboard.model.cursor==positions[2]);
        hello_keyboard_update(&keyboard,next+2*delay+1,kButtonLeft,0,kButtonA,0);
        assert(keyboard.model.cursor==positions[2]&&!keyboard.model.cursor_hold&&!hello_keyboard_model_is_choosing_layer(&keyboard.model));
        hello_keyboard_update(&keyboard,next+30000,kButtonLeft,0,0,0);
        assert(!strcmp(keyboard.model.text,initial)&&keyboard.model.cursor==positions[2]&&!keyboard.model.upper&&!keyboard.model.pending&&!keyboard.model.group);
        draw_input_field(&keyboard,0);assert(!strcmp(rendered,initial));hello_keyboard_close(&keyboard);
    }
}

static void test_input_cursor_stays_in_current_glyph_cell(const HelloKeyboard* reference) {
    HelloKeyboard keyboard={0};keyboard.pd=reference->pd;keyboard.config=reference->config;memcpy(keyboard.art,reference->art,sizeof(keyboard.art));
    keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);cursor_font_metrics=true;
    const struct {const char* text;size_t at;int x,width;} cases[]={
        {"WiW",0,0,7},{"WiW",1,7,1},{"WiW",2,8,8},
        {"Wl1.W",1,8,1},{"Wl1.W",2,9,1},{"Wl1.W",3,10,1},{"ii",0,0,2},{"ii",1,2,2}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        hello_keyboard_model_init(&keyboard.model,cases[i].text,cases[i].at);
        text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
        assert(text_draws==1&&!strcmp(rendered,cases[i].text)&&text_x==23);
        assert(cursor_draws==1&&cursor_x==23+cases[i].x&&cursor_width==cases[i].width);
    }
    char long_text[83];memset(long_text,'W',80);strcpy(long_text+80,"iW");
    hello_keyboard_model_init(&keyboard.model,long_text,80);text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
    assert(text_draws==1&&!strcmp(rendered,long_text)&&text_x<23&&cursor_x==text_x+639&&cursor_width==1&&cursor_x+cursor_width<=374);
    // Bitmap text uses its real ink width and excludes the one-pixel tracking
    // gap. Compare against the glyph origins actually sent to drawBitmap.
    keyboard.config.font=NULL;capture_bitmap_text=true;
    const char* bitmap_cases[]={"Wil1.W",long_text};
    for(unsigned text_case=0;text_case<2;text_case++) {
        const char* value=bitmap_cases[text_case];const size_t first=text_case?80:0,last=text_case?81:strlen(value)-1;
        for(size_t at=first;at<=last;at++) {
            hello_keyboard_model_init(&keyboard.model,value,at);drawn_glyphs=cursor_draws=0;draw_input_field(&keyboard,0);
            assert(cursor_draws==1&&cursor_x==glyph_x[at]&&cursor_width==glyph_width((unsigned char)value[at]));
            if(at+1<strlen(value))assert(cursor_x+cursor_width<glyph_x[at+1]);
        }
    }
    capture_bitmap_text=false;keyboard.config.font=(LCDFont*)&bitmap;
    BrbUnicodeFont unicode={.pd=reference->pd};keyboard.config.unicode_font=&unicode;
    const BrbEmojiGlyph* emoji=brb_emoji_find(brb_unicode_font_emoji_set(&unicode),"😂",strlen("😂"));assert(emoji);
    const size_t offsets[]={0,1,2,6,7};const int origins[]={0,7,9,9+emoji->advance,10+emoji->advance};
    const int widths[]={7,2,emoji->advance,1,8};
    for(unsigned i=0;i<5;i++) {
        hello_keyboard_model_init(&keyboard.model,"Wi😂iW",offsets[i]);cursor_draws=0;draw_input_field(&keyboard,0);
        assert(cursor_draws==1&&cursor_x==23+origins[i]&&cursor_width==widths[i]);
    }
    keyboard.config.unicode_font=NULL;hello_input_field_init(&keyboard.input_field,HELLO_INPUT_MASKEDTEXT);
    assert(hello_input_field_toggle_mask(&keyboard.input_field));hello_keyboard_model_init(&keyboard.model,long_text,80);
    cursor_draws=0;draw_input_field(&keyboard,0);
    assert(strlen(rendered)==82&&cursor_draws==1&&cursor_x==text_x+640&&cursor_width==8&&cursor_x+cursor_width<=350);
    for(unsigned i=0;i<82;i++)assert(rendered[i]=='*');
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);hello_keyboard_model_init(&keyboard.model,"",0);
    draw_input_field(&keyboard,0);assert(cursor_x==25&&cursor_width>=6); // Empty/end caret retains its minimum.
    cursor_font_metrics=false;
}

static void test_cursor_blink_tracks_actual_keyboard_activity(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;config.button_navigation_scale=.5f;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    const char* input="abcdefghijklmnopqrstuvwxyz";assert(hello_keyboard_open(&keyboard,input));
    cursor_draws=0;draw_input_field(&keyboard,500);assert(!cursor_draws); // Fresh idle retains the global off phase.
    hello_keyboard_update(&keyboard,600,kButtonA,kButtonA,0,0);hello_keyboard_update(&keyboard,950,kButtonA,0,0,0);
    hello_keyboard_update(&keyboard,1000,kButtonA|kButtonLeft,kButtonLeft,0,0);
    for(uint32_t now=1000;now<=1900;now+=100) {
        hello_keyboard_update(&keyboard,now,kButtonA|kButtonLeft,0,0,0);cursor_draws=0;draw_input_field(&keyboard,now);assert(cursor_draws==1);
    }
    hello_keyboard_update(&keyboard,1920,kButtonA,0,kButtonLeft,0);
    const uint32_t stopped=keyboard.cursor_blink.anchor;assert(stopped==1920);
    cursor_draws=0;draw_input_field(&keyboard,2339);assert(cursor_draws==1);
    cursor_draws=0;draw_input_field(&keyboard,2340);assert(!cursor_draws&&keyboard.cursor_blink.anchor==stopped);
    hello_keyboard_update(&keyboard,2341,kButtonA|kButtonLeft,kButtonLeft,0,0);
    cursor_draws=0;draw_input_field(&keyboard,2341);assert(cursor_draws==1); // Resume during a global off phase.
    hello_keyboard_update(&keyboard,2342,0,0,kButtonA|kButtonLeft,0);
    hello_keyboard_update(&keyboard,2343,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    assert(!keyboard.cursor_blink.active);hello_keyboard_update(&keyboard,2693,kButtonA|kButtonB,0,0,0);
    assert(keyboard.cancel_dialog_active);cursor_draws=0;draw_input_field(&keyboard,2762);assert(!cursor_draws);hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open(&keyboard,input));
    hello_keyboard_update(&keyboard,4300,kButtonA,kButtonA,0,0);hello_keyboard_update(&keyboard,4650,kButtonA,0,0,0);
    hello_keyboard_update(&keyboard,4660,kButtonA,0,0,-60);cursor_draws=0;draw_input_field(&keyboard,4660);assert(cursor_draws==1);
    hello_keyboard_update(&keyboard,4676,kButtonA,0,0,0);assert(!keyboard.cursor_blink.active&&keyboard.cursor_blink.anchor==4676);
    cursor_draws=0;draw_input_field(&keyboard,5095);assert(cursor_draws==1);
    cursor_draws=0;draw_input_field(&keyboard,5096);assert(!cursor_draws);
    hello_keyboard_update(&keyboard,6000,0,0,kButtonA,0);
    hello_keyboard_update(&keyboard,6400,kButtonRight,kButtonRight,0,0);
    hello_keyboard_update(&keyboard,6410,kButtonRight|kButtonA,kButtonA,0,0);
    assert(keyboard.model.pending&&keyboard.cursor_blink.active);cursor_draws=0;draw_input_field(&keyboard,6410);assert(cursor_draws==1);
    hello_keyboard_update(&keyboard,6411,kButtonRight|kButtonA|kButtonB,kButtonB,0,0);assert(!keyboard.cursor_blink.active);
    hello_keyboard_update(&keyboard,6761,kButtonRight|kButtonA|kButtonB,0,0,0);assert(keyboard.cancel_dialog_active);
    cursor_draws=0;draw_input_field(&keyboard,6831);assert(!cursor_draws);
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,input));cursor_draws=0;draw_input_field(&keyboard,500);assert(!cursor_draws);hello_keyboard_close(&keyboard);
}
static void test_crank_wrap_paints_full_endpoint_during_gate(const HelloKeyboard* reference) {
    const char* fitted[HELLO_KEYBOARD_GROUP_COUNT]={"1234567890",NULL,NULL,NULL,NULL,NULL};uint32_t fitted_settle[2]={0};
    for(int variant=0;variant<3;variant++)for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,direction,variant==0?fitted:variant==1?NULL:kOverflowGroups,550000);
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        reset_group_scrolls(&keyboard);keyboard.selection_marker.active=false;keyboard.model.pending_side=3;
        for(uint32_t now=550000;now<=550600;now+=20)draw_group(&keyboard,1,105,69,now);
        const uint32_t begin=550620;const float threshold=crank_test_threshold(&keyboard,direction);
        hello_keyboard_update(&keyboard,begin,kButtonUp,0,0,(float)direction*(threshold+0.001f));
        const int endpoint=direction>0?0:hello_keyboard_model_group_length(&keyboard.model,1)-1;
        assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==1&&keyboard.model.pending_side==3);
        char selected[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        uint32_t settled=UINT32_MAX;
        for(uint32_t elapsed=0;elapsed<=1240;elapsed+=20) {
            hello_keyboard_update(&keyboard,begin+elapsed,kButtonUp,0,0,0);
            capture_group_view=true;expected_selected_item=selected;expected_item_draws=group_draw_count=marker_draws=0;expected_item_draw_index=-1;
            draw_group(&keyboard,1,105,69,begin+elapsed);
            assert(expected_item_draws==1&&marker_draws==1&&expected_item_draw_index>=0);
            const int available=group_bounds(&keyboard,1).width-24;
            const int slot=group_metrics(&keyboard,1,available,true).selected_slot;
            const int center=expected_item_x+keyboard_text_width(&keyboard,selected)/2,left=center-slot/2,right=center+slot-slot/2;
            assert(left>=group_clip_x-1&&right<=group_clip_x+group_clip_w+1);
            for(int item=0;item<group_draw_count;item++)if(item!=expected_item_draw_index)
                assert(group_item_x[item]+group_item_width[item]<=left+1||group_item_x[item]>=right-1);
            assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
            expected_selected_item=NULL;capture_group_view=false;
            assert(keyboard.model.index==endpoint&&!keyboard.model.text[0]);
            if(!keyboard.selection_lock_group){settled=elapsed;break;}
        }
        assert(settled!=UINT32_MAX&&settled<=1250);
        if(!variant)fitted_settle[direction>0]=settled;else assert(settled==fitted_settle[direction>0]);
    }
}

static void test_active_group_freezes_only_vertical_hover(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard;memset(&keyboard,0,sizeof(keyboard));keyboard.pd=reference->pd;keyboard.config=reference->config;
        hello_keyboard_model_init(&keyboard.model,"",0);keyboard.model.upper=group==6;keyboard.model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
        hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);keyboard.visible=true;
        const uint32_t begin=350000;const int other=group==2?4:2;
        capture_group_view=true;group_draw_count=0;draw_group(&keyboard,group,105,69,begin);const int entry_y=group_pill_y;
        hello_keyboard_update(&keyboard,begin,directions[group],directions[group],0,0);
        assert(keyboard.model.group==group&&keyboard.model.group_at==begin);
        group_draw_count=0;draw_group(&keyboard,group,105,69,begin);assert(group_pill_y==entry_y);
        group_draw_count=0;draw_group(&keyboard,other,236,118,begin);const int other_y=group_pill_y;bool other_moved=false;
        hello_keyboard_update(&keyboard,begin+16,directions[group]|kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,begin+24,directions[group],0,kButtonA,0);
        for(uint32_t elapsed=32;elapsed<=3000;elapsed+=32) {
            hello_keyboard_update(&keyboard,begin+elapsed,directions[group],0,0,0);
            marker_draws=0;group_draw_count=0;draw_group(&keyboard,group,105,69,begin+elapsed);
            assert(group_pill_y==entry_y&&marker_draws==1&&marker_y==entry_y+2);
            group_draw_count=0;draw_group(&keyboard,other,236,118,begin+elapsed);if(group_pill_y!=other_y)other_moved=true;
        }
        assert(other_moved);
        hello_keyboard_update(&keyboard,begin+3016,0,0,directions[group],0);
        assert(!keyboard.model.group);
        group_draw_count=0;draw_group(&keyboard,group,105,69,begin+3016);
        const int original_height=group==2||group==4?38:36;
        const int height=keyboard.config.group_height?keyboard.config.group_height:original_height;
        assert(group_pill_y==69+hello_floating_offset(begin+3016,(float)(group-1)*.45f)+(original_height-height)/2);
        capture_group_view=false;
    }
}

static void accept_editing_save(HelloKeyboard* keyboard,uint32_t now) {
    assert(save_editing_popup(keyboard,now)==HELLO_KEYBOARD_DONE);
    assert(!keyboard->cancel_dialog_active&&!keyboard->model.submit_requested);
}

static void test_numeric_full_layout_and_commit_validation(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;config.group_height=30;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(hello_keyboard_open_field(&keyboard,"12345",HELLO_INPUT_CONTACT_NUMBER));
    assert(keyboard.model.field.type==HELLO_INPUT_CONTACT_NUMBER&&keyboard.model.field.max_length==6);
    check_base_page(&keyboard,0); // Numeric fields expose the same letters and Up page.
    for(int upper=0;upper<2;upper++) {
        if(upper) {
            hello_keyboard_update(&keyboard,1200,kButtonA,kButtonA,0,0);
            hello_keyboard_update(&keyboard,1216,0,0,kButtonA,0);assert(keyboard.model.upper);
        }
        const uint32_t start=100+(uint32_t)upper*1300;
        hello_keyboard_update(&keyboard,start,kButtonRight,kButtonRight,0,0);assert(keyboard.model.group==2);
        hello_keyboard_update(&keyboard,start+16,kButtonRight|kButtonA,kButtonA,0,0);
        assert(keyboard.model.pending==(uint32_t)(upper?'N':'n')&&!strcmp(keyboard.model.text,"12345"));
        draw_group(&keyboard,2,236,118,start+16);
        hello_keyboard_update(&keyboard,start+24,kButtonRight,0,kButtonA,0);
        assert(hello_keyboard_update(&keyboard,start+32,0,0,kButtonRight,0)==HELLO_KEYBOARD_NONE);
        assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,"12345"));
        assert(!strcmp(keyboard.error_storage,"Only digits are allowed"));
        if(upper)check_base_page(&keyboard,start+40);
    }
    hello_keyboard_update(&keyboard,2800,kButtonA,kButtonA,0,0);hello_keyboard_update(&keyboard,2816,0,0,kButtonA,0);
    assert(!keyboard.model.upper);hello_keyboard_update(&keyboard,2900,kButtonUp,kButtonUp,0,0);assert(keyboard.model.group==1);
    hello_keyboard_update(&keyboard,2916,kButtonUp|kButtonA,kButtonA,0,0);assert(keyboard.model.pending=='1');
    hello_keyboard_update(&keyboard,2924,kButtonUp,0,kButtonA,0);hello_keyboard_update(&keyboard,2932,0,0,kButtonUp,0);
    assert(!strcmp(keyboard.model.text,"123451"));
    hello_keyboard_update(&keyboard,3000,kButtonA,kButtonA,0,0);hello_keyboard_update(&keyboard,3016,0,0,kButtonA,0);
    hello_keyboard_update(&keyboard,3100,kButtonDown,kButtonDown,0,0);
    hello_keyboard_update(&keyboard,3116,kButtonDown|kButtonB,kButtonB,0,0);assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);
    assert(hello_keyboard_update(&keyboard,3124,0,0,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);accept_editing_save(&keyboard,3130);
    assert(!strcmp(keyboard.model.text,"123451"));hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open(&keyboard,"abc"));
    hello_keyboard_update(&keyboard,4000,kButtonB,kButtonB,0,0);assert(!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_update(&keyboard,4349,kButtonB,0,0,0);assert(!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_update(&keyboard,4350,kButtonB,0,0,0);assert(!strcmp(keyboard.model.text,"a"));
    hello_keyboard_update(&keyboard,4450,kButtonB,0,0,0);assert(!keyboard.model.text[0]);
    hello_keyboard_update(&keyboard,4500,0,0,kButtonB,0);hello_keyboard_close(&keyboard);
}

static void test_active_footer_icons_and_shared_caption(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    const int dpad_frames[]={0,1,2,3,4,1,1};
    const PDButtons held_directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int upper=0;upper<2;upper++) {
        keyboard.model.group=group;keyboard.model.alternate=false;keyboard.model.field.type=(group==5)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;keyboard.model.upper=group==6;
        keyboard.model.upper=upper;
        capture_footer_canvas=capture_footer_labels=true;
        footer_circles=footer_white_fills=footer_caption_count=footer_label_clips=footer_label_fills=0;
        expected_table=2;expected_frame=dpad_frames[group];keyboard.footer_dpad_buttons=held_directions[group];
        native_dpad_begin(3);draw_footer(&keyboard,2000);native_dpad_check(dpad_frames[group]);
        assert(footer_circles==2&&!footer_white_fills&&footer_caption_count==4&&footer_label_clips==4);
        assert(!strcmp(footer_captions[0],"Select")&&footer_caption_x[0]==27&&footer_caption_clip_w[0]==158);
        assert(!strcmp(footer_captions[1],"A")&&!strcmp(footer_captions[2],"B")&&!strcmp(footer_captions[3],"Cursor"));
        assert(footer_circle_x[0]>=215&&footer_circle_x[1]-footer_circle_x[0]==24);
        assert(footer_caption_x[3]==footer_circle_x[1]+24&&footer_caption_x[3]+measure(NULL,"Cursor",strlen("Cursor"),kUTF8Encoding,0)==396);
        assert(footer_caption_modes[1]==kDrawModeNXOR&&footer_caption_modes[2]==kDrawModeNXOR);
        assert(footer_caption_modes[0]==kDrawModeCopy&&footer_caption_modes[3]==kDrawModeCopy);
        capture_footer_canvas=capture_footer_labels=false;
    }
    char custom[]="CUSTOM SELECTION CAPTION LONGER THAN ITS CELL";
    char custom_select[]="CUSTOM SELECT CAPTION LONGER THAN ITS CELL";
    const HelloKeyboardLabels labels={.selection_text=custom,.select_text=custom_select};
    assert(hello_keyboard_set_labels(&keyboard,&labels));custom[0]='x';custom_select[0]='x';
    capture_footer_canvas=capture_footer_labels=true;
    footer_circles=footer_white_fills=footer_caption_count=footer_label_clips=footer_label_fills=0;
    native_dpad_begin(3);draw_footer(&keyboard,2000);native_dpad_check(1);
    assert(footer_circles==2&&footer_caption_count==4);
    assert(!strcmp(footer_captions[0],"CUSTOM SELECT CAPTION LONGER THAN ITS CELL")&&footer_caption_x[0]==27&&footer_caption_clip_w[0]==158);
    assert(!strcmp(footer_captions[3],"CUSTOM SELECTION CAPTION LONGER THAN ITS CELL"));
    assert(footer_circle_x[0]==215&&footer_circle_x[1]==239&&footer_caption_x[3]==263);
    assert(footer_clip_x==263&&footer_clip_w==133&&footer_clip_x+footer_clip_w==396);
    capture_footer_canvas=capture_footer_labels=false;
}

static void test_external_labels_footer_and_view_reset(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    char custom[]="CUSTOM DELETE LABEL LONGER THAN ITS CELL";
    HelloKeyboardLabels labels={.delete_text=custom,.clear="Remove",.done="Accept",.cancel="Back"};
    HelloKeyboardFeedbackConfig feedback={.duration_ms=120};
    const HelloKeyboardOpenOptions options={.labels=&labels,.feedback=&feedback};
    struct playdate_sys* system=(struct playdate_sys*)reference->pd->system;
    void (*original_rename)(PDMenuItem*,const char*)=system->setMenuItemTitle;
    capture_menu_labels=true;menu_label_adds=menu_label_removes=menu_label_renames=0;
    assert(hello_keyboard_open_field_with_options(&keyboard,"",HELLO_INPUT_PLAINTEXT,&options));
    assert(menu_label_adds==3&&!menu_label_removes&&!menu_label_renames);
    assert(!strcmp(menu_titles[0],"Remove")&&!strcmp(menu_titles[1],"Accept")&&!strcmp(menu_titles[2],"Back"));
    assert(menu_callbacks[0]==clear_callback&&menu_callbacks[1]==done_callback&&menu_callbacks[2]==cancel_callback);
    for(int i=0;i<3;i++)assert(menu_userdata[i]==&keyboard);
    PDMenuItem* handles[]={keyboard.clear_item,keyboard.done_item,keyboard.cancel_item};
    custom[0]='x';
    assert(!strcmp(keyboard.config.labels.delete_text,"CUSTOM DELETE LABEL LONGER THAN ITS CELL"));
    capture_footer_labels=true;footer_label_clips=footer_label_fills=text_draws=footer_caption_count=0;
    draw_idle_footer_labels(&keyboard);
    assert(footer_label_clips==1&&!footer_label_fills&&text_draws==1);
    assert(!strcmp(footer_captions[0],"CUSTOM DELETE LABEL LONGER THAN ITS CELL"));
    assert(footer_clip_y==212&&footer_clip_h==28&&footer_clip_x+footer_clip_w<=185);
    capture_footer_labels=false;
    const HelloKeyboardLabels renamed={.clear="Empty",.done="Save",.cancel="Return"};
    system->setMenuItemTitle=NULL;
    assert(!hello_keyboard_set_labels(&keyboard,&renamed));
    assert(!strcmp(keyboard.config.labels.clear,"Remove")&&!strcmp(menu_titles[0],"Remove"));
    assert(menu_label_adds==3&&!menu_label_removes&&!menu_label_renames);
    system->setMenuItemTitle=rename_menu;
    assert(hello_keyboard_set_labels(&keyboard,&renamed));
    assert(menu_label_renames==3&&menu_label_adds==3&&!menu_label_removes);
    assert(!strcmp(menu_titles[0],"Empty")&&!strcmp(menu_titles[1],"Save")&&!strcmp(menu_titles[2],"Return"));
    assert(keyboard.clear_item==handles[0]&&keyboard.done_item==handles[1]&&keyboard.cancel_item==handles[2]);
    assert(hello_keyboard_set_labels(&keyboard,NULL)&&menu_label_renames==6);
    assert(!strcmp(menu_titles[0],"clear")&&!strcmp(menu_titles[1],"done")&&!strcmp(menu_titles[2],"cancel"));
    hello_keyboard_close(&keyboard);assert(menu_label_removes==3);
    menu_label_adds=menu_label_removes=menu_label_renames=0;
    assert(hello_keyboard_open_field_with_options(&keyboard,"",HELLO_INPUT_PLAINTEXT,NULL));
    assert(!strcmp(keyboard.config.labels.clear,"clear")&&!strcmp(keyboard.config.labels.done,"done")&&
        !strcmp(keyboard.config.labels.cancel,"cancel")&&keyboard.config.feedback.duration_ms==5000);
    capture_footer_labels=true;footer_label_clips=footer_label_fills=text_draws=footer_caption_count=0;
    draw_idle_footer_labels(&keyboard);
    assert(footer_label_clips==1&&!footer_label_fills&&text_draws==1);
    assert(!strcmp(footer_captions[0],"Delete"));
    capture_footer_labels=false;hello_keyboard_close(&keyboard);
    assert(menu_label_adds==3&&menu_label_removes==3&&!menu_label_renames);
    capture_menu_labels=false;system->setMenuItemTitle=original_rename;
}
static const char* art_feedback_resolver(void* userdata,const char* code,const char* fallback) {
    assert(userdata&&code&&fallback);
    return !strcmp(code,"E_VIEW")?(const char*)userdata:NULL;
}
typedef struct {int calls;char code[32];} PolicyFeedbackCapture;
static const char* policy_feedback_message(void* userdata,const char* code,const char* fallback) {
    (void)fallback;PolicyFeedbackCapture* capture=userdata;capture->calls++;
    snprintf(capture->code,sizeof(capture->code),"%s",code);return "Policy rejected";
}
static size_t art_family_text(char* output,size_t count) {
    const char* family="👨‍👩‍👧‍👦";const size_t bytes=strlen(family);
    assert(count*bytes<=HELLO_KEYBOARD_TEXT_BYTES);
    for(size_t i=0;i<count;i++)memcpy(output+i*bytes,family,bytes);
    output[count*bytes]=0;return count*bytes;
}
static void test_instance_field_policies_large_text_and_completion(const HelloKeyboard* reference) {
    HelloKeyboardConfig one,two;hello_keyboard_default_config(&one);hello_keyboard_default_config(&two);
    one.font=two.font=(LCDFont*)&bitmap;one.font_size=two.font_size=14;
    one.field=(HelloKeyboardFieldPolicy){.type=HELLO_INPUT_PLAINTEXT,.min_length=1,.max_length=160,.trim_for_length=true};
    two.field=(HelloKeyboardFieldPolicy){.type=HELLO_INPUT_MASKEDTEXT,.min_length=3,.max_length=5,.max_bytes=24};
    one.menu_actions=HELLO_KEYBOARD_MENU_CANCEL;
    HelloKeyboard first,second;loads=0;hello_keyboard_init_with_config(&first,reference->pd,&one);
    loads=0;hello_keyboard_init_with_config(&second,reference->pd,&two);
    const HelloKeyboardLabels first_labels={.done="Send",.cancel="Keep"},second_labels={.done="Store",.cancel="Back"};
    PolicyFeedbackCapture capture={0};const HelloKeyboardFeedbackConfig feedback={.resolver=policy_feedback_message,.userdata=&capture};
    const HelloKeyboardOpenOptions first_options={.labels=&first_labels},second_options={.labels=&second_labels,.feedback=&feedback};
    char families[HELLO_KEYBOARD_TEXT_BYTES+2],padded[HELLO_KEYBOARD_TEXT_BYTES+2];art_family_text(families,160);
    strcpy(padded,"\u00a0");strcpy(padded+2,families);strcat(padded,"\u3000");
    assert(hello_keyboard_open_configured(&first,padded,&first_options));
    assert(!first.clear_item&&!first.done_item&&first.cancel_item);
    assert(!strcmp(first.initial_text,padded)&&!strcmp(first.config.labels.done,"Send"));
    draw_input_field(&first,0);assert(!strcmp(rendered,padded)&&cursor_x>=22&&cursor_x<377);
    HelloKeyboardFieldPolicy masked_large=one.field;masked_large.type=HELLO_INPUT_MASKEDTEXT;
    const HelloKeyboardOpenOptions large_options={.field=&masked_large,.labels=&second_labels};
    assert(hello_keyboard_open_configured(&second,families,&large_options));
    assert(second.config.field.max_length==5&&second.model.field.max_length==160&&!strcmp(second.config.labels.done,"Store"));
    assert(hello_input_field_toggle_mask(&second.input_field));draw_input_field(&second,0);
    assert(strlen(rendered)==160);for(int i=0;i<160;i++)assert(rendered[i]=='*');
    assert(hello_keyboard_model_move_cursor(&second.model,-1)==-1&&second.model.cursor==3975);
    draw_input_field(&second,0);assert(strlen(rendered)==160&&cursor_x>=22&&cursor_x<377);
    draw_input_field(&first,0);assert(!strcmp(rendered,padded)); // Scratch belongs to each instance.
    hello_keyboard_close(&second);
    assert(hello_keyboard_open_configured(&second,"ab",&second_options));
    const HelloKeyboardFieldPolicy before=second.config.field;
    HelloKeyboardFieldPolicy invalid=before;invalid.max_length=2;
    assert(!hello_keyboard_set_field_policy(&second,&invalid)&&!memcmp(&before,&second.config.field,sizeof(before)));
    assert(second.model.field.max_length==5&&first.model.field.max_length==160);
    done_callback(&second);
    assert(hello_keyboard_update(&second,360000,kButtonB,kButtonB,0,360)==HELLO_KEYBOARD_NONE);
    assert(second.visible&&second.cancel_dialog_active&&!strcmp(second.model.text,"ab")&&!capture.calls);
    assert(save_editing_popup(&second,360001)==HELLO_KEYBOARD_NONE);
    assert(!strcmp(second.model.text,"ab")&&!strcmp(second.error_storage,"Policy rejected"));
    assert(capture.calls==1&&!strcmp(capture.code,"FIELD_TOO_SHORT")&&!strcmp(first.model.text,padded));
    done_callback(&first);
    assert(hello_keyboard_update(&first,361001,kButtonB,kButtonB,0,-360)==HELLO_KEYBOARD_NONE);
    assert(save_editing_popup(&first,361002)==HELLO_KEYBOARD_DONE);
    assert(!strcmp(first.model.text,families)&&brb_grapheme_count(first.model.text)==160);
    assert(!strcmp(second.model.text,"ab")&&!strcmp(second.config.labels.done,"Store"));
    hello_keyboard_close(&first);hello_keyboard_close(&second);
    art_family_text(padded,161);assert(!hello_keyboard_open_configured(&first,padded,&first_options));
    assert(!first.visible&&!strcmp(first.model.text,families));
    size_t at=art_family_text(padded,159);padded[at++]='a';
    for(int i=0;i<60;i++){memcpy(padded+at,"́",2);at+=2;}padded[at]=0;assert(at==4096);
    assert(hello_keyboard_open_configured(&first,padded,&first_options));
    draw_input_field(&first,0);assert(strlen(rendered)==4096);hello_keyboard_close(&first);
    at-=2;memcpy(padded+at,"⃝",3);at+=3;padded[at]=0;assert(at==4097);
    assert(!hello_keyboard_open_configured(&first,padded,&first_options)&&strlen(first.model.text)==4096);
    HelloKeyboardFieldPolicy empty={0};const HelloKeyboardOpenOptions empty_options={.field=&empty};
    assert(hello_keyboard_open_configured(&second,"",&empty_options));assert(second.model.field.max_length==0&&second.config.field.max_length==5);
    done_callback(&second);assert(hello_keyboard_update(&second,362100,0,0,0,0)==HELLO_KEYBOARD_NONE);
    assert(save_editing_popup(&second,362101)==HELLO_KEYBOARD_DONE);hello_keyboard_close(&second);
}

static void test_editing_popup_and_password_hide_menu(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
    config.menu_actions=HELLO_KEYBOARD_MENU_HIDE;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    options_adds=options_removes=0;
    assert(hello_keyboard_open_field(&keyboard,"plain",HELLO_INPUT_PLAINTEXT));
    assert(!keyboard.hide_item&&!keyboard.cancel_item&&!keyboard.done_item&&!keyboard.clear_item&&!options_adds);hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open_field(&keyboard,"12345",HELLO_INPUT_CONTACT_NUMBER));assert(!keyboard.hide_item&&!options_adds);hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open_field(&keyboard,"secret",HELLO_INPUT_MASKEDTEXT));
    assert(keyboard.hide_item&&options_adds==1&&!options_value&&!keyboard.input_field.masked&&!keyboard.cancel_item);
    assert(!strcmp(options_title,"Hide")&&!strcmp(options_labels[0],"No")&&!strcmp(options_labels[1],"Yes"));
    options_value=1;options_callback(options_userdata);assert(keyboard.input_field.masked&&!strcmp(keyboard.model.text,"secret"));
    options_value=0;options_callback(options_userdata);assert(!keyboard.input_field.masked);
    hello_keyboard_update(&keyboard,420000,kButtonA|kButtonB,kButtonA|kButtonB,0,360);
    hello_keyboard_update(&keyboard,420350,kButtonA|kButtonB,0,0,0);
    assert(keyboard.cancel_dialog_active&&!keyboard.input_field.masked&&!strcmp(keyboard.model.text,"secret"));
    const uint32_t motion_times[]={420350,420750,421650};
    for(unsigned t=0;t<3;t++) {
        capture_cancel_popup=capture_footer_canvas=capture_footer_labels=true;
        cancel_popup_fills=cancel_popup_frame_count=footer_circles=footer_caption_count=footer_white_fills=footer_dividers=popup_footer_white=0;
        draw_cancel_prompt(&keyboard,motion_times[t]);
        assert(cancel_popup_fills==1&&cancel_popup_h==44&&!cancel_popup_frame_count&&footer_circles==2&&footer_dividers==1&&popup_footer_white==1);
        assert((footer_circle_x[0]==396-keyboard_text_width(&keyboard,keyboard.config.labels.save)-24&&footer_circle_x[1]==3)||(footer_circle_x[1]==396-keyboard_text_width(&keyboard,keyboard.config.labels.save)-24&&footer_circle_x[0]==3));
        bool question=false,no=false,yes=false;
        for(int i=0;i<footer_caption_count;i++) {
            if(!strcmp(footer_captions[i],"Done typing? What's next?")){question=true;assert(footer_caption_x[i]==cancel_popup_x+(272-measure(NULL,"Done typing? What's next?",strlen("Done typing? What's next?"),kUTF8Encoding,0))/2&&footer_caption_y[i]==cancel_popup_y+15);}
            if(!strcmp(footer_captions[i],"Discard")){no=true;assert(footer_caption_x[i]==27&&footer_caption_y[i]==218);}
            if(!strcmp(footer_captions[i],"Save")){yes=true;assert(footer_caption_x[i]==396-keyboard_text_width(&keyboard,keyboard.config.labels.save)&&footer_caption_y[i]==218);}
        }
        assert(question&&no&&yes);capture_cancel_popup=capture_footer_canvas=capture_footer_labels=false;
    }
    capture_cancel_popup=capture_footer_canvas=true;footer_circles=footer_dividers=0;cancel_popup_frame_count=0;
    hello_keyboard_draw(&keyboard,420350);assert(footer_circles==2&&footer_dividers==1); // No keypad footer behind it.
    capture_cancel_popup=capture_footer_canvas=false;
    hello_keyboard_update(&keyboard,420351,0,0,kButtonA|kButtonB,0);
    assert(discard_editing_popup(&keyboard,420360)==HELLO_KEYBOARD_CANCELLED);
    assert(!keyboard.cancel_dialog_active&&!keyboard.visible&&!strcmp(keyboard.model.text,"secret"));
    hello_keyboard_close(&keyboard);assert(options_removes==1);
    options_value=1;assert(hello_keyboard_open_field(&keyboard,"secret",HELLO_INPUT_MASKEDTEXT));
    assert(!options_value&&!keyboard.input_field.masked);hello_keyboard_close(&keyboard);
    config.group_height=34;config.menu_actions=HELLO_KEYBOARD_MENU_HIDE;
    config.labels.cancel_question="Really cancel this input and discard all changes?";
    config.labels.discard="No, continue editing the current input safely";config.labels.save="Yes, discard this input and leave the keyboard";
    loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,"body"));
    hello_keyboard_update(&keyboard,422000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    hello_keyboard_update(&keyboard,422350,kButtonA|kButtonB,0,0,0);
    const unsigned marquee_times[]={0,800,840,1600};
    for(unsigned t=0;t<4;t++) {
        capture_cancel_popup=capture_footer_canvas=capture_footer_labels=true;
        cancel_popup_fills=cancel_popup_frame_count=footer_circles=footer_caption_count=footer_white_fills=footer_dividers=popup_footer_white=0;
        draw_cancel_prompt(&keyboard,422000+marquee_times[t]);
        assert(cancel_popup_h==48&&footer_circles==2&&footer_dividers==1&&popup_footer_white==1);
        assert((footer_circle_x[0]==215&&footer_circle_x[1]==3)||(footer_circle_x[1]==215&&footer_circle_x[0]==3));
        int first_question=10000;bool no=false,yes=false;
        for(int i=0;i<footer_caption_count;i++) {
            if(!strcmp(footer_captions[i],keyboard.config.labels.cancel_question)) {
                if(footer_caption_x[i]<first_question)first_question=footer_caption_x[i];
                assert(footer_caption_clip_w[i]==240&&footer_caption_y[i]==cancel_popup_y+17);
            }
            if(!strcmp(footer_captions[i],keyboard.config.labels.discard)){no=true;assert(footer_caption_x[i]==27&&footer_caption_clip_w[i]<=158&&footer_caption_x[i]+footer_caption_clip_w[i]<=185&&footer_caption_y[i]==218);}
            if(!strcmp(footer_captions[i],keyboard.config.labels.save)){yes=true;assert(footer_caption_x[i]+footer_caption_clip_w[i]==396&&footer_caption_y[i]==218);}
        }
        assert(no&&yes&&first_question==cancel_popup_x+16-(marquee_times[t]<=800?0:(int)(marquee_times[t]-800)/40));
        capture_cancel_popup=capture_footer_canvas=capture_footer_labels=false;
    }
    hello_keyboard_close(&keyboard);
    config.group_height=0;config.labels.cancel_question=NULL;config.labels.discard=NULL;config.labels.save=NULL;
    // A generic Done request also needs a fresh decision. Failed Save
    // validation keeps the field and clears the completed dialog state.
    config.menu_actions=HELLO_KEYBOARD_MENU_ALL;config.field.min_length=3;config.field.max_length=5;
    loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,"ab"));
    hello_keyboard_update(&keyboard,421000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    hello_keyboard_update(&keyboard,421350,kButtonA|kButtonB,0,0,0);assert(keyboard.cancel_dialog_active);
    done_callback(&keyboard);assert(hello_keyboard_update(&keyboard,421351,0,0,kButtonA|kButtonB,0)==HELLO_KEYBOARD_NONE);
    assert(keyboard.visible&&keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"ab"));
    assert(save_editing_popup(&keyboard,421352)==HELLO_KEYBOARD_NONE);
    assert(!strcmp(keyboard.model.text,"ab")&&keyboard.error_storage[0]);
    assert(!keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready);hello_keyboard_close(&keyboard);
}

static void test_emoji_input_instance_policy(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
    assert(config.emoji_input_enabled);
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    const BrbEmojiSet* retained=hello_keyboard_emoji_set(&keyboard);
    assert(hello_keyboard_open(&keyboard,"abc"));keyboard.model.upper=true;
    HelloKeyboardPillBounds geometry[3];
    for(int group=2;group<=4;group++)geometry[group-2]=group_bounds(&keyboard,group);
    hello_keyboard_close(&keyboard);
    for(int enabled=0;enabled<2;enabled++) {
        assert(hello_keyboard_set_emoji_input_enabled(&keyboard,enabled!=0));
        assert(hello_keyboard_emoji_set(&keyboard)==retained&&keyboard.config.groups[HELLO_KEYBOARD_EMOJI_GROUP-1][0]);
        assert(hello_keyboard_open(&keyboard,"abc"));keyboard.model.upper=true;
        assert(!hello_keyboard_set_emoji_input_enabled(&keyboard,enabled==0));
        assert(keyboard.config.emoji_input_enabled==(enabled!=0));
        assert(hello_keyboard_model_layer_count(&keyboard.model)==(enabled?2u:1u));
        for(int group=2;group<=4;group++) {
            const HelloKeyboardPillBounds actual=group_bounds(&keyboard,group);
            assert(actual.x==geometry[group-2].x&&actual.width==geometry[group-2].width);
        }
        capture_group_block=true;block_count=input_pill_count=0;
        hello_keyboard_draw(&keyboard,660000);
        assert(input_pill_count==1&&block_count==4);capture_group_block=false;
        hello_keyboard_update(&keyboard,660010,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(&keyboard,660020,kButtonUp,0,0,720);
        assert(keyboard.model.group==(enabled?HELLO_KEYBOARD_EMOJI_GROUP:1));
        if(!enabled) {
            assert(keyboard.model.upper&&!strcmp(keyboard.model.text,"abc"));
            assert(!strcmp(hello_keyboard_model_group_chars(&keyboard.model,keyboard.model.group),config.groups[0]));
        }
        hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"abc"));keyboard.model.upper=true;
        hello_keyboard_update(&keyboard,661000,kButtonRight,kButtonRight,0,0);
        hello_keyboard_update(&keyboard,661010,kButtonRight|kButtonA,kButtonA,0,0);
        assert(keyboard.model.group==2&&keyboard.model.pending);
        hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"abc"));
        hello_keyboard_update(&keyboard,662000,kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,662010,kButtonA|kButtonUp,kButtonUp,0,0);
        assert(keyboard.model.view==(enabled?HELLO_KB_VIEW_EMOJI_SETS:HELLO_KB_VIEW_KEYBOARD));
        hello_keyboard_close(&keyboard);
    }
    config.emoji_input_enabled=false;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(!keyboard.config.emoji_input_enabled&&hello_keyboard_model_layer_count(&keyboard.model)==1);
}

static void test_offset_chord_restores_immediate_face_action(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    char draft[256]="xa";
    for(int i=0;i<100;i++)memcpy(draft+2+i*2,"\xcc\x81",2);
    draft[202]='y';draft[203]=0;
    const uint32_t offsets[]={0,20,39};
    for(int first=0;first<2;first++)for(unsigned i=0;i<3;i++) {
        assert(hello_keyboard_open(&keyboard,draft));keyboard.model.cursor=202;
        const PDButtons face=first?kButtonB:kButtonA,opposite=first?kButtonA:kButtonB;
        hello_keyboard_update(&keyboard,680000,face,face,0,0);
        if(first)assert(!strcmp(keyboard.model.text,"xy")&&keyboard.model.cursor==1);
        else assert(keyboard.model.hold[HELLO_KB_BUTTON_A].active);
        assert(!keyboard.cancel_dialog_active); // Immediate singles; no delay timer.
        hello_keyboard_update(&keyboard,680000+offsets[i],face|opposite,opposite,0,0);
        assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,draft)&&keyboard.model.cursor==202);
        assert(!keyboard.model.upper&&!keyboard.model.group&&!keyboard.model.pending);
        hello_keyboard_close(&keyboard);
        assert(hello_keyboard_open(&keyboard,"draft"));
        hello_keyboard_update(&keyboard,681000,kButtonUp,kButtonUp,0,0);
        hello_keyboard_model_crank(&keyboard.model,2,681001);
        const uint32_t pending=keyboard.model.pending,at=keyboard.model.pending_at;
        const int index=keyboard.model.index,side=keyboard.model.pending_side;
        hello_keyboard_update(&keyboard,681010,kButtonUp|face,face,0,0);
        assert(keyboard.model.index!=index);
        hello_keyboard_update(&keyboard,681010+offsets[i],kButtonUp|face|opposite,opposite,0,0);
        assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"draft"));
        assert(keyboard.model.group==1&&keyboard.model.index==index&&keyboard.model.pending==pending);
        assert(keyboard.model.pending_at==at&&keyboard.model.pending_side==side);
        hello_keyboard_close(&keyboard);
    }
    assert(hello_keyboard_open(&keyboard,"abc"));
    hello_keyboard_update(&keyboard,682000,kButtonB,kButtonB,0,0);assert(!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_update(&keyboard,682020,kButtonA,kButtonA,kButtonB,0);
    assert(!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"abc"));
    hello_keyboard_update(&keyboard,683000,kButtonB,kButtonB,0,0);assert(!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_update(&keyboard,683350,kButtonB,0,0,0);assert(!strcmp(keyboard.model.text,"a"));
    hello_keyboard_update(&keyboard,683351,kButtonA|kButtonB,kButtonA,0,0);
    assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"a")); // Accepted repeats stay accepted.
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"abc"));
    hello_keyboard_update(&keyboard,684000,kButtonB,kButtonB,0,0);
    clear_callback(&keyboard);
    hello_keyboard_update(&keyboard,684020,kButtonA|kButtonB,kButtonA,0,0);
    assert(keyboard.cancel_dialog_active&&!keyboard.model.text[0]);
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"abc"));
    hello_keyboard_update(&keyboard,685000,kButtonB,kButtonB,0,0);assert(!strcmp(keyboard.model.text,"ab"));
    hello_keyboard_update(&keyboard,685010,kButtonB,kButtonB,kButtonB,0);assert(!strcmp(keyboard.model.text,"a"));
    hello_keyboard_update(&keyboard,685020,kButtonA|kButtonB,kButtonA,0,0);
    assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,"ab")); // First tap was already accepted.
    hello_keyboard_close(&keyboard);
}

static void test_disabled_emoji_up_uses_visible_symbols(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;config.emoji_input_enabled=false;
    config.groups[0]="12!?_";
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    for(int upper=0;upper<2;upper++)for(int action=0;action<3;action++) {
        assert(hello_keyboard_open(&keyboard,""));keyboard.model.upper=upper!=0;
        capture_group_block=true;block_count=input_pill_count=0;
        hello_keyboard_draw(&keyboard,670000);capture_group_block=false;
        assert(block_count==4&&input_pill_count==1);
        hello_keyboard_update(&keyboard,670010,kButtonUp,kButtonUp,0,0);
        assert(keyboard.model.group==1&&group_partner(&keyboard,3)==1);
        if(action<2) {
            const PDButtons button=action?kButtonB:kButtonA;
            hello_keyboard_update(&keyboard,670020,kButtonUp|button,button,0,0);
            assert(keyboard.model.pending==(uint32_t)(action?'_':'1'));
        } else {
            hello_keyboard_update(&keyboard,670020,kButtonUp,0,0,40);
            assert(keyboard.model.pending&&strchr("12!?_",(int)keyboard.model.pending));
        }
        assert(keyboard.model.upper==(bool)upper);
        hello_keyboard_close(&keyboard);
    }
    for(int symbol=0;symbol<2;symbol++) {
        assert(hello_keyboard_open_field(&keyboard,"12345",HELLO_INPUT_CONTACT_NUMBER));
        keyboard.model.upper=true;
        hello_keyboard_update(&keyboard,671000,kButtonUp,kButtonUp,0,0);
        if(symbol)hello_keyboard_model_crank(&keyboard.model,3,671010);
        else hello_keyboard_update(&keyboard,671010,kButtonUp|kButtonA,kButtonA,0,0);
        assert(keyboard.model.pending==(uint32_t)(symbol?'!':'1'));
        hello_keyboard_update(&keyboard,671020,0,0,kButtonUp|kButtonA,0);
        assert(!strcmp(keyboard.model.text,symbol?"12345":"123451"));
        if(symbol)assert(!strcmp(keyboard.error_storage,"Only digits are allowed"));
        assert(keyboard.model.upper&&!keyboard.model.shifted_up_enabled);
        hello_keyboard_close(&keyboard);
    }
}

static void test_editing_popup_choices_have_no_hold_progress(const HelloKeyboard* reference) {
    for(int save=0;save<2;save++) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        assert(hello_keyboard_open(&keyboard,"original"));strcpy(keyboard.model.text,"draft");keyboard.model.cursor=5;
        hello_keyboard_update(&keyboard,630000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,630350,kButtonA|kButtonB,0,0,0);
        const unsigned samples[]={0,174,349,10000};
        for(unsigned sample=0;sample<sizeof(samples)/sizeof(samples[0]);sample++) {
            const uint32_t now=630350+samples[sample];
            assert(hello_keyboard_update(&keyboard,now,kButtonA|kButtonB,0,0,0)==HELLO_KEYBOARD_NONE);
            assert(keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready&&!strcmp(keyboard.model.text,"draft"));
            capture_cancel_popup=capture_footer_canvas=true;cancel_popup_frame_count=footer_white_fills=footer_circles=0;
            draw_cancel_prompt(&keyboard,now);
            assert(footer_circles==2&&!footer_white_fills);
            capture_cancel_popup=capture_footer_canvas=false;
        }
        hello_keyboard_update(&keyboard,640351,0,0,kButtonA|kButtonB,0);
        assert(keyboard.cancel_dialog_ready);
        const PDButtons choice=save?kButtonA:kButtonB;
        assert(hello_keyboard_update(&keyboard,640360,choice,choice,0,0)==(save?HELLO_KEYBOARD_DONE:HELLO_KEYBOARD_CANCELLED));
        assert(!keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text,save?"draft":"original"));
        // Confirmation owns its release and continued hold; neither reaches
        // ordinary keyboard input or emits the result a second time.
        assert(hello_keyboard_update(&keyboard,640361,choice,0,0,0)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,650361,choice,0,0,0)==HELLO_KEYBOARD_NONE);
        assert(hello_keyboard_update(&keyboard,650362,0,0,choice,0)==HELLO_KEYBOARD_NONE);
        assert(!strcmp(keyboard.model.text,save?"draft":"original")&&!keyboard.model.upper);
        hello_keyboard_close(&keyboard);
    }
}

static void test_idle_escape_label_tracks_physical_buttons(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    const PDButtons buttons[]={0,kButtonA,kButtonB,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonA|kButtonB};
    assert(!strcmp(keyboard.config.labels.escape_text,"– Escape –"));
    for(unsigned button=0;button<sizeof(buttons)/sizeof(buttons[0]);button++) {
        assert(hello_keyboard_open(&keyboard,"abcdef"));
        hello_keyboard_update(&keyboard,600000,buttons[button],buttons[button],0,0);
        assert(keyboard.footer_buttons==buttons[button]);
        expected_selected_item=keyboard.config.labels.escape_text;expected_item_draws=0;draw_footer(&keyboard,600000);
        assert(expected_item_draws==(buttons[button]?0:1));
        if(!buttons[button])assert(expected_item_x==(400-keyboard_text_width(&keyboard,expected_selected_item))/2);
        hello_keyboard_update(&keyboard,600016,0,0,buttons[button],0);
        expected_item_draws=0;draw_footer(&keyboard,600016);assert(expected_item_draws==1);
        expected_selected_item=NULL;hello_keyboard_close(&keyboard);
    }
    assert(hello_keyboard_open(&keyboard,"abcdef"));
    hello_keyboard_update(&keyboard,601000,kButtonA,kButtonA,0,0);hello_keyboard_update(&keyboard,601350,kButtonA,0,0,0);
    expected_selected_item=keyboard.config.labels.escape_text;expected_item_draws=0;draw_footer(&keyboard,601350);assert(!expected_item_draws);expected_selected_item=NULL;hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open(&keyboard,"abcdef"));hello_keyboard_update(&keyboard,602000,kButtonA|kButtonB,kButtonA|kButtonB,0,0);
    hello_keyboard_update(&keyboard,602350,kButtonA|kButtonB,0,0,0);hello_keyboard_update(&keyboard,602351,0,0,kButtonA|kButtonB,0);
    expected_selected_item=keyboard.config.labels.escape_text;expected_item_draws=0;capture_cancel_popup=true;cancel_popup_frame_count=0;
    draw_cancel_prompt(&keyboard,602351);assert(!expected_item_draws);capture_cancel_popup=false;expected_selected_item=NULL;hello_keyboard_close(&keyboard);
    assert(hello_keyboard_open(&keyboard,"abcdef"));keyboard.config.font=NULL;keyboard.config.unicode_font=NULL;
    capture_bitmap_text=capture_escape_dash=true;drawn_glyphs=escape_dash_lines=0;draw_footer(&keyboard,603000);
    assert(escape_dash_lines==2);for(int glyph=0;glyph<drawn_glyphs;glyph++)assert(glyph_frames[glyph]!=63-32);
    capture_bitmap_text=capture_escape_dash=false;hello_keyboard_close(&keyboard);
}
static void test_physical_return_editing_decision_all_types(const HelloKeyboard* reference) {
    const char* family="👨‍👩‍👧‍👦";
    for(int type=0;type<3;type++)for(int opener=0;opener<2;opener++)for(int save=0;save<2;save++) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
        config.field=(HelloKeyboardFieldPolicy){.type=(HelloInputType)type,.min_length=type==HELLO_INPUT_CONTACT_NUMBER?6:1,.max_length=type==HELLO_INPUT_CONTACT_NUMBER?6:160,.trim_for_length=true};
        config.groups[2]=opener?"a⎵":"⎵a";
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        const char* original=type==HELLO_INPUT_CONTACT_NUMBER?"111111":"original";
        char raw[64];snprintf(raw,sizeof(raw)," %s ",type==HELLO_INPUT_CONTACT_NUMBER?"654321":family);
        assert(hello_keyboard_open_configured(&keyboard,original,NULL));strcpy(keyboard.model.text,raw);
        keyboard.model.upper=true;keyboard.model.cursor=1;
        if(type==HELLO_INPUT_MASKEDTEXT)assert(hello_input_field_toggle_mask(&keyboard.input_field));
        const PDButtons face=opener?kButtonB:kButtonA;
        hello_keyboard_update(&keyboard,610000,kButtonDown,kButtonDown,0,0);hello_keyboard_update(&keyboard,610016,kButtonDown|face,face,0,0);
        assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);
        assert(hello_keyboard_update(&keyboard,610024,face,0,kButtonDown,0)==HELLO_KEYBOARD_NONE);
        assert(keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready&&!strcmp(keyboard.model.text,raw));
        assert(hello_keyboard_update(&keyboard,611000,face,0,0,0)==HELLO_KEYBOARD_NONE);
        capture_cancel_popup=true;cancel_popup_frame_count=0;
        expected_selected_item="Done typing? What's next?";expected_item_draws=0;draw_cancel_prompt(&keyboard,611001);
        assert(expected_item_draws==1);capture_cancel_popup=false;expected_selected_item=NULL;
        const HelloKeyboardResult result=tap_editing_choice(&keyboard,612000,save?kButtonA:kButtonB);
        assert(result==(save?HELLO_KEYBOARD_DONE:HELLO_KEYBOARD_CANCELLED));
        const char* expected=save?(type==HELLO_INPUT_CONTACT_NUMBER?"654321":family):original;
        assert(!strcmp(keyboard.model.text,expected)&&!keyboard.model.submit_requested&&!keyboard.model.pending);
        assert(keyboard.visible==(bool)save&&!keyboard.cancel_dialog_active);
        assert(hello_keyboard_update(&keyboard,612353,save?kButtonA:kButtonB,0,0,0)==HELLO_KEYBOARD_NONE&&!strcmp(keyboard.model.text,expected));hello_keyboard_close(&keyboard);
    }
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.field=(HelloKeyboardFieldPolicy){.type=HELLO_INPUT_CONTACT_NUMBER,.min_length=6,.max_length=6,.trim_for_length=true};
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open_configured(&keyboard," 12345 ",NULL));keyboard.model.upper=true;keyboard.model.cursor=2;
    hello_keyboard_update(&keyboard,620000,kButtonDown,kButtonDown,0,0);hello_keyboard_update(&keyboard,620016,kButtonDown|kButtonB,kButtonB,0,0);
    assert(hello_keyboard_update(&keyboard,620024,0,0,kButtonDown|kButtonB,0)==HELLO_KEYBOARD_NONE);
    assert(keyboard.cancel_dialog_active&&!keyboard.error_storage[0]&&!strcmp(keyboard.model.text," 12345 "));
    assert(save_editing_popup(&keyboard,620025)==HELLO_KEYBOARD_NONE);
    assert(!keyboard.cancel_dialog_active&&keyboard.visible&&keyboard.error_storage[0]&&!strcmp(keyboard.model.text," 12345 ")&&keyboard.model.cursor==2&&!keyboard.model.submit_requested);hello_keyboard_close(&keyboard);
    hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
    loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,"typed"));keyboard.model.upper=true;
    hello_keyboard_update(&keyboard,621000,kButtonDown,kButtonDown,0,0);hello_keyboard_update(&keyboard,621016,kButtonDown,0,0,-0.02f);
    assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);
    assert(hello_keyboard_update(&keyboard,621024,0,0,kButtonDown,0)==HELLO_KEYBOARD_NONE&&keyboard.cancel_dialog_active&&keyboard.cancel_dialog_ready);
    assert(save_editing_popup(&keyboard,621025)==HELLO_KEYBOARD_DONE&&!strcmp(keyboard.model.text,"typed"));hello_keyboard_close(&keyboard);
}

static void test_editing_save_pending_transaction_and_rollback(const HelloKeyboard* reference) {
    for(int valid=0;valid<2;valid++)for(int masked=0;masked<2;masked++) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);
        config.field=(HelloKeyboardFieldPolicy){.type=masked?HELLO_INPUT_MASKEDTEXT:HELLO_INPUT_PLAINTEXT,
            .min_length=valid?2:3,.max_length=5,.trim_for_length=true};
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        assert(hello_keyboard_open_configured(&keyboard," a ",NULL));keyboard.model.cursor=2;
        if(masked)assert(hello_input_field_toggle_mask(&keyboard.input_field));
        hello_keyboard_update(&keyboard,622000,kButtonRight,kButtonRight,0,0);
        hello_keyboard_update(&keyboard,622010,kButtonRight|kButtonA,kButtonA,0,0);
        hello_keyboard_update(&keyboard,622018,kButtonRight,0,kButtonA,0);assert(keyboard.model.pending=='n');
        hello_keyboard_update(&keyboard,622020,kButtonRight|kButtonA|kButtonB,kButtonA|kButtonB,0,0);
        hello_keyboard_update(&keyboard,622370,kButtonRight|kButtonA|kButtonB,0,0,0);
        assert(keyboard.cancel_dialog_active&&!strcmp(keyboard.model.text," a ")&&keyboard.model.cursor==2);
        assert(save_editing_popup(&keyboard,622371)==(valid?HELLO_KEYBOARD_DONE:HELLO_KEYBOARD_NONE));
        assert(keyboard.visible&&!keyboard.cancel_dialog_active&&!keyboard.model.pending&&!keyboard.model.group);
        assert(!strcmp(keyboard.model.text,valid?"an":" a ")&&keyboard.model.cursor==2);
        assert(hello_input_field_is_masked(&keyboard.input_field)==(bool)masked);
        assert(valid?!keyboard.error_storage[0]:keyboard.error_storage[0]!=0);
        hello_keyboard_update(&keyboard,622724,kButtonA,0,0,0);
        assert(!strcmp(keyboard.model.text,valid?"an":" a ")&&!keyboard.model.upper);
        hello_keyboard_close(&keyboard);
    }
}

static void test_external_feedback_title_timing(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);
    config.font=(LCDFont*)&bitmap;config.font_size=14;config.title="Compose";
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    char message[]="Specific feedback";
    const HelloKeyboardFeedbackConfig feedback={.duration_ms=120,.resolver=art_feedback_resolver,.userdata=message};
    const HelloKeyboardOpenOptions options={.feedback=&feedback};
    assert(hello_keyboard_open_field_with_options(&keyboard,"",HELLO_INPUT_PLAINTEXT,&options));
    hello_keyboard_show_feedback(&keyboard,"E_VIEW","Fallback",200000);
    message[0]='x';assert(!strcmp(keyboard.error_storage,"Specific feedback"));
    draw_title(&keyboard,200000);assert(!strcmp(rendered,"Specific feedback"));
    draw_title(&keyboard,200119);assert(!strcmp(rendered,"Specific feedback"));
    draw_title(&keyboard,200120);assert(!strcmp(rendered,"Compose"));
    hello_keyboard_show_feedback(&keyboard,"E_UNKNOWN","Fallback",200140);
    draw_title(&keyboard,200140);assert(!strcmp(rendered,"Fallback"));
    hello_keyboard_clear_error(&keyboard);draw_title(&keyboard,200141);assert(!strcmp(rendered,"Compose"));
    hello_keyboard_show_feedback(&keyboard,"E_UNKNOWN","Fallback",200160);
    hello_keyboard_update(&keyboard,200161,kButtonUp,kButtonUp,0,0);
    assert(!keyboard.error_storage[0]);draw_title(&keyboard,200161);assert(!strcmp(rendered,"Compose"));
    hello_keyboard_close(&keyboard);
}

static void test_crank_wrap_scroll_gate(const HelloKeyboard* reference) {
    const char* const custom[HELLO_KEYBOARD_GROUP_COUNT]={
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?",NULL,NULL,NULL,NULL};
    const int groups[]={1,HELLO_KEYBOARD_EMOJI_GROUP,1,2};
    for(unsigned variant=0;variant<4;variant++)
    for(int direction=-1;direction<=1;direction+=2)
    for(uint32_t frame_ms=20;frame_ms<=40;frame_ms+=20)
    for(int oversized=0;oversized<2;oversized++) {
        HelloKeyboard keyboard;
        crank_test_prepare(&keyboard,reference,groups[variant],direction,variant<2?NULL:custom,20000);
        keyboard.config.font_size=15;
        const int group=keyboard.model.group,count=hello_keyboard_model_group_length(&keyboard.model,group);
        assert(count==(variant==0?40:variant==1?21:64));
        // Prime the actual selected endpoint at this font and group geometry.
        for(uint32_t t=20020;t<=20600;t+=20)draw_group(&keyboard,group,105,69,t);
        const uint32_t begin=20620;
        const float change=(float)direction*(oversized?10000:crank_test_threshold(&keyboard,direction)+0.001f);
        hello_keyboard_update(&keyboard,begin,0,0,0,change);
        const int endpoint=direction>0?0:count-1;
        assert(keyboard.model.index==endpoint&&keyboard.model.pending);
        assert(keyboard.selection_lock_group==group&&keyboard.selection_lock_index==endpoint&&keyboard.selection_lock_crank);
        const uint32_t pending=keyboard.model.pending;
        char pending_text[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];strcpy(pending_text,keyboard.model.pending_text);
        const float remainder=keyboard.crank_remainder;
        const int steps=keyboard.crank_release_steps,release=keyboard.crank_release_direction;
        assert(remainder==0); // The rest of a large SDK delta cannot cross the new endpoint.
        bool saw_travel=false;uint32_t arrived=UINT32_MAX,first_visible=UINT32_MAX;
        capture_group_view=true;
        for(uint32_t elapsed=0;elapsed<=1240;elapsed+=frame_ms) {
            const float dropped=elapsed%80==0?1000:elapsed%80==20?-1000:elapsed%80==40?0:10000;
            hello_keyboard_update(&keyboard,begin+elapsed,0,0,0,dropped);
            assert(keyboard.model.index==endpoint&&keyboard.model.pending==pending);
            assert(!strcmp(keyboard.model.pending_text,pending_text)&&!keyboard.model.text[0]);
            assert(keyboard.crank_remainder==remainder&&keyboard.crank_release_steps==steps&&
                keyboard.crank_release_direction==release);
            group_draw_count=marker_draws=0;draw_group(&keyboard,group,105,69,begin+elapsed);
            if(marker_draws&&first_visible==UINT32_MAX)first_visible=elapsed;
            if(keyboard.group_scroll_motion[group].remaining_ms) {
                saw_travel=true;assert(keyboard.selection_lock_group==group);
            }
            if(!keyboard.selection_lock_group) {
                arrived=elapsed;
                assert(keyboard.group_scroll[group].position==keyboard.group_scroll_motion[group].target);
                assert(marker_draws==1&&keyboard.selection_marker.active&&keyboard.selection_marker.tag==group);
                assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
                assert(fabsf((keyboard.selection_marker.left+keyboard.selection_marker.right)/2-
                    keyboard.selection_marker.target)<1);
                break;
            }
        }
        capture_group_view=false;
        assert(saw_travel&&arrived<=1250&&first_visible!=UINT32_MAX&&arrived-first_visible<=650);
        // Neither the discarded rotation nor the large initial delta is replayed.
        for(uint32_t elapsed=20;elapsed<=40;elapsed+=20) {
            hello_keyboard_update(&keyboard,begin+arrived+elapsed,0,0,0,0);
            draw_group(&keyboard,group,105,69,begin+arrived+elapsed);
            assert(keyboard.model.index==endpoint&&keyboard.crank_remainder==0);
        }
        uint32_t resumed=begin+arrived+60;
        const float threshold=crank_test_threshold(&keyboard,direction);
        resumed=crank_render_travel(&keyboard,direction,threshold-0.05f,resumed);
        assert(keyboard.model.index==endpoint);
        crank_render_travel(&keyboard,direction,0.051f,resumed);
        assert(keyboard.model.index==endpoint+direction&&!keyboard.selection_lock_crank);
    }
    BrbElasticMarker marker={.left=120,.right=126,.target=124,
        .left_velocity=40,.right_velocity=-20};
    rebase_selection_marker(&marker,24,20,160);
    assert(marker.right-marker.left==6&&marker.target==24);
    assert(marker.left_velocity==40&&marker.right_velocity==-20);
}

static void test_wrap_gate_lifecycle(const HelloKeyboard* reference) {
    for(int action=0;action<7;action++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,-1,NULL,30000);
        hello_keyboard_update(&keyboard,30020,0,0,0,-10000);
        assert(keyboard.selection_lock_group==1);
        if(action==0)hello_keyboard_update(&keyboard,30040,kButtonDown,kButtonDown,0,0);
        else if(action==1||action==2) {
            const PDButtons face=action==1?kButtonA:kButtonB;
            const int index=keyboard.model.index,steps=keyboard.crank_release_steps,
                direction=keyboard.crank_release_direction;
            const float remainder=keyboard.crank_remainder;
            hello_keyboard_update(&keyboard,30040,face,face,0,0);
            hello_keyboard_update(&keyboard,30060,0,0,face,0);
            assert(keyboard.model.index==index&&keyboard.selection_lock_group==1);
            assert(keyboard.crank_release_steps==steps&&keyboard.crank_release_direction==direction&&
                keyboard.crank_remainder==remainder);
            crank_test_finish_wrap(&keyboard,30060);
        } else if(action==3)clear_callback(&keyboard);
        else if(action==4)hello_keyboard_close(&keyboard);
        else {
            keyboard.requested_action=action==5?HELLO_KEYBOARD_DONE:HELLO_KEYBOARD_CANCELLED;
            assert(hello_keyboard_update(&keyboard,30040,0,0,0,0)==
                (action==5?HELLO_KEYBOARD_NONE:HELLO_KEYBOARD_CANCELLED));
            if(action==5)assert(save_editing_popup(&keyboard,30041)==HELLO_KEYBOARD_DONE);
        }
        assert(!keyboard.selection_lock_group&&!keyboard.selection_lock_crank);
        hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,""));
        assert(!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons);
        hello_keyboard_update(&keyboard,31000,kButtonUp,kButtonUp,0,0);
        hello_keyboard_update(&keyboard,31020,kButtonUp,0,0,0);
        hello_keyboard_update(&keyboard,31040,kButtonUp|kButtonA,kButtonA,0,0);
        assert(keyboard.model.pending&&keyboard.model.index==0);
    }
}

static void test_matching_release_retires_gate_and_empty_group(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2)
    for(int crank=0;crank<2;crank++)
    for(int pending=0;pending<2;pending++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,
            crank?NULL:kOverflowGroups,190000);
        const PDButtons dpad=directions[group],face=direction>0?kButtonA:kButtonB,
            opposite=direction>0?kButtonB:kButtonA;
        hello_keyboard_update(&keyboard,190020,dpad|(crank?0:face),crank?0:face,0,
            crank?(float)direction*(crank_test_threshold(&keyboard,direction)+0.001f):0);
        assert(keyboard.selection_lock_group==group&&keyboard.model.pending);
        hello_keyboard_update(&keyboard,190028,dpad,0,crank?0:face,0);
        hello_keyboard_update(&keyboard,190036,dpad|opposite,opposite,0,10000);
        assert(keyboard.rejected_face_buttons&opposite);
        char selected[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];
        hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        if(!strcmp(selected,"⎵"))strcpy(selected," ");
        // A caller may already have accepted the item while retaining the held
        // group. Its subsequent empty release must insert nothing else.
        if(!pending)hello_keyboard_model_commit(&keyboard.model,true);
        hello_keyboard_update(&keyboard,190044,opposite,0,dpad,0);
        assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,selected));
        assert(!keyboard.selection_lock_group&&!keyboard.selection_lock_crank&&!keyboard.selection_marker.active);
        assert(!keyboard.crank_remainder&&!keyboard.crank_release_steps&&!keyboard.crank_release_direction);
        for(int button=0;button<2;button++)assert(!keyboard.model.hold[button].active&&!keyboard.model.feedback_active[button]);
        // A still-held old face button cannot select on the next D-pad press.
        hello_keyboard_update(&keyboard,190052,dpad|opposite,dpad,0,0);
        assert(keyboard.model.group==group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,selected));
        hello_keyboard_update(&keyboard,190060,opposite,0,dpad,0);
        assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,selected));
        hello_keyboard_update(&keyboard,190068,0,0,opposite,0);
        assert(!keyboard.rejected_face_buttons);
        hello_keyboard_update(&keyboard,190076,dpad,dpad,0,0);
        const int expected_index=opposite==kButtonB?group_last_visible_item(&keyboard):0;
        hello_keyboard_update(&keyboard,190084,dpad|opposite,opposite,0,0);
        assert(keyboard.model.pending&&keyboard.model.index==expected_index);
        const bool entry_wait=group_has_overflow(&keyboard,group)&&!group_selection_is_visible(&keyboard);
        assert(keyboard.selection_lock_group==(entry_wait?group:0)&&!keyboard.selection_lock_crank&&!keyboard.rejected_face_buttons);
        assert(keyboard.model.hold[opposite==kButtonB?HELLO_KB_BUTTON_B:HELLO_KB_BUTTON_A].at==190084);
        assert(!strcmp(keyboard.model.text,selected));
    }
}

static void test_wrap_gate_waits_for_render(const HelloKeyboard* reference) {
    HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,-1,NULL,35000);
    hello_keyboard_update(&keyboard,35020,0,0,0,-10000);
    const int endpoint=keyboard.model.index;
    // Update activity and elapsed wall time cannot impersonate a rendered arrival.
    hello_keyboard_update(&keyboard,36020,0,0,0,10000);
    assert(keyboard.selection_lock_crank&&keyboard.model.pending&&keyboard.model.index==endpoint);
    assert(!keyboard.model.text[0]&&!keyboard.crank_remainder);
    crank_test_finish_wrap(&keyboard,36020);
    assert(!keyboard.selection_lock_group&&keyboard.model.pending&&keyboard.model.index==endpoint);
}

static void test_boundary_unlock_has_no_size_timer(const HelloKeyboard* reference) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,NULL,39000);
        keyboard.model.pending_side=2;
        for(uint32_t now=39000;now<=39600;now+=20)draw_group(&keyboard,group,105,69,now);
        // A rendered endpoint and resting marker already satisfy the gate.
        // Starting a fresh marker cannot impose a separate animation timeout.
        keyboard.selection_marker.active=false;lock_boundary_selection(&keyboard);
        // This fixture starts after the one-time seam-entry animation. Only
        // actual scroll and marker rest govern the remaining boundary lock.
        keyboard.crank_marker_rebase_group=keyboard.crank_marker_pinned_group=0;
        assert(keyboard.selection_lock_group==group);
        draw_group(&keyboard,group,105,69,39600);
        assert(!keyboard.selection_lock_group&&!keyboard.selection_lock_crank);
    }
}

static void ordinary_face_tap(HelloKeyboard* keyboard,PDButtons face,uint32_t now,int expected_index) {
    hello_keyboard_update(keyboard,now,face,face,0,0);
    const int button=face==kButtonA?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B;
    assert(keyboard->model.pending&&keyboard->model.index==expected_index&&keyboard->model.pending_at==now);
    assert(keyboard->model.hold[button].active&&keyboard->model.hold[button].at==now&&keyboard->model.hold[button].hold==HELLO_KB_HOLD_REPEAT);
    assert(!keyboard->crank_remainder&&!keyboard->crank_release_steps&&!keyboard->crank_release_direction&&!keyboard->crank_wrapped);
    assert(!keyboard->selection_lock_group&&!keyboard->selection_lock_crank&&!keyboard->rejected_face_buttons);
    draw_group(keyboard,keyboard->model.group,105,69,now);
    hello_keyboard_update(keyboard,now+8,0,0,face,0);
    assert(!keyboard->model.hold[button].active&&keyboard->model.index==expected_index&&keyboard->model.pending_at==now);
    draw_group(keyboard,keyboard->model.group,105,69,now+8);
}

static uint64_t face_trace_word(uint64_t hash,uint32_t value) {
    return (hash^(uint64_t)value)*UINT64_C(1099511628211);
}
static uint64_t face_trace_frame(const HelloKeyboard* keyboard,uint64_t hash) {
    const HelloKeyboardModel* model=&keyboard->model;
    hash=face_trace_word(hash,(uint32_t)model->index);
    hash=face_trace_word(hash,model->pending);
    hash=face_trace_word(hash,model->pending_at);
    hash=face_trace_word(hash,(uint32_t)model->pending_side);
    hash=face_trace_word(hash,model->group_at);
    for(int button=0;button<2;button++) {
        const int physical=1-button;
        const HelloKeyboardHold* hold=&model->hold[physical];
        hash=face_trace_word(hash,hold->active);hash=face_trace_word(hash,hold->at);
        hash=face_trace_word(hash,(uint32_t)hold->group);hash=face_trace_word(hash,(uint32_t)hold->hold);
        hash=face_trace_word(hash,hold->handled);hash=face_trace_word(hash,hold->next_delete_at);
        hash=face_trace_word(hash,model->feedback_active[physical]);
        hash=face_trace_word(hash,model->feedback_at[physical]);
    }
    return hash;
}
static void test_build68_face_input_trace(const HelloKeyboard* reference) {
    // Logical traces generated independently from archived Build 68. The
    // Historical Up content plus first/last-role A/B normalization keeps clocks exact;
    // the newly requested jelly rendering is deliberately outside this hash.
    static const uint64_t expected[6][2]={
        {UINT64_C(16285204183000523929),UINT64_C(15990123908061918300)},
        {UINT64_C(14204962932527092749),UINT64_C(12682718998130093688)},
        {UINT64_C(13359525030654655171),UINT64_C(18262759985761213627)},
        {UINT64_C(14318373613672910186),UINT64_C(5256575238472191391)},
        {UINT64_C(934798623867906944),UINT64_C(14119122416620502317)},
        {UINT64_C(15515701749955187505),UINT64_C(5825412639740207687)}};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int last=0;last<2;last++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,last?1:-1,kBuild68Groups,100000);
        keyboard.config.font_size=10;keyboard.model.pending=0;keyboard.model.pending_text[0]=0;
        keyboard.selection_marker.active=false;
        const PDButtons face=last?kButtonB:kButtonA,opposite=last?kButtonA:kButtonB;
        const PDButtons taps[]={face,face,face,opposite,opposite};
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),direction=last?-1:1;
        const int offsets[]={0,1,2,1,0};
        uint64_t hash=UINT64_C(14695981039346656037);
        uint32_t now=100020;
        for(int tap=0;tap<5;tap++,now+=16) {
            ordinary_face_tap(&keyboard,taps[tap],now,(last?count-1:0)+direction*offsets[tap]);
            hash=face_trace_frame(&keyboard,hash);
            draw_group(&keyboard,group,105,69,now+8);
            hash=face_trace_frame(&keyboard,hash);
        }
        for(int frame=0;frame<35;frame++,now+=16) {
            hello_keyboard_update(&keyboard,now,0,0,0,0);draw_group(&keyboard,group,105,69,now);
            assert(!keyboard.selection_lock_group&&!keyboard.model.text[0]);
            hash=face_trace_frame(&keyboard,hash);
        }
        assert(hash==expected[group-1][last]);
    }
}

static void check_current_item_render(HelloKeyboard* keyboard,const char* item,uint32_t now) {
    capture_group_view=true;expected_selected_item=item;expected_item_draws=group_draw_count=marker_draws=0;
    draw_group(keyboard,keyboard->model.group,105,69,now);
    assert(expected_item_draws==1&&marker_draws==1);
    assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
    assert(marker_y==group_clip_y&&marker_h==group_clip_h);
    assert(expected_item_x>=marker_x&&expected_item_x+keyboard_text_width(keyboard,item)<=marker_x+marker_w);
    expected_selected_item=NULL;capture_group_view=false;
}

static uint32_t scaled_button_wrap_duration(const HelloKeyboard* reference,float scale) {
    HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,-1,kOverflowGroups,400000);
    assert(hello_keyboard_set_button_navigation_scale(&keyboard,scale));
    hello_keyboard_update(&keyboard,400020,kButtonB,kButtonB,0,0);
    assert(keyboard.selection_lock_group==1&&keyboard.selection_lock_crank&&keyboard.model.index==63);
    hello_keyboard_update(&keyboard,400028,0,0,kButtonB,0);
    return crank_test_finish_wrap(&keyboard,400028)-400020;
}
static void test_button_scale_rendering_and_unmodified_crank(const HelloKeyboard* reference) {
    const uint32_t ordinary=scaled_button_wrap_duration(reference,1.0f),faster=scaled_button_wrap_duration(reference,.5f);
    assert(faster<ordinary&&faster*10<=ordinary*7&&faster<=650);
    HelloKeyboard normal,scaled;
    crank_test_prepare(&normal,reference,1,-1,kOverflowGroups,410000);scaled=normal;
    assert(normal.model.button_navigation_scale==1.0f&&hello_keyboard_set_button_navigation_scale(&scaled,.5f));
    assert(!hello_keyboard_set_button_navigation_scale(&scaled,0)&&scaled.config.button_navigation_scale==.5f&&scaled.model.button_navigation_scale==.5f);
    const float travel=crank_test_threshold(&normal,-1)+0.001f;
    assert(crank_test_threshold(&scaled,-1)==crank_test_threshold(&normal,-1));
    hello_keyboard_update(&normal,410020,0,0,0,-travel);hello_keyboard_update(&scaled,410020,0,0,0,-travel);
    assert(normal.selection_lock_group==1&&scaled.selection_lock_group==1);
    for(uint32_t elapsed=0;elapsed<=1240;elapsed+=20) {
        const uint32_t now=410020+elapsed;
        hello_keyboard_update(&normal,now,0,0,0,0);hello_keyboard_update(&scaled,now,0,0,0,0);
        draw_group(&normal,1,105,69,now);draw_group(&scaled,1,105,69,now);
        assert(normal.model.index==scaled.model.index&&normal.model.pending==scaled.model.pending);
        assert(normal.group_scroll[1].position==scaled.group_scroll[1].position&&normal.group_scroll_motion[1].remaining_ms==scaled.group_scroll_motion[1].remaining_ms);
        assert(normal.selection_lock_group==scaled.selection_lock_group&&normal.crank_remainder==scaled.crank_remainder);
        assert(normal.selection_marker.left==scaled.selection_marker.left&&normal.selection_marker.right==scaled.selection_marker.right);
        assert(normal.selection_marker.left_velocity==scaled.selection_marker.left_velocity&&normal.selection_marker.right_velocity==scaled.selection_marker.right_velocity);
    }
    assert(!normal.selection_lock_group&&!scaled.selection_lock_group);
}

static void test_accepted_button_hold_resumes_after_rendered_wrap(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)for(int button=0;button<2;button++)for(int half=0;half<2;half++)for(int overlap=0;overlap<3;overlap++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,button==HELLO_KB_BUTTON_A?1:-1,kOverflowGroups,440000);
        assert(hello_keyboard_set_button_navigation_scale(&keyboard,half?.5f:1.0f));

        const int count=hello_keyboard_model_group_length(&keyboard.model,group),direction=button==HELLO_KB_BUTTON_A?1:-1;
        select_group_item(&keyboard,group,direction>0?count-2:1);keyboard.selection_marker.active=false;
        for(uint32_t now=440000;now<=440600;now+=20)draw_group(&keyboard,group,105,69,now);
        const PDButtons dpad=directions[group],face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;
        const uint32_t begin=440620,hold=hello_keyboard_model_selection_hold_ms(&keyboard.model),repeat=hello_keyboard_model_selection_repeat_ms(&keyboard.model);
        hello_keyboard_update(&keyboard,begin,dpad|face,face,0,0);
        const int edge=direction>0?count-1:0,endpoint=direction>0?0:count-1;
        assert(keyboard.model.index==edge&&!keyboard.selection_lock_group&&keyboard.model.hold[button].active);
        draw_group(&keyboard,group,105,69,begin);
        hello_keyboard_update(&keyboard,begin+hold-1,dpad|face,0,0,0);assert(keyboard.model.index==edge);
        hello_keyboard_update(&keyboard,begin+hold,dpad|face,0,0,0);
        assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==group&&keyboard.selection_lock_crank);
        assert(keyboard.model.hold[button].active&&keyboard.model.hold[button].hold==HELLO_KB_HOLD_REPEAT);
        const uint32_t wrapped=begin+hold;uint32_t settled=0;
        for(uint32_t elapsed=0;elapsed<=1240;elapsed+=20) {
            const uint32_t now=wrapped+elapsed;
            const PDButtons opposite=button==HELLO_KB_BUTTON_A?kButtonB:kButtonA;
            const bool down=overlap&&elapsed==20;
            const bool coalesced=down&&overlap==2;
            hello_keyboard_update(&keyboard,now,dpad|face|(down&&!coalesced?opposite:0),down?opposite:0,coalesced?opposite:0,0);
            if(down) {
                // The formerly paused overlap now opens immediately, including
                // a complete opposite tap reported while the accepted face is held.
                assert(keyboard.cancel_dialog_active&&!keyboard.cancel_dialog_ready);
                assert(keyboard.model.index==endpoint&&!keyboard.selection_lock_group);
                assert(!keyboard.model.hold[button].active&&!keyboard.model.hold[1-button].active);
                assert(!keyboard.model.text[0]&&keyboard.model.pending);
                break;
            }
            assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==group);
            draw_group(&keyboard,group,105,69,now);
            assert(keyboard.model.hold[button].active&&!(keyboard.rejected_face_buttons&face));
            if(!keyboard.selection_lock_group){settled=now;break;}
        }
        if(overlap){hello_keyboard_close(&keyboard);continue;}
        assert(settled&&settled-wrapped<=1250);
        uint32_t resumed=0;
        for(uint32_t elapsed=20;elapsed<=repeat+40;elapsed+=20) {
            hello_keyboard_update(&keyboard,settled+elapsed,dpad|face,0,0,0);
            if(keyboard.model.index!=endpoint) {
                assert(keyboard.model.index==endpoint+direction&&!keyboard.selection_lock_group);
                resumed=settled+elapsed;break;
            }
        }
        assert(resumed&&resumed-settled<=repeat+20); // No new press is required.
        char selected[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        hello_keyboard_update(&keyboard,resumed+1,dpad,0,face,0);
        const int stopped=keyboard.model.index;
        hello_keyboard_update(&keyboard,resumed+30000,dpad,0,0,0);
        assert(keyboard.model.index==stopped&&keyboard.model.pending&&!keyboard.model.text[0]);
        hello_keyboard_update(&keyboard,resumed+30001,0,0,dpad,0);
        assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,selected));
    }
}

static void test_fresh_blocked_hold_resumes_after_actual_boundary(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=6;group++)for(int button=0;button<2;button++)for(int late=0;late<2;late++)for(int short_tap=0;short_tap<2;short_tap++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        const int direction=button==HELLO_KB_BUTTON_A?1:-1;
        const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB,dpad=directions[group];
        HelloKeyboard initial;crank_test_prepare(&initial,reference,group,direction,kOverflowGroups,480000);
        assert(hello_keyboard_set_button_navigation_scale(&initial,.5f));
        hello_keyboard_update(&initial,480020,dpad|face,face,0,0);
        hello_keyboard_update(&initial,480028,dpad,0,face,0);
        assert(initial.selection_lock_group==group);
        const int endpoint=initial.model.index;
        HelloKeyboard probe=initial;uint32_t settled=0;
        for(uint32_t now=480040;now<=481280;now+=20) {
            hello_keyboard_update(&probe,now,dpad,0,0,0);draw_group(&probe,group,105,69,now);
            if(!probe.selection_lock_group){settled=now;break;}
        }
        assert(settled&&settled>480060);
        const uint32_t pressed=late?settled-20:480040;
        HelloKeyboard keyboard=initial;
        for(uint32_t now=480040;now<=settled;now+=20) {
            const bool down=now==pressed,up=short_tap&&now==pressed+20;
            const bool held=now>=pressed&&(!short_tap||now<pressed+20);
            // A coalesced up/down snapshot still describes a new continuous hold.
            hello_keyboard_update(&keyboard,now,dpad|(held?face:0),down?face:0,(up||(!late&&down))?face:0,0);
            assert(keyboard.model.index==endpoint&&!keyboard.model.text[0]);
            draw_group(&keyboard,group,105,69,now);
            if(now<settled)assert(keyboard.selection_lock_group==group);
        }
        assert(!keyboard.selection_lock_group);
        if(short_tap) {
            hello_keyboard_update(&keyboard,settled+30000,dpad,0,0,0);
            assert(keyboard.model.index==endpoint&&!keyboard.model.hold[button].active&&!keyboard.model.text[0]);
            continue;
        }
        uint32_t resumed=0;
        for(uint32_t now=settled+1;now<=settled+196;now++) {
            const int before=keyboard.model.index;
            hello_keyboard_update(&keyboard,now,dpad|face,0,0,0);
            assert(keyboard.model.index==before||keyboard.model.index==before+direction);
            if(keyboard.model.index!=endpoint){resumed=now;break;}
        }
        assert(resumed&&resumed>=pressed+175&&keyboard.model.index==endpoint+direction);
        assert(keyboard.model.upper==(group==6));
        hello_keyboard_update(&keyboard,resumed+49,dpad|face,0,0,0);assert(keyboard.model.index==endpoint+direction);
        hello_keyboard_update(&keyboard,resumed+50,dpad|face,0,0,0);assert(keyboard.model.index==endpoint+2*direction);
        hello_keyboard_update(&keyboard,resumed+51,dpad,0,face,0);
        hello_keyboard_update(&keyboard,resumed+30000,dpad,0,0,0);
        assert(keyboard.model.index==endpoint+2*direction&&!keyboard.model.text[0]);
    }
    for(int button=0;button<2;button++)for(int cancel=0;cancel<3;cancel++) {
        const int direction=button==HELLO_KB_BUTTON_A?1:-1;
        const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB,opposite=face==kButtonA?kButtonB:kButtonA;
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,direction,kOverflowGroups,482000);
        assert(hello_keyboard_set_button_navigation_scale(&keyboard,.5f));
        hello_keyboard_update(&keyboard,482020,kButtonUp|face,face,0,0);
        hello_keyboard_update(&keyboard,482028,kButtonUp,0,face,0);
        hello_keyboard_update(&keyboard,482040,kButtonUp|face,face,0,0);
        if(cancel==0) {
            hello_keyboard_update(&keyboard,482060,face,0,kButtonUp,0);
            char committed[HELLO_KEYBOARD_ITEM_BYTES+1];strcpy(committed,keyboard.model.text);
            hello_keyboard_update(&keyboard,483060,face,0,0,0);
            assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,committed));
            hello_keyboard_update(&keyboard,483080,kButtonRight|face,kButtonRight,0,0);
            hello_keyboard_update(&keyboard,484080,kButtonRight|face,0,0,0);
            assert(keyboard.model.group==2&&!keyboard.model.pending&&!strcmp(keyboard.model.text,committed));
        } else if(cancel==1) {
            hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,"fresh"));
            hello_keyboard_update(&keyboard,483060,face,0,0,0);
            assert(!keyboard.model.pending&&!keyboard.model.group&&!strcmp(keyboard.model.text,"fresh"));
            hello_keyboard_close(&keyboard);
        } else {
            // Cancel still requires fresh physical faces; a previously
            // rejected held face cannot silently become an opening chord.
            hello_keyboard_update(&keyboard,482050,kButtonUp,0,face,0);
            hello_keyboard_update(&keyboard,482060,kButtonUp|face|opposite,face|opposite,0,0);
            hello_keyboard_update(&keyboard,482410,kButtonUp|face|opposite,0,0,0);assert(keyboard.cancel_dialog_active);
            hello_keyboard_update(&keyboard,482411,kButtonUp,0,face|opposite,0);
            assert(discard_editing_popup(&keyboard,482420)==HELLO_KEYBOARD_CANCELLED);
            assert(!keyboard.visible&&!keyboard.cancel_dialog_active);
            hello_keyboard_update(&keyboard,482773,kButtonUp,0,kButtonB,0);
            const int selected=keyboard.model.index;
            for(uint32_t now=482800;now<484000;now+=20) {
                hello_keyboard_update(&keyboard,now,kButtonUp,0,0,0);draw_group(&keyboard,1,105,69,now);
                assert(keyboard.model.index==selected&&!keyboard.model.text[0]);
            }
        }
    }
}

static void test_rapid_taps_step_on_press_only(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    const uint32_t cadence[]={16,40,60,100};
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)for(int button=0;button<2;button++)for(unsigned rate=0;rate<4;rate++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard={0};keyboard.pd=reference->pd;keyboard.config=reference->config;
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        hello_keyboard_model_init(&keyboard.model,"",0);hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
        keyboard.visible=true;keyboard.model.upper=group==6;
        assert(hello_keyboard_set_button_navigation_scale(&keyboard,.5f));
        const uint32_t begin=460000;const PDButtons dpad=directions[group],face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;
        hello_keyboard_update(&keyboard,begin,dpad,dpad,0,0);draw_group(&keyboard,group,105,69,begin);
        const int first=button==HELLO_KB_BUTTON_A?0:group_last_visible_item(&keyboard),direction=button==HELLO_KB_BUTTON_A?1:-1;
        for(unsigned tap=0;tap<4;tap++) {
            const uint32_t now=begin+16+tap*cadence[rate];const int expected=first+(int)tap*direction;
            hello_keyboard_update(&keyboard,now,dpad|face,face,0,0);
            assert(keyboard.model.index==expected&&keyboard.model.pending_at==now&&!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons);
            draw_group(&keyboard,group,105,69,now);
            hello_keyboard_update(&keyboard,now+cadence[rate]/2,dpad,0,face,0);
            assert(keyboard.model.index==expected&&!keyboard.model.hold[button].active); // Never a second release step.
        }
        char selected[HELLO_KEYBOARD_ITEM_BYTES+1];hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        hello_keyboard_update(&keyboard,begin+30000,dpad,0,0,0);
        assert(keyboard.model.index==first+3*direction&&keyboard.model.pending&&!keyboard.model.text[0]);
        hello_keyboard_update(&keyboard,begin+30001,0,0,dpad,0);assert(!strcmp(keyboard.model.text,selected));
    }
    // True endpoints now also advance on press. Their existing presentation
    // gate remains until the actual row and marker have settled.
    for(int button=0;button<2;button++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,1,button==HELLO_KB_BUTTON_A?1:-1,kOverflowGroups,470000);

        const int direction=button==HELLO_KB_BUTTON_A?1:-1;select_group_item(&keyboard,1,direction>0?62:1);
        const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;
        hello_keyboard_update(&keyboard,470020,kButtonUp|face,face,0,0);assert(keyboard.model.index==(direction>0?63:0));
        hello_keyboard_update(&keyboard,470028,kButtonUp,0,face,0);
        hello_keyboard_update(&keyboard,470036,kButtonUp|face,face,0,0);
        assert(keyboard.model.index==(direction>0?0:63)&&keyboard.selection_lock_group==1);
        hello_keyboard_update(&keyboard,470044,kButtonUp,0,face,0);
        assert(keyboard.model.index==(direction>0?0:63)&&keyboard.selection_lock_group==1&&keyboard.selection_lock_crank);
        const int endpoint=keyboard.model.index;
        hello_keyboard_update(&keyboard,470052,kButtonUp|face,face,0,0);assert(keyboard.rejected_face_buttons&face);
        const uint32_t settled=crank_test_finish_wrap(&keyboard,470052);
        hello_keyboard_update(&keyboard,settled+20,kButtonUp|face,0,0,0);assert(keyboard.model.index==endpoint);
        hello_keyboard_update(&keyboard,settled+28,kButtonUp,0,face,0);assert(!keyboard.rejected_face_buttons);
    }
}

static void test_tap_then_hold_repeats_current_selection(const HelloKeyboard* reference) {
    const char* letters="nomkqjplr";
    for(int origin=0;origin<3;origin++)for(int upper=0;upper<2;upper++)for(int button=0;button<2;button++) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;config.button_navigation_scale=.5f;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);assert(hello_keyboard_open(&keyboard,""));keyboard.model.upper=upper;
        const uint32_t begin=370000;const PDButtons face=button==HELLO_KB_BUTTON_A?kButtonA:kButtonB;const int direction=button==HELLO_KB_BUTTON_A?1:-1;
        hello_keyboard_update(&keyboard,begin,kButtonRight,kButtonRight,0,0);
        if(origin==0)hello_keyboard_model_crank(&keyboard.model,5,begin+80);
        else for(unsigned tap=0;tap<5;tap++) {
            const PDButtons initial=origin==1?kButtonA:kButtonB;const uint32_t now=begin+16+tap*32;
            hello_keyboard_update(&keyboard,now,kButtonRight|initial,initial,0,0);hello_keyboard_update(&keyboard,now+8,kButtonRight,0,initial,0);
        }
        assert(keyboard.model.index==4);
        hello_keyboard_update(&keyboard,begin+200,kButtonRight|face,face,0,0);assert(keyboard.model.index==4+direction);
        hello_keyboard_update(&keyboard,begin+208,kButtonRight,0,face,0);assert(keyboard.model.index==4+direction);
        hello_keyboard_update(&keyboard,begin+216,kButtonRight|face,face,0,0);
        assert(keyboard.model.index==4+2*direction&&keyboard.model.hold[button].hold==HELLO_KB_HOLD_REPEAT);
        hello_keyboard_update(&keyboard,begin+390,kButtonRight|face,0,0,0);assert(keyboard.model.index==4+2*direction);
        hello_keyboard_update(&keyboard,begin+391,kButtonRight|face,0,0,0);assert(keyboard.model.index==4+3*direction);
        hello_keyboard_update(&keyboard,begin+441,kButtonRight|face,0,0,0);assert(keyboard.model.index==4+4*direction);
        const char raw=letters[keyboard.model.index];char selected[2]={(char)(upper?raw-'a'+'A':raw),0};
        assert(!strcmp(keyboard.model.pending_text,selected)&&keyboard.model.upper==(bool)upper);
        check_current_item_render(&keyboard,selected,begin+441);
        hello_keyboard_update(&keyboard,begin+442,kButtonRight,0,face,0);assert(!strcmp(keyboard.model.pending_text,selected));
        hello_keyboard_update(&keyboard,begin+30000,kButtonRight,0,0,0);assert(!strcmp(keyboard.model.pending_text,selected)&&!keyboard.model.text[0]);
        hello_keyboard_update(&keyboard,begin+30001,0,0,kButtonRight,0);assert(!strcmp(keyboard.model.text,selected)&&keyboard.model.upper==(bool)upper);hello_keyboard_close(&keyboard);
    }
}

static void test_visible_hold_repeat_and_release_deadline(const HelloKeyboard* reference) {
    const int groups[]={2,3,4,1};const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(unsigned g=0;g<4;g++)for(int direction=-1;direction<=1;direction+=2) {
        const int group=groups[g];HelloKeyboard keyboard;
        const char* fitted_digits[HELLO_KEYBOARD_GROUP_COUNT]={"1234567890",NULL,NULL,NULL,NULL,NULL};
        crank_test_prepare(&keyboard,reference,group,direction<0?1:-1,group==1?fitted_digits:NULL,280000);
        keyboard.model.pending=0;keyboard.model.pending_text[0]=0;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),start=direction>0?0:count-1;
        const PDButtons dpad=directions[group],face=direction>0?kButtonA:kButtonB;
        const uint32_t begin=280020;
        hello_keyboard_update(&keyboard,begin,dpad|face,face,0,0);assert(keyboard.model.index==start&&!keyboard.selection_lock_group);
        draw_group(&keyboard,group,105,69,begin);
        hello_keyboard_update(&keyboard,begin+349,dpad|face,0,0,0);assert(keyboard.model.index==start);
        hello_keyboard_update(&keyboard,begin+350,dpad|face,0,0,0);assert(keyboard.model.index==start+direction);
        hello_keyboard_update(&keyboard,begin+450,dpad|face,0,0,0);assert(keyboard.model.index==start+2*direction);
        hello_keyboard_update(&keyboard,begin+1450,dpad|face,0,0,0);assert(keyboard.model.index==start+3*direction);
        hello_keyboard_update(&keyboard,begin+1451,dpad|face,0,0,0);assert(keyboard.model.index==start+3*direction);
        uint32_t now=begin+1550;int index=start+4*direction;
        hello_keyboard_update(&keyboard,now,dpad|face,0,0,0);assert(keyboard.model.index==index);
        while(index>=0&&index<count) {
            now+=100;index+=direction;
            const bool wraps=index<0||index>=count;if(wraps)index=direction>0?0:count-1;
            hello_keyboard_update(&keyboard,now,dpad|face,0,0,0);draw_group(&keyboard,group,105,69,now);
            assert(keyboard.model.index==index&&!keyboard.selection_lock_group);
            if(wraps)break;
        }
        hello_keyboard_update(&keyboard,now+1,dpad|face,0,0,0);assert(keyboard.model.index==index);
        now+=100;index+=direction;hello_keyboard_update(&keyboard,now,dpad|face,0,0,0);assert(keyboard.model.index==index);
        char selected[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];hello_keyboard_model_pending_text(&keyboard.model,selected,sizeof(selected));
        assert(hello_keyboard_update(&keyboard,now+100,face,0,dpad,0)==HELLO_KEYBOARD_NONE);
        assert(!keyboard.model.group&&!keyboard.model.pending&&!strcmp(keyboard.model.text,selected));
        assert(!keyboard.model.hold[face==kButtonA?HELLO_KB_BUTTON_A:HELLO_KB_BUTTON_B].active);
    }
}
static void test_offscreen_entry_gate_repeat_and_crank(const HelloKeyboard* reference) {
    const PDButtons directions[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=1;group<=6;group++)for(int action=0;action<3;action++) {
        if(group==HELLO_KEYBOARD_NUMBERS_GROUP)continue;
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,1,kOverflowGroups,290000);
        hello_keyboard_model_commit(&keyboard.model,false);keyboard.model.text[0]=0;keyboard.model.cursor=0;
        reset_selection_lock(&keyboard);reset_crank(&keyboard);keyboard.selection_marker.active=false;
        const PDButtons dpad=directions[group];const uint32_t begin=290020;
        hello_keyboard_update(&keyboard,begin,dpad,dpad,0,0);draw_group(&keyboard,group,105,69,begin);
        hello_keyboard_update(&keyboard,begin+16,dpad|kButtonA,kButtonA,0,0);
        assert(keyboard.model.index==0&&keyboard.selection_lock_group==group&&!keyboard.selection_lock_crank);
        if(action)hello_keyboard_update(&keyboard,begin+24,dpad,0,kButtonA,0);
        if(action==2) {
            hello_keyboard_update(&keyboard,begin+32,dpad,0,0,5);
            assert(keyboard.model.index==0&&keyboard.selection_lock_group==group);
            const float threshold=crank_test_threshold(&keyboard,1);
            crank_render_travel(&keyboard,1,threshold-5+0.001f,begin+32);
            assert(keyboard.model.index==1&&!keyboard.selection_lock_group&&keyboard.crank_release_direction==1);
            continue;
        }
        if(action==1) {
            hello_keyboard_update(&keyboard,begin+32,dpad|kButtonA,kButtonA,0,0);
            assert(keyboard.rejected_face_buttons&kButtonA);
        }
        uint32_t arrived=UINT32_MAX;
        for(uint32_t elapsed=40;elapsed<=1240;elapsed+=20) {
            hello_keyboard_update(&keyboard,begin+elapsed,dpad|kButtonA,0,0,0);
            assert(keyboard.model.index==0&&!keyboard.model.text[0]);
            draw_group(&keyboard,group,105,69,begin+elapsed);
            if(!keyboard.selection_lock_group){arrived=begin+elapsed;break;}
        }
        assert(arrived!=UINT32_MAX&&arrived-begin<=1250);
        hello_keyboard_update(&keyboard,arrived+1,dpad|kButtonA,0,0,0);
        if(action==1) {
            // A continuous blocked hold has already reached its hold threshold
            // and resumes once the actual row/marker gate has cleared.
            assert(keyboard.model.index==1&&keyboard.model.hold[HELLO_KB_BUTTON_A].hold==HELLO_KB_HOLD_REPEAT);
        } else {
            assert(keyboard.model.index==0);
            const uint32_t deadline=keyboard.model.hold[HELLO_KB_BUTTON_A].next_selection_at;
            assert(deadline>arrived+1);hello_keyboard_update(&keyboard,deadline-1,dpad|kButtonA,0,0,0);assert(keyboard.model.index==0);
            hello_keyboard_update(&keyboard,deadline,dpad|kButtonA,0,0,0);
        }
        assert(keyboard.model.index==1&&!keyboard.selection_lock_group);
    }
}
static void test_ten_digit_viewport_override(const HelloKeyboard* reference) {
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
    for(int group=0;group<6;group++)assert(!config.visible_prefix_items[group]);
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(hello_keyboard_open(&keyboard,""));
    assert(group_bounds(&keyboard,1).width==297);hello_keyboard_set_visible_prefix(&keyboard,1,10);
    assert(keyboard.config.visible_prefix_items[0]==10&&group_bounds(&keyboard,1).width==234);
    capture_group_view=capture_initial_digits=true;initial_digit_count=group_draw_count=0;
    draw_group(&keyboard,1,105,69,300000);initial_digits[initial_digit_count]=0;
    assert(initial_digit_count==10&&!strcmp(initial_digits,"1234567890")&&group_draw_count==10);
    capture_group_view=capture_initial_digits=false;
    hello_keyboard_update(&keyboard,300020,kButtonUp,kButtonUp,0,0);
    assert(group_last_visible_item(&keyboard)==9);
    hello_keyboard_update(&keyboard,300028,kButtonUp|kButtonB,kButtonB,0,0);
    assert(keyboard.model.index==9&&keyboard.model.pending=='0'&&!keyboard.selection_lock_group);
    hello_keyboard_update(&keyboard,300036,kButtonUp,0,kButtonB,0);
    hello_keyboard_update(&keyboard,300044,kButtonUp|kButtonA,kButtonA,0,0);
    assert(keyboard.model.index==10&&keyboard.model.pending=='.'&&!keyboard.selection_lock_group);
    hello_keyboard_update(&keyboard,300052,kButtonUp,0,kButtonA,0);
    assert(hello_keyboard_update(&keyboard,300060,0,0,kButtonUp,0)==HELLO_KEYBOARD_NONE&&!strcmp(keyboard.model.text,"."));
    hello_keyboard_close(&keyboard);assert(hello_keyboard_open(&keyboard,""));
    hello_keyboard_update(&keyboard,300100,kButtonUp,kButtonUp,0,0);
    const float first=crank_test_threshold(&keyboard,-1);
    hello_keyboard_update(&keyboard,300600,kButtonUp,0,0,-first-0.001f);
    assert(keyboard.model.index==9&&keyboard.model.pending=='0'&&!keyboard.selection_lock_group&&!keyboard.crank_wrapped);
    const float reverse=crank_test_threshold(&keyboard,1)+fabsf(keyboard.crank_remainder)+0.001f;
    hello_keyboard_update(&keyboard,301100,kButtonUp,0,0,reverse);
    assert(keyboard.model.index==10&&keyboard.model.pending=='.'&&!keyboard.selection_lock_group&&!keyboard.crank_wrapped);
    hello_keyboard_set_visible_prefix(&keyboard,1,100);assert(keyboard.config.visible_prefix_items[0]==64);
    hello_keyboard_set_visible_prefix(&keyboard,1,0);assert(group_bounds(&keyboard,1).width==297);
    hello_keyboard_close(&keyboard);
}
static void test_immediate_crank_entry_keeps_visible_cell(const HelloKeyboard* reference) {
    for(unsigned prefix=8;prefix<=10;prefix+=2)for(int sign=-1;sign<=1;sign+=2) {
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        hello_keyboard_set_visible_prefix(&keyboard,1,prefix);assert(hello_keyboard_open(&keyboard,""));
        hello_keyboard_update(&keyboard,400000,kButtonUp,kButtonUp,0,0);
        draw_group(&keyboard,1,105,69,400000);
        for(int frame=1;frame<=4;frame++) {
            hello_keyboard_update(&keyboard,400000+(uint32_t)frame*16,kButtonUp,0,0,(float)sign*0.01f);
            marker_draws=0;draw_group(&keyboard,1,105,69,400000+(uint32_t)frame*16);
            assert(!keyboard.model.pending&&!marker_draws&&!keyboard.crank_remainder);
        }
        const int entry=sign<0?(int)prefix-1:0;
        hello_keyboard_update(&keyboard,400080,kButtonUp,0,0,(float)sign*360);
        assert(keyboard.model.index==entry&&keyboard.model.pending==(uint32_t)"1234567890"[entry]);
        assert(keyboard.crank_entry_breakaway&&!keyboard.crank_remainder&&!keyboard.crank_wrapped&&!keyboard.selection_lock_group);
        capture_group_view=true;marker_draws=0;draw_group(&keyboard,1,105,69,400080);
        assert(marker_draws==1);capture_group_view=false;
        // Even a full first revolution selects exactly the visible entry. Zero
        // and balanced sub-threshold motion cannot replay its discarded travel.
        for(uint32_t now=400096;now<=400240;now+=16) {
            hello_keyboard_update(&keyboard,now,kButtonUp,0,0,0);draw_group(&keyboard,1,105,69,now);
            assert(keyboard.model.index==entry&&!keyboard.crank_wrapped&&!keyboard.selection_lock_group);
        }
        // A same-direction step pays the initial well; reversing during the
        // following frames pays the reversal well and returns one cell exactly.
        const int advance=sign<0?-1:1;
        hello_keyboard_update(&keyboard,400256,kButtonUp,0,0,(float)advance*crank_test_threshold(&keyboard,advance)*1.101f);
        assert(keyboard.model.index==entry+advance&&!keyboard.crank_entry_breakaway);
        draw_group(&keyboard,1,105,69,400256);
        const float reverse=fabsf(keyboard.crank_remainder)+crank_test_threshold(&keyboard,-advance)*1.101f;
        hello_keyboard_update(&keyboard,400272,kButtonUp,0,0,(float)-advance*reverse);
        assert(keyboard.model.index==entry&&!keyboard.crank_wrapped&&!keyboard.selection_lock_group);
        for(uint32_t now=400288;now<=401072;now+=16) {
            hello_keyboard_update(&keyboard,now,kButtonUp,0,0,0);draw_group(&keyboard,1,105,69,now);
            assert(keyboard.model.index==entry&&!keyboard.model.text[0]);
        }
        assert(!keyboard.group_scroll_motion[1].remaining_ms&&brb_elastic_marker_settled(&keyboard.selection_marker,keyboard.selection_marker.left,keyboard.selection_marker.right));
        hello_keyboard_close(&keyboard);
    }
}

static void test_return_submission_and_native_icon(const HelloKeyboard* reference) {
    const size_t capacities[]={0,6,160};
    for(unsigned sample=0;sample<3;sample++) {
        const size_t capacity=capacities[sample];char initial[HELLO_KEYBOARD_DEFAULT_MAX_CHARACTERS+1];memset(initial,'x',capacity);initial[capacity]=0;
        HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;
        HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
        hello_keyboard_set_max_length(&keyboard,capacity);assert(hello_keyboard_open(&keyboard,initial));keyboard.model.upper=true;
        hello_keyboard_update(&keyboard,310000,kButtonDown,kButtonDown,0,0);
        hello_keyboard_update(&keyboard,310008,kButtonDown|kButtonB,kButtonB,0,0);
        assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT&&!keyboard.selection_lock_group);
        capture_group_view=capture_return_icon=true;return_line_count=group_draw_count=marker_draws=0;
        draw_group(&keyboard,3,105,157,310008);assert(return_line_count==4&&marker_draws==1);
        assert(return_lines[0][0]==return_lines[0][2]&&return_lines[0][3]-return_lines[0][1]==6);
        assert(return_lines[1][0]-return_lines[1][2]==10&&return_lines[1][1]==return_lines[1][3]);
        assert(return_lines[2][2]-return_lines[2][0]==3&&return_lines[2][3]-return_lines[2][1]==-3);
        assert(return_lines[3][2]-return_lines[3][0]==3&&return_lines[3][3]-return_lines[3][1]==3);
        for(int segment=0;segment<4;segment++)assert(return_lines[segment][0]>=marker_x&&return_lines[segment][2]>=marker_x&&
            return_lines[segment][0]<marker_x+marker_w&&return_lines[segment][2]<marker_x+marker_w);
        capture_group_view=capture_return_icon=false;draw_input_field(&keyboard,0);assert(!strcmp(rendered,initial));
        hello_keyboard_update(&keyboard,310016,kButtonDown,0,kButtonB,0);
        assert(hello_keyboard_update(&keyboard,310024,0,0,kButtonDown,0)==HELLO_KEYBOARD_NONE);accept_editing_save(&keyboard,310025);
        assert(keyboard.visible&&!keyboard.model.pending&&!keyboard.model.group&&!keyboard.model.submit_requested&&!strcmp(keyboard.model.text,initial));
        hello_keyboard_show_error(&keyboard,"Keep editing",311000,5000);
        assert(hello_keyboard_update(&keyboard,311001,0,0,0,0)==HELLO_KEYBOARD_NONE);
        done_callback(&keyboard);assert(hello_keyboard_update(&keyboard,311002,0,0,0,0)==HELLO_KEYBOARD_NONE);
        assert(save_editing_popup(&keyboard,311003)==HELLO_KEYBOARD_DONE);
        assert(hello_keyboard_update(&keyboard,311356,0,0,kButtonA,0)==HELLO_KEYBOARD_NONE&&!strcmp(keyboard.model.text,initial));
        hello_keyboard_close(&keyboard);
    }
    HelloKeyboardConfig config;hello_keyboard_default_config(&config);config.font=(LCDFont*)&bitmap;config.font_size=14;config.groups[2]="a⎵";
    HelloKeyboard keyboard;loads=0;hello_keyboard_init_with_config(&keyboard,reference->pd,&config);
    assert(hello_keyboard_open_field(&keyboard,"123456",HELLO_INPUT_CONTACT_NUMBER));keyboard.model.upper=true;
    hello_keyboard_update(&keyboard,320000,kButtonDown,kButtonDown,0,0);hello_keyboard_update(&keyboard,320008,kButtonDown|kButtonB,kButtonB,0,0);
    assert(keyboard.model.pending==HELLO_KEYBOARD_RETURN_CODEPOINT);hello_keyboard_update(&keyboard,320016,kButtonDown,0,kButtonB,0);
    assert(hello_keyboard_update(&keyboard,320024,0,0,kButtonDown,0)==HELLO_KEYBOARD_NONE);accept_editing_save(&keyboard,320025);assert(!strcmp(keyboard.model.text,"123456"));
    hello_keyboard_close(&keyboard);
}

static void test_rapid_face_selection_without_boundary(const HelloKeyboard* reference) {
    // Primary groups, digits and emoji all accept 16 ms taps while the marker,
    // endpoint scroll are still following earlier taps.
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int entry=0;entry<2;entry++) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,entry?1:-1,NULL,40000);
        keyboard.model.pending=0;keyboard.model.pending_text[0]=0;keyboard.selection_marker.active=false;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group);
        const PDButtons face=entry?kButtonB:kButtonA,opposite=entry?kButtonA:kButtonB;
        const int first=entry?count-1:0,direction=entry?-1:1;
        ordinary_face_tap(&keyboard,face,40020,first);
        ordinary_face_tap(&keyboard,face,40036,first+direction);
        ordinary_face_tap(&keyboard,face,40052,first+2*direction);
        ordinary_face_tap(&keyboard,opposite,40068,first+direction);
        ordinary_face_tap(&keyboard,opposite,40084,first);
        assert(!keyboard.model.text[0]);
    }
    // An accepted active-group hold repeats selection without creating a gate.
    HelloKeyboard held;crank_test_prepare(&held,reference,2,-1,NULL,50000);
    held.model.pending=0;held.model.pending_text[0]=0;
    hello_keyboard_update(&held,50020,kButtonA,kButtonA,0,0);
    assert(held.model.pending=='n');
    for(uint32_t elapsed=0;elapsed<=held.model.timing.hold_ms+20;elapsed+=20) {
        hello_keyboard_update(&held,50020+elapsed,kButtonA,0,0,0);
        draw_group(&held,2,236,112,50020+elapsed);
        assert(!held.selection_lock_group&&!held.rejected_face_buttons);
    }
    assert(held.model.pending=='o'&&held.model.index==1&&held.model.hold[HELLO_KB_BUTTON_A].handled);
    hello_keyboard_update(&held,50420,0,0,kButtonA,0);
    assert(!held.model.hold[HELLO_KB_BUTTON_A].active&&held.model.pending=='o');
    ordinary_face_tap(&held,kButtonA,50436,2);
    assert(held.model.pending=='m');
}

static void test_fitted_face_wraps_and_crank_boundary(const HelloKeyboard* reference) {
    const int groups[]={2,3,4,1};
    for(unsigned group_at=0;group_at<sizeof(groups)/sizeof(groups[0]);group_at++)
    for(int layer=0;layer<3;layer++)
    for(int direction=-1;direction<=1;direction+=2) {
        const int group=groups[group_at];
        const char* fitted_digits[HELLO_KEYBOARD_GROUP_COUNT]={"1234567890",NULL,NULL,NULL,NULL,NULL};
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,group==1?fitted_digits:NULL,180000);
        keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=14;keyboard.config.unicode_font=NULL;
        keyboard.model.upper=layer==1;keyboard.model.field.type=(group==5||layer==2)?HELLO_INPUT_CONTACT_NUMBER:HELLO_INPUT_PLAINTEXT;
        select_group_item(&keyboard,group,keyboard.model.index);
        reset_group_scrolls(&keyboard);keyboard.selection_marker.active=false;
        assert(!group_has_overflow(&keyboard,group));
        const HelloKeyboard initial=keyboard;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),edge=keyboard.model.index;
        const int endpoint=direction>0?0:count-1;
        const PDButtons face=direction>0?kButtonA:kButtonB,opposite=direction>0?kButtonB:kButtonA;
        const PDButtons taps[]={face,face,opposite,opposite,face};
        const int indices[]={endpoint,endpoint+direction,endpoint,edge,endpoint};
        uint32_t now=180020;
        for(unsigned tap=0;tap<sizeof(taps)/sizeof(taps[0]);tap++,now+=16) {
            ordinary_face_tap(&keyboard,taps[tap],now,indices[tap]);
            capture_group_view=true;group_draw_count=marker_draws=0;
            draw_group(&keyboard,group,105,69,now+8);
            // The space indicator in group 3 is drawn from three lines.
            assert(group_draw_count==count-(group==3?1:0)&&marker_draws==1&&keyboard.group_scroll[group].position==0);
            assert(marker_x>=group_clip_x&&marker_x+marker_w<=group_clip_x+group_clip_w);
            capture_group_view=false;
        }
        for(unsigned frame=0;frame<64;frame++,now+=20) {
            hello_keyboard_update(&keyboard,now,0,0,0,0);draw_group(&keyboard,group,105,69,now);
            assert(keyboard.model.index==endpoint&&keyboard.model.pending&&!keyboard.model.text[0]);
            assert(!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons&&!keyboard.crank_remainder);
        }
        // The same fitted geometry retains the crank's existing seam gate.
        HelloKeyboard crank=initial;
        hello_keyboard_update(&crank,182000,0,0,0,
            (float)direction*(crank_test_threshold(&crank,direction)+0.001f));
        assert(crank.model.index==endpoint&&crank.selection_lock_group==group&&crank.selection_lock_crank);
        const int release=crank.crank_release_steps;
        hello_keyboard_update(&crank,182016,face,face,0,10000);
        assert(crank.model.index==endpoint&&(crank.rejected_face_buttons&face)&&!crank.crank_remainder);
        assert(crank.crank_release_steps==release&&release==5);
        const uint32_t settled=crank_test_finish_wrap(&crank,182016);
        assert(crank.model.index==endpoint&&!crank.crank_remainder);
        hello_keyboard_update(&crank,settled+8,0,0,face,0);
        ordinary_face_tap(&crank,face,settled+16,endpoint+direction);
    }
}

static void test_face_boundary_gate(const HelloKeyboard* reference) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group++)
    for(int direction=-1;direction<=1;direction+=2)
    for(uint32_t frame_ms=20;frame_ms<=40;frame_ms+=20) {
        HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,group,direction,kOverflowGroups,60000);
        keyboard.config.font_size=15;assert(group_has_overflow(&keyboard,group));
        for(uint32_t now=60020;now<=60600;now+=20)draw_group(&keyboard,group,105,69,now);
        const uint32_t begin=60620;
        const PDButtons face=direction>0?kButtonA:kButtonB,opposite=direction>0?kButtonB:kButtonA;
        const int count=hello_keyboard_model_group_length(&keyboard.model,group),endpoint=direction>0?0:count-1;
        hello_keyboard_update(&keyboard,begin,face,face,0,0);
        assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==group&&keyboard.selection_lock_crank);
        const uint32_t selected=keyboard.model.pending;
        hello_keyboard_update(&keyboard,begin+8,0,0,face,0);
        hello_keyboard_update(&keyboard,begin+16,face,face,0,10000);
        assert(keyboard.rejected_face_buttons&face);
        hello_keyboard_update(&keyboard,begin+24,0,0,face,-10000);
        hello_keyboard_update(&keyboard,begin+32,opposite,opposite,0,10000);
        assert(keyboard.rejected_face_buttons&opposite);
        uint32_t arrived=UINT32_MAX,first_visible=UINT32_MAX;
        for(uint32_t elapsed=40;elapsed<=1240;elapsed+=frame_ms) {
            hello_keyboard_update(&keyboard,begin+elapsed,opposite,0,0,elapsed%80?10000:-10000);
            assert(keyboard.model.index==endpoint&&keyboard.model.pending==selected&&!keyboard.model.text[0]);
            assert(!keyboard.crank_remainder&&!keyboard.crank_release_steps);
            assert(!keyboard.model.hold[direction>0?HELLO_KB_BUTTON_B:HELLO_KB_BUTTON_A].active);
            marker_draws=0;draw_group(&keyboard,group,105,69,begin+elapsed);
            if(marker_draws&&first_visible==UINT32_MAX)first_visible=elapsed;
            if(!keyboard.selection_lock_group){arrived=elapsed;break;}
        }
        assert(arrived<=1250&&first_visible!=UINT32_MAX&&arrived-first_visible<=650);
        // This synthetic selection has no physically held associated D-pad.
        // Its rejected input remains ineligible after the rendered gate ends.
        hello_keyboard_update(&keyboard,begin+arrived+frame_ms,opposite,0,0,0);
        assert(keyboard.model.index==endpoint&&keyboard.model.pending==selected);
        hello_keyboard_update(&keyboard,begin+arrived+2*frame_ms,0,0,opposite,0);
        assert(!keyboard.rejected_face_buttons);
        ordinary_face_tap(&keyboard,face,begin+arrived+3*frame_ms,endpoint+direction);
    }
}

static void test_pressure_and_valid_direction_boundary_policy(const HelloKeyboard* reference) {
    HelloKeyboard keyboard;crank_test_prepare(&keyboard,reference,2,-1,NULL,70000);
    keyboard.model.pending=0;keyboard.model.pending_text[0]=0;keyboard.selection_marker.active=false;
    ordinary_face_tap(&keyboard,kButtonA,70020,0);
    hello_keyboard_update(&keyboard,70032,0,0,0,20);
    draw_group(&keyboard,2,236,112,70032);
    assert(keyboard.model.index==0&&!keyboard.selection_lock_group&&keyboard.crank_remainder==20);
    ordinary_face_tap(&keyboard,kButtonA,70048,1);
    assert(!keyboard.crank_remainder);
    const uint32_t stepped=crank_render_travel(&keyboard,1,crank_test_threshold(&keyboard,1)+0.001f,70064);
    assert(keyboard.model.index==2&&!keyboard.selection_lock_group);
    ordinary_face_tap(&keyboard,kButtonB,stepped+16,1);

    crank_test_prepare(&keyboard,reference,1,-1,kOverflowGroups,80000);keyboard.model.field.type=HELLO_INPUT_CONTACT_NUMBER;
    hello_keyboard_update(&keyboard,80020,kButtonB,kButtonB,0,0);
    const int endpoint=hello_keyboard_model_group_length(&keyboard.model,1)-1;
    assert(keyboard.model.index==endpoint&&keyboard.selection_lock_group==1);
    hello_keyboard_update(&keyboard,80028,0,0,kButtonB,0);
    // Numeric input has the same valid Left route. Retargeting retires the old
    // gate and shows its new selection; type rejection happens only on commit.
    hello_keyboard_update(&keyboard,80040,kButtonLeft|kButtonA,kButtonLeft|kButtonA,0,0);
    assert(keyboard.model.group==4&&keyboard.model.index==0&&keyboard.model.pending);
    assert(!keyboard.selection_lock_group&&!keyboard.rejected_face_buttons&&!keyboard.crank_remainder&&!keyboard.model.text[0]);

}

static void test_last_entry_pressure_keeps_endpoint_motion(HelloKeyboard* keyboard) {
    for(int group=1;group<=HELLO_KEYBOARD_GROUP_COUNT;group+=5) {
        hello_keyboard_model_init(&keyboard->model,"",0);reset_crank(keyboard);
        reset_group_scrolls(keyboard);keyboard->selection_marker.active=false;
        keyboard->model.group=group;
        draw_group(keyboard,group,105,69,60000);
        hello_keyboard_model_press(&keyboard->model,HELLO_KB_BUTTON_B,60016);
        hello_keyboard_model_release(&keyboard->model,HELLO_KB_BUTTON_B,60017);
        draw_group(keyboard,group,105,69,60016);
        crank_test_selection(keyboard,-5,60032);marker_draws=0;
        draw_group(keyboard,group,105,69,60032);
        assert(keyboard->model.pending_side==2&&!marker_draws);
        assert(keyboard->group_scroll_motion[group].remaining_ms==508);
        for(uint32_t now=60048;now<=60544;now+=16)draw_group(keyboard,group,105,69,now);
        assert(keyboard->selection_marker.active&&!keyboard->group_scroll_motion[group].remaining_ms);
    }
}

int main(void) {
    test_crank_global_sensitivity();
    test_crank_update_mirrors();
    struct playdate_graphics graphics = {0};
    struct playdate_sys system = {0};
    PlaydateAPI pd = {0};
    graphics.fillRect=fill;
    graphics.loadBitmapTable = load_table;
    graphics.getTableBitmap = get_frame;
    system.getCurrentTimeMilliseconds = fake_time;
    system.addMenuItem=add_menu;system.removeMenuItem=remove_menu;
    system.addOptionsMenuItem=add_options_menu;system.getMenuItemValue=get_options_value;system.setMenuItemValue=set_options_value;
    pd.graphics = &graphics;
    pd.system = &system;
    static HelloKeyboard keyboard;
    hello_keyboard_init(&keyboard, &pd);
    assert(loads == 3);
    assert(keyboard.config.emoji_picker_enabled&&hello_keyboard_model_layer_count(&keyboard.model)==2);
    assert(hello_keyboard_emoji_set(&keyboard)==brb_emoji_everyday_set());
    char name[40];
    assert(image(&keyboard,"background-0-0-0")==NULL);
    for (int code = 32; code <= 126; ++code) {
        snprintf(name, sizeof(name), "glyph-%d", code);
        check(&keyboard, name, 0, code - 32);
    }
    for (int button = 0; button < 2; ++button)
        for (int fill = 0; fill <= 21; ++fill) {
            snprintf(name, sizeof(name), "footer-%c-%d", 'A' + button, fill);
            check(&keyboard, name, 1, button * 22 + fill);
        }
    const char* directions[] = {"none", "up", "right", "down", "left"};
    for (int i = 0; i < 5; ++i) {
        snprintf(name, sizeof(name), "dpad-%s", directions[i]);
        check(&keyboard, name, 2, i);
    }
    assert(image(&keyboard, "unknown") == NULL);
    assert(image(&keyboard, "glyph-999") == NULL);
    graphics.drawScaledBitmap=scaled_bitmap;
    for(int state=HELLO_KEYBOARD_DPAD_BLACK_NEUTRAL;state<=HELLO_KEYBOARD_DPAD_ALL_LIGHT;state++) {
        native_dpad_begin(55);hello_keyboard_draw_dpad(&keyboard,(HelloKeyboardDpadState)state,55,215);native_dpad_check(state);
    }
    footer_dpad_draws=0;
    const int footer_frames[]={0,1,2,3,4,1,1};
    const PDButtons footer_held[]={0,kButtonUp,kButtonRight,kButtonDown,kButtonLeft,kButtonUp,kButtonUp};
    for(int group=0;group<=HELLO_KEYBOARD_GROUP_COUNT;group++) {
        keyboard.model.group=group;expected_table=2;expected_frame=footer_frames[group];keyboard.footer_dpad_buttons=footer_held[group];
        native_dpad_begin(55);draw_footer_dpad_at(&keyboard,55);native_dpad_check(footer_frames[group]);
        keyboard.footer_dpad_buttons=0;native_dpad_begin(55);draw_footer_dpad_at(&keyboard,55);native_dpad_check(0);
    }
    assert(footer_dpad_draws==2*(HELLO_KEYBOARD_GROUP_COUNT+1));
    for(unsigned mask=0;mask<16;mask++) {
        keyboard.footer_dpad_buttons=(mask&1?kButtonUp:0)|(mask&2?kButtonRight:0)|(mask&4?kButtonDown:0)|(mask&8?kButtonLeft:0);
        native_dpad_begin(3);draw_footer_dpad_at(&keyboard,3);native_dpad_check_buttons(keyboard.footer_dpad_buttons);
    }
    hello_keyboard_model_init(&keyboard.model,"ab",1);
    keyboard.model.hold[HELLO_KB_BUTTON_A]=(HelloKeyboardHold){.active=true,.handled=true,.hold=HELLO_KB_HOLD_CASE};
    const HelloKeyboardDirection cursor_directions[]={HELLO_KB_LEFT,HELLO_KB_RIGHT};
    for(int i=0;i<2;i++) {
        hello_keyboard_model_direction(&keyboard.model,cursor_directions[i],1000+(uint32_t)i*100);
        assert(keyboard.model.cursor_hold&&keyboard.model.group==0);
        keyboard.footer_dpad_buttons=i==0?kButtonLeft:kButtonRight;
        expected_frame=i==0?4:2;native_dpad_begin(55);draw_footer_dpad_at(&keyboard,55);native_dpad_check(expected_frame);
        hello_keyboard_model_direction_release(&keyboard.model,cursor_directions[i],1001+(uint32_t)i*100);
        keyboard.footer_dpad_buttons=0;
        expected_frame=0;native_dpad_begin(55);draw_footer_dpad_at(&keyboard,55);native_dpad_check(0);
    }
    graphics.setFont=set_font;graphics.setDrawMode=draw_mode;graphics.getTextWidth=measure;
    graphics.drawText=text;graphics.fillRect=fill;graphics.drawRect=outline_rect;graphics.setClipRect=clip;graphics.clearClipRect=clear_clip;
    graphics.drawBitmap=draw_bitmap;
    graphics.drawEllipse=eye;graphics.fillEllipse=pupil;graphics.drawLine=line;graphics.fillTriangle=triangle;
    graphics.fillRoundRect=rounded_fill;graphics.drawRoundRect=rounded_outline;
    keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=10;
    test_crank_edge_release(&keyboard);
    test_crank_rendered_five_character_release(&keyboard);
    test_crank_rendered_velocity_and_idle(&keyboard);
    test_first_crank_wrap_softer_then_full_boundary(&keyboard);
    test_crank_reversal_during_wrap_release(&keyboard);
    hello_keyboard_model_init(&keyboard.model,"",0);
    keyboard.model.group=1;keyboard.model.pending='.';
    marker_draws=0;draw_group(&keyboard,1,105,69,0);
    assert(marker_draws==1&&marker_h==32&&marker_y==71);const int resting_width=marker_w;
    keyboard.model.index=9;keyboard.model.pending='!';
    draw_group(&keyboard,1,105,69,0);
    marker_draws=0;draw_group(&keyboard,1,105,69,40);
    assert(marker_draws==1&&marker_w>resting_width);
    assert(marker_x>=(400-test_group_pair_width(&keyboard,1))/2&&
        marker_x+marker_w<=(400+test_group_pair_width(&keyboard,1))/2&&marker_y>=71&&marker_h==32);
    for(uint32_t now=60;now<=1000;now+=20)draw_group(&keyboard,1,105,69,now);
    assert(marker_w==resting_width);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_PLAINTEXT);
    for(const char** value=(const char*[]){"ii","ié🧡z",NULL};*value;value++) {
        const size_t bytes=strlen(*value);
        for(size_t offset=0;offset<=bytes;) {
            hello_keyboard_model_init(&keyboard.model,*value,offset);
            text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
            assert(text_draws==1&&!strcmp(rendered,*value)&&text_x==23); // No suffix/prefix shifting.
            assert(cursor_draws==1);
            if(offset==bytes)assert(cursor_width>=6&&cursor_x==23+measure(NULL,*value,bytes,kUTF8Encoding,0)+2);
            else {
                uint32_t codepoint;const int count=hello_keyboard_utf8_decode(*value+offset,&codepoint);
                const int glyph=measure(NULL,*value+offset,(size_t)count,kUTF8Encoding,0);
                assert(cursor_width==glyph&&cursor_x==23+measure(NULL,*value,offset,kUTF8Encoding,0));
            }
            text_draws=cursor_draws=0;draw_input_field(&keyboard,420);
            assert(text_draws==1&&!strcmp(rendered,*value)&&text_x==23&&cursor_draws==0);
            if(offset==bytes)break;
            uint32_t codepoint;offset+=(size_t)hello_keyboard_utf8_decode(*value+offset,&codepoint);
        }
    }
    hello_keyboard_model_init(&keyboard.model,"a字",1);keyboard.model.pending=0x1f9e1;
    text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
    assert(!strcmp(rendered,"a🧡字")&&text_draws==1&&cursor_draws==1&&text_x==23);
    hello_input_field_init(&keyboard.input_field,HELLO_INPUT_MASKEDTEXT);keyboard.input_field.masked=true;
    text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
    assert(!strcmp(rendered,"***")&&text_draws==1&&cursor_draws==1);
    keyboard.model.pending=0x23b5;keyboard.input_field.masked=false;
    draw_input_field(&keyboard,0);assert(!strcmp(rendered,"a 字"));
    const char* clusters="a👨‍👩‍👧‍👦🇩🇪👍🏽éz";
    hello_keyboard_model_init(&keyboard.model,clusters,1);
    text_draws=cursor_draws=0;draw_input_field(&keyboard,0);
    assert(!strcmp(rendered,clusters)&&text_draws==1&&cursor_draws==1);
    assert(cursor_width==measure(NULL,"👨‍👩‍👧‍👦",strlen("👨‍👩‍👧‍👦"),kUTF8Encoding,0));
    keyboard.input_field.masked=true;
    draw_input_field(&keyboard,0);assert(!strcmp(rendered,"******")); // One star per grapheme.
    hello_keyboard_model_init(&keyboard.model,"",0);
    strcpy(keyboard.model.pending_text,"👨‍👩‍👧‍👦");
    hello_keyboard_utf8_decode(keyboard.model.pending_text,&keyboard.model.pending);
    draw_input_field(&keyboard,0);assert(!strcmp(rendered,"*"));
    keyboard.input_field.masked=false;
    draw_input_field(&keyboard,0);assert(!strcmp(rendered,"👨‍👩‍👧‍👦"));
    // Missing atlas/read failures must still use the measured Unicode cell.
    BrbUnicodeFont missing={0};missing.pd=&pd;
    keyboard.config.unicode_font=&missing;keyboard.config.font=NULL;keyboard.config.font_size=9;
    capture_bitmap_text=true;drawn_glyphs=0;
    draw_bitmap_text(&keyboard,"a👨‍👩‍👧‍👦b",10,20,false);
    assert(drawn_glyphs==3&&glyph_frames[1]==63-32);
    assert(glyph_x[0]==10&&glyph_x[2]==10+glyph_width('a')+1+17);
    assert(keyboard_text_width(&keyboard,"a👨‍👩‍👧‍👦b")==glyph_width('a')+1+17+glyph_width('b'));
    drawn_glyphs=marker_draws=0;
    keyboard.model.group=1;keyboard.model.index=0;
    draw_group(&keyboard,1,105,69,1000);
    assert(marker_draws==1&&marker_h>=19&&marker_y<=glyph_y[0]&&glyph_y[0]+16<=marker_y+marker_h);
    drawn_glyphs=0;draw_input_field(&keyboard,0);
    assert(glyph_y[0]==37&&cursor_y<=37&&cursor_y+cursor_height>=53);
    capture_bitmap_text=false;keyboard.config.unicode_font=NULL;
    keyboard.config.font=(LCDFont*)&bitmap;keyboard.config.font_size=10;
    hello_keyboard_model_init(&keyboard.model,"",0);keyboard.model.group=1;
    keyboard.model.pending='S';strcpy(keyboard.model.pending_text,"SS");
    keyboard.selection_marker.active=false;marker_draws=0;
    draw_group(&keyboard,1,105,69,2000);
    assert(marker_draws==1&&marker_w>=measure(NULL,"SS",2,kUTF8Encoding,0)+4);
    char title[102];memset(title,'x',95);memcpy(title+95,"🧡",5);
    hello_keyboard_set_title(&keyboard,title);assert(strlen(keyboard.config.title)==95);
    hello_keyboard_show_error(&keyboard,title,0,5000);assert(strlen(keyboard.error_storage)==95);
    char composed_title[110];memset(composed_title,'x',94);strcpy(composed_title+94,"é");
    hello_keyboard_set_title(&keyboard,composed_title);assert(strlen(keyboard.config.title)==94);
    test_group_viewport(&keyboard);
    test_approved_emoji_pack_geometry(&keyboard);
    test_equal_counts_and_independent_snapped_group_widths(&keyboard);
    test_commit_homing_and_early_reactivation(&keyboard);
    test_last_entry_matches_home_curve(&keyboard);
    test_non_endpoint_follow_unchanged();
    test_visible_rtl_entry_waits_for_direction_release(&keyboard);
    test_scroll_session_lifecycle(&keyboard);
    test_no_cross_layout_and_alphabet_reachability(&keyboard);
    test_complete_default_side_groups_and_long_overflow(&keyboard);
    test_immediate_crank_entry_and_face_button_isolation(&keyboard);
    test_crank_wrap_scroll_gate(&keyboard);
    test_wrap_gate_lifecycle(&keyboard);
    test_wrap_gate_waits_for_render(&keyboard);
    test_matching_release_retires_gate_and_empty_group(&keyboard);
    test_boundary_unlock_has_no_size_timer(&keyboard);
    test_rapid_face_selection_without_boundary(&keyboard);
    test_visible_hold_repeat_and_release_deadline(&keyboard);
    test_tap_then_hold_repeats_current_selection(&keyboard);
    test_rapid_taps_step_on_press_only(&keyboard);
    test_accepted_button_hold_resumes_after_rendered_wrap(&keyboard);
    test_fresh_blocked_hold_resumes_after_actual_boundary(&keyboard);
    test_button_scale_rendering_and_unmodified_crank(&keyboard);
    test_offscreen_entry_gate_repeat_and_crank(&keyboard);
    test_ten_digit_viewport_override(&keyboard);
    test_immediate_crank_entry_keeps_visible_cell(&keyboard);
    test_return_submission_and_native_icon(&keyboard);
    test_build68_face_input_trace(&keyboard);
    test_fitted_face_wraps_and_crank_boundary(&keyboard);
    test_face_boundary_gate(&keyboard);
    test_pressure_and_valid_direction_boundary_policy(&keyboard);
    test_last_entry_pressure_keeps_endpoint_motion(&keyboard);
    test_selection_native_size_and_height(&pd);
    test_jelly_both_inputs_and_group_entries(&keyboard);
    test_jelly_wrap_both_inputs(&keyboard);
    test_parallel_wrap_presentation(&keyboard);
    test_default_up_digits_tilde_and_scrolled_items(&keyboard);
    test_held_group_ink_and_immediate_release(&keyboard);
    test_white_canvas_and_dynamic_footer(&keyboard);
    test_idle_a_case_layer_chooser_and_base_pages(&keyboard);
    test_emoji_picker_views_selection_and_shared_font(&keyboard);
    test_numeric_full_layout_and_commit_validation(&keyboard);
    test_active_group_freezes_only_vertical_hover(&keyboard);
    test_input_cursor_stays_in_current_glyph_cell(&keyboard);
    test_cursor_blink_tracks_actual_keyboard_activity(&keyboard);
    test_crank_wrap_paints_full_endpoint_during_gate(&keyboard);
    test_dpad_cursor_uses_configured_button_repeat(&keyboard);
    test_held_a_crank_cursor_graphemes_and_lifecycle(&keyboard);
    test_active_footer_icons_and_shared_caption(&keyboard);
    test_external_labels_footer_and_view_reset(&keyboard);
    test_external_feedback_title_timing(&keyboard);
    test_idle_escape_label_tracks_physical_buttons(&keyboard);
    test_physical_return_editing_decision_all_types(&keyboard);
    test_editing_save_pending_transaction_and_rollback(&keyboard);
    test_editing_popup_and_password_hide_menu(&keyboard);
    test_emoji_input_instance_policy(&keyboard);
    test_disabled_emoji_up_uses_visible_symbols(&keyboard);
    test_offset_chord_restores_immediate_face_action(&keyboard);
    test_editing_popup_choices_have_no_hold_progress(&keyboard);
    test_instance_field_policies_large_text_and_completion(&keyboard);
    puts("keyboard art tests: cached UI art, grapheme masking/cursor and stable fallback geometry passed");
    return 0;
}
