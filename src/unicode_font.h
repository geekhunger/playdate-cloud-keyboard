#ifndef BRB_UNICODE_FONT_H
#define BRB_UNICODE_FONT_H
#include "pd_api.h"
#include "emoji_set.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BRB_FONT_PAGE_BYTES 8192
#define BRB_FONT_CACHED_PAGES 4
#define BRB_FONT_CACHED_GLYPHS 256
#define BRB_FONT_GLYPH_WAYS 4
// Persistent ~44 KiB context; keep it in application storage, not the device's
// small C stack. Initialize a fresh/closed context; close is idempotent.
typedef struct {
    PlaydateAPI* pd;
    SDFile* file;
    LCDBitmap* scratch;
    const BrbEmojiSet* emoji_set;
    bool emoji_set_configured;
    uint32_t clock;
    struct {uint32_t age;uint16_t number;bool valid;uint8_t data[BRB_FONT_PAGE_BYTES];}
        pages[BRB_FONT_CACHED_PAGES];
    struct {uint32_t age,codepoint;bool valid;uint8_t data[32];}
        glyphs[BRB_FONT_CACHED_GLYPHS];
} BrbUnicodeFont;

bool brb_unicode_font_init(BrbUnicodeFont* font,PlaydateAPI* pd);
void brb_unicode_font_close(BrbUnicodeFont* font);
// Fresh/zeroed contexts use Everyday even if the atlas could not initialize.
// Set a static theme/subset after init; explicit NULL selects atlas-only text.
void brb_unicode_font_set_emoji_set(BrbUnicodeFont* font,const BrbEmojiSet* set);
const BrbEmojiSet* brb_unicode_font_emoji_set(const BrbUnicodeFont* font);
// Draw immutable packed art directly without a font cache, allocations or IO.
// Restores the caller's draw mode; false means invalid art or no fillRect API.
bool brb_emoji_glyph_draw(PlaydateAPI* pd,const BrbEmojiSet* set,
    const BrbEmojiGlyph* glyph,int x,int y,bool white);
bool brb_text_has_unicode(const char* text,size_t length);
// ASCII and single scalars actually present in base use the existing font.
// Active-set art, missing glyphs and multi-scalar graphemes use custom drawing.
bool brb_text_cluster_needs_fallback(BrbUnicodeFont* font,LCDFont* base,
    const char* cluster,size_t length);
// Active art has theme metrics; remaining fallback glyphs use a 16x16 cell.
bool brb_unicode_font_draw(BrbUnicodeFont* font,const char* cluster,size_t length,int x,int y,bool white);
int brb_text_width(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length);
int brb_text_line_height(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length);
void brb_text_draw(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length,int x,int y,bool white);
typedef struct {size_t consumed,ink;} BrbTextLine;
BrbTextLine brb_text_wrap_line(BrbUnicodeFont* font,LCDFont* base,const char* text,size_t length,int width);
int brb_text_line_count(BrbUnicodeFont* font,LCDFont* base,const char* text,int width);
void brb_text_draw_wrapped(BrbUnicodeFont* font,LCDFont* base,const char* text,int x,int y,int width,bool white);
#endif
