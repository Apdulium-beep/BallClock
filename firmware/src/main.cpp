#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "core.h"
#include "geometry.h"
#include "net.h"
#include "render.h"
#include "settings.h"
#include "web.h"
#include "remotecontrol.h"
#include "framecodec.h"
#include "art.h"

CRGB leds[geo::LED_COUNT];
float lightAvg = 0;
static bool nightActive = false;
static uint32_t ipShowStart = 0;
static uint32_t renderUs = 0, showUs = 0;      // сколько занимают расчёт кадра и вывод на ленту (для команды ?)
static uint8_t displayedRgb[geo::LED_COUNT * 3];
static CRGB remoteLeds[geo::LED_COUNT];
static uint32_t remoteAt = 0;
static bool remoteEnabled = false, remotePending = false;
static String remoteClient;
static uint8_t gBright = 255;
static String usbTx;
static size_t usbTxPos = 0;

// Send telemetry in small nonblocking chunks: no Serial.print of a full frame.
static void usbPump() {
  if (usbTxPos >= usbTx.length()) return;
  const int available = Serial.availableForWrite();
  if (available <= 0) return;
  size_t n = usbTx.length() - usbTxPos;
  if (n > (size_t)available) n = available;
  if (n > 48) n = 48;
  usbTxPos += Serial.write((const uint8_t*)usbTx.c_str() + usbTxPos, n);
  if (usbTxPos == usbTx.length()) { usbTx = ""; usbTxPos = 0; }
}

