#include "ESP8266WiFi.h"
#include "settings.h"
#include "net.h"
#include <cstdio>
Settings S;
int main() {
  strlcpy(S.ssid,"saved-router",sizeof(S.ssid));
  netBegin();
  if(!S.apOnly || !netApMode() || WiFi.currentMode!=WIFI_AP || WiFi.beginCalls ||
     WiFi.apName!="BallClock" || WiFi.hidden || WiFi.channel!=6) return 1;
  hostMillis=60000;netTick();
  if(!netApMode() || WiFi.beginCalls) return 2;
  S.apOnly=false;netConnect(S.ssid,S.pass);
  if(!netApMode() || WiFi.currentMode!=WIFI_AP_STA || WiFi.beginCalls!=1) return 3;
  hostMillis+=21000;netTick();
  if(!netApMode() || WiFi.currentMode!=WIFI_AP_STA) return 4;
  WiFi.connectionStatus=WL_CONNECTED;netTick();
  if(!netApMode() || WiFi.currentMode!=WIFI_AP_STA || !netTakeConnectEvent()) return 5;
  S.apOnly=true;WiFi.apName.clear();WiFi.apOk=false;netConnect(S.ssid,S.pass);
  if(netApMode() || WiFi.currentMode!=WIFI_AP || WiFi.beginCalls!=1) return 6;
  WiFi.apOk=true;hostMillis+=2100;netTick();
  if(!netApMode() || WiFi.beginCalls!=1) return 7;
  WiFi.apName.clear();
  if(netApMode()) return 8; // a stale software flag is not an active AP
  hostMillis+=1100;netTick();
  if(!netApMode() || WiFi.apName!="BallClock") return 9;
  for(int restart=0;restart<3;++restart) {
    netConnect(S.ssid,S.pass);
    if(!netApMode() || WiFi.currentMode!=WIFI_AP) return 10;
  }
  const auto starts=netApStartCount();const int calls=WiFi.apCalls;
  for(int i=0;i<100;++i){S.night=(i&1);S.bright=i;hostMillis+=100;netTick();}
  if(netApStartCount()!=starts||WiFi.apCalls!=calls||!netApMode())return 11;
  for(int i=0;i<10;++i)netConnect(S.ssid,S.pass);
  if(netApStartCount()!=starts||WiFi.apCalls!=calls||WiFi.radioOffCalls||WiFi.apDisableCalls)return 12;
  std::puts("PASS: AP stays enabled in router mode; 100 night/brightness edits and repeated healthy network applies never stop/restart the AP.");
  return 0;
}
