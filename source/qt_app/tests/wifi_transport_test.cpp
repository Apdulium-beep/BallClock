#include "fakeclockserver.h"

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    FakeClock server;if(!server.listen(QHostAddress::LocalHost,0)) return 1;
    const QString address=QString("http://127.0.0.1:%1").arg(server.serverPort());
    WifiClock clock;int completed=0,errors=0,pixels=0;
    QObject::connect(&clock,&ClockTransport::completed,[&]() { ++completed; });
    QObject::connect(&clock,&ClockTransport::failure,[&](const QString &) { ++errors; });
    QObject::connect(&clock,&ClockTransport::pixelsAcknowledged,[&](const QByteArray &) { ++pixels; });
    if(!clock.open(address) || !waitFor([&]() { return clock.isReady() && !clock.isBusy(); })) return 2;
    const int before=completed;
    const QString password="a+b & = пароль";
    clock.send({"set font 2","set pass "+password,"save","state"});
    if(!waitFor([&]() { return completed>before; }) || errors || server.saves!=1 ||
       server.settings.value("font").toInt()!=2 || server.receivedPassword!=password || server.client.isEmpty()) return 3;
    QByteArray rgb;for(int i=0;i<384;++i) rgb.append(char(i));
    if(clock.sendPixels(rgb.left(383)) || !clock.sendPixels(rgb)) return 4;
    if(!waitFor([&]() { return pixels==1; }) || server.receivedRgb!=rgb) return 5;
    const QString artName="Моя + картина & =",artData(3712,'0');const int artBefore=completed;
    clock.send({"art put 7 "+artName+"|"+artData,"art delete 7"});
    if(!waitFor([&]{return completed>artBefore;})||errors||server.artName!=artName||server.artData!=artData||server.artSlot!=7||server.artDeletes!=1||server.wifiConnects)return 10;
    server.failSave=true;const int oldCompleted=completed,oldStates=server.states;
    clock.send({"set bright 120","save","state"});
    if(!waitFor([&]() { return errors==1; }) || completed!=oldCompleted || server.states!=oldStates || clock.isBusy()) return 6;
    clock.close();server.wrongDevice=true;
    clock.open(address);
    if(!waitFor([&]() { return errors==2; }) || clock.isReady()) return 7;
    server.wrongDevice=false;server.redirect=true;clock.open(address);
    if(!waitFor([&]() { return errors==3; }) || server.redirected || clock.isReady()) return 8;
    clock.close();
    if(clock.open("http://user:secret@127.0.0.1") || clock.open(address+"/other")) return 9;
    std::puts("PASS: Wi-Fi state/settings/frames, Unicode form escaping, save-failure propagation, wrong-device and redirect rejection.");
    return 0;
}
