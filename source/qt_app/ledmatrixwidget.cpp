#include "ledmatrixwidget.h"
#include "geometry.h"
#include <QPainter>
#include <cmath>

LedMatrixWidget::LedMatrixWidget(QWidget *parent) : QWidget(parent) {
    if (!geo::init()) qFatal("Invalid firmware LED geometry");
    setMinimumSize(520, 180);
    clear();
}

bool LedMatrixWidget::setFrame(const QByteArray &rgb) {
    if (rgb.size() != geo::LED_COUNT * 3) return false;
    m_rgb = rgb; update(); return true;
}
void LedMatrixWidget::clear() { m_rgb.clear(); update(); }

void LedMatrixWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(16, 16, 18));
    const qreal dy = std::sqrt(3.0) / 2;
    const qreal d = qMin((width() - 24.0) / geo::W, (height() - 24.0) / (1 + (geo::ROWS - 1) * dy));
    const qreal ox = (width() - geo::W * d) / 2;
    const qreal oy = (height() - (1 + (geo::ROWS - 1) * dy) * d) / 2;
    for (int i = 0; i < geo::LED_COUNT; ++i) {
        const QColor c = m_rgb.isEmpty() ? QColor(35,35,38) :
            QColor(quint8(m_rgb[3*i]), quint8(m_rgb[3*i+1]), quint8(m_rgb[3*i+2]));
        p.setBrush(c); p.setPen(QColor(65,65,68));
        p.drawEllipse(QPointF(ox + (geo::ledX[i] * 0.5 + 0.5) * d,
                             oy + (geo::ledRow[i] * dy + 0.5) * d), d * 0.46, d * 0.46);
    }
}
