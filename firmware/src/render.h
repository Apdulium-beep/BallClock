#pragma once
#include <FastLED.h>
#include "settings.h"

// Рисует кадр с часами и фоном по настройкам S. leds — в порядке ленты.
// timeOk=false: вместо цифр рисуются прочерки. nightActive: ночной режим сейчас включён.
void drawFrame(CRGB* leds, uint8_t hour, uint8_t minute, uint32_t ms, bool timeOk, bool nightActive);

// Показывает число 0..999 (например, часть IP-адреса) на чёрном фоне.
void drawNumber(CRGB* leds, uint16_t n);
