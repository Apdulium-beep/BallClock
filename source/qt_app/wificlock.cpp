#include "wificlock.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QUuid>

WifiClock::WifiClock(QObject *parent) : ClockTransport(parent),m_client(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    m_network.setProxy(QNetworkProxy::NoProxy); // never send local clock credentials through a web proxy
}
WifiClock::~WifiClock() { close(); }

QByteArray WifiClock::form(const QList<QPair<QString,QString>> &fields) {
    QByteArray body;
    for(const auto &f:fields) {
        if(!body.isEmpty()) body+='&';
        body+=QUrl::toPercentEncoding(f.first)+'='+QUrl::toPercentEncoding(f.second);
    }
    return body;
}

bool WifiClock::open(const QString &address) {
    close();
    QString text=address.trimmed(); if(!text.contains("://")) text="http://"+text;
    m_base=QUrl(text);
    if(!m_base.isValid() || m_base.scheme()!="http" || m_base.host().isEmpty() ||
       !m_base.userInfo().isEmpty() || m_base.hasQuery() || m_base.hasFragment() ||
       (!m_base.path().isEmpty() && m_base.path()!="/")) {
        emit failure("Введите локальный IP или имя часов, например 192.168.1.50 или ballclock.local."); return false;
    }
    m_base.setPath("/"); m_open=true;
    m_queue.enqueue("state"); next();
    emit message("Подключение к часам по Wi-Fi: "+m_base.toString());
    return true;
}

void WifiClock::close() {
    m_open=false; m_ready=false; m_protocol=0; m_queue.clear(); m_pending.clear();
    if(m_reply) {
        auto *reply=m_reply; m_reply=nullptr; reply->disconnect(this); reply->abort(); reply->deleteLater();
    }
    emit readyChanged(false);
}

void WifiClock::fail(const QString &text,bool disconnected) {
    m_queue.clear(); m_pending.clear();
    if(disconnected) { m_open=false; m_ready=false; m_protocol=0; emit readyChanged(false); }
    emit failure(text);
}

void WifiClock::send(const QStringList &commands) {
    if(!m_ready) { emit failure("Сначала подключитесь к часам по Wi-Fi."); return; }
    if(isBusy()) { emit failure("Дождитесь завершения текущего запроса Wi-Fi."); return; }
    for(const auto &c:commands) {
        if(c.contains('\r') || c.contains('\n') || !(c=="state" || c=="?" || c=="save" || c=="stop" ||
           c=="wifi_connect" || c.startsWith("art put ") || c.startsWith("art delete ") || c.startsWith("set ") || c.startsWith("time "))) {
            emit failure("Неподдерживаемая команда Wi-Fi."); return;
        }
    }
    for(const auto &c:commands) m_queue.enqueue(c);
    next();
}

bool WifiClock::sendPixels(const QByteArray &rgb) {
    if(!canStream() || isBusy() || rgb.size()!=384) return false;
    m_pixels=rgb; m_pending="pixels";
    request("/api/v1/frame",form({{"rgb",QString::fromLatin1(rgb.toHex())}}),true);
    return true;
}

void WifiClock::next() {
    if(m_queue.isEmpty() || m_reply || !m_open) return;
    m_pending=m_queue.dequeue();
    if(m_pending=="state" || m_pending=="?") request("/api/v1/state");
    else if(m_pending.startsWith("art put ")) {
        const int space=m_pending.indexOf(' ',8),bar=m_pending.indexOf('|',space+1);
        if(space<0||bar<0){fail("Неверная картина",false);return;}
        request("/api/v1/art",form({{"slot",m_pending.mid(8,space-8)},{"name",m_pending.mid(space+1,bar-space-1)},{"data",m_pending.mid(bar+1)}}),true);
    } else if(m_pending.startsWith("art delete "))request("/api/v1/art/delete",form({{"slot",m_pending.mid(11)}}),true);
    else if(m_pending.startsWith("set ")) {
        const int space=m_pending.indexOf(' ',4);
        if(space<0) { fail("Неверная команда настройки",false); return; }
        request("/api/v1/settings",form({{m_pending.mid(4,space-4),m_pending.mid(space+1)}}),true);
    } else if(m_pending.startsWith("time ")) {
        const auto parts=m_pending.split(' ');
        if(parts.size()!=3) { fail("Неверная команда времени",false); return; }
        request("/api/v1/time",form({{"epoch",parts[1]},{"tz",parts[2]}}),true);
    } else if(m_pending=="save") request("/api/v1/save",{},true);
    else if(m_pending=="stop") request("/api/v1/stop",{},true);
    else if(m_pending=="wifi_connect") request("/api/v1/wifi/connect",{},true);
}

void WifiClock::request(const QString &path,const QByteArray &body,bool post) {
    QUrl url=m_base; url.setPath(path);
    QNetworkRequest request(url);
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("X-BallClock-Client",m_client.toUtf8());
    if(post) request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    m_reply=post ? m_network.post(request,body) : m_network.get(request);
    auto *reply=m_reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply]() { finished(reply); });
}

void WifiClock::finished(QNetworkReply *reply) {
    if(reply!=m_reply) { reply->deleteLater(); return; }
    m_reply=nullptr;
    const auto data=reply->readAll();
    const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto networkError=reply->error(); const QString error=reply->errorString();
    reply->deleteLater();
    const auto doc=QJsonDocument::fromJson(data);
    const auto json=doc.object();
    if(status<200 || status>=300 || networkError!=QNetworkReply::NoError) {
        const QString detail=json.value("error").toString();
        fail(QString("Wi-Fi: %1 (HTTP %2)").arg(detail.isEmpty()?error:detail).arg(status),status==0 || !m_ready);
        return;
    }
    const QString command=m_pending; m_pending.clear();
    if(command=="state" || command=="?") {
        if(!doc.isObject() || json.value("device").toString()!="ballclock" || json.value("api_version").toInt()!=1 ||
           !json.value("s").isObject() || !json.value("t").isObject()) {
            fail("Адрес не является совместимыми часами BallClock API v1. Проверьте IP и обновление прошивки.",true); return;
        }
        m_protocol=json.value("protocol").toInt();
        const bool wasReady=m_ready; m_ready=true;
        if(!wasReady) emit readyChanged(true);
        emit stateReceived(json);
    } else {
        const bool requiresSave=command.startsWith("art ") || command=="save" || command=="stop" || command=="wifi_connect";
        if(!doc.isObject() || !json.value("ok").toBool() || (command=="pixels" && !json.value("displayed").toBool()) ||
           (requiresSave && !json.value("saved").toBool())) {
            fail("Часы не подтвердили запрос Wi-Fi.",false); return;
        }
        if(command=="pixels") emit pixelsAcknowledged(m_pixels);
    }
    if(m_queue.isEmpty()) emit completed(); else next();
}
