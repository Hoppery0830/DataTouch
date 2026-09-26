// ============================================================
//  cloud_upload.h — 设备上云（HTTPS 上传实验记录）
//  ------------------------------------------------------------
//  作用：
//    把"一次实验"的四维特征 + 综合评分，按云端约定字段拼成 JSON，
//    用 WiFiClientSecure + HTTPClient 以 HTTPS POST 上传；
//    断网/失败时进本地环形队列（NVS 持久化，掉电不丢），
//    联网后自动补传，dedupId 保证幂等（重传不会产生重复记录）。
//
//  设计原则（与现有工程的关系）：
//    · 只新增文件，不改协议（src/protocol.h 零改动）、不改 STM32；
//    · 不改 WiFi 配网 / WebSocket / 内嵌显示页 / DEMO 逻辑；
//    · main.cpp 只需 5 处小改动：include、setup() 调 cloudUploadInit()、
//      loop() 里 STATE_DONE 入队一次 + 排空"网页广播队列"、
//      新建 cloudTask(core 0) 周期调 cloudUploadLoop()、
//      handleCommand() 增加 "upload" 命令（网页「上传测试」按钮）。
//    · 全部 I/O 超时有界（连接/发送各 ≤ 6s），退避用 millis() 计时，
//      不在主 loop 里做 HTTPS，不阻塞网页/WebSocket 的正常推送。
//    · startedAt = "实验开始时刻"的 epoch 毫秒（13 位 JSON number）：
//      main.cpp 在 g_state 进入 STATE_RUNNING 的状态沿调
//      cloudUploadNoteRunningEdge() 记录；入队时由 cloudUploadResolveStartedAt()
//      把它换算成绝对时间。从未运行过（如 DEMO 直接 STOP）时回退"入队时刻"。
//
//  ============================================================
//  线程模型（V2 并发重构，务必遵守）
//  ------------------------------------------------------------
//    · 主 loop（core 1，loopTask）：
//        调用 cloudUploadEnqueueCurrent() / cloudUploadTestNow() / cloudUploadEnqueue()
//        入队，调用 cloudUploadGetConfig() / SetConfig() 读写配置，
//        并 **独占** 操作 WebSocketsServer（broadcastTXT）。
//    · cloudTask（core 0，main.cpp 里用 xTaskCreatePinnedToCore 创建）：
//        周期调用 cloudUploadLoop() 推进上传状态机（HTTPS 在这里阻塞，不再拖慢主 loop）。
//    · 模块内部：配置(s_cfg) / 环形队列(s_queue,s_head,s_count,s_seq,s_dropped) /
//        状态机(s_state,s_nextAttemptMs,s_failCount) / 最近结果 / NVS(s_prefs)
//        由一把互斥量保护，临界区只碰 RAM/NVS（毫秒级），**绝不**在持锁期间做网络 I/O
//        或等 SNTP —— 都是"锁内取副本 → 解锁 → 发网络 → 再加锁回写"。
//    · 通知回调可能从 cloudTask 线程触发，因此回调实现里 **不能** 直接调用
//        webSocket.broadcastTXT()；main.cpp 的做法是"回调只把消息投进 FreeRTOS 队列，
//        由主 loop 取出后再广播"，从而保证 WebSocketsServer 永远单线程访问。
//  ============================================================
//
//  使用示例（main.cpp）：
//    setup():  xQueueCreate(...) 建"网页广播队列"
//              cloudUploadSetNotifyCallback(cb);  // cb 里只做 xQueueSend
//              cloudUploadInit();                 // 读 NVS 配置 + 恢复队列 + 启动 SNTP
//              xTaskCreatePinnedToCore(cloudTask, … , 0);  // 必须在 Init 之后
//    loop():   if (g_state == STATE_DONE 的状态沿) cloudUploadEnqueueCurrent();
//              if (g_state == STATE_RUNNING 的状态沿) cloudUploadNoteRunningEdge();
//              drainCloudWsQueue();               // 主 loop 内 broadcastTXT
//    cloudTask(): cloudUploadLoop(); vTaskDelay(...);   // 推进上传状态机
//    网页命令: cloudUploadTestNow();              // 用当前 g_metrics/g_score 立刻传一条
//    网页路由: GET /cloud + POST /cloud/config   // 免重烧改 token/fabricName/deviceId
// ============================================================
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "protocol.h"

// ============================================================
//  编译期默认配置（可被 NVS 覆盖；NVS 里没有时用这些默认值）
//  也可以在 platformio.ini 的 build_flags 里 -D 覆盖，例如：
//    -DCLOUD_TOKEN=\"你的真实token\"
// ============================================================
#ifndef CLOUD_UPLOAD_URL
// 微信云开发 HTTP 访问服务地址（云函数 deviceUpload）
#define CLOUD_UPLOAD_URL "https://cloud1-d5gncnkxj2606d114-1487403371.ap-shanghai.app.tcloudbase.com/deviceUpload"
#endif

