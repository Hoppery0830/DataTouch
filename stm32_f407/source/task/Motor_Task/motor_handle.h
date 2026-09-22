/**
 * @file motor_handle.h
 * @brief 当前路径点的非阻塞执行器；下发选中轴、确认到位、处理停留。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "motor_state_machine.h"
/* One non-blocking step: -1 fault, 0 waiting, 1 point+dwell completed. */
/** @brief 推进当前点一次：-1=故障或超时，0=等待，1=全部选中轴到位且停留完成。
 * 每轴只下发一次，不阻塞等待；段超时包含下发等待、运动和停留。 */
int Motor_Handle_Point(motion_state_t *s, motor_t motors[AXIS_COUNT], uint32_t now);
