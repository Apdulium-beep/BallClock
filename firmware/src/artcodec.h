#pragma once
#include "gradient.h"
#include <cstdio>
constexpr int ART_COUNT=128, ART_SLOTS=8, ART_RECORD=29, ART_DATA=ART_COUNT*ART_RECORD;
struct ArtPixel { uint8_t effect=0,period=20,phase=0,rgb[12]={}; };
static_assert(sizeof(ArtPixel)==15,"Drawing engine ABI must match Python/Qt bindings");
extern ArtPixel Drawing[ART_COUNT];
inline bool artNameValid(const char *name) {
  if(!name||!*name||strlen(name)>64)return false;
  char text[96];snprintf(text,sizeof(text),"%s|ffffff,ffffff,ffffff,ffffff",name);
  return gradientValid(text);
}
inline bool artDataValid(const char *data) {
  if(!data||strlen(data)!=ART_DATA)return false;
  for(int i=0;i<ART_COUNT;++i){const char *p=data+i*ART_RECORD;
    if(p[0]<'0'||p[0]>'3')return false;
    for(int j=1;j<ART_RECORD;++j)if(gradientHex(p[j])<0)return false;
    if(p[0]>='2'&&gradientHex(p[1])*16+gradientHex(p[2])==0)return false;
  }return true;
}
inline void artDecode(const char *data,ArtPixel *out) {
  for(int i=0;i<ART_COUNT;++i){const char *p=data+i*ART_RECORD;out[i].effect=p[0]-'0';
    out[i].period=gradientHex(p[1])*16+gradientHex(p[2]);out[i].phase=gradientHex(p[3])*16+gradientHex(p[4]);
    for(int c=0;c<12;++c)out[i].rgb[c]=gradientHex(p[5+c*2])*16+gradientHex(p[6+c*2]);}
}
inline void artEncode(const ArtPixel *pixels,char *data) {
  const char hex[]="0123456789abcdef";
  for(int i=0;i<ART_COUNT;++i){char *p=data+i*ART_RECORD;const auto &v=pixels[i];p[0]='0'+v.effect;
    p[1]=hex[v.period>>4];p[2]=hex[v.period&15];p[3]=hex[v.phase>>4];p[4]=hex[v.phase&15];
    for(int c=0;c<12;++c){p[5+c*2]=hex[v.rgb[c]>>4];p[6+c*2]=hex[v.rgb[c]&15];}}
  data[ART_DATA]=0;
}
inline void artColor(const ArtPixel &p,uint32_t ms,uint8_t out[3]) {
  if(!p.effect){out[0]=out[1]=out[2]=0;return;}
  if(p.effect==1){memcpy(out,p.rgb,3);return;}
  const uint32_t period=(p.period?p.period:1)*100u;
  const uint8_t t=uint8_t(((ms%period)*256u/period)+p.phase),stop=t/64,next=(stop+1)%4,f=(t%64)*4;
  for(int c=0;c<3;++c)out[c]=p.effect==3?p.rgb[stop*3+c]:uint8_t((p.rgb[stop*3+c]*(256u-f)+p.rgb[next*3+c]*f+128u)/256u);
}
