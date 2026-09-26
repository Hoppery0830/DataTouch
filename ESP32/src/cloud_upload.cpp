// ============================================================
//  cloud_upload.cpp — 设备上云（HTTPS 上传实验记录）实现
//  ------------------------------------------------------------
//  · 组包：把一条实验记录拼成 JSON（字段与云端对齐，见 buildBody()）
//  · 上传：WiFiClientSecure + HTTPClient，POST application/json（超时有界）
//  · 断网：环形队列（默认 16 条）+ NVS 持久化（掉电不丢）
//  · 失败：指数退避 5s/15s/60s/300s；联网后自动补传
//  · 幂等：dedupId = deviceId-millis-seq（seq 落 NVS，重启不重号）
//  · 非阻塞：loop() 里不用长 delay()，退避靠 millis() 计时；
//            仅当"到期且有网"时做一次 HTTP 调用（连接/发送各 ≤ 6s）
//
//  · 并发（V2 重构）：本模块的状态机现由 main.cpp 的独立任务 cloudTask（core 0）推进，
//    而"入队 / 改配置 / 读配置"仍由主 loop（core 1）发起，两个任务会同时访问
//    配置(s_cfg)、环形队列(s_queue/s_head/s_count)、状态机(s_state/s_nextAttemptMs…)
//    和 NVS(s_prefs) → 统一用一把互斥量 s_mtx 保护。
//    ⚠ 铁律：临界区里只允许碰 RAM/NVS（微秒~毫秒级），
//      **绝不**在持锁期间做 HTTPS/TLS 网络调用，也不在持锁期间等 SNTP(最长 1.5s)。
//      做法：锁内取"待发 JSON 副本 / 配置副本 / 计数"，解锁后再发；发完再加锁回写状态。
//
//  · 通知回调（→ 网页广播）可能从 cloudTask 线程被调用，
//    所以 main.cpp 注册的回调只做"投递 FreeRTOS 队列"，真正的 broadcastTXT 由主 loop 执行。
// ============================================================
#include "cloud_upload.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <esp_timer.h>   // esp_timer_get_time(): 开机以来微秒, 用于推算毫秒子秒部分
#include <freertos/FreeRTOS.h>    // 并发重构: 互斥量
#include <freertos/semphr.h>

// ------------------------------------------------------------
//  main.cpp 里的"当前实验数据"（只引用、不定义；这样不必改动 main.cpp 的结构）
//    g_metrics: 四维特征（已归一化 0~1）+ flags（bit0=数据有效）
//    g_score  : 综合评分 0~100
// ------------------------------------------------------------
extern MetricsFrame g_metrics;
extern float        g_score;

// ============================================================
//  内部状态（文件作用域，外部不可见）
// ============================================================
namespace {

Preferences  s_prefs;                    // NVS 命名空间 "cloud"（与 WiFi 的 "wifi" 分开）
bool         s_nvsReady = false;
CloudConfig  s_cfg;                      // 当前生效配置

String       s_queue[CLOUD_QUEUE_SIZE];  // 环形队列：槽里存"已序列化好的 JSON 原文"
uint8_t      s_head  = 0;                // 最旧一条所在槽位
uint8_t      s_count = 0;                // 有效条数
uint32_t     s_seq   = 0;                // 单调递增实验序号（NVS 持久化）→ dedupId 唯一
uint32_t     s_dropped = 0;              // 队列满而丢弃的累计条数

enum CloudState { CLOUD_ST_IDLE, CLOUD_ST_WAIT, CLOUD_ST_SENDING };
CloudState   s_state         = CLOUD_ST_IDLE;
uint32_t     s_nextAttemptMs = 0;        // 下一次允许尝试上传的时刻
uint32_t     s_failCount     = 0;        // 连续失败次数（决定退避档位）
bool         s_offlineLogged = false;    // 离线提示只打一次，避免刷屏

String       s_lastResult = "无";        // 最近一次上传结果
CloudNotifyCallback s_notify = nullptr;  // 状态回调（→ 网页广播）

// ---- 并发保护（主 loop 与 cloudTask 共享以上所有状态）----
// 一把互斥量即可：临界区都很短，且队列/配置/状态机之间有"要么一起改"的耦合关系。
SemaphoreHandle_t s_mtx = nullptr;

// ---- 时间 / 实验开始时刻 ----
uint32_t     s_sntpLastReqMs   = 0;      // 上次调 configTime() 的时刻（限流用）
bool         s_sntpSyncLogged  = false;  // "同步成功"只打印一次
bool         s_expHasStart     = false;  // 是否记录过"实验开始时刻"
uint32_t     s_expStartMillis  = 0;      // 实验开始时刻的单调时钟（millis()）
uint64_t     s_expStartEpochMs = 0;      // 实验开始时刻的 epoch 毫秒（记录时若已同步则直接有值）

}  // namespace

// ============================================================
//  并发原语：一把互斥量 + 加解锁
//  ------------------------------------------------------------
//  创建点固定在 cloudUploadInit()；main.cpp 的顺序是
//    cloudUploadInit()  →  xTaskCreatePinnedToCore(cloudTask…)
//  即"先建锁、再起云任务"，因此不存在两任务同时创建锁的竞态。
//  加锁失败（s_mtx 尚未创建，只有极早期调用可能遇到）时按"无锁"继续，
//  保证不因为重构把原有单线程流程卡死。
// ============================================================
static void cloudMutexEnsure() {
  if (s_mtx == nullptr) s_mtx = xSemaphoreCreateMutex();
}

