#include "ledmatrixwidget.h"
#include "geometry.h"
#include <QApplication>
#include <QImage>
#include <cmath>
#include <cstdio>
#include "pcrenderer.h"

int main(int argc, char **argv) {
    QApplication app(argc,argv);
    LedMatrixWidget display; display.resize(640,220);
    QByteArray rgb;
    for (int i=0; i<geo::LED_COUNT; ++i) { rgb.append(char(i*2)); rgb.append(char(255-i)); rgb.append(char(i)); }
    if (!display.setFrame(rgb) || display.setFrame(rgb.left(383))) return 1;
    QImage image(display.size(), QImage::Format_RGB32); display.render(&image);
    const double dy = std::sqrt(3.0)/2;
    const double d = qMin((display.width()-24.0)/geo::W, (display.height()-24.0)/(1+(geo::ROWS-1)*dy));
    const double ox = (display.width()-geo::W*d)/2, oy = (display.height()-(1+(geo::ROWS-1)*dy)*d)/2;
    for (int i=0; i<geo::LED_COUNT; ++i) {
        const int x = int(ox+(geo::ledX[i]*0.5+0.5)*d);
        const int y = int(oy+(geo::ledRow[i]*dy+0.5)*d);
        if (image.pixelColor(x,y) != QColor(i*2,255-i,i)) return 2;
    }
    std::puts("PASS: all 128 physical LED indices retain their exact received RGB at firmware coordinates.");
    ClockParameters p;p.font=1;p.pal=1;p.speed=0;p.scale=50;p.bright=180;
    QByteArray raw,shown;QString error;
    if(!renderOnPc(p,21,39,0,raw,shown,error)) return 3;
    display.setFrame(shown);display.render(&image);
    if(!image.save(QCoreApplication::applicationDirPath()+"/gradient-preview.png")) return 4;
    return 0;
}
