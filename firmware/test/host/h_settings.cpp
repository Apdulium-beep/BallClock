#include "LittleFS.h"
#include "settings.h"
std::map<std::string,std::string> FS_; LittleFSClass LittleFS;
int main(){
  const char* ops[][2]={{"font","1"},{"font","99"},{"font","-4"},{"color","ff8800"},{"color","GGGGGG"},{"lead","1"},{"blink","0"},{"bmode","2"},{"pal","8"},{"pal","40"},
   {"bbright","300"},{"speed","77"},{"scale","-5"},{"angle","-90"},{"angle","999"},{"autoB","1"},{"bright","10"},{"bmin","3"},{"bmax","250"},{"adcMin","5"},{"adcMax","5000"},
   {"night","1"},{"ncolor","00ff00"},{"nthr","123"},{"gmt","120"},{"gmt","-9999"},{"ntp","time.\"x\".org"},{"showIp","0"},{"ssid","My \"Home\" \\ WiFi"},{"pass","p=ss w0rd"},{"nope","1"},{"ver","2"}};
  for(auto& o:ops) printf("set %s=%s -> %d\n",o[0],o[1],(int)settingsSet(o[0],o[1]));
  printf("JSON: %s\n",settingsToJson().c_str());
  settingsSaveNow(); printf("FILE:\n%s",FS_["/settings.txt"].c_str());
  S=Settings(); settingsBegin(); printf("RELOAD JSON: %s\n",settingsToJson().c_str());
  const char* olds[]={"font=3\n","font=2\n","font=1\n","font=0\n","ver=2\nfont=2\n"};
  for(auto o:olds){ FS_["/settings.txt"]=o; S=Settings(); settingsBegin(); printf("MIGRATE %s-> font=%d\n",o[0]=='v'?"v2 ":"old ",S.font); }
}
