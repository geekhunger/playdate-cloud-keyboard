#include "unicode_font.h"
#include "unicode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE* atlas;static int token,reads,opens,draws,frees,closes;
static uint8_t data[64],mask[64],last_mask[64];
static LCDBitmapDrawMode current_mode=kDrawModeCopy;
static bool fail_open,fail_bitmap,fail_read,short_read,allow_blank;
static int fail_seek,bitmap_stride=4;
static int native_font_token,native_page_token,native_glyph_token,native_bitmap_token;
static int measure_calls,text_calls;static char last_text[128];
static uint8_t drawn_pixels[14][64];
static int draw_origin_x,draw_origin_y,art_draws;
static bool capture_art;
static LCDColor expected_ink;
static void fill_rect(int x,int y,int width,int height,LCDColor ink) {
    art_draws++;
    assert(ink==kColorBlack||ink==kColorWhite);
    if(!capture_art)return;
    assert(ink==expected_ink&&height==1&&width>0);
    assert(x>=draw_origin_x&&x+width<=draw_origin_x+64);
    assert(y>=draw_origin_y&&y<draw_origin_y+14);
    for(int column=x;column<x+width;++column)drawn_pixels[y-draw_origin_y][column-draw_origin_x]=1;
}
static SDFile* open_atlas(const char* path,FileOptions options) {
    assert(!strcmp(path,"unicode/glyphs.bin")&&options==kFileRead);opens++;
    atlas=fail_open?NULL:fopen("Source/unicode/glyphs.bin","rb");return (SDFile*)atlas;
}
static int close_atlas(SDFile* file){assert(file==(SDFile*)atlas);closes++;int result=fclose(atlas);atlas=NULL;return result;}
static int seek_atlas(SDFile* file,int offset,int origin){return fail_seek?fail_seek:fseek((FILE*)file,offset,origin);}
static int read_atlas(SDFile* file,void* output,unsigned int length) {
    assert(length==BRB_FONT_PAGE_BYTES);reads++;
    return fail_read?-1:(int)fread(output,1,length-(short_read?1:0),(FILE*)file);
}
static LCDBitmap* create(int w,int h,LCDColor color) {
    assert(w==16&&h==16&&color==kColorClear);return fail_bitmap?NULL:(LCDBitmap*)&token;
}
static void release(LCDBitmap* bitmap){assert(bitmap==(LCDBitmap*)&token);frees++;}
static void pixels(LCDBitmap* bitmap,int* w,int* h,int* stride,uint8_t** m,uint8_t** d) {
    assert(bitmap==(LCDBitmap*)&token);*w=16;*h=16;*stride=bitmap_stride;*m=mask;*d=data;
}
static LCDBitmapDrawMode mode(LCDBitmapDrawMode value) {
    LCDBitmapDrawMode previous=current_mode;current_mode=value;return previous;
}
static void bitmap(LCDBitmap* value,int x,int y,LCDBitmapFlip flip) {
    (void)x;(void)y;assert(value==(LCDBitmap*)&token&&flip==kBitmapUnflipped);draws++;
    assert(current_mode==kDrawModeCopy||current_mode==kDrawModeFillWhite);
    bool ink=false;
    for(int row=0;row<16;row++)for(int byte=0;byte<2;byte++) {
        assert(data[row*4+byte]==(uint8_t)~mask[row*4+byte]);ink|=mask[row*4+byte]!=0;
    }
    assert(ink||allow_blank);memcpy(last_mask,mask,sizeof(mask));
}
static uint8_t height(LCDFont* font){(void)font;return 10;}
static LCDFontPage* native_page(LCDFont* font,uint32_t codepoint) {
    return font==(LCDFont*)&native_font_token&&codepoint==0xe9?(LCDFontPage*)&native_page_token:NULL;
}
static LCDFontGlyph* native_glyph(LCDFontPage* page,uint32_t codepoint,LCDBitmap** bitmap,int* advance) {
    assert(page==(LCDFontPage*)&native_page_token&&codepoint==0xe9);
    *bitmap=(LCDBitmap*)&native_bitmap_token;*advance=4;return (LCDFontGlyph*)&native_glyph_token;
}
static int measure(LCDFont* font,const void* text,size_t length,PDStringEncoding encoding,int tracking) {
    assert(encoding==kUTF8Encoding&&tracking==0);measure_calls++;
    if(font!=(LCDFont*)&native_font_token)return (int)length*5;
    const char* input=text;int width=0;
    for(size_t at=0;at<length;) {
        uint32_t cp;size_t bytes=brb_utf8_decode(input+at,length-at,&cp);assert(bytes);
        width+=cp==0xe9?4:5;at+=bytes;
    }
    // Model a kerning pair which must survive the Unicode/native boundary.
    for(size_t at=0;at+strlen("AéV")<=length;at++)
        if(!memcmp(input+at,"AéV",strlen("AéV")))width-=2;
    return width;
}
static void set_font(LCDFont* font){(void)font;}
static int text(const void* value,size_t length,PDStringEncoding encoding,int x,int y) {
    (void)x;(void)y;assert(encoding==kUTF8Encoding);text_calls++;
    assert(length<sizeof(last_text));memcpy(last_text,value,length);last_text[length]=0;return 0;
}
static void character(uint32_t cp,char out[5]) {
    assert(cp>=0x800&&cp<0x10000);
    out[0]=(char)(0xe0|(cp>>12));out[1]=(char)(0x80|((cp>>6)&63));
    out[2]=(char)(0x80|(cp&63));out[3]=0;
}

