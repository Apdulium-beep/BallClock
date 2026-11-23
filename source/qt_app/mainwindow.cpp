#include "mainwindow.h"
#include "pcrenderer.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStandardItemModel>
#include "gradient.h"
#include "arteditor.h"
#include <QJsonArray>

namespace {
QSettings profileStore() {
    const QString testFile=qEnvironmentVariable("BALLCLOCK_PROFILE_FILE");
    if(!testFile.isEmpty()) return QSettings(testFile,QSettings::IniFormat);
    return QSettings("BallClock","ControllerUSBv3");
}
QString valueText(const QJsonValue &v) {
    return v.isString() ? v.toString() : QString::number(v.toInt());
}
struct Field { const char *key; const char *label; char type; int lo = 0; int hi = 255; QStringList options; };
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_usb(new UsbClock(this)),m_wifi(new WifiClock(this)),m_clock(m_usb) {
    setWindowTitle("BallClock — USB и Wi-Fi"); resize(720, 950);
    auto *scroll = new QScrollArea(this); scroll->setWidgetResizable(true); setCentralWidget(scroll);
    auto *body = new QWidget; scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body);
    auto *row = new QHBoxLayout;
    m_connectionMode=new QComboBox; m_connectionMode->addItems({"USB / провод","Wi-Fi / IP"}); row->addWidget(m_connectionMode);
    m_ports = new QComboBox; row->addWidget(m_ports);
    m_networkAddress=new QLineEdit(profileStore().value("clockAddress","ballclock.local").toString());
    m_networkAddress->setPlaceholderText("IP часов, например 192.168.1.50"); row->addWidget(m_networkAddress);
    auto *refresh = new QPushButton("Обновить порты"); row->addWidget(refresh);
    m_connect = new QPushButton("Подключить USB"); row->addWidget(m_connect); layout->addLayout(row);
    m_status = new QLabel("Изображение формирует Qt. Подключение — по проводу USB или по Wi-Fi в локальной сети.");
    m_status->setWordWrap(true); layout->addWidget(m_status);
    m_display = new LedMatrixWidget; layout->addWidget(m_display);
    m_info = new QLabel("Нет данных с устройства"); m_info->setWordWrap(true); layout->addWidget(m_info);
    auto *query = new QPushButton("Прочитать состояние часов"); layout->addWidget(query);
    auto *sync = new QPushButton("Установить дату и время с ПК"); layout->addWidget(sync);
    auto *joinWifi=new QPushButton("Применить режим Wi-Fi / подключить"); layout->addWidget(joinWifi);
    joinWifi->setObjectName("applyWifiMode");
    m_controls = new QWidget; auto *controls = new QVBoxLayout(m_controls);
    buildSettings(controls); layout->addWidget(m_controls);
    m_apply = new QPushButton("Все изменения сохранены"); m_apply->setEnabled(false); layout->addWidget(m_apply);
    m_log = new QTextEdit; m_log->setReadOnly(true); m_log->setMaximumHeight(160);
    m_log->document()->setMaximumBlockCount(500); layout->addWidget(m_log);
    m_debounce=new QTimer(this); m_debounce->setSingleShot(true); m_debounce->setInterval(180);
    connect(m_debounce,&QTimer::timeout,this,&MainWindow::apply);
    m_desired=QJsonDocument::fromJson(profileStore().value("profile").toByteArray()).object();
    m_haveLocalProfile=!m_desired.isEmpty();
    if (m_desired.isEmpty()) m_desired=QJsonObject{{"font",1},{"color","ffffff"},{"lead",0},{"blink",1},
        {"bmode",1},{"bcolor","063c6d"},{"pal",0},{"bbright",200},{"speed",50},{"scale",50},{"angle",0},
        {"autoB",0},{"bright",100},{"bmin",10},{"bmax",255},{"adcMin",0},{"adcMax",1023},
        {"night",0},{"ncolor","ff0000"},{"nthr",50},{"gmt",180},{"ntp","pool.ntp.org"},{"showIp",1},{"ssid",""}};
    if(!m_desired.contains("apOnly")) m_desired.insert("apOnly",1);
    m_art=QJsonDocument::fromJson(profileStore().value("artNames").toByteArray()).object();
    for(int i=0;i<ART_SLOTS;++i){const QByteArray data=profileStore().value("artScene"+QString::number(i)).toByteArray();if(artDataValid(data.constData()))m_artScenes[i]=data;}
    refreshPorts();
    connect(m_connectionMode,&QComboBox::currentIndexChanged,this,[this,refresh](int index) {
        if(m_clock->isOpen()) return;
        m_clock=index==0 ? static_cast<ClockTransport *>(m_usb) : static_cast<ClockTransport *>(m_wifi);
        refresh->setEnabled(index==0); updateConnectionUi();
    });
    m_connectionMode->setCurrentIndex(profileStore().value("connectionMode",0).toInt()==1 ? 1 : 0);
    updateConnectionUi();
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_connect, &QPushButton::clicked, this, [this]() {
        if (m_clock->isOpen()) { m_disconnectRequested=true; apply(); return; }
        const QString endpoint=m_clock==m_usb ? m_ports->currentText() : m_networkAddress->text();
        if (endpoint.trimmed().isEmpty()) { log("Не указан COM-порт или IP часов"); return; }
        if (m_clock->open(endpoint)) {
            updateConnectionUi();
            m_status->setText(m_clock==m_usb ? "Ожидание запуска платы и проверки USB..." : "Соединение с часами по Wi-Fi...");
        }
    });
    for(ClockTransport *transport:{static_cast<ClockTransport *>(m_usb),static_cast<ClockTransport *>(m_wifi)}) {
    connect(transport,&ClockTransport::message,this,[this,transport](const QString &text) { if(transport==m_clock) log(text); });
    connect(transport, &ClockTransport::failure, this, [this,transport](const QString &text) {
        if(transport!=m_clock) return;
        if(m_applying) { m_autoSaveBlocked=true; m_debounce->stop(); m_apply->setText("Ошибка сохранения — повторить"); }
        log("Ошибка: " + text); m_status->setText(text); m_applying = false;
        m_stopping=false; m_sent.clear();
        m_apply->setEnabled(m_clock->hasTelemetry() && !m_dirty.isEmpty());
        updateConnectionUi();
        if((m_disconnectRequested || m_closing) && m_dirty.isEmpty()) {
            QTimer::singleShot(0,this,[this]() { m_clock->close(); if(m_closing) close(); });
        }
    });
    connect(transport, &ClockTransport::readyChanged, this, [this,transport](bool ready) {
        if(transport!=m_clock) return;
        if (!ready) {
            m_status->setText("Соединение отключено. Часы продолжают работу с последними сохранёнными настройками.");
            m_apply->setEnabled(false); m_applying = false; m_timeSynced=false;
            m_disconnectRequested=false; m_stopping=false; m_wifiJoinRequested=false;
        } else {
            m_autoSaveBlocked=false;
            m_status->setText("Соединение установлено. Проверка режима Qt → часы...");
            QTimer::singleShot(0, this, [this]() { if (m_clock->isReady() && !m_clock->isBusy()) m_clock->send({"state"}); });
        }
        updateConnectionUi();
    });
    connect(transport,&ClockTransport::stateReceived,this,[this,transport](const QJsonObject &state) { if(transport==m_clock) adoptState(state); });
    connect(transport, &ClockTransport::pixelsAcknowledged, this, [this,transport](const QByteArray &) {
        if(transport!=m_clock) return;
        m_lastFrame.restart();
        m_status->setText(m_clock==m_usb ? "Qt → часы по USB: кадр показан на ленте." : "Qt → часы по Wi-Fi: кадр показан на ленте.");
    });
    connect(transport, &ClockTransport::completed, this, [this,transport]() {
        if(transport!=m_clock) return;
        if (m_applying) {
            m_applying = false;
            auto *password=qobject_cast<QLineEdit *>(m_fields.value("pass"));
            if(m_sent.contains("pass") && password->text()==m_sent.value("pass")) password->clear();
            for(auto it=m_sent.begin();it!=m_sent.end();++it) if(m_dirty.value(it.key())==it.value()) m_dirty.remove(it.key());
            m_sent.clear(); m_apply->setEnabled(!m_dirty.isEmpty() || !password->text().isEmpty());
            m_apply->setText(m_dirty.isEmpty() ? "Сохранено в памяти часов" : "Сохраняются новые изменения...");
            persistProfile();
        }
        if (m_stopping) {
            m_stopping=false; m_clock->close();
            if(m_closing) QTimer::singleShot(0,this,&QWidget::close);
        }
    });
    }
    connect(m_apply, &QPushButton::clicked, this, [this]() { stageWifiPassword(); apply(); });
    connect(query, &QPushButton::clicked, this, [this]() { m_clock->send({m_clock->hasTelemetry() ? "state" : "?"}); });
    connect(sync, &QPushButton::clicked, this, [this]() {
        if (!m_clock->canStream() || m_clock->isBusy()) { log("Дождитесь подключения и повторите синхронизацию."); return; }
        const auto now = QDateTime::currentDateTime();
        m_clock->send({QString("time %1 %2").arg(now.toSecsSinceEpoch()).arg(m_desired.value("gmt").toInt()), "save", "state"});
    });
    connect(joinWifi,&QPushButton::clicked,this,[this]() {
        if(!m_clock->isReady()) { log("Сначала подключитесь к часам по USB или IP."); return; }
        if(!m_desired.value("apOnly").toInt() && m_desired.value("ssid").toString().isEmpty()) { log("Введите SSID и пароль в разделе Wi-Fi."); return; }
        stageWifiPassword();
        m_wifiJoinRequested=true; apply();
        log(m_desired.value("apOnly").toInt() ? "Будет включена точка BallClock без пароля. Подключите ПК к ней и используйте IP 192.168.4.1." : "Настройки Wi-Fi будут сохранены; затем часы подключатся к роутеру. Новый IP появится в состоянии часов.");
    });
    m_lastState.start(); m_lastFrame.start();
    m_animationClock.start();
    auto *timer = new QTimer(this); timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, &MainWindow::poll); timer->start();
    auto *renderTimer=new QTimer(this); renderTimer->setInterval(33);
    connect(renderTimer,&QTimer::timeout,this,&MainWindow::renderPreview); renderTimer->start();
    adoptState(QJsonObject{{"s",m_desired},{"t",QJsonObject{}}});
    log("Изменения автоматически передаются на часы и сохраняются. После отключения остаются последние подтверждённые параметры.");
}

