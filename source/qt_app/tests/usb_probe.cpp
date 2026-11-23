#include "usbclock.h"
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    if (app.arguments().size() != 2) { out << "Usage: usb_probe COM5\n"; return 2; }
    UsbClock usb;
    QObject::connect(&usb, &UsbClock::message, &app, [&out](const QString &text) { out << text << Qt::endl; });
    QObject::connect(&usb, &UsbClock::failure, &app, [&app, &out](const QString &text) {
        out << "FAIL: " << text << Qt::endl;
        QTimer::singleShot(0, &app, [&app]() { app.exit(1); });
    });
    QObject::connect(&usb, &UsbClock::readyChanged, &app, [&app, &out](bool ready) {
        if (ready) { out << "PASS: ballclock-fw USB status response received; no settings changed." << Qt::endl; app.quit(); }
    });
    QTimer::singleShot(12000, &app, [&app]() { app.exit(3); });
    QTimer::singleShot(0, &app, [&usb, &app]() { usb.open(app.arguments().at(1)); });
    return app.exec();
}
