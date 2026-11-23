#include "art.h"
#include "settings.h"
#include <LittleFS.h>
ArtPixel Drawing[ART_COUNT];
static char names[ART_SLOTS][65]={};
static int loaded=-1;
static String path(int slot){return String("/art")+String(slot)+".txt";}
static String escaped(const String &text){String out;for(size_t i=0;i<text.length();++i){char c=text[i];if(c=='"'||c=='\\')out+='\\';if((uint8_t)c>=32)out+=c;}return out;}
static String read(int slot){File f=LittleFS.open(path(slot),"r");if(!f)return "";if(f.size()>ART_DATA+65){f.close();return "";}String data=f.readString();f.close();return data;}
static bool split(const String &text,String &name,String &data){int at=text.indexOf('|');if(at<1)return false;name=text.substring(0,at);data=text.substring(at+1);return name.length()==strlen(name.c_str())&&data.length()==ART_DATA&&artNameValid(name.c_str())&&artDataValid(data.c_str());}
void artSelect(){if(loaded==S.drawSlot)return;loaded=S.drawSlot;for(auto &p:Drawing)p=ArtPixel();String name,data;if(split(read(loaded),name,data))artDecode(data.c_str(),Drawing);}
void artBegin(){for(int i=0;i<ART_SLOTS;++i){names[i][0]=0;String name,data;if(split(read(i),name,data))strlcpy(names[i],name.c_str(),sizeof(names[i]));}loaded=-1;artSelect();}
bool artSave(int slot,const String &name,const String &data){
  if(slot<0||slot>=ART_SLOTS||name.length()!=strlen(name.c_str())||data.length()!=ART_DATA||!artNameValid(name.c_str())||!artDataValid(data.c_str()))return false;
  File f=LittleFS.open("/art.tmp","w");if(!f)return false;
  f.print(name);f.print('|');f.print(data);f.flush();bool ok=f.getWriteError()==0;f.close();
  if(ok)ok=LittleFS.rename("/art.tmp",path(slot));
  if(!ok)return false;
  strlcpy(names[slot],name.c_str(),sizeof(names[slot]));if(slot==S.drawSlot){loaded=-1;artSelect();}return true;
}
bool artDelete(int slot){if(slot<0||slot>=ART_SLOTS)return false;if(LittleFS.exists(path(slot))&&!LittleFS.remove(path(slot)))return false;names[slot][0]=0;if(slot==S.drawSlot){loaded=-1;artSelect();}return true;}
static String encoded(){String data;data.reserve(ART_DATA);char record[ART_RECORD+1];const char hex[]="0123456789abcdef";for(const auto &p:Drawing){record[0]='0'+p.effect;record[1]=hex[p.period>>4];record[2]=hex[p.period&15];record[3]=hex[p.phase>>4];record[4]=hex[p.phase&15];for(int c=0;c<12;++c){record[5+c*2]=hex[p.rgb[c]>>4];record[6+c*2]=hex[p.rgb[c]&15];}record[29]=0;data+=record;}return data;}
String artJson(bool includeData){artSelect();String out="{\"names\":[";for(int i=0;i<ART_SLOTS;++i){if(i)out+=',';out+='"';out+=escaped(names[i]);out+='"';}out+=']';if(includeData)out+=",\"data\":\""+encoded()+"\"";return out+"}";}
String artGetJson(int slot){if(slot<0||slot>=ART_SLOTS)return "{}";String name,data;if(!split(read(slot),name,data)){name="";data.reserve(ART_DATA);for(int i=0;i<ART_DATA;++i)data+='0';}return String("{\"slot\":")+String(slot)+",\"name\":\""+escaped(name)+"\",\"data\":\""+data+"\"}";}
