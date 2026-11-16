/** @file visual_result.h
 * @brief 独立视觉结果持久化队列；loopTask 收帧/回执，cloudTask 存储/上传。
 */
#pragma once
#include <Arduino.h>
void visualBegin();
void visualFrame(const uint8_t *p, uint8_t len);
void visualTick();
void visualStore(const String &deviceId);
String visualNext(String &deviceId, String &token);
void visualUploaded(const String &deviceId, const String &token);