static inline void cloudLock()   { if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void cloudUnlock() { if (s_mtx) xSemaphoreGive(s_mtx); }

// 锁内取一份配置副本：调用方拿到副本后立刻解锁，后续组包/HTTPS 都用副本 →
// 既不会读到"改配置改到一半"的状态，也不会持锁做阻塞网络操作。
static CloudConfig cloudCfgSnapshot() {
  cloudLock();
  if (!s_nvsReady) { s_prefs.begin("cloud", false); s_nvsReady = true; }  // NVS 延迟打开(见 nvsBegin)
  CloudConfig c = s_cfg;
  cloudUnlock();
  return c;
}

// ============================================================
//  小工具
// ============================================================

// NVS 延迟打开（cloudUploadSetConfig 可能早于 cloudUploadInit 被调用）
static void nvsBegin() {
  if (!s_nvsReady) {
    s_prefs.begin("cloud", false);
    s_nvsReady = true;
  }
}

// NaN / Inf 判定（不依赖 math 宏，避免编译差异）
static inline bool badFloat(float v) {
  return (v != v) || (v > 3.0e38f) || (v < -3.0e38f);
}

// 四维限幅到 [0,1]
static float sane01(float v) {
  if (badFloat(v)) return 0.0f;
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// 综合评分限幅到 [0,100]
static float sane100(float v) {
  if (badFloat(v)) return 0.0f;
  return v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
}

// 绝对时间（毫秒）。SNTP 未同步时返回 0（ESP32 上电默认从 1970 开始计时，>2020-09 才算已同步）
//
// ⚠ 这里必须是 uint64_t：当前 epoch 毫秒约 1.73e12（13 位），
//   用 uint32_t 会溢出取模 —— 这正是修复前 startedAt=2693757455 的根因。
//
// 子秒部分用开机以来单调时钟 esp_timer_get_time() 推算（不再用 millis()%1000：
// 那样毫秒会在"秒"的任意相位跳变，最大能差到近 1 秒且不单调）。
static uint64_t nowEpochMs() {
  time_t t = time(nullptr);
  if (t < (time_t)CLOUD_EPOCH_SEC_MIN) return 0;      // 未同步 → 不返回假的 1970 时间
  uint64_t us = (uint64_t)esp_timer_get_time();       // 开机以来微秒
  uint64_t subMs = (us / 1000ULL) % 1000ULL;
  return (uint64_t)t * 1000ULL + subMs;
}

// millis() 环绕安全的"到点判断"
static inline bool timeReached(uint32_t now, uint32_t due) {
  return (int32_t)(now - due) >= 0;
}

// JSON 字符串转义（引号 / 反斜杠 / 控制字符；中文 UTF-8 高字节原样保留）
static String jsonEsc(const char *s) {
  String o;
  if (!s) return o;
  o.reserve(strlen(s) + 8);
  for (const char *p = s; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20)         { o += ' '; }   // 控制字符替换为空格，保证 JSON 合法
    else                       { o += (char)c; }
  }
  return o;
}

// token 打日志时脱敏
static String maskToken(const String &t) {
  if (t.length() <= 4) return "****";
  return t.substring(0, 4) + "****";
}

// 截断长文本（响应体打日志用）
static String brief(const String &s, size_t maxLen = 200) {
  String t = s;
  t.replace('\n', ' ');
  t.replace('\r', ' ');
  if (t.length() > maxLen) t = t.substring(0, maxLen) + "...";
  return t;
}

// ============================================================
//  队列（环形 + NVS 持久化）
//  ⚠ 本节的 4 个函数都只允许在 **持锁（cloudLock）** 状态下调用：
//    它们会改 s_head/s_count/s_queue 与 NVS，必须与另一线程的读/写互斥。
// ============================================================

static String keyOf(uint8_t slot) {           // NVS 键名: q0..q15（键长 ≤15 字符）
  String k = "q";
  k += (int)slot;
  return k;
}

static uint8_t slotOf(uint8_t i) {            // 第 i 条(0=最旧)对应的槽位
  return (uint8_t)((s_head + i) % CLOUD_QUEUE_SIZE);
}

static void queuePersistMeta() {
  s_prefs.putUChar("qhead", s_head);
  s_prefs.putUChar("qcount", s_count);
}

// 入队：NVS 立刻落盘（掉电不丢）；队列满则丢最旧并计数
static void queuePush(const String &body) {
  uint8_t slot;
  if (s_count < CLOUD_QUEUE_SIZE) {
    slot = slotOf(s_count);
    s_count++;
  } else {
    slot = s_head;                                            // 满：让位给最新
    s_head = (uint8_t)((s_head + 1) % CLOUD_QUEUE_SIZE);
    s_dropped++;
    s_prefs.putUInt("drop", s_dropped);
    Serial.printf("[CLOUD] ! 队列已满(%d)，丢弃最旧一条（累计丢弃 %lu）\n",
                  (int)CLOUD_QUEUE_SIZE, (unsigned long)s_dropped);
  }
  s_queue[slot] = body;
  s_prefs.putString(keyOf(slot).c_str(), body);
  queuePersistMeta();
}

static bool queuePeek(String &out) {           // [须持锁] 只读队首（取副本）
  if (s_count == 0) return false;
  out = s_queue[s_head];
  return out.length() > 0;
}

// 出队：只有云端确认成功(code=0/2xx)后才调用
static void queuePop() {
  if (s_count == 0) return;
  s_prefs.remove(keyOf(s_head).c_str());
  s_queue[s_head] = "";
  s_head = (uint8_t)((s_head + 1) % CLOUD_QUEUE_SIZE);
  s_count--;
  queuePersistMeta();
}

// ============================================================
//  dedupId / JSON 组包
// ============================================================

// 唯一幂等键：deviceId-millis-seq（seq 落 NVS 单调递增 → 重启后也不会重号）
// [须持锁] 会写 s_seq 与 NVS
static void makeDedupId(const CloudConfig &cfg, char *out, size_t n) {
  s_seq++;
  s_prefs.putUInt("seq", s_seq);
  snprintf(out, n, "%s-%lu-%lu",
           cfg.deviceId.c_str(),
           (unsigned long)millis(),
           (unsigned long)s_seq);
}

// 组包：字段名与云端约定一致（顺序保持稳定，便于日志比对）
// {"token","deviceId","fabricName","operator","softness","smoothness","roughness",
//  "rebound","composite","metricsNormalized":true,"weights"{...},"curveRef":"",
//  "dedupId","startedAt","source":"device"}
// startedAt 是 JSON number（13 位 epoch 毫秒；时间无效时才为 0），不是字符串。
// [不必持锁] 只读传入的 cfg 副本与 r，不碰任何共享状态（V2: 配置改为传副本进来，
//            这样"拼 JSON"这一步可以在锁外做，临界区更短）
static String buildBody(const CloudConfig &cfg, const CloudRecord &r,
                        const char *dedupId, uint64_t startedAtMs) {
  String b;
  b.reserve(600);
  b += "{\"token\":\"";                  b += jsonEsc(cfg.token.c_str());      b += "\",";
  b += "\"deviceId\":\"";                b += jsonEsc(cfg.deviceId.c_str());   b += "\",";
  b += "\"fabricName\":\"";              b += jsonEsc(cfg.fabricName.c_str()); b += "\",";
  b += "\"operator\":\"";                b += jsonEsc(r.op);                     b += "\",";
  b += "\"softness\":";                  b += String(sane01(r.softness), 3);     b += ",";
  b += "\"smoothness\":";                b += String(sane01(r.smoothness), 3);   b += ",";
  b += "\"roughness\":";                 b += String(sane01(r.roughness), 3);    b += ",";
  b += "\"rebound\":";                   b += String(sane01(r.rebound), 3);      b += ",";
  b += "\"composite\":";                 b += String(sane100(r.composite), 1);   b += ",";
  b += "\"metricsNormalized\":true,";
  b += "\"weights\":{\"softness\":0.30,\"smoothness\":0.25,\"roughness\":0.20,\"rebound\":0.25},";
  b += "\"curveRef\":\"\",";
  b += "\"dedupId\":\"";                 b += jsonEsc(dedupId);                  b += "\",";
  // 用 snprintf("%llu") 拼 64 位整数：String(uint64_t) 在部分核心上会被截断/歧义
  char tbuf[32];
  snprintf(tbuf, sizeof(tbuf), "%llu", (unsigned long long)startedAtMs);
  b += "\"startedAt\":";                 b += tbuf;
  b += ",\"source\":\"device\"}";
  return b;
}

// ============================================================
//  极简 JSON 取值（只为打日志/判成功，不引入 JSON 库）
// ============================================================

static bool parseJsonInt(const String &s, const char *key, long &out) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat);
  if (i < 0) return false;
  i = s.indexOf(':', i + (int)pat.length());
  if (i < 0) return false;
  i++;
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) i++;
  bool neg = false;
  if (i < (int)s.length() && s[i] == '-') { neg = true; i++; }
  if (i >= (int)s.length() || s[i] < '0' || s[i] > '9') return false;
  long v = 0;
  while (i < (int)s.length() && s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); i++; }
  out = neg ? -v : v;
  return true;
}

