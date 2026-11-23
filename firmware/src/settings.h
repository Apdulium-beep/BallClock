#pragma once
#include <Arduino.h>
#include "gradient.h"

enum FontStyle : uint8_t { FS_NONE = 0, FS_DIAG = 1, FS_4X5 = 2 };   // шрифт часов
enum BgMode    : uint8_t { BG_NONE = 0, BG_GRADIENT = 1, BG_NOISE = 2, BG_SOLID = 3, BG_DRAW = 4 };

struct Settings {
  // Часы
  uint8_t  font   = FS_DIAG;    // FontStyle
  uint32_t color  = 0xFFFFFF;
  bool     lead   = false;      // ведущий ноль (07:30)
  bool     blink  = true;       // мигание двоеточия
  // Фон
  uint8_t  bmode   = BG_GRADIENT; // BgMode
  uint8_t  pal     = 0;
  uint8_t  bbright = 200;
  uint32_t bcolor  = 0x063C6D;  // отдельный цвет однотонного фона
  char gradients[GRADIENT_SLOTS][GRADIENT_BYTES] = {};
  uint8_t drawSlot=0;
  bool drawDigits=false;
  uint8_t  speed   = 50;
  uint8_t  scale   = 50;
  int16_t  angle   = 0;         // -180..180
  // Яркость
  bool     autoB  = false;
  uint8_t  bright = 100;
  uint8_t  bmin   = 10, bmax = 255;
  uint16_t adcMin = 0, adcMax = 1023;
  // Ночной режим
  bool     night  = false;
  uint32_t ncolor = 0xFF0000;
  uint16_t nthr   = 50;
  // Время
  int16_t  gmt    = 60;         // смещение от UTC в минутах
  char     ntp[40] = "pool.ntp.org";
  // WiFi
  bool     apOnly = true;       // прямое управление через точку BallClock
  bool     showIp = true;
  char     ssid[33] = "";
  char     pass[65] = "";
};
extern Settings S;

void   settingsBegin();                                  // LittleFS + загрузка
void   settingsTick();                                   // отложенное сохранение (через 2 с после изменения)
bool   settingsSaveNow();
bool   settingsAreSaved();
bool   settingsSet(const String& key, const String& value);  // false, если ключ неизвестен
bool   settingsValidate(const String& key, const String& value); // strict validation for API v1
String settingsToJson();                                 // пароль не отдаётся
