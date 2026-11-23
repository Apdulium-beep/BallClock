#pragma once
#include <QDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>
#include <functional>
#include "artcodec.h"
#include "geometry.h"

class ArtCanvas:public QWidget {
public:
    ArtPixel pixels[ART_COUNT];QSet<int> selected;std::function<void(int)> clicked;
    QElapsedTimer elapsed;
    explicit ArtCanvas(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(440,180);elapsed.start();auto *timer=new QTimer(this);timer->setInterval(40);connect(timer,&QTimer::timeout,this,[this]{update();});timer->start();geo::init();}
    QPointF point(int i)const{const double scale=qMin(width()/42.0,height()/15.0);return QPointF(width()/2.0+(geo::ledX[i]-19)*scale,height()/2.0+(geo::ledRow[i]-3)*1.732*scale);}
    void paintEvent(QPaintEvent *)override{QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.fillRect(rect(),QColor("#101923"));const double radius=qMin(width()/42.0,height()/15.0)*.79;
        for(int i=0;i<ART_COUNT;++i){uint8_t rgb[3];artColor(pixels[i],uint32_t(elapsed.elapsed()),rgb);painter.setBrush(QColor(rgb[0],rgb[1],rgb[2]));painter.setPen(QPen(selected.contains(i)?QColor("#91e0c2"):QColor("#536477"),selected.contains(i)?3:1));painter.drawEllipse(point(i),radius,radius);}}
    void hit(QPointF pos){const double radius=qMin(width()/42.0,height()/15.0);for(int i=0;i<ART_COUNT;++i)if(QLineF(pos,point(i)).length()<=radius){if(clicked)clicked(i);update();return;}}
    void mousePressEvent(QMouseEvent *e)override{if(e->button()==Qt::LeftButton)hit(e->position());}
    void mouseMoveEvent(QMouseEvent *e)override{if(e->buttons()&Qt::LeftButton)hit(e->position());}
};

class ArtEditor:public QDialog {
public:
    ArtCanvas *canvas;QLineEdit *name;QComboBox *effect,*tool;QSlider *period,*phase;ArtPixel brush;QLabel *info;QPushButton *colors[4];
    explicit ArtEditor(const QString &title,const QByteArray &data,QWidget *parent=nullptr):QDialog(parent){
        setWindowTitle("Рисование — картина в памяти часов");resize(760,720);auto *layout=new QVBoxLayout(this);name=new QLineEdit(title);name->setPlaceholderText("Название картины");layout->addWidget(name);
        canvas=new ArtCanvas;canvas->setObjectName("artCanvas");if(artDataValid(data.constData()))artDecode(data.constData(),canvas->pixels);layout->addWidget(canvas,1);
        auto *row=new QHBoxLayout;tool=new QComboBox;tool->addItems({"Выделение шариков","Кисть","Пипетка"});row->addWidget(tool);auto *all=new QPushButton("Выделить всё");auto *none=new QPushButton("Снять выделение");row->addWidget(all);row->addWidget(none);layout->addLayout(row);
        effect=new QComboBox;effect->addItems({"Выключен","Постоянный цвет","Плавное переливание 4 цветов","Мигание 4 цветов"});effect->setCurrentIndex(1);brush.effect=1;brush.rgb[0]=255;brush.rgb[6]=255;layout->addWidget(effect);
        auto *colorRow=new QHBoxLayout;for(int i=0;i<4;++i){colors[i]=new QPushButton(QString("Цвет %1").arg(i+1));colorRow->addWidget(colors[i]);connect(colors[i],&QPushButton::clicked,this,[this,i]{auto *p=brush.rgb+i*3;const QColor picked=QColorDialog::getColor(QColor(p[0],p[1],p[2]),this);if(!picked.isValid())return;p[0]=picked.red();p[1]=picked.green();p[2]=picked.blue();refreshBrush();});}layout->addLayout(colorRow);
        auto *speedLabel=new QLabel;period=new QSlider(Qt::Horizontal);period->setRange(1,255);period->setValue(20);layout->addWidget(speedLabel);layout->addWidget(period);
        auto *phaseLabel=new QLabel;phase=new QSlider(Qt::Horizontal);phase->setRange(0,255);layout->addWidget(phaseLabel);layout->addWidget(phase);
        connect(period,&QSlider::valueChanged,this,[this,speedLabel](int v){brush.period=v;speedLabel->setText(QString("Полный цикл: %1 с").arg(v/10.0,0,'f',1));});period->valueChanged(20);
        connect(phase,&QSlider::valueChanged,this,[this,phaseLabel](int v){brush.phase=v;phaseLabel->setText(QString("Сдвиг анимации: %1°").arg(v*360/256));});phase->valueChanged(0);
        connect(effect,&QComboBox::currentIndexChanged,this,[this](int v){brush.effect=v;if(v>=2&&!brush.period){brush.period=20;period->setValue(20);}refreshBrush();});
        auto *actions=new QHBoxLayout;auto *apply=new QPushButton("Применить к выделенным");auto *fill=new QPushButton("Залить всё кистью");auto *clear=new QPushButton("Очистить выделенные");actions->addWidget(apply);actions->addWidget(fill);actions->addWidget(clear);layout->addLayout(actions);
        info=new QLabel("Выберите шарики; настройте кисть и примените. Изменения — черновик до сохранения.");info->setWordWrap(true);layout->addWidget(info);
        connect(all,&QPushButton::clicked,this,[this]{for(int i=0;i<ART_COUNT;++i)canvas->selected.insert(i);canvas->update();});connect(none,&QPushButton::clicked,this,[this]{canvas->selected.clear();canvas->update();});
        connect(apply,&QPushButton::clicked,this,[this]{for(int i:canvas->selected)canvas->pixels[i]=brush;canvas->update();});connect(fill,&QPushButton::clicked,this,[this]{for(auto &p:canvas->pixels)p=brush;canvas->update();});connect(clear,&QPushButton::clicked,this,[this]{for(int i:canvas->selected)canvas->pixels[i]=ArtPixel();canvas->update();});
        canvas->clicked=[this](int i){if(tool->currentIndex()==1)canvas->pixels[i]=brush;else if(tool->currentIndex()==2){brush=canvas->pixels[i];effect->setCurrentIndex(brush.effect);period->setValue(qMax(1,int(brush.period)));phase->setValue(brush.phase);refreshBrush();}else canvas->selected.insert(i);info->setText(QString("Шарик %1 · выделено %2. До сохранения часы не меняются.").arg(i+1).arg(canvas->selected.size()));};
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[this]{if(!artNameValid(name->text().trimmed().toUtf8().constData())){info->setText("Введите имя: до 64 байт UTF-8, без | и управляющих символов.");return;}accept();});refreshBrush();
    }
    void refreshBrush(){for(int i=0;i<4;++i){auto *p=brush.rgb+i*3;colors[i]->setStyleSheet("background:"+QColor(p[0],p[1],p[2]).name());colors[i]->setEnabled(brush.effect>=2||i==0);}}
    QByteArray encoded()const{QByteArray out(ART_DATA+1,0);artEncode(canvas->pixels,out.data());out.resize(ART_DATA);return out;}
};
