#include "mainwindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QJsonDocument>
#include <cstdio>
#include <QTemporaryDir>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTimer>
#include <QPushButton>
#include "arteditor.h"

class SettingsUiTest {
public:
    static int run() {
        MainWindow window;
        const auto state = QJsonDocument::fromJson(R"({"protocol":2,"s":{"autoB":1,"bright":90,"font":2,"color":"12abef","pal":8,"night":1,"speed":72},"t":{"time":"08:15:33","adc":42,"night":1,"ip":"192.168.4.1"}})").object();
        window.adoptState(state);
        if (!window.m_dirty.isEmpty()) return 1; // reading must never send/dirty any field
        if (!qobject_cast<QCheckBox *>(window.m_fields["autoB"])->isChecked()) return 2;
        auto *bright = qobject_cast<QSpinBox *>(window.m_fields["bright"]);
        if (bright->value() != 90) return 3;
        bright->setValue(91);
        if (window.m_dirty.size()!=1 || window.m_dirty.value("bright")!="91") return 4;
        window.adoptState(state);
        if (bright->value()!=91 || window.m_dirty.contains("autoB")) return 5; // retain unsent edits
        bright->setValue(90);
        if (!window.m_dirty.isEmpty()) return 6;
        auto shared=state;auto settings=shared.value("s").toObject();
        settings.insert("grad0","Северное сияние|123456,38c3ff,91e0c2,ffffff");shared.insert("s",settings);
        window.adoptState(shared);
        auto *pal=qobject_cast<QComboBox *>(window.m_fields["pal"]);
        if(pal->itemText(9)!="Северное сияние"||!window.m_dirty.isEmpty())return 7;
        window.changed("pal","9");
        QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;dialog->findChild<QLineEdit *>("gradientName")->setText("Мой закат");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();});
        window.editGradient(true);
        if(!window.m_dirty.value("grad0").startsWith("Мой закат|"))return 8;
        window.adoptState(shared);
        if(!window.m_desired.value("grad0").toString().startsWith("Мой закат|"))return 9;
        window.m_dirty.clear();window.adoptState(shared);
        if(pal->itemText(9)!="Северное сияние")return 10;
        ArtEditor drawing("Картина",QByteArray(3712,'0'));
        drawing.canvas->clicked(1);
        for(auto *button:drawing.findChildren<QPushButton *>())if(button->text()=="Применить к выделенным")button->click();
        if(drawing.canvas->pixels[1].effect!=1||drawing.canvas->pixels[0].effect!=0||!artDataValid(drawing.encoded().constData()))return 11;
        drawing.tool->setCurrentIndex(1);drawing.effect->setCurrentIndex(3);drawing.phase->setValue(64);drawing.canvas->clicked(127);
        if(drawing.canvas->pixels[127].effect!=3||drawing.canvas->pixels[127].phase!=64)return 12;
        std::puts("PASS: native drawing editor selected-pixel apply, brush, per-pixel phase and encoded export.");
        std::puts("PASS: clock-owned gradient names load in Qt without writes; visual dialog rename stages preset; incoming reads retain unsent edits.");
        std::puts("PASS: actual settings loaded without writes; auto-brightness retained; only edited fields dirty.");
        return 0;
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); QTemporaryDir profile;
    qputenv("BALLCLOCK_PROFILE_FILE",(profile.path()+"/profile.ini").toUtf8());
    return SettingsUiTest::run();
}
