#include "input_field.h"
#include "unicode.h"

HelloKeyboardFieldPolicy hello_input_field_default_policy(void) {
    return (HelloKeyboardFieldPolicy){.type=HELLO_INPUT_PLAINTEXT,
        .max_length=HELLO_INPUT_DEFAULT_MAX_CHARACTERS,.max_bytes=HELLO_INPUT_TEXT_BYTES};
}

bool hello_input_field_policy_valid(const HelloKeyboardFieldPolicy* policy) {
    return policy&&(unsigned)policy->type<=HELLO_INPUT_CONTACT_NUMBER&&
        (unsigned)policy->length_unit<=HELLO_KEYBOARD_LENGTH_BYTES&&
        policy->min_length<=policy->max_length&&policy->max_length<=HELLO_INPUT_TEXT_BYTES&&
        policy->max_bytes<=HELLO_INPUT_TEXT_BYTES;
}

static bool unicode_space(uint32_t cp) {
    return (cp>=9&&cp<=13)||cp==32||cp==0x85||cp==0xa0||cp==0x1680||
        (cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000||cp==0xfeff;
}

HelloInputValidation hello_input_field_validate(const HelloKeyboardFieldPolicy* policy,
    const char* text,bool completion,HelloInputFieldValue* value) {
    if(value)*value=(HelloInputFieldValue){0};
    if(!hello_input_field_policy_valid(policy))return HELLO_FIELD_INVALID_POLICY;
    if(!text)return HELLO_FIELD_INVALID_TEXT;
    size_t length=0;
    while(length<=HELLO_INPUT_TEXT_BYTES&&text[length])++length;
    if(length>HELLO_INPUT_TEXT_BYTES)return HELLO_FIELD_TOO_LONG;
    size_t at=0,count=0,first=SIZE_MAX,last=0,first_index=0,last_index=0;
    while(at<length) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes)return HELLO_FIELD_INVALID_TEXT;
        bool whitespace=true;size_t inside=0;
        while(inside<bytes) {
            uint32_t cp;const size_t decoded=brb_utf8_decode(text+at+inside,bytes-inside,&cp);
            if(!decoded)return HELLO_FIELD_INVALID_TEXT;
            if(!unicode_space(cp))whitespace=false;
            inside+=decoded;
        }
        if(!whitespace) {
            if(first==SIZE_MAX){first=at;first_index=count;}
            last=at+bytes;last_index=count+1;
        }
        ++count;at+=bytes;
    }
    HelloInputFieldValue span={.bytes=length,.characters=count};
    if(policy->trim_for_length)span=first==SIZE_MAX?(HelloInputFieldValue){.offset=length}:
        (HelloInputFieldValue){.offset=first,.bytes=last-first,.characters=last_index-first_index};
    if(value)*value=span;
    if(policy->type==HELLO_INPUT_CONTACT_NUMBER)
        for(size_t i=0;i<span.bytes;++i)
            if(text[span.offset+i]<'0'||text[span.offset+i]>'9')return HELLO_FIELD_INVALID_TYPE;
    const size_t maximum_bytes=policy->max_bytes?policy->max_bytes:HELLO_INPUT_TEXT_BYTES;
    const size_t units=policy->length_unit==HELLO_KEYBOARD_LENGTH_BYTES?span.bytes:span.characters;
    if(span.bytes>maximum_bytes||units>policy->max_length)return HELLO_FIELD_TOO_LONG;
    if(completion&&units<policy->min_length)
        return !units?HELLO_FIELD_REQUIRED:HELLO_FIELD_TOO_SHORT;
    return HELLO_FIELD_VALID;
}
size_t hello_input_field_max_length(HelloInputType type) {
    return type==HELLO_INPUT_CONTACT_NUMBER ? 6 : 0;
}

void hello_input_field_init(HelloInputField* field, HelloInputType type) {
    field->type = type;
    field->masked = false;
}

bool hello_input_field_can_toggle_mask(const HelloInputField* field) {
    return field->type == HELLO_INPUT_MASKEDTEXT;
}

bool hello_input_field_toggle_mask(HelloInputField* field) {
    if (!hello_input_field_can_toggle_mask(field)) return false;
    field->masked = !field->masked;
    return true;
}

bool hello_input_field_is_masked(const HelloInputField* field) {
    return field->masked;
}

char hello_input_field_display_character(const HelloInputField* field, char character) {
    return field->masked && character != '\0' ? '*' : character;
}

HelloInputIcon hello_input_field_icon(const HelloInputField* field) {
    if (field->type != HELLO_INPUT_MASKEDTEXT) return HELLO_INPUT_ICON_NONE;
    return field->masked ? HELLO_INPUT_ICON_EYE_OFF : HELLO_INPUT_ICON_EYE;
}