int main(void) {
    struct playdate_graphics graphics={0};struct playdate_file files={0};PlaydateAPI pd={0};
    pd.graphics=&graphics;pd.file=&files;
    files.open=open_atlas;files.close=close_atlas;files.seek=seek_atlas;files.read=read_atlas;
    graphics.newBitmap=create;graphics.freeBitmap=release;graphics.getBitmapData=pixels;
    graphics.setDrawMode=mode;graphics.drawBitmap=bitmap;graphics.getFontHeight=height;
    graphics.getTextWidth=measure;graphics.setFont=set_font;graphics.drawText=text;graphics.fillRect=fill_rect;
    static BrbUnicodeFont font;
    assert(brb_unicode_font_init(&font,&pd));assert(opens==1&&reads==0);
    const BrbEmojiSet* approved=brb_emoji_everyday_set();
    const int art_reads=reads,art_opens=opens;
    draw_origin_x=7;draw_origin_y=9;
    for(size_t i=0;i<approved->count;++i) {
        const BrbEmojiGlyph* item=&approved->glyphs[i];
        const uint8_t* bits=brb_emoji_bitmap(approved,item);assert(bits);
        assert(brb_text_width(&font,NULL,item->utf8,item->utf8_length)==item->advance);
        assert(brb_text_line_height(&font,NULL,item->utf8,item->utf8_length)==14);
        assert(brb_text_cluster_needs_fallback(&font,NULL,item->utf8,item->utf8_length));
        for(int white=0;white<2;++white) {
            memset(drawn_pixels,0,sizeof(drawn_pixels));capture_art=true;
            expected_ink=white?kColorWhite:kColorBlack;
            assert(brb_unicode_font_draw(&font,item->utf8,item->utf8_length,7,9,white!=0));
            capture_art=false;
            for(unsigned row=0;row<14;++row)for(unsigned column=0;column<64;++column) {
                const size_t bit=(size_t)row*item->width+column;
                const bool expected=column<item->width&&(bits[bit/8]&(0x80u>>(bit%8)));
                assert(drawn_pixels[row][column]==expected);
            }
        }
    }
    assert(reads==art_reads&&opens==art_opens&&draws==0&&art_draws>0);
    assert(brb_text_width(&font,NULL,"❤️",strlen("❤️"))==brb_text_width(&font,NULL,"❤",strlen("❤")));
    const BrbEmojiGlyph* first=&approved->glyphs[0];
    const BrbEmojiGlyph* second=&approved->glyphs[1];
    BrbTextLine art_line=brb_text_wrap_line(&font,NULL,approved->keyboard_utf8,approved->keyboard_bytes,first->advance);
    assert(art_line.ink==first->utf8_length&&art_line.consumed==first->utf8_length);
    art_line=brb_text_wrap_line(&font,NULL,approved->keyboard_utf8,approved->keyboard_bytes,first->advance+second->advance);
    assert(art_line.ink==first->utf8_length+second->utf8_length);
    assert(brb_text_line_height(&font,NULL,"A❤️字",strlen("A❤️字"))==16);
    const BrbEmojiGlyph* heart=brb_emoji_find(approved,"❤️",strlen("❤️"));
    assert(heart&&brb_text_width(&font,NULL,"A❤️B",strlen("A❤️B"))==10+heart->advance);
    // Theme art remains available in a zeroed/closed context without an atlas.
    static BrbUnicodeFont no_atlas;
    no_atlas.pd=&pd;
    const int atlas_reads=reads,atlas_opens=opens;
    assert(brb_unicode_font_draw(&no_atlas,first->utf8,first->utf8_length,0,0,false));
    assert(brb_text_line_height(&no_atlas,NULL,first->utf8,first->utf8_length)==14);
    BrbEmojiSet subset=*approved;subset.count=1;subset.alias_count=0;
    brb_unicode_font_set_emoji_set(&no_atlas,&subset);
    assert(brb_text_width(&no_atlas,NULL,first->utf8,first->utf8_length)==first->advance);
    assert(brb_text_width(&no_atlas,NULL,second->utf8,second->utf8_length)==16);
    assert(!brb_unicode_font_draw(&no_atlas,second->utf8,second->utf8_length,0,0,false));
    brb_unicode_font_set_emoji_set(&no_atlas,NULL);
    assert(brb_text_width(&no_atlas,NULL,first->utf8,first->utf8_length)==16);
    assert(!brb_unicode_font_draw(&no_atlas,first->utf8,first->utf8_length,0,0,false));
    assert(reads==atlas_reads&&opens==atlas_opens);
    const char* emoji[]={"😀","👨‍👩‍👧‍👦","🇩🇪","👍🏽","👩‍💻","❤️","1️⃣","é","é","字","Ω"};
    for(size_t i=0;i<sizeof(emoji)/sizeof(emoji[0]);i++) {
        assert(brb_unicode_font_draw(&font,emoji[i],strlen(emoji[i]),0,0,false));
        const int before=reads;uint8_t previous[64];memcpy(previous,last_mask,64);
        assert(brb_unicode_font_draw(&font,emoji[i],strlen(emoji[i]),0,0,true));
        assert(reads==before&&!memcmp(previous,last_mask,64));
        assert(current_mode==kDrawModeCopy);
    }
    const char* family="ab👨‍👩‍👧‍👦cd";const int before=reads;
    assert(brb_text_width(&font,NULL,family,strlen(family))==36);
    assert(reads==before); // Measurement never reads font pages.
    BrbTextLine line=brb_text_wrap_line(&font,NULL,family,strlen(family),26);
    assert(line.consumed==2+strlen("👨‍👩‍👧‍👦")&&line.ink==line.consumed);
    assert(brb_text_line_count(&font,NULL,family,26)==2);
    assert(brb_text_line_height(&font,NULL,family,strlen(family))==16);
    assert(brb_text_line_height(&font,NULL,"hello",5)==10);
    brb_text_draw_wrapped(&font,NULL,family,0,0,26,false);
    assert(opens==1&&sizeof(font)<50*1024&&frees==0);
    // Five maximally populated custom groups can span far more than four pages.
    // Their warmed glyphs must stay hot without per-frame atlas I/O.
    char items[80][5];
    for(int i=0;i<80;i++) {
        character(0x4e00+(uint32_t)i*256,items[i]);
        assert(brb_unicode_font_draw(&font,items[i],3,0,0,false));
    }
    const int warmed_reads=reads;
    for(int frame=0;frame<50;frame++)for(int i=0;i<80;i++)
        assert(brb_unicode_font_draw(&font,items[i],3,0,0,(frame&1)!=0));
    assert(reads==warmed_reads);
    font.clock=UINT32_MAX;
    assert(brb_unicode_font_draw(&font,items[0],3,0,0,false));
    assert(reads==warmed_reads&&font.clock==1);
    for(int i=0;i<80;i++)assert(brb_unicode_font_draw(&font,items[i],3,0,0,false));
    assert(reads==warmed_reads);
    // Unsupported joined pictures are one replacement, not false overprints.
    assert(brb_unicode_font_draw(&font,"�",strlen("�"),0,0,false));
    uint8_t replacement[64];memcpy(replacement,last_mask,64);
    assert(brb_unicode_font_draw(&font,"😀‍😀",strlen("😀‍😀"),0,0,false));
    assert(!memcmp(replacement,last_mask,64));
    const int unchanged_reads=reads,unchanged_draws=draws;
    assert(!brb_unicode_font_draw(&font,"\xf0\x9f",2,0,0,false));
    assert(!brb_unicode_font_draw(&font,"ab",2,0,0,false));
    assert(!brb_unicode_font_draw(&font,"",0,0,0,false));
    assert(reads==unchanged_reads&&draws==unchanged_draws);
    allow_blank=true;
    assert(brb_unicode_font_draw(&font,"\xe2\x80\x83",3,0,0,false));
    allow_blank=false;
    assert(brb_text_line_count(&font,NULL,"a\r\nb",100)==2);
    assert(brb_text_line_count(&font,NULL,"a\n",100)==2);
    assert(brb_text_line_count(&font,NULL,"a\xe2\x80\xa8" "b",100)==2);
    line=brb_text_wrap_line(&font,NULL,"a\r\nb",4,100);
    assert(line.ink==1&&line.consumed==3);
    // Preserve a supported non-ASCII glyph in the existing font, including its
    // metrics and kerning with the surrounding native characters.
    LCDFont* native=(LCDFont*)&native_font_token;
    assert(brb_text_cluster_needs_fallback(&font,native,"é",strlen("é")));
    graphics.getFontPage=native_page;graphics.getPageGlyph=native_glyph;
    assert(!brb_text_cluster_needs_fallback(&font,native,"é",strlen("é")));
    assert(!brb_text_cluster_needs_fallback(&font,native,"A",1));
    assert(brb_text_cluster_needs_fallback(&font,native,"字",strlen("字")));
    assert(brb_text_cluster_needs_fallback(&font,native,"é",strlen("é")));
    const int native_reads=reads,native_draws=draws,native_measures=measure_calls;
    assert(brb_text_width(&font,native,"AéV",strlen("AéV"))==12);
    assert(measure_calls==native_measures+1);
    assert(brb_text_line_height(&font,native,"AéV",strlen("AéV"))==10);
    const int native_text_calls=text_calls;
    brb_text_draw(&font,native,"AéV",strlen("AéV"),0,0,false);
    assert(text_calls==native_text_calls+1&&!strcmp(last_text,"AéV"));
    assert(reads==native_reads&&draws==native_draws);
    assert(brb_text_width(&font,native,"AéV字AéV",strlen("AéV字AéV"))==40);
    assert(brb_text_line_height(&font,native,"AéV字AéV",strlen("AéV字AéV"))==16);
    graphics.getPageGlyph=NULL;
    assert(brb_text_width(&font,native,"é",strlen("é"))==16);
    assert(brb_text_line_height(&font,native,"é",strlen("é"))==16);
    assert(reads==native_reads);
    graphics.getFontPage=NULL;
    brb_unicode_font_close(&font);assert(frees==1&&!font.file&&!font.scratch);
    brb_unicode_font_close(&font);brb_unicode_font_close(NULL);assert(frees==1);
    assert(!brb_unicode_font_init(NULL,&pd));assert(!brb_unicode_font_init(&font,NULL));
    fail_open=true;assert(!brb_unicode_font_init(&font,&pd));fail_open=false;
    const int before_close=closes;
    fail_bitmap=true;assert(!brb_unicode_font_init(&font,&pd));fail_bitmap=false;
    assert(closes==before_close+1&&!font.file&&!font.scratch);
    bitmap_stride=65;assert(!brb_unicode_font_init(&font,&pd));bitmap_stride=4;
    assert(closes==before_close+2&&!font.file&&!font.scratch);
    for(int failure=0;failure<4;failure++) {
        assert(brb_unicode_font_init(&font,&pd));
        fail_read=failure==0;short_read=failure==1;fail_seek=failure==2?-1:failure==3?1:0;
        const int before_draw=draws;
        assert(!brb_unicode_font_draw(&font,"😀",strlen("😀"),0,0,false));
        assert(draws==before_draw);
        fail_read=short_read=false;fail_seek=0;
        assert(brb_unicode_font_draw(&font,"😀",strlen("😀"),0,0,false));
        brb_unicode_font_close(&font);
    }
    puts("unicode atlas: emoji sequences, both inks, bounded lazy cache, grapheme wrapping and unchanged ASCII passed");
}
