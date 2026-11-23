#pragma once
#include <cstdint>
#include "artcodec.h"
struct ClockParameters {
    int font=1, color=0xffffff, lead=0, blink=1;
    int bmode=1, pal=0, bbright=200, speed=50, scale=50, angle=0;
    int bright=100, night=0, ncolor=0xff0000;
    int bcolor=0x063c6d;
    char customGradient[96]={};
    int drawDigits=0;
    ArtPixel art[ART_COUNT];
};
void clockRender(const ClockParameters &parameters, int hours, int minutes,
                 uint32_t milliseconds, uint8_t *rawRgb, uint8_t *displayRgb);
