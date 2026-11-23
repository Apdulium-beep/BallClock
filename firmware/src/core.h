#pragma once
#include <Arduino.h>
extern float lightAvg;             // сглаженное значение фоторезистора (A0)
bool timeValid();                  // время синхронизировано (NTP или с телефона)
void timeApply();                  // применить часовой пояс и NTP-сервер из настроек
void setClock(uint32_t epoch);     // установить время вручную (UTC, секунды)
