#include "palettes.h"
#include <QStringList>

QColor Palette::colorAt(int idx) const {
    if (stops.isEmpty()) return Qt::black;
    idx = qBound(0, idx, 255);
    for (int i = 0; i + 1 < stops.size(); ++i) {
        const auto &a = stops[i]; const auto &b = stops[i + 1];
        if (idx >= a.pos && idx <= b.pos) {
            const int range = b.pos - a.pos;
            if (!range) return a.color;
            const int t = (idx - a.pos) * 255 / range;
            return QColor(a.color.red() + (b.color.red() - a.color.red()) * t / 255,
                          a.color.green() + (b.color.green() - a.color.green()) * t / 255,
                          a.color.blue() + (b.color.blue() - a.color.blue()) * t / 255);
        }
    }
    return stops.last().color;
}

// IDs 0..8 match ballclock-fw/src/render.cpp; FastLED previews are approximate.
const QVector<Palette> &getPalettes() {
    static const QVector<Palette> palettes = {
        {"Закат", {{0,{40,0,60}}, {85,{200,30,80}}, {170,{255,140,30}}, {255,{255,220,120}}}},
        {"Океан", {{0,{0,20,60}}, {85,{0,90,160}}, {170,{0,200,200}}, {255,{180,255,240}}}},
        {"Лес", {{0,{5,40,20}}, {85,{20,120,50}}, {170,{140,200,60}}, {255,{230,240,150}}}},
        {"Неон", {{0,{90,0,160}}, {85,{255,0,140}}, {170,{0,200,255}}, {255,{255,255,255}}}},
        {"Радуга", {{0,{255,0,0}}, {43,{255,255,0}}, {85,{0,255,0}}, {128,{0,255,255}}, {171,{0,0,255}}, {214,{255,0,255}}, {255,{255,0,0}}}},
        {"Party", {{0,{85,0,171}}, {64,{255,0,0}}, {128,{171,85,0}}, {192,{0,0,255}}, {255,{85,0,171}}}},
        {"Лава", {{0,{0,0,0}}, {85,{128,0,0}}, {170,{255,64,0}}, {255,{255,255,255}}}},
        {"Огонь / Heat", {{0,{0,0,0}}, {85,{255,0,0}}, {170,{255,255,0}}, {255,{255,255,255}}}},
        {"Облака", {{0,{0,0,255}}, {85,{0,0,128}}, {170,{135,206,235}}, {255,{255,255,255}}}}
    };
    return palettes;
}

QString getPaletteListString() {
    QStringList names;
    for (const auto &p : getPalettes()) names << p.name;
    return names.join(';');
}
