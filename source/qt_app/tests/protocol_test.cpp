#include "usbclock.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <cstdio>

class UsbClockProtocolTest {
public:
    static int run() {
        UsbClock usb;
        QByteArray received;
        QJsonObject state;
        QObject::connect(&usb, &UsbClock::frameReceived, [&received](const QByteArray &rgb) { received = rgb; });
        QObject::connect(&usb, &UsbClock::stateReceived, [&state](const QJsonObject &s) { state = s; });
        usb.m_pending = "state";
        usb.handleLine("STATE {\"protocol\":2,\"s\":{\"font\":1,\"autoB\":1},\"t\":{}}");
        if (!usb.hasTelemetry() || state.value("s").toObject().value("font").toInt() != 1) return 1;
        QByteArray rgb;
        for (int i=0; i<384; ++i) rgb.append(char(i & 255));
        usb.m_pending = "frame"; usb.handleLine("FRAME " + rgb.toHex());
        if (received != rgb || !usb.m_pending.isEmpty()) return 2;
        received.clear(); usb.m_pending = "frame";
        usb.handleLine("FRAME " + rgb.toHex().left(766));
        if (!received.isEmpty() || usb.m_pending.isEmpty()) return 3;
        auto invalid = rgb.toHex(); invalid[100] = 'z';
        usb.handleLine("FRAME " + invalid);
        if (!received.isEmpty() || usb.m_pending.isEmpty()) return 4;
        usb.m_pending = "state"; usb.handleLine("STATE {broken");
        if (usb.m_pending.isEmpty()) return 5;
        usb.m_pending = "state"; usb.handleLine("T HH:MM | W | ? | set <ключ> <значение>");
        if (!usb.m_pending.isEmpty()) return 6;
        usb.m_pending="state";
        usb.handleLine("STATE {\"protocol\":3,\"s\":{},\"t\":{}}");
        if(!usb.canStream()) return 7;
        int acknowledgements=0;
        QObject::connect(&usb,&UsbClock::pixelsAcknowledged,[&](const QByteArray &frame) { if(frame==rgb) ++acknowledgements; });
        usb.m_pending="pixels "+rgb.toHex(); usb.m_pixels=rgb;
        usb.handleLine("ok pixels");
        if(acknowledgements!=1 || !usb.m_pending.isEmpty()) return 8;
        usb.m_pending="art put 0 Name|"+QByteArray(3712,'0');usb.handleLine("ok");if(!usb.m_pending.isEmpty())return 9;
        usb.m_pending="art delete 0";usb.handleLine("ok");if(!usb.m_pending.isEmpty())return 10;
        std::puts("PASS: state protocol, exact 128 RGB LEDs, truncated/invalid frames rejected, legacy fallback.");
        return 0;
    }
};
int main(int argc, char **argv) { QCoreApplication app(argc,argv); return UsbClockProtocolTest::run(); }