static String parseJsonStr(const String &s, const char *key) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat);
  if (i < 0) return "";
  i = s.indexOf(':', i + (int)pat.length());
  if (i < 0) return "";
  i = s.indexOf('"', i);
  if (i < 0) return "";
  int j = s.indexOf('"', i + 1);
  if (j < 0) return "";
  return s.substring(i + 1, j);
}

// ============================================================
//  状态通知（串口 + 可选网页广播）
//  ------------------------------------------------------------
//  ⚠ 本函数可能被 cloudTask(core 0) 调用：
//    · 锁内只"取回调指针副本"，解锁后再调用回调 —— 避免回调里再访问共享状态造成死锁；
//    · 回调约定为"只投递 FreeRTOS 队列"（main.cpp 里就是这么注册的），
//      所以这里不会出现"云任务直接操作 WebSocketsServer"的跨线程问题。
// ============================================================
static void notify(const char *ev, const String &detail) {
  Serial.printf("[CLOUD] %s: %s\n", ev, detail.c_str());

  cloudLock();
  CloudNotifyCallback cb = s_notify;      // 锁内取副本
  cloudUnlock();
  if (!cb) return;

  String d = detail;
  d.replace(',', ' ');
  d.replace('\n', ' ');
  d.replace('\r', ' ');
  if (d.length() > 120) d = d.substring(0, 120);
  String m = String("CLOUD,") + ev + "," + d;
  cb(m.c_str());                          // 锁外调用（回调内部只做 xQueueSend，非阻塞）
}

// ============================================================
//  HTTPS POST
// ============================================================
struct HttpOutcome {
  bool     ok;        // 业务成功（2xx 且 code==0）
  int      httpCode;  // HTTPClient 返回码（<0 表示连接/超时类错误）
  uint32_t costMs;
  String   resp;      // 响应体（截断后仅用于日志）
  String   err;       // 失败原因
};

