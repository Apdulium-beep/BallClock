#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

class UsbLiveTest {
public:
    static int run(QApplication &app,const QString &port) {
        MainWindow window;
        int phase=0,frames=0,originalBlink=0;
        QObject::connect(window.m_usb,&UsbClock::failure,&app,[&](const QString &e) {
            std::fprintf(stderr,"FAIL: %s\n",e.toUtf8().constData()); app.exit(1);
        });
        QObject::connect(window.m_usb,&UsbClock::stateReceived,&app,[&](const QJsonObject &state) {
            const auto s=state.value("s").toObject();
            if(phase==0) {
                if(state.value("protocol").toInt()<3) { app.exit(2); return; }
                originalBlink=s.value("blink").toInt(); phase=1;
                QTimer::singleShot(0,&window,[&]() {
                    qobject_cast<QCheckBox *>(window.m_fields["blink"])->setChecked(!originalBlink);
                });
            } else if(phase==1 && s.value("blink").toInt()!=originalBlink) {
                std::puts("PASS: changing a Qt control automatically updated/saved the clock.");
                phase=2;
                QTimer::singleShot(0,&window,[&]() {
                    qobject_cast<QCheckBox *>(window.m_fields["blink"])->setChecked(originalBlink);
                });
            } else if(phase==2 && s.value("blink").toInt()==originalBlink) phase=3;
        });
        QObject::connect(window.m_usb,&UsbClock::pixelsAcknowledged,&app,[&](const QByteArray &rgb) {
            if(rgb.size()!=384) { app.exit(3); return; }
            if(++frames>=3 && phase==3) {
                phase=4;
                QTimer::singleShot(0,&window,[&]() { window.close(); });
            }
        });
        QObject::connect(window.m_usb,&UsbClock::readyChanged,&app,[&](bool ready) {
            if(!ready && phase==4) {
                phase=5;
                std::puts("PASS: Qt-generated frames acknowledged; original parameter restored; clean close saved/stopped remote mode.");
                QTimer::singleShot(0,&app,[&]() { app.exit(0); });
            }
        });
        const int index=window.m_ports->findText(port);
        if(index<0) return 4;
        window.m_ports->setCurrentIndex(index);
        QTimer::singleShot(0,&window,[&]() { window.m_connect->click(); });
        QTimer::singleShot(25000,&app,[&]() { std::puts("FAIL: GUI integration timeout"); app.exit(5); });
        return app.exec();
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); QTemporaryDir profile;
    qputenv("BALLCLOCK_PROFILE_FILE",(profile.path()+"/profile.ini").toUtf8());
    if(app.arguments().size()!=2) return 6;
    return UsbLiveTest::run(app,app.arguments().at(1));
}
