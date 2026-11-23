#include "clockengine.h"
#include "render.h"
#include "geometry.h"
Settings S;
ArtPixel Drawing[ART_COUNT];

extern "C" __declspec(dllexport) void clock_render(const ClockParameters *parameters, int h, int m, uint32_t ms, uint8_t *raw, uint8_t *shown) {
    const auto &p = *parameters;
    static const bool geometryOk = geo::init();
    (void)geometryOk;
    S.font=p.font; S.color=p.color; S.lead=p.lead; S.blink=p.blink;
    S.bmode=p.bmode; S.pal=p.pal; S.bbright=p.bbright; S.speed=p.speed; S.scale=p.scale; S.angle=p.angle;
    S.ncolor=p.ncolor;
    S.bcolor=p.bcolor;
    S.drawDigits=p.drawDigits!=0;memcpy(Drawing,p.art,sizeof(Drawing));
    if(p.pal>=9 && p.pal<9+GRADIENT_SLOTS) {
        // The boundary accepts only a bounded, validated preset string.
        char text[GRADIENT_BYTES];memcpy(text,p.customGradient,sizeof(text));text[sizeof(text)-1]=0;
        if(gradientValid(text))memcpy(S.gradients[p.pal-9],text,strlen(text)+1);
        else S.gradients[p.pal-9][0]=0;
    }
    CRGB leds[geo::LED_COUNT];
    drawFrame(leds,h,m,ms,true,p.night != 0);
    const uint8_t brightness = calculate_max_brightness_for_power_mW(leds,geo::LED_COUNT,p.bright,12500);
    for(int i=0;i<geo::LED_COUNT;++i) {
        raw[i*3]=leds[i].r; raw[i*3+1]=leds[i].g; raw[i*3+2]=leds[i].b;
        shown[i*3]=scale8(leds[i].r,brightness);
        shown[i*3+1]=scale8(leds[i].g,brightness);
        shown[i*3+2]=scale8(leds[i].b,brightness);
    }
}