MainWindow::~MainWindow() {
    persistProfile();
    profileStore().setValue("serialPort", m_ports->currentText());
    profileStore().setValue("connectionMode",m_connectionMode->currentIndex());
    profileStore().setValue("clockAddress",m_networkAddress->text());
    m_usb->disconnect(this); m_wifi->disconnect(this); m_usb->close(); m_wifi->close();
}

void MainWindow::updateConnectionUi() {
    const bool usb=m_clock==m_usb,opened=m_clock->isOpen();
    m_ports->setVisible(usb); m_networkAddress->setVisible(!usb);
    m_ports->setEnabled(!opened); m_networkAddress->setEnabled(!opened); m_connectionMode->setEnabled(!opened);
    m_connect->setText(opened ? (usb?"Отключить USB":"Отключить Wi-Fi") : (usb?"Подключить USB":"Подключить Wi-Fi"));
}

void MainWindow::stageWifiPassword() {
    const QString password=qobject_cast<QLineEdit *>(m_fields.value("pass"))->text();
    if(!password.isEmpty()) changed("pass",password);
}

void MainWindow::refreshPorts() {
    if (m_clock->isOpen()) return;
    QString previous = m_ports->currentText();
    if (previous.isEmpty()) previous = profileStore().value("serialPort").toString();
    m_ports->clear(); m_ports->addItems(UsbClock::ports());
    const int index = m_ports->findText(previous); if (index >= 0) m_ports->setCurrentIndex(index);
}

