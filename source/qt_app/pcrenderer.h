#pragma once
#include "native/clockengine.h"
#include <QByteArray>
#include <QString>
bool renderOnPc(const ClockParameters &parameters, int hour, int minute, quint32 milliseconds,
                QByteArray &raw, QByteArray &shown, QString &error);
