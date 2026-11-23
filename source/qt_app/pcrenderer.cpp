#include "pcrenderer.h"
#include <QCoreApplication>
#include <QLibrary>

bool renderOnPc(const ClockParameters &p, int h, int m, quint32 ms,
                QByteArray &raw, QByteArray &shown, QString &error) {
    static QLibrary engine(QCoreApplication::applicationDirPath()+"/clockengine.dll");
    using Function=void (*)(const ClockParameters *,int,int,uint32_t,uint8_t *,uint8_t *);
    static Function function=reinterpret_cast<Function>(engine.resolve("clock_render"));
    if (!function) { error=engine.errorString(); return false; }
    raw.resize(384); shown.resize(384);
    function(&p,h,m,ms,reinterpret_cast<uint8_t *>(raw.data()),reinterpret_cast<uint8_t *>(shown.data()));
    return true;
}
