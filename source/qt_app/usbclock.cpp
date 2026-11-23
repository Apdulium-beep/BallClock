#include "usbclock.h"
#include <QJsonDocument>
#define NOMINMAX
#include <windows.h>

struct UsbIo {
    OVERLAPPED read = {}, write = {};
    bool reading = false, writing = false;
    char buffer[2048];
    QByteArray tx;
};

namespace {
QString systemError() { return QString("Ошибка Windows %1").arg(GetLastError()); }
}

UsbClock::UsbClock(QObject *parent) : ClockTransport(parent) {
    m_poll.setInterval(5);
    connect(&m_poll, &QTimer::timeout, this, &UsbClock::tick);
}
UsbClock::~UsbClock() { close(); }
bool UsbClock::isOpen() const { return m_handle != nullptr; }

QStringList UsbClock::ports() {
    QStringList usb, other;
    for (int i = 1; i <= 256; ++i) {
        const QString name = QString("COM%1").arg(i);
        wchar_t target[1024] = {};
        if (QueryDosDeviceW(reinterpret_cast<const wchar_t *>(name.utf16()), target, 1024)) {
            const QString device = QString::fromWCharArray(target);
            if (device.contains("Bth", Qt::CaseInsensitive)) other << name;
            else usb << name;
        }
    }
    return usb + other;
}

bool UsbClock::open(const QString &port) {
    close();
    const QString path = "\\\\.\\" + port;
    HANDLE h = CreateFileW(reinterpret_cast<const wchar_t *>(path.utf16()),
                           GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        emit failure("Не удалось открыть " + port + ": " + systemError() +
                     ". Закройте монитор порта Arduino/PlatformIO и другие программы.");
        return false;
    }
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); emit failure("Не удалось прочитать параметры COM"); return false; }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE; dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE; dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE; dcb.fInX = FALSE; dcb.fDsrSensitivity = FALSE;
    dcb.fAbortOnError = FALSE;
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD; // return available bytes immediately
    timeouts.WriteTotalTimeoutConstant = 300;
    if (!SetCommState(h, &dcb) || !SetCommTimeouts(h, &timeouts)) {
        const auto error = systemError(); CloseHandle(h); emit failure(error); return false;
    }
    m_handle = h;
    m_io = new UsbIo;
    m_io->read.hEvent = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    m_io->write.hEvent = CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!m_io->read.hEvent || !m_io->write.hEvent) { close(); emit failure("Не удалось создать события COM"); return false; }
    m_handshakeAttempts = 0;
    m_starting = true;
    m_deadline.start();
    m_poll.start();
    emit message(port + " открыт: 115200, 8N1. Ожидание запуска платы...");
    return true;
}

void UsbClock::close() {
    m_poll.stop();
    if (m_handle) {
        CancelIoEx(static_cast<HANDLE>(m_handle), nullptr);
        DWORD count=0;
        if (m_io && m_io->reading) GetOverlappedResult(static_cast<HANDLE>(m_handle),&m_io->read,&count,TRUE);
        if (m_io && m_io->writing) GetOverlappedResult(static_cast<HANDLE>(m_handle),&m_io->write,&count,TRUE);
        CloseHandle(static_cast<HANDLE>(m_handle));
    }
    if (m_io) { CloseHandle(m_io->read.hEvent); CloseHandle(m_io->write.hEvent); delete m_io; m_io=nullptr; }
    m_handle = nullptr; m_ready = false; m_starting = false;
    m_telemetry = false;
    m_protocol = 0;
    m_queue.clear(); m_pending.clear(); m_rx.clear();
    emit readyChanged(false);
}

void UsbClock::send(const QStringList &commands) {
    if (!m_ready) { emit failure("Сначала подключите часы и дождитесь ответа на проверку."); return; }
    if (!m_pending.isEmpty() || !m_queue.isEmpty()) { emit failure("Дождитесь завершения текущих команд."); return; }
    for (const auto &command : commands) {
        if (command.contains('\n') || command.contains('\r')) { emit failure("Недопустимая команда"); m_queue.clear(); return; }
        m_queue.enqueue(command.toUtf8());
    }
    sendNext();
}

void UsbClock::sendNext() {
    if (m_queue.isEmpty()) return;
    m_pending = m_queue.dequeue();
    m_io->tx = m_pending + '\n';
    ResetEvent(m_io->write.hEvent);
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(m_handle), m_io->tx.constData(), DWORD(m_io->tx.size()), &written, &m_io->write)) {
        if (GetLastError()==ERROR_IO_PENDING) m_io->writing=true;
        else { const auto error=systemError(); close(); emit failure("Ошибка записи: "+error); return; }
    } else if (written != DWORD(m_io->tx.size())) {
        close(); emit failure("Неполная запись COM"); return;
    }
    if (m_pending != "frame" && m_pending != "state" && !m_pending.startsWith("pixels "))
        emit message(m_pending.startsWith("set pass ") ? "> set pass [скрыто]" : "> " + QString::fromUtf8(m_pending));
    m_deadline.restart();
}

