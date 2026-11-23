#include "settings.h"
#include <LittleFS.h>
#include <errno.h>

Settings S;
static bool dirty = false;
static uint32_t dirtyAt = 0;
static const char* PATH = "/settings.txt";
static const uint8_t FILE_VER = 2;   // версия формата файла: 2 = нумерация шрифтов 0/1/2
static uint8_t fileVer = 0;

namespace {
// ---- Таблица настроек: единственное место, где описана каждая настройка ----
// Чтобы добавить настройку: поле в struct Settings + одна строка здесь.
enum Kind : uint8_t { K_U8, K_U16, K_I16, K_BOOL, K_COLOR, K_STR, K_GRAD };
struct Field {
  const char* key;    // имя в JSON, в файле и в запросах
  Kind        kind;
  void*       ptr;    // куда писать
  int16_t     lo, hi; // допустимый диапазон (для чисел)
  uint8_t     size;   // размер буфера (для строк)
  bool        secret; // не отдавать наружу в JSON
};

const Field FIELDS[] = {
  // Часы
  {"font",    K_U8,    &S.font,    0, 2,    0, false},
  {"color",   K_COLOR, &S.color,   0, 0,    0, false},
  {"lead",    K_BOOL,  &S.lead,    0, 0,    0, false},
  {"blink",   K_BOOL,  &S.blink,   0, 0,    0, false},
  // Фон
  {"bmode",   K_U8,    &S.bmode,   0, 4,    0, false},
  {"drawSlot", K_U8, &S.drawSlot, 0, 7, 0, false},
  {"drawDigits", K_BOOL, &S.drawDigits, 0, 0, 0, false},
  {"bcolor",  K_COLOR, &S.bcolor,  0, 0,    0, false},
  {"pal",     K_U8,    &S.pal,     0, 16,   0, false},
  {"grad0", K_GRAD, S.gradients[0], 0, 0, GRADIENT_BYTES, false},
  {"grad1", K_GRAD, S.gradients[1], 0, 0, GRADIENT_BYTES, false},
  {"grad2", K_GRAD, S.gradients[2], 0, 0, GRADIENT_BYTES, false},
  {"grad3", K_GRAD, S.gradients[3], 0, 0, GRADIENT_BYTES, false},
  {"grad4", K_GRAD, S.gradients[4], 0, 0, GRADIENT_BYTES, false},
  {"grad5", K_GRAD, S.gradients[5], 0, 0, GRADIENT_BYTES, false},
  {"grad6", K_GRAD, S.gradients[6], 0, 0, GRADIENT_BYTES, false},
  {"grad7", K_GRAD, S.gradients[7], 0, 0, GRADIENT_BYTES, false},
  {"bbright", K_U8,    &S.bbright, 0, 255,  0, false},
  {"speed",   K_U8,    &S.speed,   0, 100,  0, false},
  {"scale",   K_U8,    &S.scale,   0, 100,  0, false},
  {"angle",   K_I16,   &S.angle,  -180, 180, 0, false},
  // Яркость
  {"autoB",   K_BOOL,  &S.autoB,   0, 0,    0, false},
  {"bright",  K_U8,    &S.bright,  0, 255,  0, false},
  {"bmin",    K_U8,    &S.bmin,    0, 255,  0, false},
  {"bmax",    K_U8,    &S.bmax,    0, 255,  0, false},
  {"adcMin",  K_U16,   &S.adcMin,  0, 1023, 0, false},
  {"adcMax",  K_U16,   &S.adcMax,  0, 1023, 0, false},
  // Ночной режим
  {"night",   K_BOOL,  &S.night,   0, 0,    0, false},
  {"ncolor",  K_COLOR, &S.ncolor,  0, 0,    0, false},
  {"nthr",    K_U16,   &S.nthr,    0, 1023, 0, false},
  // Время
  {"gmt",     K_I16,   &S.gmt,    -720, 840, 0, false},
  {"ntp",     K_STR,   S.ntp,      0, 0, sizeof(S.ntp), false},
  // WiFi
  {"apOnly",  K_BOOL,  &S.apOnly,  0, 0,    0, false},
  {"showIp",  K_BOOL,  &S.showIp,  0, 0,    0, false},
  {"ssid",    K_STR,   S.ssid,     0, 0, sizeof(S.ssid), false},
  {"pass",    K_STR,   S.pass,     0, 0, sizeof(S.pass), true},
};

String hexStr(uint32_t c) { char b[8]; snprintf(b, sizeof(b), "%06x", (unsigned)(c & 0xFFFFFF)); return b; }

void store(const Field& f, const String& v) {
  const long n = constrain(v.toInt(), (long)f.lo, (long)f.hi);   // для цветов и строк диапазон не используется
  switch (f.kind) {
    case K_U8:    *(uint8_t*)f.ptr  = n; break;
    case K_U16:   *(uint16_t*)f.ptr = n; break;
    case K_I16:   *(int16_t*)f.ptr  = n; break;
    case K_BOOL:  *(bool*)f.ptr     = v.toInt() != 0; break;
    case K_COLOR: *(uint32_t*)f.ptr = strtoul(v.c_str(), nullptr, 16) & 0xFFFFFF; break;
    case K_STR: case K_GRAD: strlcpy((char*)f.ptr, v.c_str(), f.size); break;
  }
}

String valueOf(const Field& f) {
  switch (f.kind) {
    case K_U8:    return String(*(uint8_t*)f.ptr);
    case K_U16:   return String(*(uint16_t*)f.ptr);
    case K_I16:   return String(*(int16_t*)f.ptr);
    case K_BOOL:  return *(bool*)f.ptr ? "1" : "0";
    case K_COLOR: return hexStr(*(uint32_t*)f.ptr);
    default:      return String((const char*)f.ptr);
  }
}

String escaped(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if ((uint8_t)c >= 32) o += c;
  }
  return o;
}
}