String clockStateJson() {
  char tb[12]; time_t now = time(nullptr); tm t; localtime_r(&now, &t);
  snprintf(tb, sizeof(tb), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  const String ip = netConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  return "{\"device\":\"ballclock\",\"api_version\":1,\"protocol\":3,\"art\":"+artJson(S.bmode==BG_DRAW)+",\"s\":" + settingsToJson() + ",\"t\":{\"time\":\"" + String(tb)
    + "\",\"synced\":" + String(timeValid() ? 1 : 0) + ",\"adc\":" + String((int)lightAvg)
    + ",\"night\":" + String(nightActive ? 1 : 0) + ",\"brightness\":" + String(gBright)
    + ",\"remote\":" + String(remoteEnabled && millis()-remoteAt < 1500 ? 1 : 0) + ",\"ip\":\"" + ip
    + "\",\"ap\":" + String(netApMode()?1:0) + ",\"wifi\":" + String(netConnected()?2:(!S.apOnly && S.ssid[0]?1:0))
    + ",\"hasPass\":" + String(S.pass[0]?1:0) + ",\"saved\":" + String(settingsAreSaved()?1:0)
    + ",\"wifiMode\":" + String((int)WiFi.getMode()) + ",\"channel\":" + String(WiFi.channel())
    + ",\"apClients\":" + String(WiFi.softAPgetStationNum())
    + ",\"uptimeMs\":" + String(millis()) + ",\"freeHeap\":" + String(ESP.getFreeHeap())
    + ",\"apStarts\":" + String(netApStartCount()) + "}}";
}

static void usbState() {
  usbTx = "STATE " + clockStateJson() + '\n';
  usbTxPos = 0;
}

String clockDisplayedFrameHex() {
  const char hex[] = "0123456789abcdef";
  String frame; frame.reserve(sizeof(displayedRgb)*2);
  for (size_t i = 0; i < sizeof(displayedRgb); ++i) {
    frame += hex[displayedRgb[i] >> 4]; frame += hex[displayedRgb[i] & 15];
  }
  return frame;
}

static void usbFrame() {
  usbTx = "FRAME " + clockDisplayedFrameHex() + '\n'; usbTxPos = 0;
}

// ESP8266 Wi-Fi must run between pixels, not wait behind a ~4 ms interrupt
// blackout for the whole strip. Explicit scaling below preserves USB/Qt RGB.
static CRGB wireLeds[geo::LED_COUNT];
static bool stripShown=false;
static uint32_t stripShownAt=0;
static void ledsBegin() {
  FastLED.addLeds<WS2812B,LED_PIN,GRB>(wireLeds,geo::LED_COUNT).setCorrection(UncorrectedColor);
  FastLED.setBrightness(255); FastLED.setDither(0);
}
static void ledsSetBrightness(uint8_t b) { gBright = b; }
static void ledsShow() {
  // лимит тока по потребляемой мощности (5 В * MAX_MA)
  const uint8_t b = calculate_max_brightness_for_power_mW(leds, geo::LED_COUNT, gBright, 5UL * MAX_MA);
  bool changed=!stripShown;
  for (uint8_t i = 0; i < geo::LED_COUNT; i++) {
    const uint8_t r = scale8(leds[i].r, b), g = scale8(leds[i].g, b), blue = scale8(leds[i].b, b);
    changed|=displayedRgb[3*i]!=r || displayedRgb[3*i+1]!=g || displayedRgb[3*i+2]!=blue;
    displayedRgb[3*i] = r; displayedRgb[3*i+1] = g; displayedRgb[3*i+2] = blue;
    wireLeds[i]=CRGB(r,g,blue);
  }
  if(changed || millis()-stripShownAt>=500) {
    yield(); FastLED.show(); yield();
    stripShown=true; stripShownAt=millis();
  }
}

// ---------- Время ----------
bool timeValid() { return time(nullptr) > 1600000000L; }
void timeApply() { configTime(S.gmt * 60L, 0, S.ntp); }
void setClock(uint32_t epoch) { timeval tv = {(time_t)epoch, 0}; settimeofday(&tv, nullptr); }

void updateBrightness();

bool clockControlAllowed(const String &client) {
  return !remoteEnabled || millis()-remoteAt>=1500 || remoteClient==client;
}

RemoteResult clockAcceptFrame(const String &hex,const String &client,bool displayNow) {
  uint8_t decoded[geo::LED_COUNT*3];
  if(!decodeFrameHex(hex.c_str(),hex.length(),decoded,sizeof(decoded))) return REMOTE_INVALID;
  if(!clockControlAllowed(client)) return REMOTE_BUSY;
  for(int i=0;i<geo::LED_COUNT;++i) remoteLeds[i]=CRGB(decoded[i*3],decoded[i*3+1],decoded[i*3+2]);
  remoteClient=client; remoteAt=millis(); remoteEnabled=true; remotePending=!displayNow;
  if(displayNow) {
    updateBrightness();
    for(int i=0;i<geo::LED_COUNT;++i) leds[i]=remoteLeds[i];
    ledsShow(); // HTTP acknowledgement means the frame was displayed, not merely queued.
  }
  return REMOTE_OK;
}

bool clockStopRemote(const String &client) {
  if(!clockControlAllowed(client)) return false;
  remoteEnabled=false; remotePending=false; remoteClient="";
  return true;
}

// ---------- Тест ленты ----------
void walkTest() {
  Serial.println(F("Walk test: светодиоды зажигаются по одному в порядке ленты"));
  for (uint8_t i = 0; i < geo::LED_COUNT; i++) {
    fill_solid(leds, geo::LED_COUNT, CRGB::Black);
    for (uint8_t j = 0; j < i; j++) leds[j] = CRGB(0, 0, 25);
    leds[i] = CRGB::White;
    ledsShow();
    delay(120);
  }
}

// ---------- Serial: T HH:MM | W | ? | set <ключ> <значение> ----------
void handleSerial() {
  if (usbTx.length()) return; // never interleave replies with a telemetry line
  if (!Serial.available()) return;
  static String input;
  static bool overflow=false;
  bool complete = false;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n') { complete = true; break; }
    if (input.length() < 4096) input += c;else overflow=true;
  }
  if (!complete) return;
  String l = input; input = "";
  if(overflow){overflow=false;Serial.println(F("error command_too_long"));return;}
  if(l.length() && l[l.length()-1]=='\r') l=l.substring(0,l.length()-1);
  if(!l.startsWith("set ")) l.trim(); // preserve significant SSID/password whitespace
  if (!l.length()) return;
  if (l == "state") { usbState();
  } else if(l.startsWith("art put ")) {
    if(!clockControlAllowed("usb")){Serial.println(F("error busy"));return;}
    const int sep=l.indexOf(' ',8),bar=l.indexOf('|',sep+1);const String slot=l.substring(8,sep);
    if(sep<0||bar<0||slot.length()!=1||slot[0]<'0'||slot[0]>'7'||!artSave(slot.toInt(),l.substring(sep+1,bar),l.substring(bar+1)))Serial.println(F("error art"));else Serial.println(F("ok"));
  } else if(l.startsWith("art delete ")) {
    if(!clockControlAllowed("usb")){Serial.println(F("error busy"));return;}
    const String slot=l.substring(11);if(slot.length()!=1||slot[0]<'0'||slot[0]>'7'||!artDelete(slot.toInt()))Serial.println(F("error art"));else Serial.println(F("ok"));
  } else if (l == "frame") { usbFrame();
  } else if (l == "save") {
    Serial.println(settingsSaveNow() ? F("ok") : F("error save"));
  } else if (l == "stop") {
    if(!clockStopRemote("usb")) Serial.println(F("error busy"));
    else Serial.println(settingsSaveNow() ? F("ok") : F("error save"));
  } else if(l == "wifi_connect") {
    if(!clockControlAllowed("usb")) Serial.println(F("error busy"));
    else if(!S.apOnly && !S.ssid[0]) Serial.println(F("error ssid"));
    else if(!settingsSaveNow()) Serial.println(F("error save"));
    else { netConnect(S.ssid,S.pass); Serial.println(F("ok")); }
  } else if (l.startsWith("pixels ")) {
    const auto result=clockAcceptFrame(l.substring(7),"usb",false);
    if(result==REMOTE_INVALID) Serial.println(F("error pixels"));
    else if(result==REMOTE_BUSY) Serial.println(F("error busy"));
  } else if (l.startsWith("sample ")) {
    unsigned h,m,ms;
    if (sscanf(l.c_str()+7,"%u %u %u",&h,&m,&ms)==3 && h<24 && m<60) {
      CRGB sample[geo::LED_COUNT]; drawFrame(sample,h,m,ms,true,nightActive);
      const char hex[]="0123456789abcdef";
      usbTx="SAMPLE "; usbTx.reserve(776);
      for(int i=0;i<geo::LED_COUNT;++i) for(int c=0;c<3;++c) {
        const uint8_t v=sample[i].raw[c]; usbTx+=hex[v>>4]; usbTx+=hex[v&15];
      }
      usbTx+='\n'; usbTxPos=0;
    } else Serial.println(F("error sample"));
  } else if (l.startsWith("time ")) {
    if(!clockControlAllowed("usb")) { Serial.println(F("error busy")); return; }
    unsigned long epoch; int gmt;
    if (sscanf(l.c_str() + 5, "%lu %d", &epoch, &gmt) == 2 && epoch > 1600000000UL && gmt >= -720 && gmt <= 840) {
      settingsSet("gmt", String(gmt)); setClock(epoch); timeApply(); Serial.println(F("ok"));
    } else Serial.println(F("error time"));
  } else if (l[0] == 'T' || l[0] == 't') {
    int h, m;
    if (sscanf(l.c_str() + 1, "%d:%d", &h, &m) == 2) { setClock(1700000000UL / 86400 * 86400 + h * 3600UL + m * 60UL - S.gmt * 60L); Serial.println(F("время установлено")); }
  } else if (l[0] == 'W' || l[0] == 'w') walkTest();
  else if (l.startsWith("set ")) {
    if(!clockControlAllowed("usb")) { Serial.println(F("error busy")); return; }
    const int sp = l.indexOf(' ', 4);
    const String key = sp > 0 ? l.substring(4, sp) : String();
    if (sp > 0 && settingsSet(key, l.substring(sp + 1))) {
      if (key == "gmt" || key == "ntp") timeApply();
      Serial.println(F("ok"));
    } else Serial.println(F("неизвестный ключ"));
  } else if (l[0] == '?') {
    Serial.printf("A0=%d (среднее %.0f), ночь=%d, IP=%s\nкадр: расчёт %lu мкс, вывод на ленту %lu мкс, период %u мс\n", analogRead(LIGHT_PIN), lightAvg, nightActive,
                  netConnected() ? WiFi.localIP().toString().c_str() : "нет", (unsigned long)renderUs, (unsigned long)showUs, FRAME_MS);
  } else Serial.println(F("T HH:MM | W | ? | set <ключ> <значение>"));
}

