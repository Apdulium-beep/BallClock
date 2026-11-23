#ifndef PALETTES_H
#define PALETTES_H

#include <QColor>
#include <QVector>
#include <QString>

// ============================================================
//  Цветовые палитры из проекта BallClock (FastLED)
//  Каждая палитра — список контрольных точек (0..255, цвет).
// ============================================================

struct PaletteStop {
    int     pos;     // 0..255
    QColor  color;
};

struct Palette {
    QString             name;
    QVector<PaletteStop> stops;

    // Интерполировать цвет по позиции 0..255
    QColor colorAt(int idx) const;
};

// Список всех палитр
const QVector<Palette> &getPalettes();

// Названия палитр (для ComboBox) — разделитель ;
QString getPaletteListString();

#endif // PALETTES_H