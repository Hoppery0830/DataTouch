#pragma once
#include <Arduino.h>
#include "remote_protocol.h"
void remoteLinkBegin();
void remoteLinkSubmit(const String &cmd,const char *cloudId);
void remoteLinkTick();
void remoteLinkFrame(uint8_t type,const uint8_t *p,uint8_t len);
// Implemented in main.cpp; all are called on loopTask only.
void remoteWireSend(const uint8_t *data,size_t len);
void remoteApplyStatus(const remote_status_t &status,bool alive);
