#pragma once
#include <stddef.h>
#include <stdint.h>

inline bool decodeFrameHex(const char *hex,size_t length,uint8_t *output,size_t bytes) {
    if(!hex || !output || length!=bytes*2) return false;
    // Validate the entire message before touching the destination frame.
    for(size_t i=0;i<length;++i) {
        const char c=hex[i];
        if(!((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'))) return false;
    }
    for(size_t i=0;i<bytes;++i) {
        unsigned value=0;
        for(int n=0;n<2;++n) {
            const char c=hex[i*2+n];
            value=(value<<4)|(c<='9' ? c-'0' : c<='F' ? c-'A'+10 : c-'a'+10);
        }
        output[i]=uint8_t(value);
    }
    return true;
}
