#include "mainwindow.h"
#include "fakeclockserver.h"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QTemporaryDir>

class WifiUiTest {
public:
    static int run() {
        FakeClock server;if(!server.listen(QHostAddress::LocalHost,0)) return 1;
        MainWindow window;
        window.m_connectionMode->setCurrentIndex(1);
        window.m_networkAddress->setText(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        if(window.m_clock!=window.m_wifi || !window.m_ports->isHidden() || window.m_networkAddress->isHidden()) return 2;
        window.m_connect->click();
        if(!waitFor([&]() { return window.m_clock->isReady() && !window.m_clock->isBusy(); })) return 3;
        auto *brightnessSlider=window.findChild<QSlider *>("brightSlider");
        if(!brightnessSlider || brightnessSlider->minimum()!=0 || brightnessSlider->maximum()!=255) return 14;
        brightnessSlider->setValue(121);
        if(qobject_cast<QSpinBox *>(window.m_fields["bright"])->value()!=121) return 15;
        if(!waitFor([&]() { return server.settings.value("bright").toInt()==121 && server.saves>0 && window.m_dirty.isEmpty(); },5000)) return 4;
        if(!waitFor([&]() { return server.receivedRgb.size()==384; })) return 5;
        auto *password=qobject_cast<QLineEdit *>(window.m_fields["pass"]);
        const auto typePassword=[&](const QString &text) {
            password->setText(text);
            QMetaObject::invokeMethod(password,"textEdited",Qt::DirectConnection,Q_ARG(QString,text));
        };
        typePassword("a");
        QElapsedTimer pause;pause.start();
        waitFor([&]() { return pause.elapsed()>=900; });
        if(password->text()!="a" || !server.receivedPassword.isEmpty() || window.m_dirty.contains("pass")) return 8;
        qobject_cast<QSpinBox *>(window.m_fields["bright"])->setValue(122);
        if(brightnessSlider->value()!=122) return 16;
        if(!waitFor([&]() { return server.settings.value("bright").toInt()==122 && !window.m_applying && window.m_dirty.isEmpty(); },5000)) return 9;
        if(password->text()!="a" || !server.receivedPassword.isEmpty()) return 10;
        const QString fullPassword="a+b & = complete password";
        typePassword(fullPassword);
        window.m_apply->click();
        if(!waitFor([&]() { return server.receivedPassword==fullPassword && !window.m_applying && window.m_dirty.isEmpty() && password->text().isEmpty(); },5000)) return 11;
        std::puts("PASS: password draft survives typing pauses and unrelated saves; explicit save sends the complete password.");
        window.findChild<QPushButton *>("applyWifiMode")->click();
        if(!waitFor([&]() { return server.wifiConnects==1 && !window.m_clock->isBusy(); },5000)) return 12;
        window.m_connect->click();
        if(!waitFor([&]() { return !window.m_clock->isOpen(); })) return 6;
        window.m_connectionMode->setCurrentIndex(0);
        if(window.m_clock!=window.m_usb || window.m_ports->isHidden() || !window.m_networkAddress->isHidden()) return 7;
        window.adoptState(QJsonObject{{"protocol",3},{"s",server.settings},{"t",QJsonObject{{"ap",1},{"wifi",0},{"ip","192.168.4.1"}}}});
        if(window.m_networkAddress->text()!="192.168.4.1") return 13;
        std::puts("PASS: direct AP mode applies without router SSID; AP IP is learned over USB.");
        std::puts("PASS: Qt Wi-Fi connection, auto-save, PC frame streaming, clean disconnect and preserved USB mode.");
        return 0;
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);QTemporaryDir profile;
    qputenv("BALLCLOCK_PROFILE_FILE",(profile.path()+"/profile.ini").toUtf8());
    return WifiUiTest::run();
}
