#pragma once
#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

// One UI and command model for wired USB and local-network clients.
class ClockTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool open(const QString &endpoint)=0;
    virtual void close()=0;
    virtual bool isOpen() const=0;
    virtual bool isReady() const=0;
    virtual bool isBusy() const=0;
    virtual bool hasTelemetry() const=0;
    virtual bool canStream() const=0;
    virtual bool sendPixels(const QByteArray &rgb)=0;
    virtual void send(const QStringList &commands)=0;
signals:
    void message(const QString &text);
    void failure(const QString &text);
    void readyChanged(bool ready);
    void completed();
    void stateReceived(const QJsonObject &state);
    void frameReceived(const QByteArray &rgb);
    void pixelsAcknowledged(const QByteArray &rgb);
};