void MainWindow::log(const QString &text) {
    m_log->append(QTime::currentTime().toString("HH:mm:ss ") + text.toHtmlEscaped());
}

void MainWindow::buildSettings(QVBoxLayout *layout) {
    const QList<QPair<QString, QList<Field>>> groups = {
        {"Часы", {{"font","Шрифт",'e',0,2,{"Нет","Диагональный","Прямой 4×5"}},
                    {"color","Цвет",'c'}, {"blink","Мигание двоеточия",'b'}, {"lead","Ведущий ноль",'b'}}},
        {"Фон", {{"bmode","Режим",'e',0,4,{"Без фона","Градиент","Перлин","Однотонный","Рисование"}},
                 {"drawSlot","Картина",'e',0,7,{"Картина 1","Картина 2","Картина 3","Картина 4","Картина 5","Картина 6","Картина 7","Картина 8"}},
                 {"drawDigits","Время поверх рисунка",'b'},
                 {"bcolor","Цвет фона",'c'},
                 {"pal","Палитра",'e',0,8,{"Закат","Океан","Лес","Неон","Радуга","Вечеринка","Лава","Тепло","Облака"}},
                 {"bbright","Яркость фона",'n'}, {"speed","Скорость",'n',0,100},
                 {"scale","Масштаб",'n',0,100}, {"angle","Угол",'n',-180,180}}},
        {"Яркость", {{"autoB","Автояркость",'b'}, {"bright","Ручная яркость",'n'},
                      {"bmin","Мин. яркость",'n'}, {"bmax","Макс. яркость",'n'},
                      {"adcMin","Мин. сигнал датчика",'n',0,1023}, {"adcMax","Макс. сигнал датчика",'n',0,1023}}},
        {"Ночной режим (по датчику)", {{"night","Включён",'b'}, {"ncolor","Ночной цвет",'c'}, {"nthr","Порог",'n',0,1023}}},
        {"Время", {{"gmt","Смещение UTC, минут",'n',-720,840}, {"ntp","NTP-сервер",'t',0,39}}},
        {"Wi-Fi", {{"apOnly","Точка доступа BallClock (без роутера, без пароля)",'b'}, {"showIp","Показывать IP при подключении",'b'}, {"ssid","SSID роутера (если точка доступа выключена)",'t',0,32},
                    {"pass","Новый пароль (отправка кнопкой подключения или сохранения)",'p',0,64}}}
    };
    for (const auto &group : groups) {
        auto *box = new QGroupBox(group.first); auto *form = new QFormLayout(box); layout->addWidget(box);
        for (const auto &f : group.second) {
            const QString key = f.key; QWidget *w = nullptr,*rowWidget=nullptr;
            if (f.type == 'b') {
                auto *c = new QCheckBox; w = c;
                connect(c, &QCheckBox::toggled, this, [this,key](bool v) { changed(key, v ? "1" : "0"); });
            } else if (f.type == 'e') {
                auto *c = new QComboBox; c->addItems(f.options); w = c;
                connect(c, &QComboBox::currentIndexChanged, this, [this,key](int v) { changed(key, QString::number(v)); });
            } else if (f.type == 'n') {
                auto *c = new QSpinBox; c->setRange(f.lo,f.hi); w = c;
                auto *slider=new QSlider(Qt::Horizontal); slider->setRange(f.lo,f.hi);
                slider->setObjectName(key+"Slider");
                rowWidget=new QWidget; auto *row=new QHBoxLayout(rowWidget); row->setContentsMargins(0,0,0,0);
                row->addWidget(slider,1); row->addWidget(c); c->setMaximumWidth(90);
                connect(slider,&QSlider::valueChanged,c,&QSpinBox::setValue);
                connect(c,&QSpinBox::valueChanged,slider,&QSlider::setValue);
                connect(c, &QSpinBox::valueChanged, this, [this,key](int v) { changed(key, QString::number(v)); });
            } else if (f.type == 'c') {
                auto *c = new QPushButton("Выбрать"); w = c;
                connect(c, &QPushButton::clicked, this, [this,key,c]() {
                    const QColor color = QColorDialog::getColor(QColor("#" + c->property("rgb").toString()), this);
                    if (!color.isValid()) return;
                    const QString rgb = color.name().mid(1); c->setProperty("rgb",rgb); c->setText(color.name());
                    c->setStyleSheet("background-color:" + color.name()); changed(key,rgb);
                });
            } else {
                auto *c = new QLineEdit; c->setMaxLength(f.hi); w = c;
                if (f.type == 'p') c->setEchoMode(QLineEdit::Password);
                connect(c, &QLineEdit::textEdited, this, [this,key](const QString &v) {
                    if(key=="pass") {
                        // Password is a draft until an explicit click, never a per-character setting.
                        m_dirty.remove("pass");
                        m_apply->setEnabled(m_clock->canStream() && (!m_dirty.isEmpty() || !v.isEmpty()));
                        m_apply->setText("Сохранить настройки и пароль");
                        return;
                    }
                    changed(key,v);
                });
            }
            m_fields[key] = w; form->addRow(QString::fromUtf8(f.label), rowWidget?rowWidget:w);
        }
        if(group.first=="Фон") {
            auto *create=new QPushButton("Создать свой градиент");create->setObjectName("createGradient");
            auto *edit=new QPushButton("Изменить выбранный градиент");edit->setObjectName("editGradient");
            form->addRow(create);form->addRow(edit);
            connect(create,&QPushButton::clicked,this,[this]{editGradient(false);});
            connect(edit,&QPushButton::clicked,this,[this]{editGradient(true);});
            auto *drawing=new QPushButton("Рисование — редактор выбранной картины");drawing->setObjectName("editDrawing");form->addRow(drawing);connect(drawing,&QPushButton::clicked,this,&MainWindow::editDrawing);
        }
    }
}

