#include "emoji_set.h"
#include "unicode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char json[65536];
int main(void) {
    const BrbEmojiSet* set=brb_emoji_everyday_set();
    assert(set&&set->count==21&&set->cell_height==14&&set->pixels_size<1100);
    FILE* source=fopen("assets/emoji/sets/everyday.json","rb");assert(source);
    size_t length=fread(json,1,sizeof(json)-1,source);assert(!ferror(source)&&feof(source));
    fclose(source);json[length]=0;
    size_t inventory=0,pixel_bytes=0;
    for(size_t i=0;i<set->count;++i) {
        const BrbEmojiGlyph* glyph=&set->glyphs[i];
        assert(brb_emoji_find(set,glyph->utf8,glyph->utf8_length)==glyph);
        assert(brb_grapheme_next(glyph->utf8,glyph->utf8_length)==glyph->utf8_length);
        assert(!memcmp(set->keyboard_utf8+inventory,glyph->utf8,glyph->utf8_length));
        assert(brb_grapheme_next(set->keyboard_utf8+inventory,set->keyboard_bytes-inventory)==glyph->utf8_length);
        inventory+=glyph->utf8_length;
        char key[100];snprintf(key,sizeof(key),"\"id\": \"%s\"",glyph->id);
        const char* record=strstr(json,key);assert(record);
        const char* rows=strstr(record,"\"pixels\"");assert(rows);
        rows=strchr(rows,'[');assert(rows);++rows;
        const uint8_t* bits=brb_emoji_bitmap(set,glyph);assert(bits);
        // Compare every emitted C bit independently with the approved JSON.
        for(unsigned row=0;row<set->cell_height;++row) {
            rows=strchr(rows,'"');assert(rows);++rows;
            for(unsigned column=0;column<glyph->width;++column) {
                assert(rows[column]=='.'||rows[column]=='#');
                const size_t bit=(size_t)row*glyph->width+column;
                assert(!!(bits[bit/8]&(0x80u>>(bit%8)))==(rows[column]=='#'));
            }
            assert(rows[glyph->width]=='"');rows+=glyph->width+1;
        }
        pixel_bytes+=((size_t)glyph->width*set->cell_height+7)/8;
        for(size_t previous=0;previous<i;++previous)
            assert(strcmp(glyph->id,set->glyphs[previous].id));
    }
    assert(inventory==set->keyboard_bytes&&strlen(set->keyboard_utf8)==inventory);
    assert(pixel_bytes==set->pixels_size);
    assert(brb_grapheme_count(set->keyboard_utf8)==set->count);
    const BrbEmojiGlyph* heart=brb_emoji_find(set,"❤️",strlen("❤️"));
    assert(heart&&brb_emoji_find(set,"❤",strlen("❤"))==heart);
    assert(heart->width == 18 && heart->advance == 19);
    const uint8_t* heart_bits=brb_emoji_bitmap(set,heart);
    const size_t heart_center=6u*heart->width+9u;
    assert(!(heart_bits[heart_center/8]&(0x80u>>(heart_center%8))));
    assert(!brb_emoji_find(set,"❤️x",strlen("❤️x")));
    assert(!brb_emoji_find(set,"👍🏽",strlen("👍🏽")));
    assert(!brb_emoji_find(set,"👨‍👩‍👧‍👦",strlen("👨‍👩‍👧‍👦")));
    assert(!brb_emoji_find(NULL,"❤️",strlen("❤️")));
    assert(!brb_emoji_find(set,NULL,4)&&!brb_emoji_find(set,"",0));
    BrbEmojiSet subset=*set;subset.count=1;subset.alias_count=0;
    assert(brb_emoji_find(&subset,set->glyphs[0].utf8,set->glyphs[0].utf8_length));
    assert(!brb_emoji_find(&subset,set->glyphs[1].utf8,set->glyphs[1].utf8_length));
    BrbEmojiGlyph invalid=*heart;invalid.bitmap_offset=(uint16_t)set->pixels_size;
    assert(!brb_emoji_bitmap(set,&invalid));invalid=*heart;invalid.width=65;
    assert(!brb_emoji_bitmap(set,&invalid));invalid=*heart;invalid.advance=0;
    assert(!brb_emoji_bitmap(set,&invalid));subset.cell_height=15;
    assert(!brb_emoji_bitmap(&subset,heart));
    printf("emoji set: %zu approved glyphs, %zu exact packed bytes, whole clusters and replaceable subsets passed\n",set->count,pixel_bytes);
}
