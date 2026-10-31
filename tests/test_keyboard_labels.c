#include "keyboard_labels.h"
#include <assert.h>
#include <stdio.h>

static void test_defaults_and_independent_overrides(void) {
    HelloKeyboardLabelStorage owned;
    hello_keyboard_labels_store(&owned,NULL);
    HelloKeyboardLabels defaults=hello_keyboard_default_labels();
    assert(!strcmp(owned.clear,defaults.clear));
    assert(!strcmp(owned.done,"done")&&!strcmp(owned.cancel,"cancel"));
    assert(!strcmp(owned.delete_text,"Delete"));
    assert(!strcmp(owned.left,"LEFT")&&!strcmp(owned.right,"RIGHT"));
    assert(!strcmp(owned.selection_text,"Cursor"));
    assert(!strcmp(owned.escape_text,"– Escape –")&&!strcmp(owned.submit_question,"Submit input?"));
    assert(!strcmp(owned.select_text,"Select"));
    assert(!strcmp(owned.hold_layer,"HOLD"));
    assert(!strcmp(owned.layer_numbers,"123")&&!strcmp(owned.layer_letters,"abc"));
    assert(!strcmp(owned.hold_lower,"Shift")&&!strcmp(owned.hold_upper,"shift"));
    assert(!strcmp(owned.layer_text,"Layer"));
    assert(!strcmp(owned.navigation_text,"Cursor / Layer"));
    assert(!strcmp(owned.cancel_question,"Done typing? What's next?"));
    char caller_done[]="Save";
    HelloKeyboardLabels labels={.done=caller_done,.cancel="",.left="Zurück"};
    hello_keyboard_labels_store(&owned,&labels);
    caller_done[0]='X';
    HelloKeyboardLabels copied=hello_keyboard_stored_labels(&owned);
    assert(!strcmp(copied.done,"Save"));
    assert(copied.cancel[0]==0&&!strcmp(copied.clear,"clear"));
    assert(!strcmp(copied.left,"Zurück"));
    hello_keyboard_labels_store(&owned,NULL);
    assert(!strcmp(owned.done,"done")&&!strcmp(owned.cancel,"cancel"));
}

static void test_selection_caption_ownership_and_fallback(void) {
    HelloKeyboardLabelStorage owned;
    char caption[]="Vor / Zurück";
    char select[]="Wählen";
    HelloKeyboardLabels labels={.selection_text=caption,.select_text=select,
        .left="Legacy left",.right="Legacy right"};
    hello_keyboard_labels_store(&owned,&labels);
    caption[0]='X';
    select[0]='X';
    const HelloKeyboardLabels copied=hello_keyboard_stored_labels(&owned);
    assert(!strcmp(copied.selection_text,"Vor / Zurück"));
    assert(!strcmp(copied.select_text,"Wählen"));
    assert(!strcmp(copied.left,"Legacy left")&&!strcmp(copied.right,"Legacy right"));
    labels.selection_text="";
    labels.select_text="";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!owned.selection_text[0]);
    assert(!owned.select_text[0]);
    labels.selection_text="bad\ncaption";
    labels.select_text="bad\ncaption";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!strcmp(owned.selection_text,"Cursor"));
    assert(!strcmp(owned.select_text,"Select"));
    hello_keyboard_labels_store(&owned,NULL);
    assert(!strcmp(owned.selection_text,"Cursor"));
    assert(!strcmp(owned.select_text,"Select"));
}

static void test_layer_caption_ownership_and_fallback(void) {
    HelloKeyboardLabelStorage owned;
    char layer[]="Ebene %s";
    HelloKeyboardLabels labels={.layer_text=layer};
    hello_keyboard_labels_store(&owned,&labels);
    layer[0]='X';
    assert(!strcmp(hello_keyboard_stored_labels(&owned).layer_text,"Ebene %s"));
    labels.layer_text="";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!owned.layer_text[0]);
    labels.layer_text="bad\ncaption";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!strcmp(owned.layer_text,"Layer"));
    hello_keyboard_labels_store(&owned,NULL);
    assert(!strcmp(owned.layer_text,"Layer"));
}

static void test_navigation_caption(void) {
    HelloKeyboardLabelStorage owned;
    char navigation[]="Cursor / Ebene";
    HelloKeyboardLabels labels={.navigation_text=navigation};
    hello_keyboard_labels_store(&owned,&labels);
    navigation[0]='X';
    assert(!strcmp(hello_keyboard_stored_labels(&owned).navigation_text,"Cursor / Ebene"));
    labels.navigation_text="";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!owned.navigation_text[0]);
    labels.navigation_text="bad\ntext";
    hello_keyboard_labels_store(&owned,&labels);
    assert(!strcmp(owned.navigation_text,"Cursor / Layer"));
}

