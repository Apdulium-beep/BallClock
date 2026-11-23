#pragma once
#include "Arduino.h"
enum { WIFI_OFF=0, WIFI_AP=1, WIFI_STA=2, WIFI_AP_STA=3, WIFI_NONE_SLEEP=0, WL_CONNECTED=3 };
struct IPAddress {
  uint8_t bytes[4];
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d} {}
  bool operator==(const IPAddress& other) const { return memcmp(bytes,other.bytes,4)==0; }
};
struct HostWifi {
  int currentMode=0,connectionStatus=0,beginCalls=0,apCalls=0,channel=0,radioOffCalls=0,apDisableCalls=0;
  bool configOk=true,apOk=true,hidden=true;
  std::string apName;
  void persistent(bool) {}
  void setSleepMode(int) {}
  void hostname(const char*) {}
  bool mode(int value) { if(value==WIFI_OFF)++radioOffCalls;if((currentMode&WIFI_AP)&&!(value&WIFI_AP))++apDisableCalls;currentMode=value; if(value==WIFI_AP || value==WIFI_OFF) connectionStatus=0; return true; }
  int getMode() const { return currentMode; }
  String softAPSSID() const { return apName; }
  IPAddress softAPIP() const { return IPAddress(192,168,4,1); }
  bool softAPConfig(const IPAddress& ip,const IPAddress&,const IPAddress&) {
    return !apName.empty() && configOk && ip.bytes[0]==192 && ip.bytes[1]==168 && ip.bytes[2]==4 && ip.bytes[3]==1;
  }
  bool softAP(const char* name,const char*,int ch,bool hide,int) {
    ++apCalls;if(!apOk)return false;apName=name;channel=ch;hidden=hide;return true;
  }
  void begin(const char*,const char*) { ++beginCalls; }
  void disconnect() { connectionStatus=0; }
  void softAPdisconnect(bool) { ++apDisableCalls;apName.clear(); }
  int status() const { return connectionStatus; }
};
inline HostWifi WiFi;
