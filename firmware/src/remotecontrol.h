#pragma once
#include <Arduino.h>
enum RemoteResult { REMOTE_OK, REMOTE_INVALID, REMOTE_BUSY };
String clockStateJson();
String clockDisplayedFrameHex();
bool clockControlAllowed(const String &client);
RemoteResult clockAcceptFrame(const String &hex,const String &client,bool displayNow);
bool clockStopRemote(const String &client);
