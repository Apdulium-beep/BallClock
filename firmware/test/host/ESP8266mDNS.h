#pragma once
struct HostMdns {
  int starts=0;
  void end() {}
  void begin(const char*) { ++starts; }
  void update() {}
};
inline HostMdns MDNS;
