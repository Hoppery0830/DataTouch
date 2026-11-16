/** @file tactile_result.h
 * @brief 独立触觉结果持久化队列；loopTask 收帧/回执，cloudTask 存储/上传。
 */
#pragma once
#include <Arduino.h>
void tactileBegin();
void tactileFrame(const uint8_t *p, uint8_t len);
void tactileTick();
void tactileStore(const String &deviceId);
String tactileNext(String &deviceId, String &token);
void tactileUploaded(const String &deviceId, const String &token);