static HttpOutcome doHttpPost(const CloudConfig &cfg, const String &body) {
  HttpOutcome o;
  o.ok = false; o.httpCode = 0; o.costMs = 0; o.resp = ""; o.err = "";

  if (cfg.url.length() == 0) { o.err = "URL 为空"; return o; }

  WiFiClientSecure client;
  // ⚠ MVP：setInsecure() 不校验服务器证书（方便先跑通）。
  //   正式版应改为内置根 CA 校验：client.setCACert(ROOT_CA_PEM);
  //   或用证书指纹：client.setFingerprint("AA:BB:...")。
  client.setInsecure();
  client.setHandshakeTimeout(CLOUD_TLS_HANDSHAKE_TIMEOUT_S);   // 单位: 秒

  HTTPClient http;
  http.setConnectTimeout(CLOUD_CONNECT_TIMEOUT_MS);            // 单位: 毫秒
  http.setTimeout(CLOUD_RESPONSE_TIMEOUT_MS);                  // 单位: 毫秒
  http.setReuse(false);

  if (!http.begin(client, cfg.url)) { o.err = "http.begin 失败(URL 无效?)"; return o; }
  http.addHeader("Content-Type", "application/json");

  uint32_t t0 = millis();
  int code = http.POST(body);                                  // 有界阻塞：≤ 连接超时 + 响应超时
  o.costMs = millis() - t0;
  if (code > 0) o.resp = http.getString();
  http.end();
  o.httpCode = code;

  if (code <= 0) {                                             // 连接失败 / 超时 / TLS 握手失败
    o.err = String("网络或超时(HTTPClient=") + code + ")";
    return o;
  }
  if (code < 200 || code >= 300) {                             // 4xx/5xx 等
    o.err = String("HTTP ") + code;
    return o;
  }
  // 2xx：若响应体带 code 字段，则要求 code==0（云函数统一 {code:0,...}）
  long biz = 0;
  if (parseJsonInt(o.resp, "code", biz) && biz != 0) {
    o.err = String("业务失败 code=") + biz;
    return o;
  }
  o.ok = true;
  return o;
}

// 退避：5s / 15s / 60s / 300s（封顶）
static uint32_t backoffFor(uint32_t fails) {
  static const uint32_t kBackoff[4] = {
    CLOUD_BACKOFF_1_MS, CLOUD_BACKOFF_2_MS, CLOUD_BACKOFF_3_MS, CLOUD_BACKOFF_4_MS
  };
  if (fails == 0) return 0;
  uint32_t i = fails - 1;
  if (i > 3) i = 3;
  return kBackoff[i];
}

// 尝试上传队首一条：成功则出队，失败则安排退避
// ------------------------------------------------------------
// 并发要点（V2）：本函数由 cloudTask(core 0) 调用。
//   ① 锁内：取"队首 JSON 副本 + 配置副本 + 计数"→ 立刻解锁；
//   ② 锁外：做有界阻塞的 HTTPS POST（此时主 loop 可以自由入队/改配置，不会卡）；
//   ③ 锁内：回写状态机 + 成功才 queuePop；
//   ④ 锁外：notify（回调只投递 FreeRTOS 队列）。
// ============================================================
static void attemptUpload() {
  // ---- ① 锁内快照（无网络、无 SNTP 等待）----
  String      body;
  CloudConfig cfg;
  String      dedupId;
  int         pendingBefore = 0;
  uint32_t    failsBefore   = 0;
  cloudLock();
  if (!queuePeek(body)) { cloudUnlock(); return; }
  cfg           = s_cfg;
  dedupId       = parseJsonStr(body, "dedupId");
  pendingBefore = (int)s_count;
  failsBefore   = s_failCount;
  cloudUnlock();

  notify("SEND", String("第") + (unsigned int)(failsBefore + 1) + "次尝试 " +
                 (dedupId.length() ? dedupId : String("(无 dedupId)")) +
                 " pending=" + pendingBefore);

  // ---- ② 锁外：HTTPS（连接 ≤6s / TLS ≤8s / 响应 ≤6s）；期间不持锁 ----
  HttpOutcome r = doHttpPost(cfg, body);
  if (r.resp.length() > 0) {
    Serial.printf("[CLOUD] 云端响应(HTTP %d): %s\n", r.httpCode, brief(r.resp, 200).c_str());
  }

  // ---- ③ 锁内：回写状态 + 出队 ----
  String   okId;
  int      pendingAfter = 0;
  uint32_t waitMs       = 0;
  cloudLock();
  if (r.ok) {
    okId         = parseJsonStr(r.resp, "_id");
    s_failCount  = 0;
    s_lastResult = String("OK HTTP ") + r.httpCode +
                   (okId.length() ? (" _id=" + okId) : String("")) +
                   " " + (unsigned long)r.costMs + "ms";
    queuePop();                                                // 只有成功才出队
    pendingAfter = (int)s_count;
    s_nextAttemptMs = millis() + CLOUD_SUCCESS_NEXT_DELAY_MS;   // 还有积压则稍后继续
  } else {
    s_failCount++;
    waitMs          = backoffFor(s_failCount);
    s_nextAttemptMs = millis() + waitMs;
    s_lastResult    = String("FAIL ") + r.err + " HTTP " + r.httpCode;
    pendingAfter    = (int)s_count;
  }
  cloudUnlock();

  // ---- ④ 锁外：通知（串口 + 投递到"网页广播队列"）----
  if (r.ok) {
    notify("OK", String("HTTP ") + r.httpCode +
                 (okId.length() ? (" _id=" + okId) : String("")) +
                 " 耗时" + (unsigned long)r.costMs + "ms pending=" + pendingAfter);
  } else {
    notify("FAIL", String("HTTP ") + r.httpCode + " " + r.err +
                   " 耗时" + (unsigned long)r.costMs + "ms; " +
                   (unsigned long)(waitMs / 1000) + "s 后重试 pending=" + pendingAfter);
  }
}

// ============================================================
//  对外接口实现
// ============================================================

