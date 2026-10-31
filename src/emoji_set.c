#include "emoji_set.h"
#include "emoji_everyday_data.h"
#include <string.h>

const BrbEmojiSet* brb_emoji_everyday_set(void) { return &brb_emoji_everyday; }

const BrbEmojiGlyph* brb_emoji_find(const BrbEmojiSet* set,const char* cluster,size_t length) {
    if(!set||!set->glyphs||!cluster||!length)return NULL;
    for(size_t i=0;i<set->count;++i) {
        const BrbEmojiGlyph* glyph=&set->glyphs[i];
        if(glyph->utf8&&length==glyph->utf8_length&&!memcmp(cluster,glyph->utf8,length))return glyph;
    }
    if(set->aliases)for(size_t i=0;i<set->alias_count;++i) {
        const BrbEmojiAlias* alias=&set->aliases[i];
        if(alias->utf8&&alias->glyph_index<set->count&&length==alias->utf8_length&&
           !memcmp(cluster,alias->utf8,length))return &set->glyphs[alias->glyph_index];
    }
    return NULL;
}

const uint8_t* brb_emoji_bitmap(const BrbEmojiSet* set,const BrbEmojiGlyph* glyph) {
    if(!set||!glyph||!set->pixels||!set->cell_height||set->cell_height>14||
       !glyph->width||glyph->width>64||glyph->advance<glyph->width)return NULL;
    const size_t bytes=((size_t)glyph->width*set->cell_height+7)/8;
    if(glyph->bitmap_offset>set->pixels_size||bytes>set->pixels_size-glyph->bitmap_offset)return NULL;
    return set->pixels+glyph->bitmap_offset;
}
