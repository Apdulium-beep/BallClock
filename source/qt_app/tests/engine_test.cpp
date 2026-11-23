#include "pcrenderer.h"
#include <QCoreApplication>
#include <cstdio>
#include <cstring>
#include "geometry.h"

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    ClockParameters p; QByteArray raw,shown; QString error;
    p.font=0; p.bmode=0;
    if(!renderOnPc(p,12,34,123456,raw,shown,error)) { std::fprintf(stderr,"%s\n",error.toUtf8().constData()); return 1; }
    if(raw!=QByteArray(384,0) || shown!=raw) return 2;
    p.font=1; p.bmode=1;
    if(!renderOnPc(p,12,34,123456,raw,shown,error)) return 3;
    const QByteArray first=raw;
    for(int font=0;font<3;++font) for(int mode=0;mode<4;++mode) for(int pal=0;pal<9;++pal) {
        p.font=font; p.bmode=mode; p.pal=pal;
        if(!renderOnPc(p,7,9,876543,raw,shown,error) || raw.size()!=384 || shown.size()!=384) return 4;
    }
    p.font=1;p.bmode=1;p.pal=0;
    if(!renderOnPc(p,12,34,123456,raw,shown,error) || raw!=first) return 5;
    // Animation gets elapsed milliseconds, not Unix time.
    p.font=0;p.bmode=1;p.pal=0;p.speed=50;p.scale=50;p.angle=0;
    QByteArray previous;
    int frozen=0,maxStep=0;
    for(unsigned i=0;i<61;++i) {
        if(!renderOnPc(p,12,34,1000u+i*33,raw,shown,error)) return 6;
        if(!previous.isEmpty()) {
            int step=0;
            for(int c=0;c<raw.size();++c) step=qMax(step,qAbs(int(quint8(raw[c]))-int(quint8(previous[c]))));
            frozen+=step==0; maxStep=qMax(maxStep,step);
        }
        previous=raw;
    }
    if(frozen!=0 || maxStep>60) return 8;
    // Use physical half-step X / row Y, never rectangular positions-in-row.
    if(!geo::init()) return 9;
    for(int angle : {0,90}) {
        p.angle=angle;
        if(!renderOnPc(p,12,34,123456,raw,shown,error)) return 10;
        for(int a=0;a<geo::LED_COUNT;++a) for(int b=0;b<geo::LED_COUNT;++b) {
            const bool sameProjection=angle==0 ? geo::ledX[a]==geo::ledX[b] : geo::ledRow[a]==geo::ledRow[b];
            if(sameProjection && raw.mid(a*3,3)!=raw.mid(b*3,3)) return 11;
        }
    }
    // Spatial regression: Ocean must be ONE ordered transition, not repeated
    // dark/bright stripes. Check actual physical neighbours in each row.
    p.font=0;p.bmode=1;p.pal=1;p.scale=50;p.speed=0;p.angle=0;p.bbright=255;
    if(!renderOnPc(p,12,34,0,raw,shown,error)) return 12;
    int largestNeighbourStep=0;
    for(int row=0;row<geo::ROWS;++row) {
        int previousLed=-1;
        for(int x=0;x<geo::HALF_W;++x) {
            const int led=geo::grid[row][x];
            if(led<0) continue;
            if(previousLed>=0) for(int c=0;c<3;++c) {
                const int before=quint8(raw[previousLed*3+c]),after=quint8(raw[led*3+c]);
                if(after<before) return 13;
                largestNeighbourStep=qMax(largestNeighbourStep,after-before);
            }
            previousLed=led;
        }
    }
    if(largestNeighbourStep>36) return 14;
    const int right=geo::grid[3][38];
    if(quint8(raw[right*3])<150 || quint8(raw[right*3+2])<220) return 15; // no palette wrap to black
    p.font=0;p.bmode=3;p.bcolor=0x123456;p.bbright=255;p.night=0;
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 16;
    for(int i=0;i<128;++i)if(raw.mid(i*3,3)!=QByteArray::fromHex("123456"))return 17;
    const auto solid=raw;
    p.pal=8;p.speed=100;p.scale=0;p.angle=180;
    if(!renderOnPc(p,12,34,876543,raw,shown,error)||raw!=solid)return 18;
    p.bbright=0;
    if(!renderOnPc(p,12,34,0,raw,shown,error)||raw!=QByteArray(384,0))return 19;
    p.bbright=255;p.night=1;
    if(!renderOnPc(p,12,34,0,raw,shown,error)||raw!=QByteArray(384,0))return 20;
    p.night=0;p.font=1;p.color=0xffffff;
    if(!renderOnPc(p,12,34,0,raw,shown,error)||!raw.contains(QByteArray::fromHex("ffffff"))||!raw.contains(QByteArray::fromHex("123456")))return 21;
    std::puts("PASS: independent solid background RGB, brightness, time/palette independence, foreground and night-mode behavior.");
    p.font=0;p.bmode=1;p.pal=9;p.speed=0;p.angle=0;p.scale=50;p.bbright=255;
    std::strcpy(p.customGradient,"Океан свой|ff0000,ff0000,ff0000,ff0000");
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 22;
    for(int i=0;i<128;++i)if(raw.mid(i*3,3)!=QByteArray::fromHex("ff0000"))return 23;
    std::strcpy(p.customGradient,"Океан свой|0000ff,0000ff,0000ff,0000ff");
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 24;
    for(int i=0;i<128;++i)if(raw.mid(i*3,3)!=QByteArray::fromHex("0000ff"))return 25;
    std::strcpy(p.customGradient,"Переход|000000,555555,aaaaaa,ffffff");
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 26;
    const int left=geo::grid[3][0];
    if(quint8(raw[left*3])>5||quint8(raw[right*3])<245)return 27;
    p.pal=16;
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 28;
    p.customGradient[0]=0;
    if(!renderOnPc(p,12,34,0,raw,shown,error))return 29;
    std::puts("PASS: named custom gradients, in-place cache invalidation, endpoints, last slot and empty fallback.");
    p.bmode=4;p.drawDigits=0;p.font=1;p.bbright=255;p.night=0;
    p.art[0].effect=1;p.art[0].rgb[0]=255;p.art[1].effect=3;p.art[1].period=20;p.art[1].rgb[1]=255;p.art[1].rgb[5]=255;
    p.art[2].effect=2;p.art[2].period=20;p.art[2].rgb[0]=255;p.art[2].rgb[5]=255;
    for(uint32_t ms:{0u,250u,500u,1000u,1999u,0xffffffffu}){
        if(!renderOnPc(p,12,34,ms,raw,shown,error))return 30;
        for(int i=0;i<128;++i){uint8_t rgb[3];artColor(p.art[i],ms,rgb);if(raw.mid(i*3,3)!=QByteArray(reinterpret_cast<char *>(rgb),3))return 31;}
    }
    p.drawDigits=1;if(!renderOnPc(p,12,34,0,raw,shown,error)||!raw.contains(QByteArray::fromHex("ffffff")))return 32;
    p.drawDigits=0;p.night=1;if(!renderOnPc(p,12,34,0,raw,shown,error)||raw!=QByteArray(384,0))return 33;
    std::puts("PASS: drawing per-pixel static/fade/blink/phase, wrap-safe animation, optional time layer and night suppression.");
    std::printf("PASS: single spatial Ocean gradient; largest neighbouring-channel step=%d.\n",largestNeighbourStep);
    std::printf("PASS: smooth shared gradient at elapsed time (frozen=%d, max RGB step=%d); staggered physical coordinates retained.\n",frozen,maxStep);
    std::puts("PASS: native firmware renderer works offline on PC for all fonts/backgrounds/palettes; deterministic output.");
    return 0;
}
