#pragma once
#include <cstdint>
#include <cstring>

// Eight clock-owned presets, four evenly spaced stops each. UTF-8 name +
// |rrggbb,rrggbb,rrggbb,rrggbb; empty string means a free slot.
constexpr int GRADIENT_SLOTS=8, GRADIENT_BYTES=96, GRADIENT_NAME_BYTES=64;
inline int gradientHex(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
inline bool gradientValid(const char *text) {
  if(!text)return false;
  if(!*text)return true;
  const size_t length=strlen(text);
  if(length>=GRADIENT_BYTES)return false;
  const char *sep=strchr(text,'|');
  if(!sep||sep==text||sep-text>GRADIENT_NAME_BYTES||strlen(sep+1)!=27)return false;
  if(text[0]==' '||sep[-1]==' ')return false;
  // Strict UTF-8, excluding C0/C1 controls, separators and bidi controls.
  for(const unsigned char *p=(const unsigned char*)text;p<(const unsigned char*)sep;) {
    uint32_t cp=*p++;int extra=0;uint32_t minimum=0;
    if(cp>=0xc2&&cp<=0xdf){cp&=31;extra=1;minimum=0x80;}
    else if(cp>=0xe0&&cp<=0xef){cp&=15;extra=2;minimum=0x800;}
    else if(cp>=0xf0&&cp<=0xf4){cp&=7;extra=3;minimum=0x10000;}
    else if(cp>=0x80)return false;
    while(extra--){if(p>=(const unsigned char*)sep||(*p&0xc0)!=0x80)return false;cp=(cp<<6)|(*p++&63);}
    if(cp<minimum||cp<32||(cp>=127&&cp<=159)||cp==0x2028||cp==0x2029||
       (cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069)||cp>0x10ffff||
       (cp>=0xd800&&cp<=0xdfff))return false;
  }
  for(int i=0;i<27;++i)if(i%7==6 ? sep[i+1]!=',' : gradientHex(sep[i+1])<0)return false;
  return true;
}
inline bool gradientColors(const char *text,uint32_t out[4]) {
  if(!text||!*text||!gradientValid(text))return false;
  const char *p=strchr(text,'|')+1;
  for(int stop=0;stop<4;++stop){uint32_t rgb=0;for(int c=0;c<6;++c)rgb=(rgb<<4)|gradientHex(p[stop*7+c]);out[stop]=rgb;}
  return true;
}
