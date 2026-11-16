/** @file touch_task.h
 * @brief 触觉采集任务；发布状态快照并管理 K230 采图事务与补光。
 */
#pragma once
#include "touch_processing.h"
#include "touch_bsp.h"
typedef struct {
    touch_force_t force;
    touch_adxl_t adxl;
    touch_bsp_stats_t hardware;
    uint32_t adc_timestamp_ms, adc_blocks, crc_errors, invalid_values;
    bool force_online, adc_online;
} touch_status_t;
/* 便于 Ozone 直接观察；外部 C 代码使用 GetStatus 防止读到一半更新的数据。 */
extern volatile touch_status_t g_touch_status;
/**
 * @brief 初始化触觉采集任务。
 */
void TouchTask_Init(void);
/**
 * @brief 复制触觉采集和硬件状态的快照。
 * @param out 接收状态快照的缓冲区。
 */
void TouchTask_GetStatus(touch_status_t *out);

#include "touch_telemetry.h"
/**
 * @brief 复制最近发布的遥测快照。
 * @param out 接收遥测快照的缓冲区。
 */
void TouchTask_GetTelemetry(touch_telemetry_t *out);

#include "capture_sequence.h"

/**
 * @brief 由 MotorTask 向 TouchTask 提交采图请求。
 * @param seq 本次 START 命令序号。
 * @param now 为远程回调接口保留；实际曝光计时在 TouchTask 开灯时开始。
 * @return 请求被接受时返回 true；采图结果通过 TouchTask_CaptureStatus 查询。
 */
bool TouchTask_CaptureBegin(uint32_t seq, uint32_t now);
/** @brief 上轮结果已交接且采图硬件空闲时返回 true。 */
bool TouchTask_CaptureAvailable(void);
/**
 * @brief 由 MotorTask 请求取消采图，补光与串口仍由 TouchTask 管理。
 */
void TouchTask_CaptureCancel(void);
/**
 * @brief 读取指定 START 序号的采图状态快照。
 * @param seq 要查询的 START 命令序号。
 * @param out 接收采图状态的缓冲区。
 * @note 若序号不匹配，返回的状态标记为 CAP_FAILED / CAP_CANCELLED。
 */
void TouchTask_CaptureStatus(uint32_t seq, capture_sequence_t *out);

/** @brief 线程安全的独立视觉结果转发和持久化确认接口。 */
bool TouchTask_VisualTake(uint32_t now, uint8_t out[12]);
void TouchTask_VisualAck(const uint8_t payload[9]);
/** @brief 取消命令已处理且软件补光输出已关闭。 */
bool TouchTask_LightOff(void);

#include "tactile_result.h"
/** @brief MotorTask 发布实际阶段并累计最新触觉块；无网络和动态分配。 */
void TouchTask_TactileObserve(uint32_t seq, uint8_t state, uint8_t mode, uint8_t stage, uint32_t now);
/** @brief RemoteTask 读取/确认实验结果；与采集线程互斥。 */
bool TouchTask_TactileTake(uint32_t now, uint8_t out[TACTILE_RESULT_SIZE]);
void TouchTask_TactileAck(const uint8_t ack[9]);
