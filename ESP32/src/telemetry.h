/**
 * @file telemetry.h
 * @brief 缓存 STM32 触觉遥测并打包上传批次。
 */
#pragma once
#include <Arduino.h>
/**
 * @brief 初始化 STM32 遥测缓存。
 */
void telemetryInit();
/**
 * @brief 接收并缓存一帧 STM32 触觉遥测。
 * @param type 帧类型。
 * @param p 负载字节。
 * @param n 负载长度（字节）。
 */
void telemetryFrame(uint8_t type, const uint8_t *p, uint8_t n);

/**
 * @brief 从 RAM 快照生成云端遥测批次。
 * @return 待上传的遥测 JSON 字符串。
 * @note 仅在 cloudTask 调用，不执行网络 I/O。
 */
String telemetryJson();