void MainWindow::editDrawing(){
    if(!m_clock->canStream()||m_clock->isBusy()||m_applying){QMessageBox::information(this,"Рисование","Подключите часы и дождитесь завершения запросов.");return;}
    const int slot=m_desired.value("drawSlot").toInt();
    if(m_desired.value("bmode").toInt()!=4||!m_artScenes.contains(slot)){changed("bmode","4");apply();QMessageBox::information(this,"Рисование","Включён режим рисования. Дождитесь чтения картины и откройте редактор ещё раз.");return;}
    m_artEditing=true;const auto names=m_art.value("names").toArray();ArtEditor editor(slot<names.size()?names[slot].toString():QString(),m_artScenes.value(slot),this);
    const int result=editor.exec();m_artEditing=false;if(result!=QDialog::Accepted)return;
    if(m_clock->isBusy()){QMessageBox::warning(this,"Рисование","Соединение занято. Картина не сохранена.");return;}
    const QString name=editor.name->text().trimmed();
    for(int i=0;i<names.size();++i)if(i!=slot&&names[i].toString().compare(name,Qt::CaseInsensitive)==0){QMessageBox::warning(this,"Рисование","Такое имя уже есть. Картина не сохранена.");return;}
    changed("bmode","4");changed("drawSlot",QString::number(slot));
    QStringList commands;commands<<"art put "+QString::number(slot)+" "+name+"|"+QString::fromLatin1(editor.encoded());
    m_sent=m_dirty;for(auto it=m_sent.begin();it!=m_sent.end();++it)commands<<"set "+it.key()+" "+it.value();commands<<"save"<<"state";
    m_applying=true;m_apply->setEnabled(false);m_clock->send(commands);
}

