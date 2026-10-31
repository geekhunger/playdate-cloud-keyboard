#include "unicode_font.h"
#include "unicode.h"
#include "unicode_font_data.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(BrbUnicodeFont)<50*1024,"Unicode font cache must stay below 50 KiB");

static bool scratch_data(BrbUnicodeFont* font,int* stride,uint8_t** mask,uint8_t** data) {
    int width=0,height=0;*stride=0;*mask=NULL;*data=NULL;
    font->pd->graphics->getBitmapData(font->scratch,&width,&height,stride,mask,data);
    return width==16&&height==16&&*stride>=2&&*stride<=64&&*mask&&*data;
}
bool brb_unicode_font_init(BrbUnicodeFont* font,PlaydateAPI* pd) {
    if(!font)return false;
    memset(font,0,sizeof(*font));font->pd=pd;
    brb_unicode_font_set_emoji_set(font,brb_emoji_everyday_set());
    if(!pd||!pd->file||!pd->graphics||!pd->file->open||!pd->file->close||
       !pd->file->seek||!pd->file->read||!pd->graphics->newBitmap||
       !pd->graphics->freeBitmap||!pd->graphics->getBitmapData||
       !pd->graphics->setDrawMode||!pd->graphics->drawBitmap)return false;
    font->file=pd->file->open("unicode/glyphs.bin",kFileRead);
    if(!font->file)return false;
    font->scratch=pd->graphics->newBitmap(16,16,kColorClear);
    if(!font->scratch){brb_unicode_font_close(font);return false;}
    int stride;uint8_t *mask,*data;
    if(!scratch_data(font,&stride,&mask,&data)){brb_unicode_font_close(font);return false;}
    return true;
}
void brb_unicode_font_close(BrbUnicodeFont* font) {
    if(!font)return;
    if(font->scratch)font->pd->graphics->freeBitmap(font->scratch);
    if(font->file)font->pd->file->close(font->file);
    font->scratch=NULL;font->file=NULL;
    for(int i=0;i<BRB_FONT_CACHED_PAGES;i++)font->pages[i].valid=false;
    for(int i=0;i<BRB_FONT_CACHED_GLYPHS;i++)font->glyphs[i].valid=false;
}
void brb_unicode_font_set_emoji_set(BrbUnicodeFont* font,const BrbEmojiSet* set) {
    if(!font)return;
    font->emoji_set=set;font->emoji_set_configured=true;
}
const BrbEmojiSet* brb_unicode_font_emoji_set(const BrbUnicodeFont* font) {
    return font&&font->emoji_set_configured?font->emoji_set:brb_emoji_everyday_set();
}
static const BrbEmojiGlyph* emoji_for_cluster(const BrbUnicodeFont* font,const char* cluster,size_t length) {
    const BrbEmojiSet* set=brb_unicode_font_emoji_set(font);
    const BrbEmojiGlyph* found=brb_emoji_find(set,cluster,length);
    return brb_emoji_bitmap(set,found)?found:NULL;
}
bool brb_emoji_glyph_draw(PlaydateAPI* pd,const BrbEmojiSet* set,
    const BrbEmojiGlyph* glyph,int x,int y,bool white) {
    if(!pd||!pd->graphics||!pd->graphics->fillRect)return false;
    const uint8_t* bits=brb_emoji_bitmap(set,glyph);if(!bits)return false;
    const LCDBitmapDrawMode previous=pd->graphics->setDrawMode
        ?pd->graphics->setDrawMode(kDrawModeCopy):kDrawModeCopy;
    for(unsigned row=0;row<set->cell_height;++row) {
        for(unsigned column=0;column<glyph->width;) {
            size_t bit=(size_t)row*glyph->width+column;
            if(!(bits[bit/8]&(0x80u>>(bit%8)))){++column;continue;}
            const unsigned start=column++;
            while(column<glyph->width) {
                bit=(size_t)row*glyph->width+column;
                if(!(bits[bit/8]&(0x80u>>(bit%8))))break;
                ++column;
            }
            pd->graphics->fillRect(x+(int)start,y+(int)row,(int)(column-start),1,
                                          white?kColorWhite:kColorBlack);
        }
    }
    if(pd->graphics->setDrawMode)pd->graphics->setDrawMode(previous);
    return true;
}
bool brb_text_has_unicode(const char* text,size_t length) {
    if(!text)return false;
    for(size_t i=0;i<length;i++)if((unsigned char)text[i]>=0x80)return true;
    return false;
}
bool brb_text_cluster_needs_fallback(BrbUnicodeFont* font,LCDFont* base,
    const char* cluster,size_t length) {
    if(!cluster||!length||!brb_text_has_unicode(cluster,length))return false;
    if(emoji_for_cluster(font,cluster,length))return true;
    uint32_t cp;
    if(brb_utf8_decode(cluster,length,&cp)!=length||!base||!font||!font->pd||
       !font->pd->graphics||!font->pd->graphics->getFontPage||
       !font->pd->graphics->getPageGlyph)return true;
    LCDFontPage* page=font->pd->graphics->getFontPage(base,cp);
    if(!page)return true;
    LCDBitmap* bitmap=NULL;int advance=0;
    return font->pd->graphics->getPageGlyph(page,cp,&bitmap,&advance)==NULL;
}
static uint32_t cache_age(BrbUnicodeFont* font) {
    if(font->clock==UINT32_MAX) {
        for(int i=0;i<BRB_FONT_CACHED_PAGES;i++)font->pages[i].age=0;
        for(int i=0;i<BRB_FONT_CACHED_GLYPHS;i++)font->glyphs[i].age=0;
        font->clock=0;
    }
    return ++font->clock;
}
static const uint8_t* page_glyph(BrbUnicodeFont* font,uint32_t codepoint) {
    const size_t page=codepoint/256;
    if(!font->file||page>=sizeof(brb_font_pages)/sizeof(brb_font_pages[0])||brb_font_pages[page]==0xffff)return NULL;
    const uint16_t number=brb_font_pages[page];int victim=0;bool vacant=false;
    for(int i=0;i<BRB_FONT_CACHED_PAGES;i++) {
        if(font->pages[i].valid&&font->pages[i].number==number) {
            font->pages[i].age=cache_age(font);
            return font->pages[i].data+(codepoint%256)*32;
        }
        if(!font->pages[i].valid){victim=i;vacant=true;}
        else if(!vacant&&font->pages[i].age<font->pages[victim].age)victim=i;
    }
    font->pages[victim].valid=false;
    if(font->pd->file->seek(font->file,(int)number*BRB_FONT_PAGE_BYTES,SEEK_SET)!=0||
       font->pd->file->read(font->file,font->pages[victim].data,BRB_FONT_PAGE_BYTES)!=BRB_FONT_PAGE_BYTES)return NULL;
    font->pages[victim].valid=true;font->pages[victim].number=number;font->pages[victim].age=cache_age(font);
    return font->pages[victim].data+(codepoint%256)*32;
}
static const uint8_t* glyph(BrbUnicodeFont* font,uint32_t codepoint) {
    // Four-way set association avoids whole-page thrashing in mixed groups.
    const unsigned sets=BRB_FONT_CACHED_GLYPHS/BRB_FONT_GLYPH_WAYS;
    const unsigned start=((codepoint*UINT32_C(2654435761))>>26)%sets*BRB_FONT_GLYPH_WAYS;
    unsigned victim=start;bool vacant=false;
    for(unsigned i=start;i<start+BRB_FONT_GLYPH_WAYS;i++) {
        if(font->glyphs[i].valid&&font->glyphs[i].codepoint==codepoint) {
            font->glyphs[i].age=cache_age(font);return font->glyphs[i].data;
        }
        if(!font->glyphs[i].valid){victim=i;vacant=true;}
        else if(!vacant&&font->glyphs[i].age<font->glyphs[victim].age)victim=i;
    }
    const uint8_t* bits=page_glyph(font,codepoint);
    if(!bits)return NULL;
    memcpy(font->glyphs[victim].data,bits,32);
    font->glyphs[victim].codepoint=codepoint;font->glyphs[victim].age=cache_age(font);
    font->glyphs[victim].valid=true;return font->glyphs[victim].data;
}
static uint32_t emoji_glyph(const char* cluster,size_t length) {
    size_t lo=0,hi=sizeof(brb_emoji_glyphs)/sizeof(brb_emoji_glyphs[0]);
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        const size_t other=brb_emoji_glyphs[mid].length;
        int order=memcmp(cluster,brb_emoji_strings+brb_emoji_glyphs[mid].offset,length<other?length:other);
        if(!order)order=length<other?-1:length>other?1:0;
        if(order<0)hi=mid;
        else if(order>0)lo=mid+1;
        else return brb_emoji_glyphs[mid].glyph;
    }
    return 0;
}
static bool blank_scalar(uint32_t cp) {
    return cp==0xa0||cp==0x1680||(cp>=0x2000&&cp<=0x200f)||cp==0x2028||cp==0x2029||
        cp==0x202f||cp==0x205f||cp==0x2060||cp==0x3000||cp==0xfeff;
}
static bool fallback_ink(BrbUnicodeFont* font,const char* cluster,size_t length,uint8_t ink[32]) {
    bool base=false,found=false;
    for(size_t at=0;at<length;) {
        uint32_t cp;const size_t bytes=brb_utf8_decode(cluster+at,length-at,&cp);
        if(!bytes)return false;
        // Variation selectors have no ink; unsupported joined/tagged pictures
        // must not turn into a misleading pile of their component glyphs.
        if(cp==0x200d||(cp>=0xe0020&&cp<=0xe007f))return false;
        if((cp>=0xfe00&&cp<=0xfe0f)||(cp>=0xe0100&&cp<=0xe01ef)){at+=bytes;continue;}
        if(base) {
            char probe[5]={'A',0,0,0,0};memcpy(probe+1,cluster+at,bytes);
            // Only a combining continuation is safe to overlay in this atlas.
            // Conjuncts requiring a shaping engine use one replacement glyph.
            if(brb_grapheme_next(probe,bytes+1)!=bytes+1)return false;
        }
        base=true;at+=bytes;
        if(blank_scalar(cp)){found=true;continue;}
        const uint8_t* bits=glyph(font,cp);if(!bits)return false;
        bool nonempty=false;
        for(int i=0;i<32;i++){ink[i]|=bits[i];nonempty|=bits[i]!=0;}
        if(!nonempty)return false;
        found=true;
    }
    return found;
}
bool brb_unicode_font_draw(BrbUnicodeFont* font,const char* cluster,size_t length,int x,int y,bool white) {
    if(!font||!cluster||!length||
       brb_grapheme_next(cluster,length)!=length)return false;
    const BrbEmojiGlyph* art=emoji_for_cluster(font,cluster,length);
    if(art)return brb_emoji_glyph_draw(font->pd,brb_unicode_font_emoji_set(font),art,x,y,white);
    if(!font->file||!font->scratch)return false;
    uint8_t ink[32]={0};const uint32_t emoji=emoji_glyph(cluster,length);bool found=false;
    if(emoji) {
        const uint8_t* bits=glyph(font,emoji);
        if(bits){memcpy(ink,bits,32);found=true;}
    } else found=fallback_ink(font,cluster,length,ink);
    if(!found) {
        const uint8_t* bits=glyph(font,0xfffd);
        if(!bits)return false;
        memcpy(ink,bits,32);
    }
    int stride;uint8_t *mask,*data;
    if(!scratch_data(font,&stride,&mask,&data))return false;
    memset(mask,0,(size_t)stride*16);memset(data,0xff,(size_t)stride*16);
    for(int row=0;row<16;row++)for(int col=0;col<2;col++) {
        mask[row*stride+col]=ink[row*2+col];data[row*stride+col]=(uint8_t)~ink[row*2+col];
    }
    const LCDBitmapDrawMode previous=font->pd->graphics->setDrawMode(white?kDrawModeFillWhite:kDrawModeCopy);
    font->pd->graphics->drawBitmap(font->scratch,x,y,kBitmapUnflipped);
    font->pd->graphics->setDrawMode(previous);
    return true;
}
int brb_text_width(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length) {
    if(!font||!font->pd||!font->pd->graphics||!font->pd->graphics->getTextWidth||!text)return 0;
    if(!brb_text_has_unicode(text,length))return font->pd->graphics->getTextWidth(base,text,length,kUTF8Encoding,0);
    int width=0;
    for(size_t at=0;at<length;) {
        size_t bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;
        if(brb_text_cluster_needs_fallback(font,base,text+at,bytes)) {
            const BrbEmojiGlyph* art=emoji_for_cluster(font,text+at,bytes);
            const int advance=art?art->advance:16;
            if(width>INT_MAX-advance)return INT_MAX;
            width+=advance;at+=bytes;continue;
        }
        const size_t start=at;
        do {at+=bytes;if(at==length)break;bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;}
        while(!brb_text_cluster_needs_fallback(font,base,text+at,bytes));
        const int measured=font->pd->graphics->getTextWidth(base,text+start,at-start,kUTF8Encoding,0);
        if(measured>0){if(width>INT_MAX-measured)return INT_MAX;width+=measured;}
    }
    return width;
}
int brb_text_line_height(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length) {
    if(!font||!font->pd||!font->pd->graphics||!font->pd->graphics->getFontHeight)return 0;
    int height=font->pd->graphics->getFontHeight(base);
    if(height>=16||!text||!brb_text_has_unicode(text,length))return height;
    for(size_t at=0;at<length;) {
        size_t bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;
        if(brb_text_cluster_needs_fallback(font,base,text+at,bytes)) {
            const BrbEmojiGlyph* art=emoji_for_cluster(font,text+at,bytes);
            const int custom=art?brb_unicode_font_emoji_set(font)->cell_height:16;
            if(custom>height)height=custom;
        }
        at+=bytes;
    }
    return height;
}
void brb_text_draw(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length,int x,int y,bool white) {
    if(!font||!font->pd||!font->pd->graphics||!text||!font->pd->graphics->setFont||
       !font->pd->graphics->setDrawMode||!font->pd->graphics->getFontHeight||
       !font->pd->graphics->drawText||!font->pd->graphics->getTextWidth)return;
    font->pd->graphics->setFont(base);
    font->pd->graphics->setDrawMode(white?kDrawModeFillWhite:kDrawModeCopy);
    if(!brb_text_has_unicode(text,length)) {
        if(length)font->pd->graphics->drawText(text,length,kUTF8Encoding,x,y);
        font->pd->graphics->setDrawMode(kDrawModeCopy);
        return;
    }
    const int line=brb_text_line_height(font,base,text,length);
    const int text_y=y+(line-font->pd->graphics->getFontHeight(base))/2;
    for(size_t at=0;at<length;) {
        size_t bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;
        if(brb_text_cluster_needs_fallback(font,base,text+at,bytes)) {
            const BrbEmojiGlyph* art=emoji_for_cluster(font,text+at,bytes);
            const int glyph_height=art?brb_unicode_font_emoji_set(font)->cell_height:16;
            if(!brb_unicode_font_draw(font,text+at,bytes,x,y+(line-glyph_height)/2,white))
                font->pd->graphics->drawText("?",1,kUTF8Encoding,x,text_y);
            x+=art?art->advance:16;at+=bytes;continue;
        }
        const size_t start=at;
        do {at+=bytes;if(at==length)break;bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;}
        while(!brb_text_cluster_needs_fallback(font,base,text+at,bytes));
        font->pd->graphics->drawText(text+start,at-start,kUTF8Encoding,x,text_y);
        x+=font->pd->graphics->getTextWidth(base,text+start,at-start,kUTF8Encoding,0);
    }
    font->pd->graphics->setDrawMode(kDrawModeCopy);
}
BrbTextLine brb_text_wrap_line(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length,int width) {
    if(!text)return (BrbTextLine){0,0};
    size_t space=SIZE_MAX;
    for(size_t at=0;at<length;) {
        if(text[at]=='\n'||text[at]=='\r')
            return (BrbTextLine){at+1+(text[at]=='\r'&&at+1<length&&text[at+1]=='\n'),at};
        if(length-at>=3&&!memcmp(text+at,"\xe2\x80\xa8",3))return (BrbTextLine){at+3,at};
        if(length-at>=3&&!memcmp(text+at,"\xe2\x80\xa9",3))return (BrbTextLine){at+3,at};
        if(text[at]==' ')space=at;
        size_t bytes=brb_grapheme_next(text+at,length-at);if(!bytes)bytes=1;
        if(at&&brb_text_width(font,base,text,at+bytes)>width) {
            if(space!=SIZE_MAX&&space>0) {
                size_t consumed=space+1;while(consumed<length&&text[consumed]==' ')consumed++;
                return (BrbTextLine){consumed,space};
            }
            return (BrbTextLine){at,at};
        }
        at+=bytes;
    }
    size_t ink=length;while(ink&&text[ink-1]==' ')ink--;
    return (BrbTextLine){length,ink};
}
int brb_text_line_count(BrbUnicodeFont* font,LCDFont* base,const char* text,int width) {
    if(!text)return 1;
    const size_t length=strlen(text);int lines=0;
    for(size_t at=0;at<length;) {
        const BrbTextLine line=brb_text_wrap_line(font,base,text+at,length-at,width);
        if(!line.consumed)break;
        at+=line.consumed;lines++;
    }
    if(length&&(text[length-1]=='\n'||text[length-1]=='\r'||
       (length>=3&&(!memcmp(text+length-3,"\xe2\x80\xa8",3)||
                    !memcmp(text+length-3,"\xe2\x80\xa9",3)))))lines++;
    return lines?lines:1;
}
void brb_text_draw_wrapped(BrbUnicodeFont* font,LCDFont* base,const char* text,int x,int y,int width,bool white) {
    if(!text)return;
    const size_t length=strlen(text);const int height=brb_text_line_height(font,base,text,length);
    for(size_t at=0;at<length;) {
        const BrbTextLine line=brb_text_wrap_line(font,base,text+at,length-at,width);
        if(!line.consumed)break;
        const int line_height=brb_text_line_height(font,base,text+at,line.ink);
        brb_text_draw(font,base,text+at,line.ink,x,y+(height-line_height)/2,white);
        at+=line.consumed;y+=height;
    }
}
