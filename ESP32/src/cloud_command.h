// ============================================================
//  cloud_command.h — 云端远程命令（云函数 deviceCommand → ESP32 → STM32）
//  ------------------------------------------------------------
//  作用：
//    在 core 0 的 cloudTask 里按固定间隔向云函数 deviceCommand 发
//      {"action":"poll","token":...,"deviceId":...}
//    取回小程序/网页下发的控制命令（白名单 arm / start / stop / reset
//    + 运动模式 mode_press / mode_rub / mode_slide），
//    交给主 loop 执行（改 g_state、广播 WebSocket、非 DEMO 时给 STM32 发 FRAME_CMD；
//    模式命令只改 g_actionMode 并在非 DEMO 时发 FRAME_ACTION，**不改 g_state**），
//    再发 {"action":"ack",...,"id":...,"result":"ok"} 回执。
//
//  与现有工程的关系（保守增量，不破坏任何既有功能）：
//    · 只新增文件；protocol.h / cloud_upload.* / 配网门户 / WebSocket / UART 全部零改动；
//    · main.cpp 只加 5 处：① include ② setup() 调 cloudCommandInit()
//      ③ cloudTask() 里加一行 cloudCommandLoop() ④ loop() 里加一行排空命令队列
//      ⑤ handleCommand() 增加"来源=云端"的可选参数（有默认值 → 原网页调用点不变）。
//    · 不加任何第三方依赖（WiFiClientSecure / HTTPClient / FreeRTOS 都是 ESP32 自带）。
//
//  线程模型（沿用 cloud_upload.h 里 V2 的既有纪律，见该文件顶部说明）：
//    · cloudTask(core 0)：调用 cloudCommandLoop()。HTTPS 轮询(连接 4s/TLS 6s/响应 4s)
//      与 ack 全部在这条任务里**有界阻塞**；模块内部状态（轮询计时、失败退避、
//      最近处理的 id 环形表、统计计数）只被这条任务读写，无需加锁。
//    · 主 loop(core 1)：loop() 里 cloudCommandTake() 取出命令 →
//      handleCommand(cmd, true, id) 执行 → cloudCommandAckResult(id,"ok") 投回执。
//      **WebSocket 广播(pushState) 与 UART2 写(FRAME_CMD) 只在主 loop 做**，
//      与既有"云任务只投队列、主 loop 才广播"的约定完全一致。
//    · 唯一的跨线程通道是两个 FreeRTOS 队列：
//        命令队列 s_cmdQ(云任务 → 主 loop) / 回执队列 s_ackQ(主 loop → 云任务)；
//      配置(token/deviceId/上传URL)只在锁内取副本 → 走 cloudUploadGetConfig()
//      （cloud_upload.cpp 内部那把互斥量），**绝不持锁做网络**。
// ============================================================
#pragma once

#include <Arduino.h>
#include "protocol.h"

// ============================================================
//  编译期开关与可调参数（都可用 platformio.ini 的 build_flags -D 覆盖）
// ============================================================

// 远程控制总开关（默认开启）。
//   0 = 完全不轮询（连 poll 都不发）→ 设备侧彻底没有远程控制面，省流量；
//       云端未消费的命令会在其 60s 过期窗口内自然失效，重新打开时不会"补执行"旧命令。
//   1 = 正常轮询并按白名单执行。
// 运行期还可用 cloudCommandSetEnabled(false) 临时关闭（不加 NVS，避免多一份掉电状态）。
#ifndef CLOUD_CMD_ENABLE
#define CLOUD_CMD_ENABLE 1
#endif

// 命令 URL 的**编译期兜底值**：只有当上传 URL 里匹配不到 "deviceUpload" 时才用它。
// 默认与 CLOUD_UPLOAD_URL 同域名、路径换成 /deviceCommand。
#ifndef CLOUD_COMMAND_URL
#define CLOUD_COMMAND_URL "https://cloud1-d5gncnkxj2606d114-1487403371.ap-shanghai.app.tcloudbase.com/deviceCommand"
#endif

// 轮询间隔（毫秒）。越小越灵敏，越费流量/电；2000ms 时最坏命令延迟 ≈ 2s + 一次 HTTPS。
#ifndef CLOUD_CMD_POLL_MS
#define CLOUD_CMD_POLL_MS 2000
#endif

// 单次请求的超时（比上传更短：远程控制要"快"，卡住就尽快退避让位给上传）
#ifndef CLOUD_CMD_CONNECT_TIMEOUT_MS
#define CLOUD_CMD_CONNECT_TIMEOUT_MS 4000   // TCP+TLS 连接超时（毫秒）
#endif
#ifndef CLOUD_CMD_RESPONSE_TIMEOUT_MS
#define CLOUD_CMD_RESPONSE_TIMEOUT_MS 4000  // 发送/等响应超时（毫秒）
#endif
#ifndef CLOUD_CMD_TLS_HANDSHAKE_TIMEOUT_S
#define CLOUD_CMD_TLS_HANDSHAKE_TIMEOUT_S 6 // TLS 握手超时（秒，注意单位）
#endif

