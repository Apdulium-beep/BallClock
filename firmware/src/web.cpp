#include "web.h"
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include "core.h"
#include "net.h"
#include "settings.h"
#include "webpage.h"
#include "remotecontrol.h"
#include <errno.h>
#include "art.h"

static ESP8266WebServer server(80);

static void apiError(int status,const char *error) {
  server.send(status,"application/json",String("{\"ok\":false,\"error\":\"")+error+"\"}");
}

static bool sameOrigin() {
  const String origin=server.header("Origin");
  if(!origin.length() || origin=="http://"+server.hostHeader()) return true;
  apiError(403,"cross_origin_denied"); return false;
}

static String clientId() { return "net:"+server.header("X-BallClock-Client"); }

static bool apiWriteAllowed() {
  if(!sameOrigin()) return false;
  const String id=server.header("X-BallClock-Client");
  bool valid=id.length()>0 && id.length()<=64;
  for(size_t i=0;valid && i<id.length();++i) {
    const char c=id[i]; valid=(c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_';
  }
  if(!valid) { apiError(400,"client_id_required"); return false; }
  if(!clockControlAllowed(clientId())) { apiError(409,"another_controller_active"); return false; }
  return true;
}

static bool legacyWriteAllowed() {
  if(!sameOrigin()) return false;
  if(!clockControlAllowed("web")) { apiError(409,"another_controller_active"); return false; }
  return true;
}

static void sendState() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", clockStateJson());
}