// ---------- Яркость и ночной режим ----------
void updateBrightness() {
  lightAvg += (analogRead(LIGHT_PIN) - lightAvg) * 0.02f;
  if (S.night) {
    if (!nightActive && lightAvg < S.nthr) nightActive = true;
    else if (nightActive && lightAvg > S.nthr * 1.3f + 5) nightActive = false;   // гистерезис
  } else nightActive = false;

  uint8_t b = S.bright;
  if (S.autoB && S.adcMax > S.adcMin)
    b = constrain(map((long)lightAvg, S.adcMin, S.adcMax, S.bmin, S.bmax), min(S.bmin, S.bmax), max(S.bmin, S.bmax));
  ledsSetBrightness(b);
}

void setup() {
  Serial.begin(115200);
  if (!geo::init()) { Serial.println(F("Ошибка геометрии: не 128 светодиодов")); while (true) delay(1000); }
  ledsBegin();
  lightAvg = analogRead(LIGHT_PIN);
  settingsBegin();
  artBegin();
  netBegin();
  timeApply();
  webBegin();
  Serial.println(F("BallClock запущен. Откройте http://ballclock.local или IP из монитора. ? — справка"));
}

void loop() {
  usbPump(); handleSerial(); netTick(); webTick(); settingsTick(); artSelect();
  if (netTakeConnectEvent() && S.showIp) ipShowStart = millis();

  static uint32_t last = 0;
  if (millis() - last < FRAME_MS) return;      // частота кадров задаётся в config.h
  last = millis();

  updateBrightness();
  const uint32_t t0 = micros();
  if (remoteEnabled && millis()-remoteAt < 1500) {
    for(int i=0;i<geo::LED_COUNT;++i) leds[i]=remoteLeds[i];
  } else if (ipShowStart && millis() - ipShowStart < 8000UL) {       // после подключения показываем IP по частям: 4 числа по 2 с
    drawNumber(leds, WiFi.localIP()[(millis() - ipShowStart) / 2000]);
  } else {
    ipShowStart = 0;
    tm t; time_t now = time(nullptr); localtime_r(&now, &t);
    drawFrame(leds, t.tm_hour, t.tm_min, millis(), timeValid(), nightActive);
  }
  const uint32_t t1 = micros();
  ledsShow();
  if (remotePending) { remotePending=false; Serial.println(F("ok pixels")); }
  renderUs = t1 - t0; showUs = micros() - t1;
}
