#pragma once
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include "clocktransport.h"
struct UsbIo;

// Nonblocking Windows COM transport for ballclock-fw's line protocol.
class UsbClock : public ClockTransport {
    Q_OBJECT
public:
    explicit UsbClock(QObject *parent = nullptr);
    ~UsbClock() override;
    static QStringList ports();
    bool open(const QString &port);
    void close();
    bool isOpen() const;
    bool isReady() const { return m_ready; }
    bool isBusy() const { return !m_pending.isEmpty() || !m_queue.isEmpty(); }
    bool hasTelemetry() const { return m_telemetry; }
    bool canStream() const { return m_protocol >= 3; }
    bool sendPixels(const QByteArray &rgb);
    void send(const QStringList &commands);
private:
    friend class UsbClockProtocolTest;
    void tick();
    void sendNext();
    void handleLine(const QByteArray &line);
    void *m_handle = nullptr;
    UsbIo *m_io = nullptr;
    QByteArray m_pixels;
    int m_protocol = 0;
    QTimer m_poll;
    QElapsedTimer m_deadline;
    QByteArray m_rx;
    QQueue<QByteArray> m_queue;
    QByteArray m_pending;
    bool m_ready = false;
    bool m_starting = false;
    bool m_telemetry = false;
    int m_handshakeAttempts = 0;
};
