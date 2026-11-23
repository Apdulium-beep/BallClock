#pragma once
#include <Arduino.h>
void netBegin();
void netTick();
void netConnect(const char* ssid, const char* pass);   // применить режим, не выключая точку BallClock
bool netConnected();
bool netApMode();
bool netTakeConnectEvent();                            // true один раз после каждого успешного подключения
uint32_t netApStartCount();
