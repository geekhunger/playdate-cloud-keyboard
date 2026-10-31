#ifndef BRB_EMOJI_SET_H
#define BRB_EMOJI_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Immutable, replaceable 1-bit artwork. No SDK, heap or file dependency.
// Bitmap bits run left-to-right, top-to-bottom, MSB first, without row padding.
// IDs are stable theme-local identifiers; UTF-8 sequences identify wire text.
typedef struct {
    const char* id;
    const char* utf8;
    uint16_t bitmap_offset;
    uint8_t utf8_length, width, advance;
} BrbEmojiGlyph;

typedef struct {
    const char* utf8;
    uint16_t glyph_index;
    uint8_t utf8_length;
} BrbEmojiAlias;

typedef struct {
    const char* id;
    uint32_t version;
    uint8_t cell_height, baseline;
    const BrbEmojiGlyph* glyphs;
    size_t count;
    const uint8_t* pixels;
    size_t pixels_size;
    const BrbEmojiAlias* aliases;
    size_t alias_count;
    const char* keyboard_utf8;
    size_t keyboard_bytes;
} BrbEmojiSet;

// All pointers remain owned by the set; callers can supply static custom sets.
const BrbEmojiSet* brb_emoji_everyday_set(void);
const BrbEmojiGlyph* brb_emoji_find(const BrbEmojiSet* set,const char* cluster,size_t length);
// Returns packed bits only for valid glyph metrics and an in-bounds bit range.
const uint8_t* brb_emoji_bitmap(const BrbEmojiSet* set,const BrbEmojiGlyph* glyph);
#endif
