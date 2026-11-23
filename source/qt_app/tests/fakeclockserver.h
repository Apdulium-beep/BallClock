#pragma once
#include "wificlock.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <cstdio>

static bool waitFor(const std::function<bool()> &test,int milliseconds=3000) {
    QElapsedTimer timer;timer.start();
    while(!test() && timer.elapsed()<milliseconds) QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    return test();
}

class FakeClock : public QTcpServer {
public:
    QJsonObject settings{{"font",1},{"bright",100},{"apOnly",1}};
    QByteArray receivedRgb;
    QString receivedPassword,client;
    QString artName,artData;int artSlot=-1,artDeletes=0;
    int saves=0,states=0,redirected=0,wifiConnects=0;
    bool failSave=false,wrongDevice=false,redirect=false;
    FakeClock() {
        connect(this,&QTcpServer::newConnection,this,[this]() {
            while(hasPendingConnections()) {
                auto *socket=nextPendingConnection();
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
                connect(socket,&QTcpSocket::readyRead,socket,[this,socket]() { receive(socket); });
            }
        });
    }
    void receive(QTcpSocket *socket) {
        QByteArray bytes=socket->property("rx").toByteArray()+socket->readAll(); socket->setProperty("rx",bytes);
        const int split=bytes.indexOf("\r\n\r\n"); if(split<0) return;
        const auto lines=bytes.left(split).split('\n');
        int length=0;QByteArray id;
        for(auto line:lines) {
            const auto lower=line.toLower();
            if(lower.startsWith("content-length:")) length=line.mid(line.indexOf(':')+1).trimmed().toInt();
            if(lower.startsWith("x-ballclock-client:")) id=line.mid(line.indexOf(':')+1).trimmed();
        }
        if(bytes.size()<split+4+length || socket->property("handled").toBool()) return;
        socket->setProperty("handled",true);
        if(client.isEmpty()) client=QString::fromUtf8(id);
        const auto start=lines.first().trimmed().split(' ');
        const QByteArray path=start.value(1),body=bytes.mid(split+4,length);
        QUrlQuery form(QString::fromUtf8(body));
        QJsonObject response{{"ok",true}}; int status=200;QByteArray extra;
        if(path=="/api/v1/state") {
            ++states;
            response=QJsonObject{{"device",wrongDevice?"router":"ballclock"},{"api_version",1},{"protocol",3},{"s",settings},{"t",QJsonObject{}}};
            if(redirect) { status=302;extra="Location: /redirected\r\n"; }
        } else if(path=="/api/v1/settings") {
            for(const auto &f:form.queryItems(QUrl::FullyDecoded)) {
                if(f.first=="pass") receivedPassword=f.second;
                else settings.insert(f.first,f.second.toInt());
            }
        } else if(path=="/api/v1/save") {
            if(failSave) { status=500; response=QJsonObject{{"ok",false},{"error","save_failed"}}; }
            else { ++saves;response.insert("saved",true); }
        } else if(path=="/api/v1/art") {
            artName=form.queryItemValue("name",QUrl::FullyDecoded);artData=form.queryItemValue("data");artSlot=form.queryItemValue("slot").toInt();response.insert("saved",true);
        } else if(path=="/api/v1/art/delete") {
            ++artDeletes;response.insert("saved",true);
        } else if(path=="/api/v1/frame") {
            receivedRgb=QByteArray::fromHex(form.queryItemValue("rgb").toLatin1());
            response.insert("displayed",receivedRgb.size()==384);
        } else if(path=="/api/v1/stop" || path=="/api/v1/time" || path=="/api/v1/wifi/connect") {
            if(path=="/api/v1/wifi/connect") ++wifiConnects;
            response.insert("saved",true);
        } else { ++redirected;status=404;response=QJsonObject{{"ok",false}}; }
        const QByteArray payload=QJsonDocument(response).toJson(QJsonDocument::Compact);
        const QByteArray header="HTTP/1.1 "+QByteArray::number(status)+" Test\r\nContent-Type: application/json\r\nContent-Length: "+
            QByteArray::number(payload.size())+"\r\nConnection: close\r\n"+extra+"\r\n";
        socket->write(header+payload.left(7));
        QTimer::singleShot(5,socket,[socket,payload]() { socket->write(payload.mid(7));socket->disconnectFromHost(); });
    }
};
