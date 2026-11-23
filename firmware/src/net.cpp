#include "net.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "settings.h"

static bool ap = false, wasConn = false, evt = false;
static uint32_t apCheckAt = 0;
static uint32_t apStarts = 0;

static bool apConfigured() {
  return (WiFi.getMode() & WIFI_AP) && WiFi.softAPSSID()==AP_NAME &&
    WiFi.softAPIP()==IPAddress(192,168,4,1);
}

static void startAP() {
  if(apConfigured()){ap=true;return;}
  const auto wanted=!S.apOnly && S.ssid[0] ? WIFI_AP_STA : WIFI_AP;
  const bool modeOk=WiFi.getMode()==wanted || WiFi.mode(wanted);
  const IPAddress address(192,168,4,1),mask(255,255,255,0);
  // Configure SSID/beacons BEFORE IP/DHCP. After stopping the AP its old
  // configuration may be empty; an early IP failure must not skip softAP().
  ++apStarts;
  ap = modeOk && WiFi.softAP(AP_NAME,"",6,false,4) &&
    (WiFi.softAPIP()==address || WiFi.softAPConfig(address,address,mask)) && apConfigured();
}

void netBegin() {
  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);        // без энергосбережения: стабильнее отдаёт страницы
  WiFi.hostname(HOSTNAME);
  startAP(); // BallClock AP is always present, including router mode.
  if(!S.apOnly && S.ssid[0])WiFi.begin(S.ssid,S.pass);
}

void netConnect(const char* ssid, const char* pass) {
  MDNS.end();
  const auto wanted=!S.apOnly && ssid[0] ? WIFI_AP_STA : WIFI_AP;
  // Never disable the AP/radio for an explicit network apply. A healthy AP
  // also keeps its DHCP server and associated clients untouched.
  if(WiFi.getMode()!=wanted)WiFi.mode(wanted);
  ap=apConfigured(); if(!ap)startAP();
  wasConn=false;
  if(!S.apOnly && ssid[0])WiFi.begin(ssid,pass);
}

void netTick() {
  if(millis()-apCheckAt>=1000) {
    apCheckAt=millis();
    if(!apConfigured()) { ap=false; startAP(); }
  }
  const bool c = !S.apOnly && WiFi.status() == WL_CONNECTED;
  if (c && !wasConn) {
    evt = true;
    MDNS.end(); MDNS.begin(HOSTNAME);
  }
  wasConn = c;
  if (c) MDNS.update();
}

bool netConnected() { return !S.apOnly && WiFi.status() == WL_CONNECTED; }
bool netApMode()    { return ap && apConfigured(); }
bool netTakeConnectEvent() { bool e = evt; evt = false; return e; }
uint32_t netApStartCount() { return apStarts; }