void cloudUploadInit() {
  // ---- 0) 先建互斥量（必须在 cloudTask 启动之前；main.cpp 里先 Init 再建任务）----
  cloudMutexEnsure();

  // ---- 1)+2) 读配置 / 恢复队列：锁内完成（此时虽无并发，但保持与运行期一致的访问约定）----
  cloudLock();
  nvsBegin();
  s_cfg.url        = s_prefs.getString("url",    CLOUD_UPLOAD_URL);
  s_cfg.token      = s_prefs.getString("token",  CLOUD_TOKEN);
  s_cfg.deviceId   = s_prefs.getString("devid",  DEVICE_ID);
  s_cfg.fabricName = s_prefs.getString("fabric", FABRIC_NAME);
  if (s_cfg.url.length() == 0)        s_cfg.url        = CLOUD_UPLOAD_URL;
  if (s_cfg.token.length() == 0)      s_cfg.token      = CLOUD_TOKEN;
  if (s_cfg.deviceId.length() == 0)   s_cfg.deviceId   = DEVICE_ID;
  if (s_cfg.fabricName.length() == 0) s_cfg.fabricName = FABRIC_NAME;
  // 仅"首次/缺键"时写回，减少 NVS 写入磨损
  if (!s_prefs.isKey("url"))    s_prefs.putString("url",    s_cfg.url);
  if (!s_prefs.isKey("token"))  s_prefs.putString("token",  s_cfg.token);
  if (!s_prefs.isKey("devid"))  s_prefs.putString("devid",  s_cfg.deviceId);
  if (!s_prefs.isKey("fabric")) s_prefs.putString("fabric", s_cfg.fabricName);

  // ---- 2) 恢复断电前的队列（掉电不丢）----
  s_seq     = s_prefs.getUInt("seq", 0);
  s_dropped = s_prefs.getUInt("drop", 0);

  uint8_t head = (uint8_t)(s_prefs.getUChar("qhead", 0) % CLOUD_QUEUE_SIZE);
  uint8_t cnt  = s_prefs.getUChar("qcount", 0);
  if (cnt > CLOUD_QUEUE_SIZE) cnt = CLOUD_QUEUE_SIZE;

  String tmp[CLOUD_QUEUE_SIZE];
  uint8_t valid = 0;
  for (uint8_t i = 0; i < cnt; i++) {
    String body = s_prefs.getString(keyOf((uint8_t)((head + i) % CLOUD_QUEUE_SIZE)).c_str(), "");
    if (body.length() == 0) continue;                 // 空槽（异常/损坏）跳过
    tmp[valid++] = body;
  }
  if (valid != cnt) {
    // 有损坏槽位：清空重排后再落盘，保证 NVS 与 RAM 一致（仅异常路径会写 NVS）
    for (uint8_t k = 0; k < CLOUD_QUEUE_SIZE; k++) s_prefs.remove(keyOf(k).c_str());
    for (uint8_t k = 0; k < valid; k++) {
      s_queue[k] = tmp[k];
      s_prefs.putString(keyOf(k).c_str(), tmp[k]);
    }
    s_head = 0;
    s_count = valid;
    queuePersistMeta();
    Serial.printf("[CLOUD] ! 队列元数据异常，已重排为 %u 条\n", (unsigned)valid);
  } else {
    for (uint8_t k = 0; k < cnt; k++) s_queue[(uint8_t)((head + k) % CLOUD_QUEUE_SIZE)] = tmp[k];
    s_head  = head;
    s_count = cnt;
  }
  cloudUnlock();   // ← 队列恢复完毕，先解锁：下面 SNTP/打印都不需要持锁

  // ---- 3) SNTP（非阻塞；联网后自动同步，用于 startedAt 绝对时间）----
  // 只负责"发起"同步：configTime() 不在本地阻塞，同步由系统后台每秒重试。
  cloudUploadSntpSync();   // ⚠ 本函数内部自己加锁，故必须在锁外调用

  // ---- 4) 打印配置摘要（token 脱敏）----
  // 打印要读 s_cfg/s_count：重新加锁，打印完立刻解锁（纯串口输出，非网络阻塞）
  cloudLock();
  Serial.println("[CLOUD] ===== 设备上云(HTTPS) 初始化 =====");
  Serial.printf("[CLOUD] URL      : %s\n", s_cfg.url.c_str());
  Serial.printf("[CLOUD] deviceId : %s | fabricName: %s | operator默认: %s\n",
                s_cfg.deviceId.c_str(), s_cfg.fabricName.c_str(), CLOUD_DEFAULT_OPERATOR);
  Serial.printf("[CLOUD] token    : %s\n", maskToken(s_cfg.token).c_str());
  Serial.printf("[CLOUD] 队列     : %d/%d 条待传，累计丢弃 %lu\n",
                (int)s_count, (int)CLOUD_QUEUE_SIZE, (unsigned long)s_dropped);
  if (s_cfg.token == "REPLACE_ME") {
    Serial.println("[CLOUD] ! token 仍是占位值 REPLACE_ME：请用 cloudUploadSetConfig() 写入真实 token(NVS)");
  }
  if (!s_cfg.url.startsWith("https://")) {
    Serial.println("[CLOUD] ! URL 不是 https:// 开头，请确认云端地址");
  }
  uint64_t ep = nowEpochMs();
  if (ep == 0) {
    Serial.println("[CLOUD] · SNTP 尚未同步(时间无效)：startedAt 暂传 0（联网同步成功后自动变为 13 位 epoch 毫秒）");
  } else {
    Serial.printf("[CLOUD] · SNTP 已同步：now=%llu ms (epoch 毫秒)\n", (unsigned long long)ep);
    s_sntpSyncLogged = true;
  }

  // ---- 5) 若有掉电残留，稍等再开始补传（给 WiFi/SNTP 一点时间）----
  if (s_count > 0) {
    s_state = CLOUD_ST_WAIT;
    s_nextAttemptMs = millis() + CLOUD_BOOT_RETRY_DELAY_MS;
    Serial.printf("[CLOUD] 检测到 %d 条未上传记录，%lums 后自动补传\n",
                  (int)s_count, (unsigned long)CLOUD_BOOT_RETRY_DELAY_MS);
  } else {
    s_state = CLOUD_ST_IDLE;
    s_nextAttemptMs = millis();
  }
  // 供下面 notify 使用（锁外不再读共享状态）
  int    pendingNow = (int)s_count;
  String cfgDevId   = s_cfg.deviceId;
  cloudUnlock();

  notify("CFG", String("pending=") + pendingNow + " deviceId=" + cfgDevId);
}