bool UsbClock::sendPixels(const QByteArray &rgb) {
    if (!m_ready || !canStream() || isBusy() || rgb.size()!=384) return false;
    m_pixels=rgb; m_queue.enqueue("pixels "+rgb.toHex()); sendNext(); return isOpen();
}

void UsbClock::handleLine(const QByteArray &bytes) {
    const auto line = bytes.trimmed();
    if (line.isEmpty()) return;
    if (!line.startsWith("FRAME ") && !line.startsWith("STATE ") && line!="ok pixels")
        emit message("< " + QString::fromUtf8(line));
    if (m_pending.isEmpty()) return;
    bool telemetryAck = false;
    if (m_pending == "state" && line.startsWith("STATE ")) {
        const auto doc = QJsonDocument::fromJson(line.mid(6));
        if (doc.isObject() && doc.object().value("protocol").toInt() >= 2 && doc.object().value("s").isObject()) {
            m_protocol=doc.object().value("protocol").toInt();
            m_telemetry = true; telemetryAck = true;
            emit stateReceived(doc.object());
        }
    } else if (m_pending == "frame" && line.startsWith("FRAME ")) {
        const auto hex = line.mid(6);
        bool valid = hex.size() == 768;
        for (char c : hex) valid = valid && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
        if (valid) { emit frameReceived(QByteArray::fromHex(hex)); telemetryAck = true; }
    } else if (m_pending == "state" && line.startsWith("T HH:MM")) {
        m_pending.clear(); m_queue.clear();
        emit failure("На часах старая прошивка без USB-телеметрии. Для живого изображения нужна подготовленная прошивка из папки firmware.");
        return;
    }
    const bool status = m_pending == "?" && line.startsWith("A0=") && line.contains("IP=");
    const bool pixelsAck=m_pending.startsWith("pixels ") && line=="ok pixels";
    const bool ack = pixelsAck || telemetryAck || ((m_pending.startsWith("art ") || m_pending.startsWith("set ") || m_pending.startsWith("time ") || m_pending=="save" || m_pending=="stop" || m_pending=="wifi_connect") && line == "ok") || status ||
        (m_pending.startsWith("T ") && QString::fromUtf8(line).contains("время установлено"));
    if (line.contains(QString("неизвестный ключ").toUtf8()) || line.startsWith("error ")) {
        m_pending.clear(); m_queue.clear(); emit failure("Прошивка отвергла настройку. Применение прервано."); return;
    }
    if (!ack) return;
    m_pending.clear();
    if (pixelsAck) emit pixelsAcknowledged(m_pixels);
    if (status && !m_ready) { m_ready = true; emit readyChanged(true); }
    if (m_queue.isEmpty()) emit completed();
    else sendNext();
}

void UsbClock::tick() {
    DWORD count=0;
    if (m_io->writing) {
        if (GetOverlappedResult(static_cast<HANDLE>(m_handle),&m_io->write,&count,FALSE)) {
            m_io->writing=false;
            if (count!=DWORD(m_io->tx.size())) { close(); emit failure("Неполная запись COM"); return; }
        } else if (GetLastError()!=ERROR_IO_INCOMPLETE) { const auto error=systemError(); close(); emit failure(error); return; }
    }
    count=0;
    if (m_io->reading) {
        if (GetOverlappedResult(static_cast<HANDLE>(m_handle),&m_io->read,&count,FALSE)) m_io->reading=false;
        else if (GetLastError()!=ERROR_IO_INCOMPLETE) { const auto error=systemError(); close(); emit failure(error); return; }
    } else {
        ResetEvent(m_io->read.hEvent);
        if (!ReadFile(static_cast<HANDLE>(m_handle),m_io->buffer,sizeof(m_io->buffer),&count,&m_io->read)) {
            if (GetLastError()==ERROR_IO_PENDING) m_io->reading=true;
            else { const auto error=systemError(); close(); emit failure(error); return; }
        }
    }
    if (count) {
        m_rx.append(m_io->buffer, int(count));
        int pos;
        while ((pos = m_rx.indexOf('\n')) >= 0) {
            const auto line = m_rx.left(pos); m_rx.remove(0, pos + 1); handleLine(line);
            if (!isOpen()) return;
        }
        if (m_rx.size() > 8192) m_rx.clear();
    }
    if (m_starting && m_deadline.elapsed() >= 2500) {
        m_starting = false; m_handshakeAttempts = 1; m_queue.enqueue("?"); sendNext();
    } else if (!m_ready && m_pending == "?" && m_deadline.elapsed() > 2000 && m_handshakeAttempts < 4) {
        ++m_handshakeAttempts; m_pending.clear(); m_queue.enqueue("?"); sendNext();
    } else if (!m_pending.isEmpty() && m_deadline.elapsed() > 3500) {
        const QString command = QString::fromUtf8(m_pending);
        const bool wasReady = m_ready;
        close();
        emit failure("Нет подтверждения команды " + command +
                     (wasReady ? ". Связь закрыта; часть настроек могла примениться." :
                     ". Проверьте порт и прошивку ballclock-fw (115200)."));
    }
}
