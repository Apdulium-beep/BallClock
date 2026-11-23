#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
// Упрощённые заглушки FastLED: достаточно, чтобы выполнять render.cpp на компьютере
typedef uint8_t fract8;
struct CRGB { uint8_t r=0,g=0,b=0; CRGB(){} CRGB(uint8_t R,uint8_t G,uint8_t B):r(R),g(G),b(B){}
  CRGB(uint32_t c):r(c>>16),g(c>>8),b(c){}
  static const uint32_t Black=0, White=0xFFFFFF;
  CRGB& nscale8(uint8_t s){r=(uint16_t)r*s/256;g=(uint16_t)g*s/256;b=(uint16_t)b*s/256;return *this;} };
typedef uint32_t TProgmemRGBPalette16[16];
static const TProgmemRGBPalette16 RainbowColors_p={0xFF0000,0xD52A00,0xAB5500,0xAB7F00,0xABAB00,0x56D500,0x00FF00,0x00D52A,0x00AB55,0x0056AA,0x0000FF,0x2A00D5,0x5500AB,0x7F0081,0xD5002B,0xFF0000};
static const TProgmemRGBPalette16 PartyColors_p={0x5500AB,0x84007C,0xB5004B,0xE5001B,0xE81700,0xB84700,0xAB7700,0xABAB00,0xAB5500,0xDD2200,0xF2000E,0xC2003E,0x8F0071,0x5F00A1,0x2F00D0,0x0007F9};
static const TProgmemRGBPalette16 LavaColors_p={0x000000,0x800000,0x000000,0x800000,0x8B0000,0x800000,0x8B0000,0x8B0000,0x8B0000,0xFF0000,0xFFA500,0xFFFFFF,0xFFA500,0xFF0000,0x8B0000,0x000000};
static const TProgmemRGBPalette16 HeatColors_p={0x000000,0x330000,0x660000,0x990000,0xCC0000,0xFF0000,0xFF3300,0xFF6600,0xFF9900,0xFFCC00,0xFFFF00,0xFFFF33,0xFFFF66,0xFFFF99,0xFFFFCC,0xFFFFFF};
static const TProgmemRGBPalette16 CloudColors_p={0x0000FF,0x00008B,0x00008B,0x00008B,0x00008B,0x00008B,0x00008B,0x00008B,0x0000FF,0x00008B,0x87CEEB,0x87CEEB,0xADD8E6,0xFFFFFF,0xADD8E6,0x87CEEB};
struct CRGBPalette16 { CRGB e[16]; CRGBPalette16(){}
  CRGBPalette16(const CRGB& a,const CRGB& b,const CRGB& c,const CRGB& d){ const CRGB* s[4]={&a,&b,&c,&d};
    for(int i=0;i<16;i++){ float p=i*3/15.0f; int k=(int)p; if(k>2)k=2; float f=p-k; e[i]=CRGB((uint8_t)(s[k]->r+(s[k+1]->r-s[k]->r)*f),(uint8_t)(s[k]->g+(s[k+1]->g-s[k]->g)*f),(uint8_t)(s[k]->b+(s[k+1]->b-s[k]->b)*f)); } }
  CRGBPalette16(const TProgmemRGBPalette16& p){ for(int i=0;i<16;i++) e[i]=CRGB(p[i]); } };
enum TBlendType { NOBLEND, LINEARBLEND };
inline CRGB ColorFromPalette(const CRGBPalette16& p,uint8_t idx,uint8_t,TBlendType){
  int hi=idx>>4, lo=idx&15; const CRGB& a=p.e[hi]; const CRGB& b=p.e[(hi+1)&15];
  return CRGB(a.r+((int)b.r-a.r)*lo/16, a.g+((int)b.g-a.g)*lo/16, a.b+((int)b.b-a.b)*lo/16); }
inline uint8_t inoise8(uint16_t x,uint16_t y,uint16_t z){ uint32_t h=x*73856093u^y*19349663u^z*83492791u; h^=h>>13; h*=0x5bd1e995; h^=h>>15; return (uint8_t)h; }
inline uint8_t cos8(uint8_t t){ return (uint8_t)(128+127*cos(t*6.283185307/256)); }
inline uint8_t qsub8(uint8_t a,uint8_t b){ int t=a-b; return t<0?0:t; }
inline uint8_t qadd8(uint8_t a,uint8_t b){ int t=a+b; return t>255?255:t; }
inline uint8_t scale8(uint8_t i,uint8_t s){ return ((uint16_t)i*(1+(uint16_t)s))>>8; }
inline CRGB blend(const CRGB& a,const CRGB& b,fract8 m){ return CRGB(a.r+((int)b.r-a.r)*m/256, a.g+((int)b.g-a.g)*m/256, a.b+((int)b.b-a.b)*m/256); }