// 入队前的"时间兜底"：SNTP 没同步就再发起一次同步，并短等一小会儿（有界，最长
// CLOUD_TIME_SYNC_WAIT_MS），让 startedAt 尽量拿到真实绝对时间。定义见文件末尾。
static void kickTimeSyncIfNeeded();

bool cloudUploadEnqueue(const CloudRecord &rec) {
  // 由主 loop(core 1) 调用。并发要点：
  //   · 配置/队列/seq 的读写都在锁内；
  //   · kickTimeSyncIfNeeded() 最长会等 1.5s，**必须在锁外**（否则云任务会被一起卡住）；
  //   · 组包(buildBody)用锁内取出的配置副本，可在锁外做。
  CloudConfig cfg = cloudCfgSnapshot();   // 锁内：NVS 就绪 + 配置副本

  CloudRecord r = rec;

  // operator 缺省
  if (r.op[0] == '\0') {
    strncpy(r.op, CLOUD_DEFAULT_OPERATOR, sizeof(r.op) - 1);
    r.op[sizeof(r.op) - 1] = '\0';
  }
  // dedupId：缺省则生成唯一值（要写 NVS 里的 seq → 锁内）
  char did[64];
  if (r.dedupId[0] == '\0') {
    cloudLock();
    makeDedupId(cfg, did, sizeof(did));
    cloudUnlock();
  } else {
    strncpy(did, r.dedupId, sizeof(did) - 1);
    did[sizeof(did) - 1] = '\0';
  }
  // ---- startedAt（= 实验开始时刻的 epoch 毫秒）----
  // 时间无效时先尝试触发/短等一次 SNTP 同步，避免把 0 或错误数字发上云
  kickTimeSyncIfNeeded();                 // ⚠ 锁外调用（内部自己加锁；最长等 1.5s）
  uint64_t startedAt = 0;
  bool     timeOk    = cloudUploadResolveStartedAt(r, startedAt);   // 内部自己加锁

  // 组包（锁外：只用 cfg 副本；g_metrics/g_score 也是在这一刻由主 loop 快照进 JSON，
  //       云任务读到的永远是"打包好的一份完整记录"，不会读到半更新的状态）
  String body = buildBody(cfg, r, did, startedAt);

  // ---- 入队（锁内：改 RAM 环形队列 + 写 NVS + 调状态机）----
  cloudLock();
  bool wasEmpty = (s_count == 0);
  queuePush(body);
  s_lastResult = String("已入队 ") + did;
  int pendingNow = (int)s_count;
  if (wasEmpty) {
    // 队列由空变非空 → 唤醒状态机并立刻尝试；退避计数清零
    s_state = CLOUD_ST_WAIT;
    s_failCount = 0;
    s_nextAttemptMs = millis();
  }
  // 队列本来就有积压时不打断既有的退避节奏（避免失败风暴）
  cloudUnlock();

  char tbuf[32];
  snprintf(tbuf, sizeof(tbuf), "%llu", (unsigned long long)startedAt);
  notify("ENQ", String("pending=") + pendingNow +
                " " + did +
                " composite=" + String(sane100(r.composite), 1) +
                " startedAt=" + tbuf +
                (timeOk ? "" : "(时间无效)"));
  if (!timeOk) {
    Serial.println("[CLOUD] ! 警告：SNTP 未同步，本条 startedAt 只能传 0（不发送错误时间戳）；"
                   "恢复联网同步后即恢复为 13 位 epoch 毫秒");
  }
  return true;
}

bool cloudUploadEnqueueCurrent(const char *op) {
  // ⚠ 本函数由主 loop(core 1) 调用：就在这里把 g_metrics / g_score "快照"成一条记录，
  //   之后 JSON 立即冻结（重传用的是同一份字节），云任务永远读不到半更新的状态。
  CloudRecord r = {};
  r.softness     = sane01(g_metrics.softness);
  r.smoothness   = sane01(g_metrics.smoothness);
  r.roughness    = sane01(g_metrics.roughness);
  r.rebound      = sane01(g_metrics.rebound);
  r.composite    = sane100(g_score);
  r.startEpochMs = 0;                // 0 = 由 enqueue 推导（实验开始时刻 → 入队时刻）
  r.dedupId[0]   = '\0';             // 由 enqueue 生成唯一 dedupId
  const char *o = (op && *op) ? op : CLOUD_DEFAULT_OPERATOR;
  strncpy(r.op, o, sizeof(r.op) - 1);
  r.op[sizeof(r.op) - 1] = '\0';

  if ((g_metrics.flags & 0x01) == 0) {
    Serial.printf("[CLOUD] ! 四维 flags=0x%02X（bit0=数据有效 未置位），仍按当前值入队\n",
                  (unsigned)g_metrics.flags);
  }
  Serial.printf("[CLOUD] 组包(当前值): soft=%.3f smooth=%.3f rough=%.3f reb=%.3f composite=%.1f source=device\n",
                r.softness, r.smoothness, r.roughness, r.rebound, r.composite);
  return cloudUploadEnqueue(r);
}