void MainWindow::updateGradientChoices() {
    auto *combo=qobject_cast<QComboBox *>(m_fields.value("pal"));
    while(combo->count()<17)combo->addItem("");
    auto *model=qobject_cast<QStandardItemModel *>(combo->model());
    for(int i=0;i<GRADIENT_SLOTS;++i) {
        const QString text=m_desired.value("grad"+QString::number(i)).toString();
        const bool valid=!text.isEmpty()&&gradientValid(text.toUtf8().constData());
        combo->setItemText(9+i,valid?text.section('|',0,0):QString("Свободное место %1").arg(i+1));
        if(model)model->item(9+i)->setEnabled(valid);
    }
}

void MainWindow::editGradient(bool existing) {
    int slot=m_desired.value("pal").toInt()-9;
    if(!existing) {for(slot=0;slot<GRADIENT_SLOTS;++slot)if(m_desired.value("grad"+QString::number(slot)).toString().isEmpty())break;}
    if(slot<0||slot>=GRADIENT_SLOTS) {QMessageBox::information(this,"Свои градиенты",existing?"Выберите свой сохранённый градиент в палитре.":"Все 8 мест заняты. Измените или удалите один из своих градиентов.");return;}
    const QString key="grad"+QString::number(slot),stored=m_desired.value(key).toString();
    uint32_t colors[4]={0xff6248,0x8028a0,0x38c3ff,0x152544};
    if(existing&&!gradientColors(stored.toUtf8().constData(),colors))return;
    QDialog dialog(this);dialog.setWindowTitle("Свой градиент — хранится в часах");
    auto *layout=new QVBoxLayout(&dialog);auto *name=new QLineEdit(existing?stored.section('|',0,0):QString());
    name->setObjectName("gradientName");name->setPlaceholderText("Название градиента");name->setMaxLength(64);layout->addWidget(name);
    auto *strip=new QLabel;strip->setMinimumHeight(40);layout->addWidget(strip);
    auto paint=[&]{QStringList stops;for(int i=0;i<4;++i)stops<<QString("stop:%1 #%2").arg(i/3.0).arg(colors[i],6,16,QChar('0'));strip->setStyleSheet("border-radius:8px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"+stops.join(',')+");");};paint();
    for(int i=0;i<4;++i){auto *color=new QPushButton(QString("Цвет %1 — выбрать").arg(i+1));color->setObjectName("gradientStop"+QString::number(i));color->setStyleSheet("background:"+QColor::fromRgb(colors[i]).name());layout->addWidget(color);
        connect(color,&QPushButton::clicked,&dialog,[&,i,color]{const QColor picked=QColorDialog::getColor(QColor::fromRgb(colors[i]),&dialog);if(!picked.isValid())return;colors[i]=picked.rgb()&0xffffff;color->setStyleSheet("background:"+picked.name());paint();});}
    auto *error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    if(existing){auto *remove=new QPushButton("Удалить градиент");layout->addWidget(remove);connect(remove,&QPushButton::clicked,&dialog,[&]{if(QMessageBox::question(&dialog,"Удаление","Удалить этот градиент из часов?")!=QMessageBox::Yes)return;changed(key,"");if(m_desired.value("pal").toInt()==9+slot)changed("pal","0");dialog.accept();});}
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{QStringList stops;for(auto color:colors)stops<<QString("%1").arg(color,6,16,QChar('0'));const QString clean=name->text().trimmed(),encoded=clean+"|"+stops.join(',');
        if(!gradientValid(encoded.toUtf8().constData())){error->setText("Введите имя до 32 русских букв (64 байта), без символа | и управляющих символов.");return;}
        for(int i=0;i<8;++i)if(i!=slot&&m_desired.value("grad"+QString::number(i)).toString().section('|',0,0).compare(clean,Qt::CaseInsensitive)==0){error->setText("Такое имя уже есть. Выберите другое.");return;}
        changed(key,encoded);changed("pal",QString::number(9+slot));changed("bmode","1");dialog.accept();});
    dialog.exec();m_loading=true;updateGradientChoices();qobject_cast<QComboBox *>(m_fields.value("pal"))->setCurrentIndex(m_desired.value("pal").toInt());qobject_cast<QComboBox *>(m_fields.value("bmode"))->setCurrentIndex(m_desired.value("bmode").toInt());m_loading=false;
}

