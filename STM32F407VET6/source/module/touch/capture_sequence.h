/**
 * @file capture_sequence.h
 * @brief 先采图后动作的补光、曝光等待、采图确认与取消状态机。
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// @name K230 采图帧（使用累加校验，与 ESP32 的 CRC8 帧独立）
/// @{

#define CAPTURE_REQUEST 0x30 ///< F4 发起单次采图。
#define CAPTURE_DONE 0x31 ///< K230 回传采图完成及结果。
#define CAPTURE_RELEASE 0x32 ///< F4 关灯后的放行或拒绝结果。
/// @}
enum {
    CAP_IDLE,
    CAP_EXPOSURE,
    CAP_WAIT,
    CAP_READY,
    CAP_FAILED
};
enum {
    CAP_OK,
    CAP_CAMERA_ERROR,
    CAP_TIMEOUT,
    CAP_LIGHT_ERROR,
    CAP_CANCELLED
};
/// @brief 单次采图事务，令牌由命令序号和开灯时刻组成。
typedef struct
{
    uint32_t seq, epoch, since;
    uint8_t state, error;
    bool light, reply_pending;
} capture_sequence_t;
/**
 * @brief 设置补光输出。
 * @param on true 开灯，false 关灯。
 * @return 输出操作成功返回 0；不代表已测得灯的实际状态。
 */
typedef uint8_t (*capture_light_fn)(bool on);
/**
 * @brief 尝试提交一帧 K230 串口数据。
 * @param data 帧缓冲区。
 * @param size 帧长（字节）。
 * @return 发送接口已接收该帧时返回 true。
 */
typedef bool (*capture_send_fn)(const uint8_t *data, size_t size);
/**
 * @brief 开启补光并进入曝光等待。
 * @param s 要重新初始化的采图事务。
 * @param seq 本次 START 命令序号。
 * @param now 当前 HAL 毫秒计数，也用作令牌的一部分。
 * @param light 补光输出回调。
 * @return 进入曝光等待返回 true；开灯失败则进入失败状态并尝试关灯。
 */
bool Capture_Begin(capture_sequence_t *s, uint32_t seq, uint32_t now, capture_light_fn light);
/**
 * @brief 推进曝光、请求发送、超时和关灯确认。
 * @param s 当前采图事务。
 * @param now 当前 HAL 毫秒计数。
 * @param light 补光输出回调。
 * @param send 串口发送回调。
 * @note 一旦发送接口接受采图请求，便不再重发该请求。
 */
void Capture_Tick(capture_sequence_t *s, uint32_t now, capture_light_fn light,
                  capture_send_fn send);
/**
 * @brief 处理 K230 的采图结果，验证令牌和时限后关灯。
 * @param s 当前采图事务。
 * @param payload 采图结果负载：8 字节令牌和 1 字节结果码。
 * @param len 负载长度（字节）。
 * @param now 当前 HAL 毫秒计数。
 * @param light 补光输出回调。
 * @note 仅在收到当前事务的成功结果且关灯输出成功时进入 CAP_READY。
 */
void Capture_Done(capture_sequence_t *s, const uint8_t *payload, size_t len, uint32_t now,
                  capture_light_fn light);
/**
 * @brief 取消当前事务并尝试关灯。
 * @param s 要取消的采图事务。
 * @param light 补光输出回调。
 * @note 取消后迟到的采图成功消息不会恢复放行。
 */
void Capture_Cancel(capture_sequence_t *s, capture_light_fn light);
