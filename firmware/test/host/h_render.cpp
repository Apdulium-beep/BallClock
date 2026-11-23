#include "render.h"
#include "geometry.h"
#include <cstdio>
#include "LittleFS.h"
std::map<std::string,std::string> FS_; LittleFSClass LittleFS;
// сравнение отрисовки: значения нужны только числами, чтобы собирать с любой версией settings.h
int main(){
  geo::init();
  CRGB leds[geo::LED_COUNT];
  uint64_t all=1469598103934665603ULL; long frames=0; uint64_t part[3][3]={};
  auto mix=[&](uint64_t& h){ for(int i=0;i<geo::LED_COUNT;i++){ h=(h^leds[i].r)*1099511628211ULL; h=(h^leds[i].g)*1099511628211ULL; h=(h^leds[i].b)*1099511628211ULL; } };
  int hm[5][2]={{0,0},{7,5},{12,34},{23,59},{9,41}}; uint32_t mss[5]={0,137,999,1500,123456}; int ang[4]={-180,0,45,179};
  for(int font=0;font<3;font++) for(int bm=0;bm<3;bm++){ uint64_t h=1469598103934665603ULL;
   for(int pal=0;pal<9;pal++) for(int blink=0;blink<2;blink++) for(int lead=0;lead<2;lead++) for(int night=0;night<2;night++)
   for(int ok=0;ok<2;ok++) for(auto& t:hm) for(auto ms:mss) for(int a:ang){
     S.font=font; S.bmode=bm; S.pal=pal; S.blink=blink; S.lead=lead; S.angle=a; S.speed=40+pal; S.scale=30+a%7; S.bbright=200; S.color=0xFFAA33; S.ncolor=0xFF1000;
     drawFrame(leds,t[0],t[1],ms,ok,night); mix(h); mix(all); frames++; }
   part[font][bm]=h; }
  for(int f=0;f<3;f++) for(int b=0;b<3;b++) printf("font=%d bmode=%d hash=%016llx\n",f,b,(unsigned long long)part[f][b]);
  for(int n:{0,5,7,19,57,168,192,255,999}){ drawNumber(leds,n); mix(all); }
  printf("кадров: %ld  общий хеш: %016llx\n",frames,(unsigned long long)all);
}
