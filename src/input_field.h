#ifndef HELLO_INPUT_FIELD_H
#define HELLO_INPUT_FIELD_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    HELLO_INPUT_PLAINTEXT = 0,
    HELLO_INPUT_MASKEDTEXT,
    HELLO_INPUT_CONTACT_NUMBER
} HelloInputType;

enum { HELLO_INPUT_TEXT_BYTES=4096,HELLO_INPUT_DEFAULT_MAX_CHARACTERS=160 };
typedef enum {
    HELLO_KEYBOARD_LENGTH_GRAPHEMES=0,
    HELLO_KEYBOARD_LENGTH_BYTES
} HelloKeyboardLengthUnit;
typedef struct {
    HelloInputType type;
    size_t min_length,max_length; // Inclusive; units are selected explicitly below.
    bool trim_for_length; // Count an edge-trimmed span; successful DONE applies that trim.
    size_t max_bytes; // Zero selects the independent 4096-byte storage ceiling.
    HelloKeyboardLengthUnit length_unit;
} HelloKeyboardFieldPolicy;
typedef struct {size_t offset,bytes,characters;} HelloInputFieldValue;
typedef enum {
    HELLO_FIELD_VALID=0,HELLO_FIELD_INVALID_POLICY,HELLO_FIELD_INVALID_TEXT,
    HELLO_FIELD_REQUIRED,HELLO_FIELD_TOO_SHORT,HELLO_FIELD_TOO_LONG,HELLO_FIELD_INVALID_TYPE
} HelloInputValidation;
HelloKeyboardFieldPolicy hello_input_field_default_policy(void);
bool hello_input_field_policy_valid(const HelloKeyboardFieldPolicy* policy);
HelloInputValidation hello_input_field_validate(const HelloKeyboardFieldPolicy* policy,
    const char* text,bool completion,HelloInputFieldValue* value);

typedef enum {
    HELLO_INPUT_ICON_NONE = 0,
    HELLO_INPUT_ICON_EYE,
    HELLO_INPUT_ICON_EYE_OFF
} HelloInputIcon;

typedef struct {
    HelloInputType type;
    bool masked;
} HelloInputField;

void hello_input_field_init(HelloInputField* field, HelloInputType type);
// Legacy typed-open adapter cap. Configured opens use their explicit policy.
size_t hello_input_field_max_length(HelloInputType type);
bool hello_input_field_can_toggle_mask(const HelloInputField* field);
bool hello_input_field_toggle_mask(HelloInputField* field);
bool hello_input_field_is_masked(const HelloInputField* field);
char hello_input_field_display_character(const HelloInputField* field, char character);
HelloInputIcon hello_input_field_icon(const HelloInputField* field);

#endif
