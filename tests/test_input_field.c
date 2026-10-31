#include "input_field.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t family_text(char* output,size_t count) {
    const char* family="👨‍👩‍👧‍👦";const size_t bytes=strlen(family);
    assert(count*bytes<=HELLO_INPUT_TEXT_BYTES);
    for(size_t i=0;i<count;i++)memcpy(output+i*bytes,family,bytes);
    output[count*bytes]=0;return count*bytes;
}
static void test_field_policy_unicode_limits_and_trim(void) {
    HelloKeyboardFieldPolicy policy=hello_input_field_default_policy();
    policy.min_length=1;policy.trim_for_length=true;
    HelloInputFieldValue value;
    char text[HELLO_INPUT_TEXT_BYTES+2];
    for(size_t count=159;count<=161;count++) {
        const size_t bytes=family_text(text,count);
        assert(hello_input_field_validate(&policy,text,true,&value)==(count<=160?HELLO_FIELD_VALID:HELLO_FIELD_TOO_LONG));
        assert(value.characters==count&&value.bytes==bytes&&!value.offset);
        if(count==160)assert(bytes==4000);
    }
    char padded[HELLO_INPUT_TEXT_BYTES+2];strcpy(padded,"\u00a0");family_text(padded+2,160);strcat(padded,"\u3000");
    assert(hello_input_field_validate(&policy,padded,true,&value)==HELLO_FIELD_VALID);
    assert(value.offset==2&&value.characters==160&&value.bytes==4000);
    assert(hello_input_field_validate(&policy,"",false,&value)==HELLO_FIELD_VALID);
    assert(hello_input_field_validate(&policy,"\t\u00a0\u3000",true,&value)==HELLO_FIELD_REQUIRED&&!value.bytes&&!value.characters);
    assert(hello_input_field_validate(&policy," \u0301",true,&value)==HELLO_FIELD_VALID&&value.bytes==3&&!value.offset&&value.characters==1);
    assert(hello_input_field_validate(&policy,"\xf0\x9f",true,&value)==HELLO_FIELD_INVALID_TEXT);
    policy.min_length=3;
    assert(hello_input_field_validate(&policy,"éé",false,&value)==HELLO_FIELD_VALID);
    assert(hello_input_field_validate(&policy,"éé",true,&value)==HELLO_FIELD_TOO_SHORT);
    // The same160 graphemes may occupy either4096 or4097 bytes.
    size_t at=family_text(text,159);text[at++]='a';
    for(int i=0;i<60;i++){memcpy(text+at,"́",2);at+=2;}text[at]=0;
    assert(at==4096&&hello_input_field_validate(&policy,text,true,&value)==HELLO_FIELD_VALID&&value.characters==160);
    at-=2;memcpy(text+at,"⃝",3);at+=3;text[at]=0;
    assert(at==4097&&hello_input_field_validate(&policy,text,true,&value)==HELLO_FIELD_TOO_LONG);
}
static void test_field_policy_byte_units_and_atomic_validation(void) {
    HelloKeyboardFieldPolicy policy={.type=HELLO_INPUT_MASKEDTEXT,.min_length=8,.max_length=72,
        .length_unit=HELLO_KEYBOARD_LENGTH_BYTES};
    HelloInputFieldValue value;
    assert(hello_input_field_validate(&policy,"ééé",true,&value)==HELLO_FIELD_TOO_SHORT);
    assert(hello_input_field_validate(&policy,"éééé",true,&value)==HELLO_FIELD_VALID&&value.bytes==8&&value.characters==4);
    char password[80];for(int i=0;i<37;i++)memcpy(password+2*i,"é",2);password[74]=0;
    assert(hello_input_field_validate(&policy,password,true,&value)==HELLO_FIELD_TOO_LONG);
    password[72]=0;assert(hello_input_field_validate(&policy,password,true,&value)==HELLO_FIELD_VALID);
    policy=(HelloKeyboardFieldPolicy){.type=HELLO_INPUT_CONTACT_NUMBER,.min_length=6,.max_length=6};
    assert(hello_input_field_validate(&policy,"12345",false,&value)==HELLO_FIELD_VALID);
    assert(hello_input_field_validate(&policy,"12345",true,&value)==HELLO_FIELD_TOO_SHORT);
    assert(hello_input_field_validate(&policy,"123456",true,&value)==HELLO_FIELD_VALID);
    assert(hello_input_field_validate(&policy,"1234567",true,&value)==HELLO_FIELD_TOO_LONG);
    assert(hello_input_field_validate(&policy,"１２３４５６",true,&value)==HELLO_FIELD_INVALID_TYPE);
    assert(hello_input_field_validate(&policy,"12345a",true,&value)==HELLO_FIELD_INVALID_TYPE);
    policy=(HelloKeyboardFieldPolicy){0};assert(hello_input_field_policy_valid(&policy));
    assert(hello_input_field_validate(&policy,"",true,&value)==HELLO_FIELD_VALID);
    assert(hello_input_field_validate(&policy,"a",false,&value)==HELLO_FIELD_TOO_LONG);
    policy.min_length=1;assert(!hello_input_field_policy_valid(&policy));
    assert(hello_input_field_validate(&policy,"",true,&value)==HELLO_FIELD_INVALID_POLICY);
    policy=hello_input_field_default_policy();policy.max_bytes=HELLO_INPUT_TEXT_BYTES+1;
    assert(!hello_input_field_policy_valid(&policy));
    policy=hello_input_field_default_policy();policy.length_unit=(HelloKeyboardLengthUnit)99;
    assert(!hello_input_field_policy_valid(&policy));
}

int main(void) {
    test_field_policy_unicode_limits_and_trim();
    test_field_policy_byte_units_and_atomic_validation();
    HelloInputField plain;
    hello_input_field_init(&plain, HELLO_INPUT_PLAINTEXT);
    assert(!hello_input_field_can_toggle_mask(&plain));
    assert(!hello_input_field_toggle_mask(&plain));
    assert(hello_input_field_display_character(&plain, 'a') == 'a');
    assert(hello_input_field_icon(&plain) == HELLO_INPUT_ICON_NONE);

    HelloInputField secret;
    hello_input_field_init(&secret, HELLO_INPUT_MASKEDTEXT);
    assert(hello_input_field_can_toggle_mask(&secret));
    assert(!hello_input_field_is_masked(&secret));
    assert(hello_input_field_display_character(&secret, 'a') == 'a');
    assert(hello_input_field_icon(&secret) == HELLO_INPUT_ICON_EYE);
    assert(hello_input_field_toggle_mask(&secret));
    assert(hello_input_field_is_masked(&secret));
    assert(hello_input_field_display_character(&secret, 'a') == '*');
    assert(hello_input_field_icon(&secret) == HELLO_INPUT_ICON_EYE_OFF);
    assert(hello_input_field_toggle_mask(&secret));
    assert(!hello_input_field_is_masked(&secret));
    assert(hello_input_field_icon(&secret) == HELLO_INPUT_ICON_EYE);

    puts("input field tests: OK");
    return 0;
}
