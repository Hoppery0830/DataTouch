// ============================================================
//  protocol.h — STM32 <-> ESP32-S3 通信帧协议
//  数据字段与任务书 MuJoCo/CSV 输出保持一致:
//    t, x, y, z, Fx, Fy, Fz, Mx, My, Mz, contact
//  说明: y 为 Y 轴位置 (阶段二扩展轴, 现为 0)
//  帧格式 (二进制, 小端):
//    [0]      sync0 = 0xAA
//    [1]      sync1 = 0x55
//    [2]      type  (帧类型, 见 FrameType)
//    [3]      len   (负载字节数)
//    [4..]    payload (len 字节)
//    [last]   crc8   (从 sync0 到 payload 末尾各字节累加/异或校验)
// ============================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#define FRAME_SYNC0       0xAA
#define FRAME_SYNC1       0x55

// ---- 帧类型 ----
enum FrameType {
  FRAME_DATA    = 0x01, // 单帧时间序列 (10 个 float + contact)
  FRAME_STATUS  = 0x02, // 状态机 (uint8)
  FRAME_METRICS = 0x03, // 分项指标 (柔软度/顺滑度/粗细度/回弹...)
  FRAME_SCORE   = 0x04, // 综合评分 (float 0-100)
  FRAME_CMD     = 0x05, // 手机 -> STM32 控制命令 (arm/start/stop)
};

// ---- 实验状态机 ----
enum ExpState {
  STATE_IDLE    = 0,
  STATE_ARMED   = 1, // 已 ARM，等待 START
  STATE_RUNNING = 2, // 采集中
  STATE_DONE    = 3, // 本次实验结束
  STATE_ERROR   = 4,
};

// ---- 负载结构：实际实验数据帧 ----
#pragma pack(push, 1)
struct TouchFrame {
  float  t;      // 时间戳 (秒, 相对 T0)
  float  x;      // X 位移 (mm)
  float  y;      // Y 位移 (mm) [阶段二扩展轴, 现为 0]
  float  z;      // Z 位移 (mm)
  float  Fx;     // 切向力 (N)
  float  Fy;     // 侧向力 (N)
  float  Fz;     // 法向力 (N)
  float  Mx;     // 力矩 (N·mm)
  float  My;
  float  Mz;
  uint8_t contact; // 接触标志 0/1
};
#pragma pack(pop)

// 载荷字段个数 (不含 contact 的 float 数量)
constexpr size_t DATA_FLOAT_CNT = 10;

// ---- 负载结构：FRAME_METRICS 四维特征向量 ----
// 四维 = STM32 归一化后的 [0,1] 值; 综合评分(0-100)也在 STM32 上计算, 是权威值;
// 前端(网页/小程序)只负责显示。DEMO 模式下前端 JS 的加权融合仅为演示, 不代表正式链路。
// 权重 / 归一化区间可调整, 但正式以 STM32 为准, 前端权重仅用于 DEMO/展示或可选微调。
// 字段方向 (归一化后 [0,1] 说明):
//   softness   柔软度   —— [0,1], 值越大越柔软
//   smoothness 顺滑度   —— [0,1], 值越大越顺滑(摩擦力越小)
//   roughness  细腻度/粗糙度 —— [0,1], 值越小越细腻 (评分时应取反, 方向注意!)
//   rebound    回弹贴合度 —— [0,1], 值越大回弹贴合越好
//   flags      通用标志 —— bit0=数据有效, 其余预留
#pragma pack(push, 1)
struct MetricsFrame {
  float  softness;   // 柔软度       (越大越软)
  float  smoothness; // 顺滑度       (越大越顺滑)
  float  roughness;  // 细腻度/粗糙度 (越小越细腻, 评分取反)
  float  rebound;    // 回弹贴合度    (越大回弹越好)
  uint8_t flags;     // 标志位: bit0=数据有效
};
#pragma pack(pop)

// 四维特征 float 数量 (不含 flags)
constexpr size_t METRICS_FLOAT_CNT = 4;

// ---- CRC-8 (多项式 0x31, 初值 0xFF) ----
inline uint8_t crc8_over(const uint8_t *buf, size_t n) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= buf[i];
    for (int b = 0; b < 8; ++b)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

// ---- 构造一帧到底层发送缓冲区 ----
// 返回帧总长度; buf 需至少能容纳 HEADER(4) + payload + CRC(1)
inline size_t build_frame(uint8_t *buf, uint8_t type, const uint8_t *payload, size_t len) {
  size_t i = 0;
  buf[i++] = FRAME_SYNC0;
  buf[i++] = FRAME_SYNC1;
  buf[i++] = type;
  buf[i++] = (uint8_t)len;
  for (size_t k = 0; k < len; ++k) buf[i++] = payload[k];
  buf[i++] = crc8_over(buf, i); // 覆盖 sync+type+len+payload
  return i;
}
