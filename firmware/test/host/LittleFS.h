#pragma once
#include "Arduino.h"
struct File { std::vector<std::string> lines; size_t pos=0; std::string name; bool wr=false; std::string buf; bool ok=false;
  operator bool() const {return ok;}
  bool available(){return pos<lines.size();}
  String readStringUntil(char){return String(lines[pos++]);}
  void print(const char* s){buf+=s;} void print(char c){buf+=c;} void println(const String& s){buf+=s+"\n";}
  void print(const String& s){buf+=s;}
  size_t size() const{return buf.size();}
  String readString(){return buf;}
  void close();
  void flush() {}
  int getWriteError() const { return 0; }
};
extern std::map<std::string,std::string> FS_;
inline void File::close(){ if(wr) FS_[name]=buf; }
struct LittleFSClass{ bool begin(){return true;}
  bool renameOk=true;
  bool rename(const char* from,const char* to){if(!renameOk||!FS_.count(from))return false;FS_[to]=FS_[from];FS_.erase(from);return true;}
  bool rename(const char* from,const String& to){return rename(from,to.c_str());}
  bool exists(const String& p){return FS_.count(p)!=0;}
  bool remove(const String& p){return FS_.erase(p)>0;}
  File open(const String& p,const char* m){return open(p.c_str(),m);}
  File open(const char* p,const char* m){File f;f.name=p;f.ok=true;
    if(m[0]=='w'){f.wr=true;} else { if(!FS_.count(p)){f.ok=false;return f;} f.buf=FS_[p];std::stringstream ss(FS_[p]);std::string l;while(std::getline(ss,l))f.lines.push_back(l);} return f;} };
extern LittleFSClass LittleFS;
