#ifndef HELLO_KEYBOARD_LABELS_H
#define HELLO_KEYBOARD_LABELS_H

#include "unicode.h"
#include <stdbool.h>
#include <string.h>

// Labels are single-line UTF-8 and own at most this many bytes, excluding NUL.
// No allocation or SDK dependency is needed to configure or retain captions.
#define HELLO_KEYBOARD_MAX_LABEL 48

#define HELLO_KEYBOARD_LABEL_FIELDS(X) \
    X(clear, "clear") X(done, "done") X(cancel, "cancel") \
    X(delete_text, "Delete") X(left, "LEFT") X(right, "RIGHT") \
    X(hold_layer, "HOLD") X(layer_numbers, "123") X(layer_letters, "abc") \
    X(hold_lower, "Shift") X(hold_upper, "shift") \
    X(selection_text, "Cursor") X(select_text, "Select") X(layer_text, "Layer") \
    X(navigation_text, "Cursor / Layer") \
    X(hide, "Hide") X(no, "No") X(yes, "Yes") \
    X(cancel_question, "Done typing? What's next?") X(submit_question, "Submit input?") \
    X(discard, "Discard") X(save, "Save") \
    X(escape_text, "– Escape –") \
    X(picker_title, "Emoji sets") X(picker_select, "Choose set") X(use_set, "Use set")

/** Optional caller-owned source strings. NULL means the corresponding default;
 * empty text intentionally hides a caption. These captions do not change actions.
 * select_text captions the D-pad; selection_text captions the active A/B pair.
 * hold_lower/hold_upper caption the idle case action. layer_text selects the
 * default generated layer caption or a literal override; empty text hides it.
 * left/right remain for source
 * compatibility with callers that configured the earlier separate captions.
 */
typedef struct {
#define HELLO_KEYBOARD_LABEL_POINTER(name, default_value) const char* name;
    HELLO_KEYBOARD_LABEL_FIELDS(HELLO_KEYBOARD_LABEL_POINTER)
#undef HELLO_KEYBOARD_LABEL_POINTER
} HelloKeyboardLabels;

/** Fixed-capacity owned storage; initialized by hello_keyboard_labels_store. */
typedef struct {
#define HELLO_KEYBOARD_LABEL_BUFFER(name, default_value) char name[HELLO_KEYBOARD_MAX_LABEL + 1];
    HELLO_KEYBOARD_LABEL_FIELDS(HELLO_KEYBOARD_LABEL_BUFFER)
#undef HELLO_KEYBOARD_LABEL_BUFFER
} HelloKeyboardLabelStorage;

/** Return the canonical library captions; the returned pointers are static. */
static inline HelloKeyboardLabels hello_keyboard_default_labels(void) {
    return (HelloKeyboardLabels){
#define HELLO_KEYBOARD_LABEL_DEFAULT(name, default_value) .name = default_value,
        HELLO_KEYBOARD_LABEL_FIELDS(HELLO_KEYBOARD_LABEL_DEFAULT)
#undef HELLO_KEYBOARD_LABEL_DEFAULT
    };
}

// Read only a bounded prefix and truncate at a complete UTF-8 code point.
// Invalid/control-containing prefixes use the default for that field.
static inline bool hello_keyboard_label_copy(char* output, const char* input) {
    size_t at = 0;
    while (at < HELLO_KEYBOARD_MAX_LABEL && input[at]) {
        const unsigned char first = (unsigned char)input[at];
        const size_t bytes = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
            first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!bytes || first < 0x20 || first == 0x7f) return false;
        if (at + bytes > HELLO_KEYBOARD_MAX_LABEL) break;
        size_t available = 1;
        while (available < bytes && input[at + available]) ++available;
        uint32_t codepoint;
        if (available != bytes || brb_utf8_decode(input + at, bytes, &codepoint) != bytes)
            return false;
        memmove(output + at, input + at, bytes);
        at += bytes;
    }
    output[at] = '\0';
    return true;
}

/** Format the held-A layer caption without allocation. The default prefix
 * (or NULL) generates one letter per layer, with only the current letter
 * capitalized. An explicit nondefault prefix is a literal UTF-8 override;
 * an empty prefix hides it. A single layer always has no caption. Generated
 * words are bounded to the same 48-byte budget as other labels. Invalid
 * indices, text or insufficient capacity return false with empty output.
 */
static inline bool hello_keyboard_layer_caption(char* output,size_t capacity,
    const char* prefix,unsigned current,unsigned count) {
    if(!output||!capacity)return false;
    output[0]=0;
    if(!current||current>count)return false;
    if(count<=1)return true;
    const char* original=hello_keyboard_default_labels().layer_text;
    char label[HELLO_KEYBOARD_MAX_LABEL+1];
    if(!hello_keyboard_label_copy(label,prefix?prefix:original))return false;
    if(!label[0])return true;
    if(strcmp(label,original)) {
        const size_t length=strlen(label);
        if(length>=capacity)return false;
        memcpy(output,label,length+1);
        return true;
    }
    if(count>HELLO_KEYBOARD_MAX_LABEL||count>=capacity)return false;
    if(count<=5) {
        const char* word=count==2?"ly":count==3?"lyr":count==4?"layr":"layer";
        memcpy(output,word,count);
    } else {
        output[0]='l';
        memset(output+1,'a',count-4);
        memcpy(output+count-3,"yer",3);
    }
    output[current-1]=(char)(output[current-1]-'a'+'A');
    output[count]=0;
    return true;
}

/** Copy a bounded UTF-8 prefix for every field. NULL/invalid fields use defaults;
 * truncation never leaves an incomplete code point. No source pointer is kept.
 */
static inline void hello_keyboard_labels_store(HelloKeyboardLabelStorage* storage,
                                               const HelloKeyboardLabels* labels) {
    const HelloKeyboardLabels defaults = hello_keyboard_default_labels();
#define HELLO_KEYBOARD_LABEL_STORE(name, default_value) \
    if (!hello_keyboard_label_copy(storage->name, labels && labels->name ? labels->name : defaults.name)) \
        hello_keyboard_label_copy(storage->name, defaults.name);
    HELLO_KEYBOARD_LABEL_FIELDS(HELLO_KEYBOARD_LABEL_STORE)
#undef HELLO_KEYBOARD_LABEL_STORE
}

/** Borrow pointers valid for the lifetime of storage, until its next update. */
static inline HelloKeyboardLabels hello_keyboard_stored_labels(const HelloKeyboardLabelStorage* storage) {
    return (HelloKeyboardLabels){
#define HELLO_KEYBOARD_LABEL_STORED(name, default_value) .name = storage->name,
        HELLO_KEYBOARD_LABEL_FIELDS(HELLO_KEYBOARD_LABEL_STORED)
#undef HELLO_KEYBOARD_LABEL_STORED
    };
}

#undef HELLO_KEYBOARD_LABEL_FIELDS
#endif