#ifndef CLOUD_TOKEN
// ⚠ 占位值：真实 token 请用 cloudUploadSetConfig() 写入 NVS（不要提交进 Git）
#define CLOUD_TOKEN "REPLACE_ME"
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "esp32-dev01"
#endif

#ifndef FABRIC_NAME
#define FABRIC_NAME "未命名布料"
#endif

// ---------- 可调参数 ----------
#ifndef CLOUD_QUEUE_SIZE
#define CLOUD_QUEUE_SIZE 16              // 环形队列条数（NVS 持久化，掉电不丢）
#endif
#ifndef CLOUD_CONNECT_TIMEOUT_MS
#define CLOUD_CONNECT_TIMEOUT_MS 6000    // TCP+TLS 连接超时（毫秒）
#endif
#ifndef CLOUD_TLS_HANDSHAKE_TIMEOUT_S
#define CLOUD_TLS_HANDSHAKE_TIMEOUT_S 8  // TLS 握手超时（秒，注意单位）
#endif
#ifndef CLOUD_RESPONSE_TIMEOUT_MS
#define CLOUD_RESPONSE_TIMEOUT_MS 6000   // 发送/等响应超时（毫秒）
#endif
#ifndef CLOUD_OFFLINE_POLL_MS
#define CLOUD_OFFLINE_POLL_MS 2000       // 离线时的复查间隔（毫秒）
#endif
#ifndef CLOUD_BOOT_RETRY_DELAY_MS
#define CLOUD_BOOT_RETRY_DELAY_MS 3000   // 开机后补传的等待时间（等 WiFi/SNTP 就绪）
#endif
#ifndef CLOUD_SUCCESS_NEXT_DELAY_MS
#define CLOUD_SUCCESS_NEXT_DELAY_MS 1000 // 一条成功后，下一条的间隔
#endif

// 失败退避（指数）：5s → 15s → 60s → 300s（封顶）
#ifndef CLOUD_BACKOFF_1_MS
#define CLOUD_BACKOFF_1_MS 5000
#endif
#ifndef CLOUD_BACKOFF_2_MS
#define CLOUD_BACKOFF_2_MS 15000
#endif
#ifndef CLOUD_BACKOFF_3_MS
#define CLOUD_BACKOFF_3_MS 60000
#endif
#ifndef CLOUD_BACKOFF_4_MS
#define CLOUD_BACKOFF_4_MS 300000
#endif

#ifndef CLOUD_DEFAULT_OPERATOR
#define CLOUD_DEFAULT_OPERATOR "operator"   // JSON 里的 operator 字段默认值（可传入覆盖）
#endif

// ---- 时间(SNTP)相关 ----
// "时间有效"下限：epoch 秒 ≥ 1600000000（2020-09-13）。上电未同步时系统时间是 1970 年，
// 用它做判据可以避免把 1970/单片机开机秒数当成真实时间发上云。
#ifndef CLOUD_EPOCH_SEC_MIN
#define CLOUD_EPOCH_SEC_MIN 1600000000UL
#endif
// epoch 毫秒下限（13 位起点，约 2020-09-13）
#define CLOUD_EPOCH_MS_MIN 1600000000000ULL

// 入队时若 SNTP 还没同步，最多等这么久（毫秒）尝试同步；超时则 startedAt 传 0 + 串口告警。
// 选择"短等待 + 兜底传 0"而不是无限等待：loop() 不能被长时间阻塞（网页/WebSocket 要刷新）。
#ifndef CLOUD_TIME_SYNC_WAIT_MS
#define CLOUD_TIME_SYNC_WAIT_MS 1500
#endif

// 重新发起 SNTP 的最小间隔（毫秒），避免频繁 configTime()
#ifndef CLOUD_SNTP_RETRIGGER_MS
#define CLOUD_SNTP_RETRIGGER_MS 10000
#endif

// ============================================================
//  数据结构
// ============================================================

// 一条实验记录的"数值部分"（JSON 组包时用）
// dedupId 留空则由模块自动生成唯一值；
// startEpochMs = "实验开始时刻"的 epoch 毫秒（13 位）。0 = 由模块按下面顺序推导：
//   1) 实验开始时刻（cloudUploadNoteRunningEdge() 记录）换算出的绝对时间；
//   2) 回退：入队时刻（此时 SNTP 若仍未同步则为 0）。
struct CloudRecord {
  float    softness;      // 柔软度   (0~1，来自 g_metrics)
  float    smoothness;    // 顺滑度   (0~1)
  float    roughness;     // 细腻度/粗糙度 (0~1，越小越细腻)
  float    rebound;       // 回弹贴合度 (0~1)
  float    composite;     // 综合评分 (0~100，来自 g_score)
  char     op[16];        // operator 字段
  char     dedupId[64];   // 幂等键（空 = 自动生成 deviceId-millis-seq）
  uint64_t startEpochMs;  // 实验开始时刻(epoch 毫秒)；0 = 由模块推导(见上)
};