// 推进上传状态机（由 cloudTask(core 0) 周期调用；主 loop 不再调用 → 不再阻塞主循环）
// 并发要点：只在"读状态/判定到期"与"回写状态"时短暂持锁；
//           attemptUpload() 内部的 HTTPS 是锁外执行的（见该函数注释）。
void cloudUploadLoop() {
  uint32_t now = millis();

  // ---- 锁内：判定这一轮要不要发（读 s_count / s_state / s_nextAttemptMs）----
  bool     due      = false;
  bool     sending  = false;
  cloudLock();
  if (s_count == 0) {                      // 队列空：空闲
    s_state = CLOUD_ST_IDLE;
    s_failCount = 0;
    s_offlineLogged = false;
    cloudUnlock();
    return;
  }
  if (s_state == CLOUD_ST_IDLE) {          // 有新数据但状态还在空闲 → 唤醒
    s_state = CLOUD_ST_WAIT;
    s_nextAttemptMs = now;
  }
  if (s_state == CLOUD_ST_SENDING) sending = true;              // 发送中不会被重入
  else if (s_state == CLOUD_ST_WAIT) due = timeReached(now, s_nextAttemptMs);
  cloudUnlock();
  if (sending || !due) return;

  // ---- 没网：只等，不消耗退避档位（联网后自动补传）----
  if (!cloudUploadIsOnline()) {
    bool     logIt   = false;
    int      pending = 0;
    cloudLock();
    s_nextAttemptMs = millis() + CLOUD_OFFLINE_POLL_MS;
    if (!s_offlineLogged) {
      s_offlineLogged = true;
      logIt = true;
      pending = (int)s_count;
    }
    cloudUnlock();
    if (logIt) notify("OFF", String("离线(等联网) pending=") + pending);
    return;
  }

  cloudLock();
  s_offlineLogged = false;
  s_state = CLOUD_ST_SENDING;
  cloudUnlock();

  attemptUpload();                         // 锁外有界阻塞：连接/发送各 ≤ 6s，且每轮最多一条

  cloudLock();
  s_state = CLOUD_ST_WAIT;
  cloudUnlock();
}

void cloudUploadTestNow() {
  Serial.println("[CLOUD] ===== 手动测试上传（网页「上传测试」/ 串口 upload）=====");
  cloudUploadEnqueueCurrent("test");
}

bool cloudUploadIsOnline() {
  return WiFi.status() == WL_CONNECTED;    // 只读 WiFi 驱动状态，无需加锁
}

// 计数/结果查询：可能被"云任务写、主 loop 读"同时访问 → 锁内取值
int cloudUploadPendingCount() {
  cloudLock(); int n = (int)s_count; cloudUnlock(); return n;
}
int cloudUploadDroppedCount() {
  cloudLock(); int n = (int)s_dropped; cloudUnlock(); return n;
}

