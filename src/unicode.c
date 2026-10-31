#include "unicode.h"
#include <string.h>
#include "unicode_grapheme_data.h"
#include "unicode_case_data.h"

size_t brb_utf8_decode(const char* text, size_t length, uint32_t* codepoint) {
    if (!text || !length || !codepoint) return 0;
    const unsigned char* b=(const unsigned char*)text;
    if (b[0]<0x80) {*codepoint=b[0];return 1;}
    if(length>=2&&b[0]>=0xc2&&b[0]<=0xdf&&(b[1]&0xc0)==0x80) {
        *codepoint=((uint32_t)(b[0]&31)<<6)|(b[1]&63);return 2;
    }
    if(length>=3&&b[0]>=0xe0&&b[0]<=0xef&&(b[1]&0xc0)==0x80&&(b[2]&0xc0)==0x80&&
       !(b[0]==0xe0&&b[1]<0xa0)&&!(b[0]==0xed&&b[1]>=0xa0)) {
        *codepoint=((uint32_t)(b[0]&15)<<12)|((uint32_t)(b[1]&63)<<6)|(b[2]&63);return 3;
    }
    if(length>=4&&b[0]>=0xf0&&b[0]<=0xf4&&(b[1]&0xc0)==0x80&&(b[2]&0xc0)==0x80&&
       (b[3]&0xc0)==0x80&&!(b[0]==0xf0&&b[1]<0x90)&&!(b[0]==0xf4&&b[1]>=0x90)) {
        *codepoint=((uint32_t)(b[0]&7)<<18)|((uint32_t)(b[1]&63)<<12)|
            ((uint32_t)(b[2]&63)<<6)|(b[3]&63);return 4;
    }
    return 0;
}

enum {OTHER,CR,LF,CONTROL,EXTEND,ZWJ,RI,PREPEND,SPACING,L,V,T,LV,LVT};
enum {EP=16,LINKER=32,ICB_EXTEND=64,CONSONANT=128};
static unsigned properties(uint32_t cp) {
    if(cp>=0xac00&&cp<=0xd7a3)return (cp-0xac00)%28==0?LV:LVT;
    size_t lo=0,hi=sizeof(brb_unicode_ranges)/sizeof(brb_unicode_ranges[0]);
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        if(cp<brb_unicode_ranges[mid].first)hi=mid;
        else if(cp>brb_unicode_ranges[mid].last)lo=mid+1;
        else return brb_unicode_ranges[mid].properties;
    }
    return OTHER;
}
static bool control(unsigned gcb) {return gcb==CR||gcb==LF||gcb==CONTROL;}

size_t brb_grapheme_next(const char* text,size_t length) {
    uint32_t cp;
    size_t offset=brb_utf8_decode(text,length,&cp);
    if(!offset)return 0;
    unsigned previous=properties(cp),a=previous&15,ris=a==RI?1:0;
    bool pictographic=(previous&EP)!=0,zwj_pictographic=false;
    bool linker=(previous&LINKER)!=0;
    while(offset<length) {
        const size_t bytes=brb_utf8_decode(text+offset,length-offset,&cp);
        if(!bytes)return 0;
        const unsigned current=properties(cp),b=current&15;
        bool joined;
        if(a==CR&&b==LF)joined=true; // GB3
        else if(control(a)||control(b))joined=false; // GB4/5
        else if(a==L&&(b==L||b==V||b==LV||b==LVT))joined=true; // GB6
        else if((a==LV||a==V)&&(b==V||b==T))joined=true; // GB7
        else if((a==LVT||a==T)&&b==T)joined=true; // GB8
        else if(b==EXTEND||b==ZWJ||b==SPACING||a==PREPEND)joined=true; // GB9/9a/9b
        else if(linker&&(current&CONSONANT))joined=true; // GB9c (Unicode 18)
        else if(a==ZWJ&&zwj_pictographic&&(current&EP))joined=true; // GB11
        else joined=a==RI&&b==RI&&(ris%2)==1; // GB12/13/999
        if(!joined)return offset;
        zwj_pictographic=b==ZWJ&&pictographic;
        if(current&EP)pictographic=true;
        else if(b!=EXTEND)pictographic=false;
        if(current&LINKER)linker=true;
        else if(!(current&ICB_EXTEND))linker=false;
        ris=b==RI?ris+1:0;
        offset+=bytes;previous=current;a=b;
    }
    return offset;
}

bool brb_utf8_valid(const char* text) {
    if(!text)return false;
    const size_t length=strlen(text);
    for(size_t at=0;at<length;) {
        uint32_t cp;const size_t bytes=brb_utf8_decode(text+at,length-at,&cp);
        if(!bytes)return false;
        at+=bytes;
    }
    return true;
}
size_t brb_grapheme_count(const char* text) {
    if(!text)return SIZE_MAX;
    const size_t length=strlen(text);size_t count=0;
    for(size_t at=0;at<length;++count) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes)return SIZE_MAX;
        at+=bytes;
    }
    return count;
}
size_t brb_grapheme_floor(const char* text,size_t cursor) {
    const size_t length=strlen(text);
    if(cursor>=length)return length;
    size_t at=0;
    while(at<cursor) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes||at+bytes>cursor)break;
        at+=bytes;
    }
    return at;
}
size_t brb_grapheme_following(const char* text,size_t cursor) {
    const size_t length=strlen(text),at=brb_grapheme_floor(text,cursor);
    if(at>=length)return length;
    const size_t bytes=brb_grapheme_next(text+at,length-at);
    return bytes?at+bytes:length;
}
size_t brb_grapheme_ceil(const char* text,size_t cursor) {
    const size_t at=brb_grapheme_floor(text,cursor);
    return at==cursor?at:brb_grapheme_following(text,at);
}
size_t brb_grapheme_previous(const char* text,size_t cursor) {
    const size_t length=strlen(text);size_t at=0,previous=0;
    if(cursor>length)cursor=length;
    while(at<cursor) {
        const size_t bytes=brb_grapheme_next(text+at,length-at);
        if(!bytes)break;
        previous=at;at+=bytes;
    }
    return previous;
}

size_t brb_unicode_case(const char* input,bool upper,char* output,size_t capacity) {
    if(!input||!output||!capacity)return SIZE_MAX;
    const size_t length=strlen(input);size_t written=0;
    output[0]=0;
    for(size_t at=0;at<length;) {
        uint32_t cp;const size_t bytes=brb_utf8_decode(input+at,length-at,&cp);
        if(!bytes){output[0]=0;return SIZE_MAX;}
        size_t lo=0,hi=sizeof(brb_case_maps)/sizeof(brb_case_maps[0]);
        const char* mapped=input+at;size_t count=bytes;
        while(lo<hi) {
            const size_t mid=lo+(hi-lo)/2;
            if(cp<brb_case_maps[mid].codepoint)hi=mid;
            else if(cp>brb_case_maps[mid].codepoint)lo=mid+1;
            else {
                const uint16_t offset=upper?brb_case_maps[mid].upper:brb_case_maps[mid].lower;
                if(offset){mapped=(const char*)brb_case_strings+offset;count=strlen(mapped);}
                break;
            }
        }
        if(count>=capacity-written){output[0]=0;return SIZE_MAX;}
        memcpy(output+written,mapped,count);written+=count;at+=bytes;
    }
    output[written]=0;return written;
}
