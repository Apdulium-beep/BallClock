#pragma once
#include "clocktransport.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QUrl>

class WifiClock : public ClockTransport {
    Q_OBJECT
public:
    explicit WifiClock(QObject *parent=nullptr);
    ~WifiClock() override;
    bool open(const QString &address) override;
    void close() override;
    bool isOpen() const override { return m_open; }
    bool isReady() const override { return m_ready; }
    bool isBusy() const override { return m_reply || !m_pending.isEmpty() || !m_queue.isEmpty(); }
    bool hasTelemetry() const override { return m_ready; }
    bool canStream() const override { return m_ready && m_protocol>=3; }
    bool sendPixels(const QByteArray &rgb) override;
    void send(const QStringList &commands) override;
    static QByteArray form(const QList<QPair<QString,QString>> &fields);
private:
    void next();
    void request(const QString &path,const QByteArray &body={},bool post=false);
    void finished(QNetworkReply *reply);
    void fail(const QString &text,bool disconnected);
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply=nullptr;
    QQueue<QString> m_queue;
    QString m_pending,m_client;
    QByteArray m_pixels;
    QUrl m_base;
    bool m_open=false,m_ready=false;
    int m_protocol=0;
};