void MainWindow::changed(const QString &key, const QString &value) {
    if (m_loading) return;
    m_autoSaveBlocked=false;
    if(key!="pass") {
        QWidget *w=m_fields.value(key);
        const bool number=qobject_cast<QCheckBox *>(w) || qobject_cast<QComboBox *>(w) || qobject_cast<QSpinBox *>(w);
        m_desired.insert(key,number ? QJsonValue(value.toInt()) : QJsonValue(value));
    }
    m_haveLocalProfile=true; persistProfile();
    if (key == "pass" && value.isEmpty()) m_dirty.remove(key);
    else if (value == valueText(m_state.value(key))) m_dirty.remove(key);
    else m_dirty[key] = value;
    m_apply->setEnabled(m_clock->canStream() && !m_dirty.isEmpty());
    m_apply->setText(m_clock->canStream() ? "Сохраняются изменения..." : "Сохранено в Qt; подключите часы");
    m_debounce->start();
}

void MainWindow::adoptState(const QJsonObject &state) {
    const bool fromDevice=state.contains("protocol");
    if(fromDevice) {
        m_lastState.restart(); m_state=state.value("s").toObject(); m_telemetry=state.value("t").toObject();
        m_art=state.value("art").toObject();const QByteArray art=m_art.value("data").toString().toLatin1();
        if(artDataValid(art.constData())){const int slot=m_state.value("drawSlot").toInt();if(m_artScenes.value(slot)!=art){m_artScenes[slot]=art;profileStore().setValue("artScene"+QString::number(slot),art);}}
        QJsonObject artNames{{"names",m_art.value("names")}};const auto namesBytes=QJsonDocument(artNames).toJson(QJsonDocument::Compact);if(profileStore().value("artNames").toByteArray()!=namesBytes)profileStore().setValue("artNames",namesBytes);
        // The library belongs to the clock, never replay stale profile copies.
        for(int i=0;i<GRADIENT_SLOTS;++i){const QString key="grad"+QString::number(i);if(m_state.contains(key)&&!m_dirty.contains(key))m_desired.insert(key,m_state.value(key));}
        if(!m_haveLocalProfile) { m_desired=m_state; m_haveLocalProfile=true; persistProfile(); }
        for(auto it=m_fields.begin();it!=m_fields.end();++it) {
            if(it.key()=="pass" || !m_desired.contains(it.key())) continue;
            const QString desired=valueText(m_desired.value(it.key()));
            if(desired==valueText(m_state.value(it.key()))) m_dirty.remove(it.key());
            else m_dirty[it.key()]=desired;
        }
        if(!m_clock->canStream()) m_status->setText("Нужна совместимая прошивка: передача изображения из Qt на часы.");
        if(m_clock==m_usb && (m_telemetry.value("wifi").toInt()==2 || m_telemetry.value("ap").toInt()==1)) {
            const QString ip=m_telemetry.value("ip").toString();
            if(!ip.isEmpty() && ip!="0.0.0.0") { m_networkAddress->setText(ip); profileStore().setValue("clockAddress",ip); }
        }
    }
    m_loading = true;
    updateGradientChoices();
    const auto artNames=m_art.value("names").toArray();auto *artCombo=qobject_cast<QComboBox *>(m_fields.value("drawSlot"));
    for(int i=0;i<artCombo->count();++i)artCombo->setItemText(i,i<artNames.size()&&!artNames[i].toString().isEmpty()?artNames[i].toString():QString("Свободная картина %1").arg(i+1));
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
        if (!m_desired.contains(it.key())) continue;
        const auto value = m_desired.value(it.key()); QWidget *w = it.value();
        if (auto *c = qobject_cast<QCheckBox *>(w)) c->setChecked(value.toInt() != 0);
        else if (auto *c = qobject_cast<QComboBox *>(w)) c->setCurrentIndex(value.toInt());
        else if (auto *c = qobject_cast<QSpinBox *>(w)) c->setValue(value.toInt());
        else if (auto *c = qobject_cast<QLineEdit *>(w)) c->setText(value.toString());
        else if (auto *c = qobject_cast<QPushButton *>(w)) {
            c->setProperty("rgb",value.toString()); c->setText("#" + value.toString());
            c->setStyleSheet("background-color:#" + value.toString());
        }
    }
    m_loading = false;
    const auto t = m_telemetry;
    m_info->setText(QString("Время на часах: %1 | Датчик: %2 | Ночной режим: %3 | IP: %4")
                   .arg(t.value("time").toString()).arg(t.value("adc").toInt())
                   .arg(t.value("night").toInt() ? "активен" : "не активен").arg(t.value("ip").toString()));
}

