#pragma once
#include <Arduino.h>
#include "artcodec.h"
void artBegin();
void artSelect();
bool artSave(int slot,const String &name,const String &data);
bool artDelete(int slot);
String artJson(bool includeData);
String artGetJson(int slot);
