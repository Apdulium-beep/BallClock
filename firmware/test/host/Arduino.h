#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>
#include <map>
#include <sstream>
// Макросы настоящего Arduino.h (ESP8266): собственные имена не должны с ними совпадать
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define SERIAL 0
#define DISPLAY 1
#define DEFAULT 1
#define EXTERNAL 0
#define CHANGE 1
#define RISING 1
#define FALLING 2
#define LSBFIRST 0
#define MSBFIRST 1
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define A0 17
#define bit(b) (1UL << (b))
#define sq(x) ((x)*(x))
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
inline uint32_t hostMillis=0;
inline uint32_t millis(){return hostMillis;}
inline size_t strlcpy(char* d,const char* s,size_t n){size_t l=strlen(s);if(n){size_t c=l>=n?n-1:l;memcpy(d,s,c);d[c]=0;}return l;}
struct String : std::string {
  String(){} String(const char* s):std::string(s){} String(const std::string& s):std::string(s){}
  String(int v):std::string(std::to_string(v)){} String(unsigned v):std::string(std::to_string(v)){}
  String(long v):std::string(std::to_string(v)){} String(unsigned long v):std::string(std::to_string(v)){}
  String(uint8_t v):std::string(std::to_string((int)v)){} String(bool v):std::string(v?"1":"0"){}
  String(int16_t v):std::string(std::to_string((int)v)){} String(uint16_t v):std::string(std::to_string((int)v)){}
  long toInt() const {return atol(c_str());}
  void trim(){while(!empty()&&isspace(back()))pop_back();while(!empty()&&isspace(front()))erase(0,1);}
  int indexOf(char c) const {auto p=find(c);return p==npos?-1:(int)p;}
  String substring(int a) const {return substr(a);} String substring(int a,int b) const {return substr(a,b-a);}
  String& operator+=(const String& o){append(o);return *this;} String& operator+=(const char* o){append(o);return *this;} String& operator+=(char c){push_back(c);return *this;}
};
inline String operator+(const String& a,const String& b){return String(std::string(a)+std::string(b));}
inline String operator+(const String& a,const char* b){return String(std::string(a)+b);}
inline String operator+(const char* a,const String& b){return String(std::string(a)+std::string(b));}