// 最近一次上传结果：锁内拷贝到静态缓冲后返回，避免把"可能被另一线程改写的 String"暴露出去
// （仅用于串口/网页展示，非重入安全；本工程只在单点调用）
const char *cloudUploadLastResult() {
  static char buf[96];
  cloudLock();
  strncpy(buf, s_lastResult.c_str(), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  cloudUnlock();
  return buf;
}

void cloudUploadSetConfig(const String &url, const String &token,
                          const String &deviceId, const String &fabricName) {
  // 由 HTTP /cloud/config（主 loop）调用；云任务可能正在读 s_cfg → 整个"改内存 + 写 NVS"放锁内。
  // 临界区只碰 RAM/NVS（毫秒级），不含任何网络操作，所以不会拖慢网页响应或云任务。
  cloudLock();
  nvsBegin();

  s_cfg.url        = url;
  s_cfg.token      = token;
  s_cfg.deviceId   = deviceId;
  s_cfg.fabricName = fabricName;
  // 空值回落默认，避免把配置写坏
  if (s_cfg.url.length() == 0)        s_cfg.url        = CLOUD_UPLOAD_URL;
  if (s_cfg.token.length() == 0)      s_cfg.token      = CLOUD_TOKEN;
  if (s_cfg.deviceId.length() == 0)   s_cfg.deviceId   = DEVICE_ID;
  if (s_cfg.fabricName.length() == 0) s_cfg.fabricName = FABRIC_NAME;

  s_prefs.putString("url",    s_cfg.url);
  s_prefs.putString("token",  s_cfg.token);
  s_prefs.putString("devid",  s_cfg.deviceId);
  s_prefs.putString("fabric", s_cfg.fabricName);

  Serial.printf("[CLOUD] 配置已保存到 NVS: url=%s deviceId=%s fabricName=%s token=%s\n",
                s_cfg.url.c_str(), s_cfg.deviceId.c_str(),
                s_cfg.fabricName.c_str(), maskToken(s_cfg.token).c_str());
  cloudUnlock();
}

// 返回配置"副本"（锁内拷贝）→ 调用方拿到的是快照，之后 s_cfg 再变也不影响它
CloudConfig cloudUploadGetConfig() {
  cloudLock();
  CloudConfig c = s_cfg;
  cloudUnlock();
  return c;
}

void cloudUploadSetNotifyCallback(CloudNotifyCallback cb) {
  cloudLock();
  s_notify = cb;
  cloudUnlock();
}

// ============================================================
//  时间（SNTP）与"实验开始时刻"
// ============================================================

void cloudUploadSntpSync() {
  uint32_t now = millis();
  // 限流：短时间内不重复 configTime()（首次 s_sntpLastReqMs==0 时必然放行）
  // "判断 + 更新"必须原子（主 loop 也会调本函数）→ 放锁内；configTime() 放锁外
  cloudLock();
  if (s_sntpLastReqMs != 0 && (uint32_t)(now - s_sntpLastReqMs) < CLOUD_SNTP_RETRIGGER_MS) {
    cloudUnlock();
    return;
  }
  s_sntpLastReqMs = now;
  cloudUnlock();
  // configTime() 只是"发起/重启"SNTP 客户端，不阻塞；同步由系统后台任务完成
  configTime(0, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn", "pool.ntp.org");
}

bool cloudUploadTimeSynced() { return nowEpochMs() != 0; }   // 只读系统时间，无需加锁

uint64_t cloudUploadNowMs() { return nowEpochMs(); }         // 只读系统时间，无需加锁

void cloudUploadNoteRunningEdge() {
  // 主 loop 在进入 STATE_RUNNING 的状态沿调用；云任务/入队路径会读这三个字段 → 锁内写
  uint32_t m = millis();
  uint64_t ep = nowEpochMs();            // 记录瞬间就已同步的话，直接给出绝对时间
  cloudLock();
  s_expHasStart     = true;
  s_expStartMillis  = m;
  s_expStartEpochMs = ep;
  cloudUnlock();
  Serial.printf("[CLOUD] 记录实验开始时刻: millis=%lu  epochMs=%llu%s\n",
                (unsigned long)m,
                (unsigned long long)ep,
                ep ? "" : " (SNTP 未同步，稍后同步成功会自动回算)");
}

bool cloudUploadResolveStartedAt(const CloudRecord &rec, uint64_t &outMs) {
  // 1) 调用方明确给了绝对时间 → 直接用
  if (rec.startEpochMs > CLOUD_EPOCH_MS_MIN) {
    outMs = rec.startEpochMs;
    return true;
  }

  // 锁内取"实验开始时刻"的副本：本函数可能在主 loop（入队）里被调用，
  // 而 cloudUploadNoteRunningEdge() 也在主 loop，但为保持一致约定仍走锁，且打印放锁外。
  bool     hasStart = false;
  uint32_t startMs  = 0;
  uint64_t startEp  = 0;
  cloudLock();
  hasStart = s_expHasStart;
  startMs  = s_expStartMillis;
  startEp  = s_expStartEpochMs;
  cloudUnlock();

  uint64_t nowEp = nowEpochMs();

  // 2) 有"实验开始时刻"记录 → 用单调时钟回算实验开始那一刻的绝对时间
  //    （即使入队时才同步好，也能算出正确的开始时刻：now - 已经过的时长。
  //      elapsed 超过 1 天说明该记录已过期，不用它，避免报出严重偏早的时间）
  if (hasStart && nowEp != 0) {
    uint32_t elapsed = millis() - startMs;                  // millis() 环绕安全
    if (elapsed < 86400000UL) {
      outMs = nowEp - (uint64_t)elapsed;                    // nowEp ≥ 1.6e12 ≫ elapsed
      if (outMs > CLOUD_EPOCH_MS_MIN) {
        Serial.printf("[CLOUD] startedAt=实验开始时刻(回算): %llu ms (已过 %lu ms)\n",
                      (unsigned long long)outMs, (unsigned long)elapsed);
        return true;
      }
    } else {
      Serial.println("[CLOUD] ! 实验开始时刻记录已超过 1 天(疑似跨次上电残留)，本次按入队时刻处理");
    }
  }
  // 2b) 记录实验开始时刻时就已经同步 → 直接用它（此分支实际由上面的 elapsed 回算覆盖，
  //     这里兜住 elapsed 异常/回绕的边角情况）
  if (hasStart && startEp > CLOUD_EPOCH_MS_MIN) {
    outMs = startEp;
    return true;
  }

  // 3) 从未进入过运行态（如 DEMO 直接 STOP）或时间仍无效 → 回退"入队时刻"
  if (nowEp != 0) {
    outMs = nowEp;
    Serial.printf("[CLOUD] startedAt=入队时刻(回退，无实验开始记录): %llu ms\n",
                  (unsigned long long)outMs);
    return true;
  }

  // 4) 时间彻底无效：传 0，不发错误数字
  outMs = 0;
  Serial.println("[CLOUD] ! startedAt 无法确定（SNTP 未同步且无有效时间基准）→ 传 0");
  return false;
}

// SNTP 未同步时：重新发起同步 + 短等（有界），尽量让本条记录带上真实时间
// ⚠ 调用方必须处于"未持锁"状态（内部会调 cloudUploadSntpSync()，且这里要 delay 等待）
static void kickTimeSyncIfNeeded() {
  uint64_t ep = nowEpochMs();
  if (ep != 0) {
    cloudLock();
    bool first = !s_sntpSyncLogged;
    if (first) s_sntpSyncLogged = true;
    cloudUnlock();
    if (first) Serial.printf("[CLOUD] SNTP 同步成功：now=%llu ms\n", (unsigned long long)ep);
    return;
  }
  cloudUploadSntpSync();                                // 触发/重启同步（内部限流）
  uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < (uint32_t)CLOUD_TIME_SYNC_WAIT_MS) {
    delay(50);                                          // 仅主 loop 的入队路径会走到这里，最长 1.5s
    ep = nowEpochMs();
    if (ep != 0) {                                      // 等到了 → 正常值
      cloudLock();
      bool first = !s_sntpSyncLogged;
      if (first) s_sntpSyncLogged = true;
      cloudUnlock();
      if (first) {
        Serial.printf("[CLOUD] SNTP 同步成功(等待 %lu ms)：now=%llu ms\n",
                      (unsigned long)(millis() - t0), (unsigned long long)ep);
      }
      return;
    }
  }
  Serial.printf("[CLOUD] ! 等待 SNTP 同步超时(%d ms)：本次不改时间，startedAt 按规则回退/传 0\n",
                (int)CLOUD_TIME_SYNC_WAIT_MS);
}
