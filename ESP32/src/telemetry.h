#pragma once
#include <Arduino.h>
void telemetryInit();
void telemetryFrame(uint8_t type, const uint8_t *p, uint8_t n);
// Called only on cloudTask; takes a short RAM snapshot, never performs network
// I/O.
String telemetryJson();
