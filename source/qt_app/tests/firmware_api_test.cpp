#include "LittleFS.h"
#include "settings.h"
#include "framecodec.h"
#include "art.h"
#include <cstdio>

std::map<std::string,std::string> FS_;
LittleFSClass LittleFS;
int main() {
    const String before=settingsToJson();
    settingsSet("bright","123");
    if(settingsAreSaved() || !settingsSaveNow() || !settingsAreSaved()) return 10;
    hostMillis=100;settingsSet("bright","124");
    hostMillis=2201;settingsTick();
    if(!settingsAreSaved()) return 11;
    // A disconnected client is not needed for firmware's deferred flash save.
    S=Settings();
    const char *valid[][2]={{"font","2"},{"angle","-180"},{"gmt","840"},{"autoB","1"},{"color","12AbEf"},{"pass","a+b & = pass"},{"ssid","Home 2.4"}};
    const char *invalid[][2]={{"font","3"},{"angle","181"},{"bright","-1"},{"bright","1junk"},{"autoB","2"},{"color","GG0000"},{"color","12345"},{"ssid","bad\nssid"},{"ver","2"},{"unknown","1"}};
    for(const auto &f:valid) if(!settingsValidate(f[0],f[1])) return 1;
    for(const auto &f:invalid) if(settingsValidate(f[0],f[1])) return 2;
    if(settingsToJson()!=before) return 3;
    if(!S.apOnly || !settingsValidate("apOnly","0") || settingsValidate("apOnly","2")) return 9;
    if(!settingsValidate("bmode","4")||settingsValidate("bmode","5")||!settingsValidate("bcolor","12AbEf")||settingsValidate("bcolor","xyz123"))return 12;
    settingsSet("bmode","3");settingsSet("bcolor","123456");settingsSet("bbright","87");
    settingsSet("apOnly","0");
    settingsSet("ssid","Home 2.4");settingsSet("pass","  a+b & = pass  ");
    if(!settingsSaveNow()) return 7;
    S=Settings();settingsBegin();
    if(S.apOnly || std::string(S.pass)!="  a+b & = pass  " || std::string(S.ssid)!="Home 2.4") return 8;
    if(S.bmode!=BG_SOLID||S.bcolor!=0x123456||S.bbright!=87)return 13;
    const char *preset="Северное сияние|123456,38c3ff,91e0c2,000000";
    if(!settingsValidate("grad0",preset)||!settingsValidate("grad7","")||!settingsValidate("pal","16")||settingsValidate("pal","17"))return 14;
    for(const char *bad:{"|ffffff,000000,000000,ffffff","Bad|xyzxyz,ffffff,ffffff,ffffff","Bad|ffffff,000000"," Bad|ffffff,ffffff,ffffff,ffffff","Bad\nname|ffffff,ffffff,ffffff,ffffff","Bad|name|ffffff,ffffff,ffffff,ffffff","Bad\xc0\xaf|ffffff,ffffff,ffffff,ffffff"})
        if(settingsValidate("grad0",bad)||settingsSet("grad0",bad))return 15;
    settingsSet("grad0",preset);settingsSet("pal","9");
    if(!settingsSaveNow())return 16;
    S=Settings();settingsBegin();
    if(std::string(S.gradients[0])!=preset||S.pal!=9)return 17;
    uint32_t colors[4];if(!gradientColors(S.gradients[0],colors)||colors[0]!=0x123456||colors[3]!=0)return 18;
    const auto saved=FS_["/settings.txt"];
    settingsSet("grad0","Changed|ffffff,000000,ffffff,000000");LittleFS.renameOk=false;
    if(settingsSaveNow()||settingsAreSaved()||FS_["/settings.txt"]!=saved)return 19;
    LittleFS.renameOk=true;S=Settings();settingsBegin();
    if(std::string(S.gradients[0])!=preset)return 20;
    settingsSet("grad0","");if(!settingsSaveNow())return 21;
    S=Settings();settingsBegin();if(S.gradients[0][0])return 22;
    std::puts("PASS: named UTF-8 gradient validation, deletion, persistence and failed atomic-save recovery.");
    ArtPixel pixels[128];pixels[0].effect=2;pixels[0].rgb[0]=255;pixels[0].rgb[4]=255;pixels[1].effect=3;pixels[1].phase=64;
    char art[ART_DATA+1];artEncode(pixels,art);if(!artDataValid(art)||!artNameValid("Моя картина"))return 23;
    if(artNameValid("Bad|name")||artDataValid("bad"))return 24;
    art[0]='9';if(artDataValid(art))return 25;art[0]='2';
    artBegin();if(!artSave(0,"Моя картина",art))return 26;
    S.drawSlot=0;artBegin();if(Drawing[0].effect!=2||Drawing[0].rgb[0]!=255||Drawing[1].phase!=64)return 27;
    const auto oldArt=FS_["/art0.txt"];LittleFS.renameOk=false;if(artSave(0,"Новая",art)||FS_["/art0.txt"]!=oldArt)return 28;LittleFS.renameOk=true;
    if(!artDelete(0))return 29;artBegin();if(Drawing[0].effect||artJson(false).find("Моя картина")!=std::string::npos)return 30;
    std::puts("PASS: drawing codec, per-pixel effects, named atomic storage, restart load and delete.");
    uint8_t output[3]={42,42,42};
    if(!decodeFrameHex("00aBFF",6,output,3) || output[0]!=0 || output[1]!=171 || output[2]!=255) return 4;
    if(decodeFrameHex("ff001z",6,output,3) || output[0]!=0 || output[1]!=171 || output[2]!=255) return 5;
    if(decodeFrameHex("0011",4,output,3)) return 6;
    std::puts("PASS: strict firmware API validation, no state mutation, complete-frame validation before display.");
    return 0;
}
