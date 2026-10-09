#include "telemetry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <esp_system.h>
#include <math.h>
#include <string.h>
namespace {
struct Sample {
  uint32_t seq, ms, kseq, kms, received;
  uint8_t flags, light;
  float v[22];
};
Sample ring[60], snapshot[60]; // bounded six-second retry overlap; snapshot
                               // owned by cloudTask
unsigned head = 0, count = 0;
uint32_t boot = 0, generation = 0, lastSeq = 0, lastMs = 0;
SemaphoreHandle_t lock = nullptr;
uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
} // namespace
void telemetryInit() {
  lock = xSemaphoreCreateMutex();
  boot = esp_random();
}
void telemetryFrame(uint8_t type, const uint8_t *p, uint8_t n) {
  if (type != 0x21 || n != 108 || p[0] != 1 || p[3] != 0 || p[1] > 127 ||
      p[2] > 1 || !lock)
    return;
  Sample s{};
  s.flags = p[1];
  s.light = p[2];
  s.seq = u32(p + 4);
  s.ms = u32(p + 8);
  s.kseq = u32(p + 12);
  s.kms = u32(p + 16);
  s.received = millis();
  for (unsigned i = 0; i < 22; i++) {
    uint32_t bits = u32(p + 20 + 4 * i);
    memcpy(&s.v[i], &bits, 4);
    if (!isfinite(s.v[i]) || fabsf(s.v[i]) > 1e6f)
      return;
  }
  if (xSemaphoreTake(lock, 0) != pdTRUE)
    return; // loopTask never waits for cloud
  if (count && s.seq == lastSeq) {
    xSemaphoreGive(lock);
    return;
  }
  // Reset session on a reboot or uint32 wrap; never join curves across the
  // discontinuity.
  if (count && (s.seq < lastSeq || s.ms < lastMs)) {
    generation++;
    head = count = 0;
  }
  lastSeq = s.seq;
  lastMs = s.ms;
  ring[head] = s;
  head = (head + 1) % 60;
  if (count < 60)
    count++;
  xSemaphoreGive(lock);
}
String telemetryJson() {
  if (!lock)
    return "null";
  xSemaphoreTake(lock, portMAX_DELAY);
  unsigned n = count;
  uint32_t gen = generation;
  for (unsigned i = 0; i < n; i++)
    snapshot[i] = ring[(head + 60 - n + i) % 60];
  xSemaphoreGive(lock);
  if (!n)
    return "null";
  String out;
  out.reserve(18000);
  char b[100];
  snprintf(
      b, sizeof(b),
      "{\"version\":1,\"session\":\"%08lx-%lu\",\"ageMs\":%lu,\"samples\":[",
      (unsigned long)boot, (unsigned long)gen,
      (unsigned long)(millis() - snapshot[n - 1].received));
  out += b;
  for (unsigned j = 0; j < n; j++) {
    const Sample &s = snapshot[j];
    if (j)
      out += ',';
    snprintf(b, sizeof(b), "[%lu,%lu,%u,%u,%lu,%lu", (unsigned long)s.seq,
             (unsigned long)s.ms, s.flags, s.light, (unsigned long)s.kseq,
             (unsigned long)s.kms);
    out += b;
    for (unsigned i = 0; i < 22; i++) {
      snprintf(b, sizeof(b), ",%.6g", (double)s.v[i]);
      out += b;
    }
    out += ']';
  }
  out += "]}";
  return out;
}