void webBegin() {
  server.collectHeaders("Origin","X-BallClock-Client");
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", (PGM_P)PAGE_GZ, PAGE_GZ_LEN);
  });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/v1/state",HTTP_GET,sendState);
  server.on("/api/v1/art",HTTP_GET,[](){
    const String slot=server.arg("slot");if(slot.length()!=1||slot[0]<'0'||slot[0]>'7'){apiError(400,"invalid_slot");return;}
    server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",artGetJson(slot.toInt()));
  });
  server.on("/api/v1/art",HTTP_POST,[](){
    if(!apiWriteAllowed())return;
    const String slot=server.arg("slot");
    if(slot.length()!=1||slot[0]<'0'||slot[0]>'7'){apiError(400,"invalid_slot");return;}
    const String name=server.arg("name"),data=server.arg("data");
    if(!artNameValid(name.c_str())||name.length()!=strlen(name.c_str())||data.length()!=ART_DATA||!artDataValid(data.c_str())){apiError(400,"invalid_art");return;}
    if(!artSave(slot.toInt(),name,data)){apiError(500,"save_failed");return;}
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
  });
  server.on("/api/v1/art/delete",HTTP_POST,[](){
    if(!apiWriteAllowed())return;
    const String slot=server.arg("slot");if(slot.length()!=1||slot[0]<'0'||slot[0]>'7'){apiError(400,"invalid_slot");return;}
    if(!artDelete(slot.toInt())){apiError(500,"save_failed");return;}server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
  });
  server.on("/api/v1/info",HTTP_GET,[]() {
    server.send(200,"application/json",F("{\"device\":\"ballclock\",\"api_version\":1,\"protocol\":3,\"led_count\":128,\"frame_format\":\"rgb8-hex\",\"display_readback\":true,\"lease_ms\":1500}"));
  });
  server.on("/api/v1/settings",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    if(!server.args()) { apiError(400,"empty_settings"); return; }
    // Validate the whole batch first: invalid fields cannot partly alter S.
    for(uint8_t i=0;i<server.args();++i) {
      if(!settingsValidate(server.argName(i),server.arg(i))) { apiError(400,"invalid_setting"); return; }
    }
    bool timeChanged=false;
    for(uint8_t i=0;i<server.args();++i) {
      const String key=server.argName(i);
      settingsSet(key,server.arg(i)); timeChanged|=key=="gmt" || key=="ntp";
    }
    if(timeChanged) timeApply();
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":false}"));
  });
  server.on("/api/v1/save",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    if(!settingsSaveNow()) { apiError(500,"save_failed"); return; }
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
  });
  server.on("/api/v1/frame",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    if(server.args()!=1 || !server.hasArg("rgb")) { apiError(400,"invalid_frame"); return; }
    const auto result=clockAcceptFrame(server.arg("rgb"),clientId(),true);
    if(result!=REMOTE_OK) { apiError(result==REMOTE_BUSY?409:400,result==REMOTE_BUSY?"another_controller_active":"invalid_frame"); return; }
    server.send(200,"application/json",F("{\"ok\":true,\"displayed\":true,\"lease_ms\":1500}"));
  });
  server.on("/api/v1/frame",HTTP_GET,[]() {
    // Read-only display preview. Never acquires a remote-control frame lease.
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",String("{\"device\":\"ballclock\",\"api_version\":1,\"led_count\":128,\"rgb\":\"")+clockDisplayedFrameHex()+"\"}");
  });
  server.on("/api/v1/stop",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    clockStopRemote(clientId());
    if(!settingsSaveNow()) { apiError(500,"save_failed"); return; }
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
  });
  server.on("/api/v1/time",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    char *end=nullptr; errno=0;
    const String value=server.arg("epoch");
    const unsigned long epoch=strtoul(value.c_str(),&end,10);
    if(errno || !value.length() || !end || *end || epoch<=1600000000UL || !settingsValidate("gmt",server.arg("tz"))) {
      apiError(400,"invalid_time"); return;
    }
    settingsSet("gmt",server.arg("tz")); setClock(epoch); timeApply();
    server.send(200,"application/json",F("{\"ok\":true}"));
  });
  server.on("/api/v1/wifi/connect",HTTP_POST,[]() {
    if(!apiWriteAllowed()) return;
    if(!S.apOnly && !S.ssid[0]) { apiError(400,"ssid_required"); return; }
    if(!settingsSaveNow()) { apiError(500,"save_failed"); return; }
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
    netConnect(S.ssid,S.pass);
  });

  server.on("/api/set", HTTP_POST, []() {
    if(!legacyWriteAllowed()) return;
    for(uint8_t i=0;i<server.args();++i)
      if(!settingsValidate(server.argName(i),server.arg(i))){apiError(400,"invalid_setting");return;}
    bool tz = false;
    for (uint8_t i = 0; i < server.args(); i++) {
      settingsSet(server.argName(i), server.arg(i));
      if (server.argName(i) == "gmt" || server.argName(i) == "ntp") tz = true;
    }
    if (tz) timeApply();
    sendState();
  });

  server.on("/api/save",HTTP_POST,[]() {
    if(!legacyWriteAllowed()) return;
    if(!settingsSaveNow()) { apiError(500,"save_failed"); return; }
    server.send(200,"application/json",F("{\"ok\":true,\"saved\":true}"));
  });

  server.on("/api/calib", HTTP_POST, []() {            // запомнить текущее значение датчика как min/max
    if(!legacyWriteAllowed()) return;
    if (server.arg("w") == "min") settingsSet("adcMin", String((int)lightAvg));
    else                          settingsSet("adcMax", String((int)lightAvg));
    sendState();
  });

  server.on("/api/wifi", HTTP_POST, []() {
    if(!legacyWriteAllowed()) return;
    settingsSet("ssid", server.arg("ssid")); settingsSet("pass", server.arg("pass"));
    settingsSaveNow();
    netConnect(S.ssid, S.pass);
    sendState();
  });

  server.on("/api/settime", HTTP_POST, []() {          // время и пояс с телефона
    if(!legacyWriteAllowed()) return;
    const String value=server.arg("epoch");
    char *end=nullptr; errno=0;
    const uint32_t epoch = strtoul(value.c_str(), &end, 10);
    if(errno || !value.length() || !end || *end || epoch<=1600000000UL || !settingsValidate("gmt",server.arg("tz"))) {
      apiError(400,"invalid_time"); return;
    }
    setClock(epoch); settingsSet("gmt", server.arg("tz")); timeApply();
    if(!settingsSaveNow()) { apiError(500,"save_failed"); return; }
    sendState();
  });

  server.on("/api/scan", HTTP_GET, []() {              // список сетей; клиент опрашивает, пока идёт поиск
    if(S.apOnly) { apiError(409,"scan_disabled_in_direct_ap"); return; }
    const int n = WiFi.scanComplete();
    String j;
    if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); j = "{\"scanning\":1}"; }
    else if (n == WIFI_SCAN_RUNNING) j = "{\"scanning\":1}";
    else {
      j = "{\"nets\":[";
      for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i); s.replace("\\", "\\\\"); s.replace("\"", "\\\"");
        if (i) j += ',';
        j += "\"" + s + "\"";
      }
      j += "]}";
      WiFi.scanDelete();
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", j);
  });

  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
}

void webTick() { server.handleClient(); }