static void test_generated_layer_caption(void) {
    char output[HELLO_KEYBOARD_MAX_LABEL+1];
    assert(hello_keyboard_layer_caption(output,sizeof(output),NULL,1,1)&&!output[0]);
    assert(hello_keyboard_layer_caption(output,sizeof(output),"Custom",1,1)&&!output[0]);
    const struct {unsigned current,count;const char* expected;} examples[]={
        {1,2,"Ly"},{2,2,"lY"},{2,3,"lYr"},{3,4,"laYr"},
        {5,5,"layeR"},{3,6,"laAyer"},{4,7,"laaAyer"}
    };
    for(size_t i=0;i<sizeof(examples)/sizeof(examples[0]);++i) {
        assert(hello_keyboard_layer_caption(output,sizeof(output),"Layer",examples[i].current,examples[i].count));
        assert(!strcmp(output,examples[i].expected));
    }
    for(unsigned count=2;count<=HELLO_KEYBOARD_MAX_LABEL;++count)
        for(unsigned current=1;current<=count;++current) {
            assert(hello_keyboard_layer_caption(output,sizeof(output),NULL,current,count));
            assert(strlen(output)==count);
            for(unsigned i=0;i<count;++i)
                assert(i==current-1?(output[i]>='A'&&output[i]<='Z'):(output[i]>='a'&&output[i]<='z'));
        }
    assert(hello_keyboard_layer_caption(output,sizeof(output),"Ebene %s",2,3));
    assert(!strcmp(output,"Ebene %s"));
    assert(hello_keyboard_layer_caption(output,sizeof(output),"",2,3)&&!output[0]);
    assert(!hello_keyboard_layer_caption(output,sizeof(output),NULL,0,3)&&!output[0]);
    assert(!hello_keyboard_layer_caption(output,sizeof(output),NULL,4,3)&&!output[0]);
    assert(!hello_keyboard_layer_caption(output,sizeof(output),NULL,1,49)&&!output[0]);
    assert(!hello_keyboard_layer_caption(output,3,NULL,2,3)&&!output[0]);
    assert(hello_keyboard_layer_caption(output,4,NULL,2,3)&&!strcmp(output,"lYr"));
    assert(!hello_keyboard_layer_caption(output,4,"Custom",2,3)&&!output[0]);
    assert(!hello_keyboard_layer_caption(output,sizeof(output),"bad\ntext",2,3)&&!output[0]);
}

static void test_bounded_utf8(void) {
    HelloKeyboardLabelStorage owned;
    char label[80];
    memset(label,'a',sizeof(label));
    label[sizeof(label)-1]=0;
    HelloKeyboardLabels labels={.done=label};
    hello_keyboard_labels_store(&owned,&labels);
    assert(strlen(owned.done)==HELLO_KEYBOARD_MAX_LABEL);
    // A complete non-ASCII code point fits exactly at the byte budget.
    memcpy(label+46,"ä",3);
    hello_keyboard_labels_store(&owned,&labels);
    assert(strlen(owned.done)==48&&brb_utf8_valid(owned.done));
    // A split code point is omitted, rather than leaking malformed UTF-8.
    memset(label,'a',47);memcpy(label+47,"🌼",5);
    hello_keyboard_labels_store(&owned,&labels);
    assert(strlen(owned.done)==47&&brb_utf8_valid(owned.done));
    // The caller need only provide a readable bounded prefix for long labels.
    char bounded[HELLO_KEYBOARD_MAX_LABEL];memset(bounded,'b',sizeof(bounded));
    labels.done=bounded;
    hello_keyboard_labels_store(&owned,&labels);
    assert(strlen(owned.done)==48&&owned.done[47]=='b');
}

static void test_invalid_input_and_storage_aliases(void) {
    HelloKeyboardLabelStorage owned;
    HelloKeyboardLabels labels={.clear="bad\nlabel",.done="\xc0\xaf",.cancel="\xe2"};
    hello_keyboard_labels_store(&owned,&labels);
    assert(!strcmp(owned.clear,"clear")&&!strcmp(owned.done,"done"));
    assert(!strcmp(owned.cancel,"cancel"));
    labels=(HelloKeyboardLabels){.done="Fertig",.cancel="Abbrechen"};
    hello_keyboard_labels_store(&owned,&labels);
    labels=hello_keyboard_stored_labels(&owned);
    hello_keyboard_labels_store(&owned,&labels);
    assert(!strcmp(owned.done,"Fertig")&&!strcmp(owned.cancel,"Abbrechen"));
}

int main(void) {
    test_defaults_and_independent_overrides();
    test_bounded_utf8();
    test_invalid_input_and_storage_aliases();
    test_selection_caption_ownership_and_fallback();
    test_layer_caption_ownership_and_fallback();
    test_navigation_caption();
    test_generated_layer_caption();
    puts("keyboard labels: defaults, ownership, bounded UTF-8, and reset passed");
    return 0;
}