// 配置（NVS 命名空间 "cloud"）
struct CloudConfig {
  String url;         // 上传地址
  String token;       // 云端校验 token
  String deviceId;    // 设备唯一 ID
  String fabricName;  // 布料名（可配置）
};

// 状态回调：msg 已经是可直接广播/展示的文本，格式 "CLOUD,<事件>,<说明>"
//   事件: CFG(初始化) ENQ(入队) SEND(开始上传) OK(成功) FAIL(失败) OFF(离线等待) DROP(队列满丢最旧)
//   <说明> 内不含逗号
typedef void (*CloudNotifyCallback)(const char *msg);

// ============================================================
//  对外接口
//  ------------------------------------------------------------
//  线程安全说明（V2）：以下接口内部均已做互斥保护，可从主 loop(core 1) 或
//  cloudTask(core 0) 安全调用；唯一约束见 cloudUploadLoop() 的注释。
//  ⚠ 例外：通知回调（CloudNotifyCallback）可能运行在 cloudTask 线程，
//     实现里不得直接操作 WebSocketsServer / WebServer。
// ============================================================

// 初始化：读 NVS 配置（无则用编译期默认值并写回 NVS）、恢复断电前的队列、启动 SNTP。
// 在 setup() 里 WiFi / WebSocket / HTTP 就绪后调用一次；**必须早于 cloudTask 的创建**
//（互斥量在这里创建，先建锁再起云任务）。
void cloudUploadInit();

// 入队一条实验记录（显式传值）。返回 true = 已成功入队（不代表已上传成功）。
// 由主 loop 调用（会读 g_metrics/g_score 的快照并立刻冻结 JSON）。
bool cloudUploadEnqueue(const CloudRecord &rec);

// 入队一条实验记录（用当前 g_metrics / g_score 组包；即 STATE_DONE / 「上传测试」用的入口）
bool cloudUploadEnqueueCurrent(const char *op = nullptr);

// 推进上传状态机（内部仅在到期且有网时做一次有界超时的 HTTPS POST）。
// ⚠ 调用线程约定：请在 **cloudTask（core 0）** 里周期调用（每轮 20~50ms + vTaskDelay）。
//    主 loop 不再调用它 —— HTTPS 的阻塞时间（连接 ≤6s / TLS ≤8s / 响应 ≤6s）
//    只会占用 core 0 上的云任务，UART 接收与 WebSocket 推送照常。
void cloudUploadLoop();

// 立刻用当前 g_metrics / g_score 传一条测试记录（网页「上传测试」按钮 / 串口命令）
void cloudUploadTestNow();

// WiFi 是否已连上（有网才谈得上上云）
bool cloudUploadIsOnline();

// 当前待上传条数（队列深度）/ 因队列满被丢弃的累计条数
int  cloudUploadPendingCount();
int  cloudUploadDroppedCount();

// 最近一次上传结果（便于串口/网页查看）
const char *cloudUploadLastResult();

// 写 / 读配置（写 = 落 NVS，掉电不丢）
// url / token / deviceId / fabricName 任一为空则回落编译期默认值（避免把配置写坏）；
// 写完后立即生效（下一次组包就用新值），无需重启。
void        cloudUploadSetConfig(const String &url, const String &token,
                                 const String &deviceId, const String &fabricName);
CloudConfig cloudUploadGetConfig();

// 注册上传状态回调（传 nullptr 取消）；用于把状态广播到网页
void cloudUploadSetNotifyCallback(CloudNotifyCallback cb);

// ---------------- 时间（SNTP）相关接口 ----------------

// current epoch 毫秒（13 位）。时间无效（未同步/1970）时返回 0。
// 注意：返回 uint64_t —— 当前 epoch 毫秒约 1.7e12，放不进 32 位，
// 这正是修复前 startedAt 变成 2693757455 的根因。
uint64_t cloudUploadNowMs();

// 时间是否已有效（epoch 秒 ≥ CLOUD_EPOCH_SEC_MIN）
bool cloudUploadTimeSynced();

// 主动请求一次 SNTP 同步（内部限流，最短间隔 CLOUD_SNTP_RETRIGGER_MS）。
// 联网成功、或发现时间无效时可以调。
void cloudUploadSntpSync();

// 【实验开始时刻】在 g_state 进入 STATE_RUNNING 的"状态沿"调用一次（main.cpp loop() 里）。
// 记录单调时钟 millis()，之后由 cloudUploadResolveStartedAt() 换算成 epoch 毫秒。
void cloudUploadNoteRunningEdge();

// 解析本条记录要上传的 startedAt（epoch 毫秒）：
//   1) rec.startEpochMs 有效（> CLOUD_EPOCH_MS_MIN）→ 直接用；
//   2) 有"实验开始时刻"记录 → 用 当前epoch - (millis()-记录时刻) 换算（即使入队时才同步好）；
//   3) 回退：入队时刻（SNTP 未同步时为 0，并串口告警）。
// 返回 true = 是真实绝对时间；false = 时间无效（返回值 0，不发送错误数字）。
bool cloudUploadResolveStartedAt(const CloudRecord &rec, uint64_t &outMs);
