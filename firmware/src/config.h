#pragma once
#include <stdint.h>

// ---- Железо ----
constexpr uint8_t  LED_PIN   = 14;    // GPIO14 = D5 на Wemos D1 mini
constexpr uint16_t MAX_MA    = 2500;  // лимит тока ленты (блок питания 3-4 А, запас)
constexpr uint8_t  LIGHT_PIN = A0;    // фоторезистор

// ---- Кадры ----
constexpr uint16_t FRAME_MS = 25;     // соответствует ответу подключённых часов: период 25 мс

// ---- Сеть ----
#define HOSTNAME "ballclock"          // адрес в сети: http://ballclock.local
#define AP_NAME  "BallClock"          // точка доступа для первой настройки
