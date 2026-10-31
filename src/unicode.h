#ifndef BRB_UNICODE_H
#define BRB_UNICODE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Strict, bounded UTF-8. Returns bytes consumed (including one for U+0000),
// or zero for invalid/truncated input.
size_t brb_utf8_decode(const char* text, size_t length, uint32_t* codepoint);
// Extended grapheme clusters according to Unicode 18.0.0 / UAX #29.
// The explicit length permits embedded NUL in conformance tests.
size_t brb_grapheme_next(const char* text, size_t length);
// NUL-terminated application strings: SIZE_MAX means malformed UTF-8.
size_t brb_grapheme_count(const char* text);
size_t brb_grapheme_floor(const char* text, size_t cursor);
size_t brb_grapheme_ceil(const char* text, size_t cursor);
size_t brb_grapheme_previous(const char* text, size_t cursor);
size_t brb_grapheme_following(const char* text, size_t cursor);
bool brb_utf8_valid(const char* text);
// Returns UTF-8 bytes written, or SIZE_MAX if the output would be truncated.
size_t brb_unicode_case(const char* input, bool upper, char* output, size_t capacity);
#endif