void MainWindow::apply() {
    if(m_autoSaveBlocked && sender()!=m_apply) return;
    m_autoSaveBlocked=false;
    if (m_applying || !m_clock->canStream() || m_dirty.isEmpty()) return;
    if (m_clock->isBusy()) { QTimer::singleShot(100, this, &MainWindow::apply); return; }
    QStringList commands;
    m_sent=m_dirty;
    for (auto it = m_dirty.begin(); it != m_dirty.end(); ++it) commands << "set " + it.key() + " " + it.value();
    commands << "save" << "state";
    m_applying = true; m_apply->setEnabled(false); m_clock->send(commands);
}

void MainWindow::poll() {
    if(m_artEditing)return;
    if (!m_clock->isReady() || !m_clock->canStream() || m_clock->isBusy()) return;
    if(!m_dirty.isEmpty()) { if(!m_autoSaveBlocked && !m_debounce->isActive()) apply(); return; }
    if(m_disconnectRequested || m_closing) { m_stopping=true; m_clock->send({"stop"}); return; }
    if(m_wifiJoinRequested) { m_wifiJoinRequested=false; m_clock->send({"wifi_connect"}); return; }
    if(!m_timeSynced) {
        m_timeSynced=true;
        m_clock->send({QString("time %1 %2").arg(QDateTime::currentSecsSinceEpoch()).arg(m_desired.value("gmt").toInt()),"save","state"});
        return;
    }
    if(m_lastState.elapsed()>1000) { m_clock->send({"state"}); return; }
    renderPreview();
    m_clock->sendPixels(m_raw);
}