bool settingsSet(const String& k, const String& v) {
  if (k == "ver") { fileVer = v.toInt(); return true; }
  for (const Field& f : FIELDS) {
    if (k == f.key) {
      if(f.kind==K_GRAD && (v.length()!=strlen(v.c_str()) || !gradientValid(v.c_str())))return false;
      store(f, v); dirty = true; dirtyAt = millis(); return true;
    }
  }
  return false;
}

bool settingsValidate(const String& k,const String& v) {
  for(const Field& f:FIELDS) {
    if(k!=f.key) continue;
    if(f.kind==K_GRAD)return v.length()==strlen(v.c_str()) && gradientValid(v.c_str());
    if(f.kind==K_STR) return v.length()<f.size && v.indexOf('\n')<0 && v.indexOf('\r')<0;
    if(f.kind==K_COLOR) {
      if(v.length()!=6) return false;
      for(size_t i=0;i<v.length();++i) {
        const char c=v[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return false;
      }
      return true;
    }
    if(!v.length()) return false;
    char *end=nullptr; errno=0;
    const long value=strtol(v.c_str(),&end,10);
    if(errno || !end || *end) return false;
    return f.kind==K_BOOL ? (value==0 || value==1) : value>=f.lo && value<=f.hi;
  }
  return false;
}

String settingsToJson() {
  String j = "{";
  bool first = true;
  for (const Field& f : FIELDS) {
    if (f.secret) continue;
    if (!first) j += ',';
    first = false;
    const bool quoted = (f.kind == K_COLOR || f.kind == K_STR || f.kind == K_GRAD);
    j += '"'; j += f.key; j += "\":";
    if (quoted) j += '"';
    j += (f.kind == K_STR || f.kind == K_GRAD) ? escaped(valueOf(f)) : valueOf(f);
    if (quoted) j += '"';
  }
  j += '}';
  return j;
}

bool settingsSaveNow() {
  File f = LittleFS.open("/settings.tmp", "w");
  if (!f) return false;
  f.print("ver="); f.println(String(FILE_VER));
  for (const Field& fl : FIELDS) { f.print(fl.key); f.print('='); f.println(valueOf(fl)); }
  f.flush();
  bool ok = f.getWriteError() == 0;
  f.close();
  // LittleFS rename replaces the old file atomically. Never truncate the
  // last saved library/settings if a write or power interruption occurs.
  if(ok)ok=LittleFS.rename("/settings.tmp",PATH);
  dirty = !ok;
  return ok;
}

bool settingsAreSaved() { return !dirty; }

void settingsBegin() {
  LittleFS.begin();
  fileVer = 0;                               // версию определяет только прочитанный файл
  File f = LittleFS.open(PATH, "r");
  if (f) {
    int rawFont = FS_DIAG;                   // исходное значение шрифта из файла (до ограничения диапазона)
    while (f.available()) {
      String l = f.readStringUntil('\n');
      if(l.length() && l[l.length()-1]=='\r') l=l.substring(0,l.length()-1);
      const int e = l.indexOf('=');
      if (e <= 0) continue;
      const String k = l.substring(0, e), v = l.substring(e + 1);
      if (k == "font") rawFont = v.toInt();
      settingsSet(k, v);
    }
    f.close();
    if (fileVer < FILE_VER) {                // старые файлы: 1 прямой (удалён) -> диагональный, 2 -> 1, 3 (4x5) -> 2
      S.font = (rawFont == 3) ? FS_4X5 : (rawFont >= 1 ? FS_DIAG : FS_NONE);
      settingsSaveNow();
    }
  }
  dirty = false;
}

void settingsTick() {
  if (dirty && millis() - dirtyAt > 2000) settingsSaveNow();
}
