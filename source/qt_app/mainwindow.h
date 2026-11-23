#pragma once
#include <QMainWindow>
#include <QMap>
#include <QJsonObject>
#include <QElapsedTimer>
#include "usbclock.h"
#include "ledmatrixwidget.h"
#include "wificlock.h"

class QComboBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QVBoxLayout;
class QCloseEvent;
class QLineEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    friend class SettingsUiTest;
    friend class UsbLiveTest;
    friend class WifiUiTest;
    void refreshPorts();
    void log(const QString &message);
    void buildSettings(QVBoxLayout *layout);
    void adoptState(const QJsonObject &state);
    void changed(const QString &key, const QString &value);
    void apply();
    void poll();
    void renderPreview();
    void persistProfile();
    void updateConnectionUi();
    void stageWifiPassword();
    void updateGradientChoices();
    void editGradient(bool existing);
    void editDrawing();
    UsbClock *m_usb;
    WifiClock *m_wifi;
    ClockTransport *m_clock;
    LedMatrixWidget *m_display;
    QComboBox *m_ports;
    QComboBox *m_connectionMode;
    QLineEdit *m_networkAddress;
    QPushButton *m_connect;
    QPushButton *m_apply;
    QLabel *m_status;
    QLabel *m_info;
    QTextEdit *m_log;
    QWidget *m_controls;
    QMap<QString, QWidget *> m_fields;
    QMap<QString, QString> m_dirty;
    QJsonObject m_state;
    QJsonObject m_art;
    QMap<int,QByteArray> m_artScenes;
    bool m_artEditing=false;
    QJsonObject m_desired, m_telemetry;
    QMap<QString, QString> m_sent;
    QByteArray m_raw, m_shown;
    QTimer *m_debounce;
    QElapsedTimer m_lastState, m_lastFrame, m_animationClock;
    bool m_loading = false;
    bool m_applying = false;
    bool m_haveLocalProfile = false;
    bool m_closing = false, m_disconnectRequested = false, m_stopping = false;
    bool m_timeSynced = false, m_nightActive = false;
    bool m_wifiJoinRequested = false, m_autoSaveBlocked = false;
};