// 轮询失败退避：2s→4s→8s→16s→30s(封顶)，成功即清零。
#ifndef CLOUD_CMD_MAX_BACKOFF_MS
#define CLOUD_CMD_MAX_BACKOFF_MS 30000
#endif

// 命令本地过期兜底（毫秒）。云端已保证"不下发 60s 以上的命令"，这里只在
// 响应里带 ts/createdAt 且本地时间已同步时才做二次判断（判不准就不判，避免误杀）。
#ifndef CLOUD_CMD_MAX_AGE_MS
#define CLOUD_CMD_MAX_AGE_MS 60000
#endif

// 最近处理过的命令 id 环形表深度（用于同一 id 去重：云端重投只补 ack，不重复执行）
#ifndef CLOUD_CMD_RECENT_LEN
#define CLOUD_CMD_RECENT_LEN 8
#endif

// 单次 poll 最多处理几条命令（多余的忽略；未 ack 的云端可重投）
#ifndef CLOUD_CMD_MAX_PER_POLL
#define CLOUD_CMD_MAX_PER_POLL 4
#endif

// 每次 cloudCommandLoop() 最多发几条 ack（每条 ack 是一次有界阻塞 HTTPS）
#ifndef CLOUD_CMD_ACK_MAX_PER_LOOP
#define CLOUD_CMD_ACK_MAX_PER_LOOP 2
#endif

// ---- 结构尺寸（POD，供 FreeRTOS 队列按值拷贝）----
#define CLOUD_CMD_ID_LEN     40   // 云端命令 id（Mongo _id 24 位十六进制，留足余量）
#define CLOUD_CMD_NAME_LEN   12   // 命令名（arm/start/stop/reset/mode_press/mode_rub/mode_slide）
#define CLOUD_CMD_RESULT_LEN 12   // 回执 result
#define CLOUD_CMD_QUEUE_LEN  8    // 命令队列深度（云任务 → 主 loop）
#define CLOUD_CMD_ACKQ_LEN   32    // 回执队列深度（主 loop → 云任务）

// 一条待执行的远程命令
struct CloudCmdMsg {
  char id[CLOUD_CMD_ID_LEN];      // 云端命令 id（回执时要原样带回）
  char cmd[CLOUD_CMD_NAME_LEN];   // 命令名
};

// ============================================================
//  对外接口
// ============================================================

// 初始化：创建命令/回执队列与内部互斥量，打印开关状态与推导出的命令 URL。
// ⚠ 必须在 setup() 里调用，且**早于 cloudTask 的创建**（先建队列再起任务）；
//   队列要在主 loop 第一次 cloudCommandTake() 之前就绪。
void cloudCommandInit();

// 推进远程命令模块（内部：先发回执，再按间隔 poll 一次并应用命令）。
// ⚠ 调用线程约定：只在 **cloudTask（core 0）** 里周期调用（与 cloudUploadLoop 同一任务，
//   两者串行执行 → 同一时刻只有一路 TLS，栈/内存占用与既有上传完全一致）。
void cloudCommandLoop();

// 主 loop 调用：非阻塞取出至多一条待执行命令。返回 true = out 有效。
bool cloudCommandTake(CloudCmdMsg &out);

// 主 loop 调用：把一条执行结果投进回执队列（非阻塞、线程安全；队列满丢最旧）。
// result 约定："ok" / "unknown" / "expired"（字符串会写进 ack 的 result 字段）。
void cloudCommandAckResult(const char *id, const char *result);

// 远程控制开关（默认由 CLOUD_CMD_ENABLE 决定；运行期可临时切）
bool cloudCommandEnabled();
void cloudCommandSetEnabled(bool en);

// 当前生效的命令 URL（由"上传 URL"推导，见实现里的 deriveCommandUrl()）。
// 返回的是**值**（不是共享静态缓冲），可从任意线程调用，仅供串口日志/网页展示。
String cloudCommandUrlText();

// 命令是否在白名单内（arm / start / stop / reset / mode_press / mode_rub / mode_slide）；
// 供主 loop 二次校验与展示用
bool cloudCommandIsWhitelisted(const char *cmd);

#include "remote_protocol.h"
// loopTask publishes a snapshot; cloudTask includes it in the next poll.
void cloudCommandSetStatus(const remote_status_t &status,bool alive);