void MainWindow::persistProfile() {
    QJsonObject profile=m_desired; profile.remove("pass");
    profileStore().setValue("profile",QJsonDocument(profile).toJson(QJsonDocument::Compact));
}

void MainWindow::renderPreview() {
    auto v=[this](const char *key,int fallback=0) { return m_desired.value(key).toInt(fallback); };
    ClockParameters p;
    p.drawDigits=v("drawDigits");const QByteArray art=m_artScenes.value(v("drawSlot"));if(artDataValid(art.constData()))artDecode(art.constData(),p.art);
    p.font=v("font",1); p.color=m_desired.value("color").toString("ffffff").toUInt(nullptr,16);
    p.lead=v("lead"); p.blink=v("blink",1); p.bmode=v("bmode",1); p.pal=v("pal");
    p.bbright=v("bbright",200); p.speed=v("speed",50); p.scale=v("scale",50); p.angle=v("angle");
    p.bcolor=m_desired.value("bcolor").toString("063c6d").toUInt(nullptr,16);
    const auto gradient=m_desired.value("grad"+QString::number(p.pal-9)).toString().toUtf8();
    if(gradient.size()<int(sizeof(p.customGradient)))memcpy(p.customGradient,gradient.constData(),gradient.size());
    p.ncolor=m_desired.value("ncolor").toString("ff0000").toUInt(nullptr,16);
    const int adc=m_telemetry.value("adc").toInt(512);
    if(!v("night")) m_nightActive=false;
    else if(!m_nightActive && adc<v("nthr",50)) m_nightActive=true;
    else if(m_nightActive && adc>v("nthr",50)*1.3+5) m_nightActive=false;
    p.night=m_nightActive;
    p.bright=v("bright",100);
    if(v("autoB") && v("adcMax",1023)>v("adcMin")) {
        const int low=v("bmin",10),high=v("bmax",255);
        p.bright=qBound(qMin(low,high),low+(adc-v("adcMin"))*(high-low)/(v("adcMax",1023)-v("adcMin")),qMax(low,high));
    }
    const auto now=QDateTime::currentDateTimeUtc().addSecs(v("gmt",180)*60).time();
    QString error;
    if(!renderOnPc(p,now.hour(),now.minute(),quint32(m_animationClock.elapsed()),m_raw,m_shown,error)) {
        m_status->setText("Не удалось загрузить отрисовку ПК: "+error); return;
    }
    m_display->setFrame(m_shown);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    persistProfile();
    if(!m_clock->isReady() || !m_clock->canStream()) { event->accept(); return; }
    m_closing=true; m_disconnectRequested=true;
    m_status->setText("Сохранение параметров в часах перед закрытием...");
    event->ignore();
    apply();
}
