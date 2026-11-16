/**
 * @file remote_task.h
 * @brief 收发 ESP32 控制帧，桥接 MotorTask 命令与结果队列。
 */
/** @brief 通信任务：收发协议帧，通过静态队列连接 MotorTask。 */
#pragma once
#include "remote_protocol.h"
void RemoteTask_Init(void);
bool RemoteTask_Take(remote_request_t *q, bool urgent);
void RemoteTask_Reply(remote_reply_t reply);
bool RemoteTask_LinkAlive(uint32_t now);
