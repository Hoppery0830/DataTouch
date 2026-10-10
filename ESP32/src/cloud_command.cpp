// ============================================================
//  cloud_command.cpp — 云端远程命令（HTTPS 轮询 → ESP32 → STM32）实现
//  ------------------------------------------------------------
//  · 轮询：每 CLOUD_CMD_POLL_MS(默认 2s) 一次 action=poll 的 HTTPS POST；
//          失败按 2s→4s→8s→16s→30s 退避（成功清零），离线只提示一次不刷屏。
//  · 应用：白名单 arm/start/stop/reset（+ 运动模式 mode_press/mode_rub/mode_slide）→ 投进
//          FreeRTOS 队列，由**主 loop** 执行（复用 main.cpp 既有的 handleCommand 等价逻辑：
//          改 g_state/选模式 + pushState/pushActionMode 广播 + 非 DEMO 时给 STM32 发
//          FRAME_CMD/FRAME_ACTION；模式命令**不改 g_state**）。
//  · 回执：主 loop 执行完把结果投回执队列，云任务发 action=ack（result=ok/unknown/expired）。
//  · 安全：只认 4 个白名单命令；同一 id 只执行一次（环形去重表）；
//          本地 60s 过期兜底（仅当响应带 ts/createdAt 且本地时钟已同步时判断）；
//          CLOUD_CMD_ENABLE=0 时连 poll 都不发。
//  · 并发：见 cloud_command.h 顶部说明。临界区只碰 RAM（微秒级），
//          **绝不持锁做网络**；配置走 cloudUploadGetConfig() 的既有互斥量取副本。
// ============================================================
#include "cloud_command.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <stdlib.h>              // strtoull
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "cloud_upload.h"        // 复用：配置快照 / 是否联网 / 时间是否同步

// ============================================================
//  内部状态（文件作用域）
// ============================================================
namespace {

// ---- 线程通道 ----
SemaphoreHandle_t s_mtx  = nullptr;   // 只保护 s_enabled（可能被非云任务线程读写）
QueueHandle_t     s_cmdQ = nullptr;   // 云任务 → 主 loop：待执行命令
QueueHandle_t     s_ackQ = nullptr;   // 主 loop → 云任务：待发回执

// ---- 运行期开关 ----
bool s_enabled = (CLOUD_CMD_ENABLE != 0);

// ---- 轮询状态机（只被 cloudTask 读写）----
uint32_t s_nextPollMs    = 0;         // 下一次允许轮询的时刻（millis）
uint32_t s_failCount     = 0;         // 连续失败次数（决定退避档位）
bool     s_offlineLogged = false;     // 离线提示只打一次，避免刷屏

// ---- 去重表：最近处理过的命令 id（环形；只被 cloudTask 读写）----
char    s_recent[CLOUD_CMD_RECENT_LEN][CLOUD_CMD_ID_LEN] = {};
uint8_t s_recentHead = 0;
char s_recentResult[CLOUD_CMD_RECENT_LEN][CLOUD_CMD_RESULT_LEN] = {};
remote_status_t s_motorStatus{RC_IDLE,255,0,0,0};
bool s_motorAlive=false;
uint32_t s_motorUpdated=0;

// ---- 统计（只被 cloudTask 写；便于现场自检，有活动时每 60s 打一行）----
uint32_t s_cntApplied  = 0, s_cntUnknown = 0, s_cntDup = 0, s_cntExpired = 0;
uint32_t s_cntAckOk    = 0, s_cntAckFail = 0;
uint32_t s_cntPollOk   = 0, s_cntPollFail = 0;
uint32_t s_statLogMs   = 0;
bool     s_statDirty   = false;

// 回执消息（POD，供 FreeRTOS 队列按值拷贝）
// fromLoop = 这条回执是"主 loop 执行完"投回来的（用于统计"真正执行的条数"；
//            云任务自己投的 unknown/expired/重复 回执为 0）
struct AckMsg {
  char    id[CLOUD_CMD_ID_LEN];
  char    result[CLOUD_CMD_RESULT_LEN];
  uint8_t fromLoop;
};

// 解析出来的一条原始命令（应用前还可能被过期/去重拦掉）
struct RawCmd {
  char     id[CLOUD_CMD_ID_LEN];
  char     cmd[CLOUD_CMD_NAME_LEN];
  bool     hasTs;     // 响应里是否带时间戳
  uint64_t ts;        // 原始时间戳（可能是秒或毫秒，判断时再归一化）
};

// ============================================================
//  小工具（与 cloud_upload.cpp 同风格；static/匿名命名空间 → 不产生符号冲突）
// ============================================================

// millis() 环绕安全的"到点判断"
static inline bool timeReached(uint32_t now, uint32_t due) {
  return (int32_t)(now - due) >= 0;
}

// 截断拷贝（保证 NUL 结尾；源可能是云端字符串）
static void copyTrunc(char *dst, size_t n, const String &src) {
  if (n == 0) return;
  size_t len = src.length();
  if (len > n - 1) len = n - 1;
  if (len > 0) memcpy(dst, src.c_str(), len);
  dst[len] = '\0';
}

// JSON 字符串转义（token 里可能有引号/反斜杠；与上传模块保持同样的保守写法）
static String jsonEsc(const char *s) {
  String o;
  if (!s) return o;
  o.reserve(strlen(s) + 8);
  for (const char *p = s; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20)         { o += ' '; }
    else                       { o += (char)c; }
  }
  return o;
}

// 截断长文本（响应体打日志用）
static String brief(const String &s, size_t maxLen = 160) {
  String t = s;
  t.replace('\n', ' ');
  t.replace('\r', ' ');
  if (t.length() > maxLen) t = t.substring(0, maxLen) + "...";
  return t;
}

// ============================================================
//  命令 URL 推导
//  ------------------------------------------------------------
//  规则：把"上传 URL"里的 deviceUpload 换成 deviceCommand。
//    ① 严格后缀：.../deviceUpload → .../deviceCommand（主路径）；
//    ② 兜底：整串里最后一次出现 deviceUpload 的位置替换（兼容带 query 的地址）；
//    ③ 都匹配不到 → 用编译期兜底宏 CLOUD_COMMAND_URL。
//  上传 URL 可在设备网页 /cloud 改（NVS），所以命令 URL 会**跟着变**，无需新增配置项。
//  [不必持锁] 只读传进来的副本（调用方用 cloudUploadGetConfig() 取快照）
// ============================================================
static String deriveCommandUrl(const String &uploadUrl) {
  String u = uploadUrl;
  u.trim();
  if (u.length() == 0) return String(CLOUD_COMMAND_URL);
  while (u.endsWith("/")) u.remove(u.length() - 1);            // 去掉末尾多余的 '/'

  const char *kUp  = "deviceUpload";
  const char *kCmd = "deviceCommand";

  // ① 严格后缀匹配（推荐形态：https://host/deviceUpload）
  String suffix = String("/") + kUp;
  if (u.endsWith(suffix)) {
    u.remove(u.length() - suffix.length());
    return u + "/" + kCmd;
  }
  // ② 兜底：地址里任意位置出现 deviceUpload（例如带 ?a=1 的地址）
  int i = u.lastIndexOf(kUp);
  if (i >= 0) {
    return u.substring(0, i) + kCmd +
           u.substring(i + (int)strlen(kUp));                  // 越界时 substring 返回空串
  }
  // ③ 完全匹配不到 → 编译期兜底宏
  return String(CLOUD_COMMAND_URL);
}

// ============================================================
//  极简 JSON（只为组包/取值，不引入 JSON 库）
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

// 判断 "key":false（用于识别 {"ok":false,...}；只在 key 后面紧跟 ':' 时才算命中）
static bool jsonBoolIsFalse(const String &s, const char *key) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat);
  while (i >= 0) {
    int j = i + (int)pat.length();
    while (j < (int)s.length() && (s[j] == ' ' || s[j] == '\t' ||
                                   s[j] == '\n' || s[j] == '\r')) j++;
    if (j < (int)s.length() && s[j] == ':') {
      j++;
      while (j < (int)s.length() && (s[j] == ' ' || s[j] == '\t' ||
                                     s[j] == '\n' || s[j] == '\r')) j++;
      return s.startsWith("false", (unsigned int)j);
    }
    i = s.indexOf(pat, i + 1);
  }
  return false;
}

// 找 "key" 后面的值的起始位置（跳过空白）
static bool jsonFindKey(const String &s, const char *key, int from, int &valPos) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat, from);
  if (i < 0) return false;
  i = s.indexOf(':', i + (int)pat.length());
  if (i < 0) return false;
  i++;
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t' ||
                                 s[i] == '\n' || s[i] == '\r')) i++;
  valPos = i;
  return true;
}

// 取 pos 处的标量：带引号的字符串（做简单反转义）或裸数字/true/false
static String jsonValueAt(const String &s, int pos) {
  if (pos < 0 || pos >= (int)s.length()) return String("");
  if (s[pos] == '"') {
    String o;
    o.reserve(24);
    for (int i = pos + 1; i < (int)s.length(); i++) {
      char c = s[i];
      if (c == '\\' && i + 1 < (int)s.length()) { o += s[i + 1]; i++; continue; }
      if (c == '"') break;
      o += c;
    }
    return o;
  }
  int i = pos;
  while (i < (int)s.length()) {
    char c = s[i];
    if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' ||
        c == '\n' || c == '\r') break;
    i++;
  }
  return s.substring(pos, i);
}

// 取 "key" 的标量值（在 [from, end) 范围内找第一个）
static String jsonScalar(const String &s, const char *key, int from = 0) {
  int p = 0;
  if (!jsonFindKey(s, key, from, p)) return String("");
  return jsonValueAt(s, p);
}

// ============================================================
//  命令解析
//  ------------------------------------------------------------
//  兼容两种响应形态（`commands` 这个 key 只要能找到、后面紧跟数组即可）：
//    ① {"ok":true,"commands":[{"id":"...","cmd":"..."}],"serverTime":...}
//    ② {"code":0,"data":{"commands":[...]}}
//  id 支持字符串（Mongo _id）与裸数字两种写法；额外认 ts / createdAt 作为可选时间戳。
// ============================================================
static int parseCommands(const String &resp, RawCmd *out, int maxN) {
  int kp = 0;
  if (!jsonFindKey(resp, "commands", 0, kp)) return 0;
  if (kp >= (int)resp.length() || resp[kp] != '[') return 0;   // "commands":null / 不是数组
  int end = resp.indexOf(']', (unsigned int)kp);
  if (end < 0) end = (int)resp.length();                       // 容错：截断的响应也只解析已到达部分

  int n   = 0;
  int pos = kp + 1;
  while (n < maxN && pos < end) {
    int ob = resp.indexOf('{', (unsigned int)pos);
    if (ob < 0 || ob >= end) break;
    int oe = resp.indexOf('}', (unsigned int)ob);
    if (oe < 0) break;
    String obj = resp.substring(ob, oe + 1);
    pos = oe + 1;

    String id  = jsonScalar(obj, "id");
    String cmd = jsonScalar(obj, "cmd");
    if (id.length() == 0 && cmd.length() == 0) continue;       // 空对象/异常元素，跳过

    copyTrunc(out[n].id,  sizeof(out[n].id),  id);
    copyTrunc(out[n].cmd, sizeof(out[n].cmd), cmd);
    out[n].hasTs = false;
    out[n].ts    = 0;
    String ts = jsonScalar(obj, "ts");
    if (ts.length() == 0) ts = jsonScalar(obj, "createdAt");
    if (ts.length() > 0) {
      uint64_t v = strtoull(ts.c_str(), nullptr, 10);
      if (v > 0) { out[n].hasTs = true; out[n].ts = v; }
    }
    n++;
  }
  return n;
}

// ============================================================
//  白名单 / 去重 / 过期
// ============================================================

// 命令白名单：只认这 7 个（与网页按钮一致；安全命令，不含任何"标定/擦除"类操作）
//   · 状态类：arm / start / stop / reset      → 主 loop 改 g_state + 发 FRAME_CMD
//   · 模式类：mode_press / mode_rub / mode_slide → 主 loop 改 g_actionMode + 发 FRAME_ACTION
//              （模式号映射：mode_press→0(PRESS)、mode_rub→1(RUB)、mode_slide→2(SLIDE)，
//                映射在 main.cpp 的 handleCommand() 里做；**模式命令不改 g_state**）
static const char *const kWhitelist[] = {
  "arm", "start", "stop", "reset",
  "mode_press", "mode_rub", "mode_slide"
};

bool cloudCommandIsWhitelisted(const char *cmd) {
  if (!cmd || !*cmd) return false;
  for (size_t i = 0; i < sizeof(kWhitelist) / sizeof(kWhitelist[0]); i++) {
    if (strcmp(cmd, kWhitelist[i]) == 0) return true;
  }
  return false;
}

// 是否最近处理过（同一 id 只执行一次；云端重投时只补 ack）
static bool recentContains(const char *id) {
  if (!id || !*id) return false;
  for (int i = 0; i < CLOUD_CMD_RECENT_LEN; i++) {
    if (s_recent[i][0] && strcmp(s_recent[i], id) == 0) return true;
  }
  return false;
}

static void recentAdd(const char *id) {
  if (!id || !*id) return;
  copyTrunc(s_recent[s_recentHead], sizeof(s_recent[s_recentHead]), String(id));
  s_recentResult[s_recentHead][0] = 0;
  s_recentHead = (uint8_t)((s_recentHead + 1) % CLOUD_CMD_RECENT_LEN);
}

// 本地过期兜底：仅当响应带时间戳且本地 SNTP 已同步时才判断，判不准就不判（避免误杀）。
// 兼容 10 位(秒) / 13 位(毫秒) 两种写法；疑似"开机毫秒"等不合理值直接不判。
static bool cmdIsExpired(const RawCmd &c) {
  if (!c.hasTs) return false;
  if (!cloudUploadTimeSynced()) return false;           // 本地时间不可信 → 交给云端 60s 与去重兜底
  uint64_t ts = c.ts;
  if (ts >= 100000000000ULL) {                          // 13 位 → epoch 毫秒
    // 原样使用
  } else if (ts >= 1000000000ULL) {                     // 10 位 → epoch 秒
    ts *= 1000ULL;
  } else {
    return false;                                       // 既不是秒也不是毫秒 → 不判
  }
  uint64_t now = cloudUploadNowMs();
  if (now == 0) return false;
  if (ts > now + 5000ULL) return false;                 // 未来时间（时钟漂移）→ 不当过期
  return (now - ts) > (uint64_t)CLOUD_CMD_MAX_AGE_MS;
}

// ============================================================
//  HTTPS POST（与上传模块同结构，但用更短的超时；每次新建连接，不复用）
//  [不持锁] 调用方保证：传进来的是配置副本，且此时没有任何锁
// ============================================================
struct CmdHttpOut {
  bool     ok;        // 业务成功（2xx 且 code==0 且 ok!=false）
  int      code;      // HTTPClient 返回码（<0 = 连接/超时类错误）
  uint32_t costMs;
  String   resp;      // 响应体（截断后仅用于日志/解析）
  String   err;       // 失败原因
};

static CmdHttpOut cmdHttpPost(const String &url, const String &body) {
  CmdHttpOut o;
  o.ok = false; o.code = 0; o.costMs = 0; o.resp = ""; o.err = "";

  if (url.length() == 0) { o.err = "URL 为空"; return o; }

  WiFiClientSecure client;
  // ⚠ 与上传同一个 MVP 取舍：setInsecure() 不校验服务器证书。正式版应改为
  //   client.setCACert(ROOT_CA_PEM); 或 setFingerprint(...) 做指纹校验。
  client.setInsecure();
  client.setHandshakeTimeout(CLOUD_CMD_TLS_HANDSHAKE_TIMEOUT_S);   // 单位: 秒

  HTTPClient http;
  http.setConnectTimeout(CLOUD_CMD_CONNECT_TIMEOUT_MS);            // 单位: 毫秒
  http.setTimeout(CLOUD_CMD_RESPONSE_TIMEOUT_MS);                  // 单位: 毫秒
  http.setReuse(false);

  if (!http.begin(client, url)) { o.err = "http.begin 失败(URL 无效?)"; return o; }
  http.addHeader("Content-Type", "application/json");

  uint32_t t0 = millis();
  int code = http.POST(body);                                      // 有界阻塞：≤ 连接超时 + 响应超时
  o.costMs = millis() - t0;
  if (code > 0) o.resp = http.getString();
  http.end();
  o.code = code;

  if (code <= 0) { o.err = String("网络或超时(HTTPClient=") + code + ")"; return o; }
  if (code < 200 || code >= 300) { o.err = String("HTTP ") + code; return o; }
  long biz = 0;
  if (parseJsonInt(o.resp, "code", biz) && biz != 0) {
    o.err = String("业务失败 code=") + biz;
    return o;
  }
  if (jsonBoolIsFalse(o.resp, "ok")) { o.err = "业务失败 ok=false"; return o; }
  o.ok = true;
  return o;
}

// ============================================================
//  组包
// ============================================================
static String buildPollBody(const CloudConfig &cfg) {
  remote_status_t status;bool alive;uint32_t updated;
  xSemaphoreTake(s_mtx,portMAX_DELAY);
  status=s_motorStatus;alive=s_motorAlive;updated=s_motorUpdated;
  xSemaphoreGive(s_mtx);
  alive=alive && millis()-updated<2000;
  String b="{\"action\":\"poll\",\"token\":\"";
  b+=jsonEsc(cfg.token.c_str());b+="\",\"deviceId\":\"";b+=jsonEsc(cfg.deviceId.c_str());
  char snapshot[180];
  snprintf(snapshot,sizeof(snapshot),"\",\"status\":{\"alive\":%s,\"state\":%u,\"mode\":%u,\"stage\":%u,\"online\":%u,\"fault\":%u}}",alive?"true":"false",status.state,status.mode,status.stage,status.online,status.fault);
  b+=snapshot;return b;
}

static String buildAckBody(const CloudConfig &cfg, const char *id, const char *result) {
  String b;
  b.reserve(220);
  b += "{\"action\":\"ack\",\"token\":\"";
  b += jsonEsc(cfg.token.c_str());
  b += "\",\"deviceId\":\"";
  b += jsonEsc(cfg.deviceId.c_str());
  b += "\",\"id\":\"";
  b += jsonEsc(id ? id : "");
  b += "\",\"result\":\"";
  b += jsonEsc(result ? result : "ok");
  b += "\"}";
  return b;
}

// 轮询失败退避：2s→4s→8s→16s→30s(封顶)
static uint32_t pollBackoffFor(uint32_t fails) {
  if (fails == 0) return 0;
  uint32_t wait = (uint32_t)CLOUD_CMD_POLL_MS;
  for (uint32_t i = 1; i < fails && wait < (uint32_t)CLOUD_CMD_MAX_BACKOFF_MS; i++) {
    wait *= 2;
    if (wait > (uint32_t)CLOUD_CMD_MAX_BACKOFF_MS) break;
  }
  return wait > (uint32_t)CLOUD_CMD_MAX_BACKOFF_MS ? (uint32_t)CLOUD_CMD_MAX_BACKOFF_MS : wait;
}

// ============================================================
//  回执队列：主 loop 投递（另一个线程）→ 云任务取出后 HTTP POST
// ============================================================

// [线程安全] 可从任意线程调用：只做 xQueueSend（非阻塞）
// fromLoop=true 表示这条回执来自主 loop 的命令执行路径（cloudCommandAckResult）
static void ackQueuePush(const char *id, const char *result, bool fromLoop) {
  if (!id || !*id) {                       // 没有 id 就没法回执，只提示
    Serial.println("[CMD] ! ack 缺少 id，已跳过（不影响本地命令执行）");
    return;
  }
  if (!s_ackQ) { Serial.println("[CMD] ! 回执队列未初始化，ack 丢失"); return; }

  AckMsg m = {};
  copyTrunc(m.id, sizeof(m.id), String(id));
  copyTrunc(m.result, sizeof(m.result), String(result && *result ? result : "ok"));
  m.fromLoop = fromLoop ? 1 : 0;

  // 非阻塞投递：队列满则丢最旧一条（保证网页/云端看到的是最新回执，不阻塞主 loop）
  if (xQueueSend(s_ackQ, &m, 0) != pdTRUE) {
    AckMsg old;
    xQueueReceive(s_ackQ, &old, 0);
    xQueueSend(s_ackQ, &m, 0);
  }
}

// [云任务] 取出至多 CLOUD_CMD_ACK_MAX_PER_LOOP 条回执，逐条 POST 给云端
static void drainAckQueue() {
  static AckMsg retry{};static bool waiting=false;static uint32_t retryAt=0;
  if(!s_ackQ || (waiting && !timeReached(millis(),retryAt)))return;
  for(int budget=CLOUD_CMD_ACK_MAX_PER_LOOP;budget>0;--budget){
    AckMsg m{};
    if(waiting){m=retry;waiting=false;}
    else if(xQueueReceive(s_ackQ,&m,0)!=pdTRUE)return;
    for(unsigned i=0;i<CLOUD_CMD_RECENT_LEN;i++)if(strcmp(s_recent[i],m.id)==0)
      strlcpy(s_recentResult[i],m.result,sizeof(s_recentResult[i]));
    CloudConfig cfg=cloudUploadGetConfig();
    CmdHttpOut res=cmdHttpPost(deriveCommandUrl(cfg.url),buildAckBody(cfg,m.id,m.result));
    if(!res.ok){retry=m;waiting=true;retryAt=millis()+2000;s_cntAckFail++;return;}
    s_cntAckOk++;s_statDirty=true;
    Serial.printf("[CMD] STM32 result delivered: id=%s result=%s\n",m.id,m.result);
  }
}

// ============================================================
//  单条命令处理（云任务内）
// ============================================================
static void handleOneCommand(const RawCmd &c) {
  // ① 白名单：不在名单里的一律忽略（并回 unknown，让云端把该条标成失败原因可见）
  if (!cloudCommandIsWhitelisted(c.cmd)) {
    s_cntUnknown++;
    s_statDirty = true;
    Serial.printf("[CMD] 忽略未知命令: '%s' (id=%s) → ack unknown\n", c.cmd, c.id);
    ackQueuePush(c.id, "unknown", false);
    return;
  }
  // ② 过期兜底（云端已保证 60s；这里只在能确定时才拦）
  if (cmdIsExpired(c)) {
    s_cntExpired++;
    s_statDirty = true;
    Serial.printf("[CMD] 忽略过期命令: %s (id=%s ts=%llu) → ack expired\n",
                  c.cmd, c.id, (unsigned long long)c.ts);
    ackQueuePush(c.id, "expired", false);
    return;
  }
  // ③ 去重：同一 id 只执行一次（云端重投/ack 丢失后重投 → 只补 ack，不重复动作）
  if (recentContains(c.id)) {
    s_cntDup++;
    s_statDirty = true;
    Serial.printf("[CMD] 忽略重复命令: %s (id=%s 已处理过) → 补 ack ok\n", c.cmd, c.id);
    for(unsigned i=0;i<CLOUD_CMD_RECENT_LEN;i++)
      if(strcmp(s_recent[i],c.id)==0 && s_recentResult[i][0])ackQueuePush(c.id,s_recentResult[i],false);
    return;
  }

  // ④ 投给主 loop 执行：g_state / WebSocket 广播 / UART2 写都只能在主 loop 线程做
  CloudCmdMsg m = {};
  copyTrunc(m.id,  sizeof(m.id),  String(c.id));
  copyTrunc(m.cmd, sizeof(m.cmd), String(c.cmd));
  if (!s_cmdQ || xQueueSend(s_cmdQ, &m, 0) != pdTRUE) {
    // 理论极端（队列满）：**不记入去重表、不回 ack** → 云端重投时仍会执行，不会静默丢命令
    Serial.printf("[CMD] ! 命令队列满/未初始化，暂不执行 %s (id=%s)，等云端重投\n", c.cmd, c.id);
    return;
  }
  recentAdd(c.id);          // 只有真的排队成功才记去重（保证"未执行的不会因去重被吃掉"）
  s_statDirty = true;
  Serial.printf("[CMD] 远程命令入队: %s (id=%s) → 等主 loop 执行\n", c.cmd, c.id);
}

// ============================================================
//  一次轮询（云任务内）
// ============================================================
static void doPollOnce() {
  // ---- 锁内取配置副本（cloudUploadGetConfig 内部自带互斥量）→ 锁外做网络 ----
  CloudConfig cfg = cloudUploadGetConfig();
  String url  = deriveCommandUrl(cfg.url);
  String body = buildPollBody(cfg);

  CmdHttpOut r = cmdHttpPost(url, body);                 // 锁外有界阻塞（4s/6s/4s）
  if (!r.ok) {
    s_cntPollFail++;
    s_statDirty = true;
    s_failCount++;
    uint32_t waitMs = pollBackoffFor(s_failCount);
    s_nextPollMs = millis() + waitMs;
    // 不刷屏：第 1 次失败 + 之后每 10 次失败才打印
    if (s_failCount == 1 || (s_failCount % 10) == 0) {
      Serial.printf("[CMD] ! 轮询失败(%s HTTP %d, %lums)，%lus 后重试\n",
                    r.err.c_str(), r.code, (unsigned long)r.costMs,
                    (unsigned long)(waitMs / 1000));
      if (r.resp.length() > 0) {
        Serial.printf("[CMD] 云端响应: %s\n", brief(r.resp).c_str());
      }
    }
    return;
  }

  s_cntPollOk++;
  s_statDirty = true;
  if (s_failCount > 0) {                                 // 从失败恢复：打一行，便于现场判断
    Serial.printf("[CMD] 轮询已恢复(HTTP %d, %lums)\n", r.code, (unsigned long)r.costMs);
    s_failCount = 0;
  }

  // ---- 解析并逐条处理（应用动作只投队列，真正执行在主 loop）----
  RawCmd cmds[CLOUD_CMD_MAX_PER_POLL] = {};
  int n = parseCommands(r.resp, cmds, CLOUD_CMD_MAX_PER_POLL);
  for (int i = 0; i < n; i++) handleOneCommand(cmds[i]);
}

// [云任务] 有活动时每 60s 打一行统计（现场不看云端也能确认模块在跑）
static void statTick() {
  uint32_t now = millis();
  if (s_statLogMs == 0) { s_statLogMs = now; return; }
  if ((uint32_t)(now - s_statLogMs) < 60000UL) return;
  s_statLogMs = now;
  if (!s_statDirty) return;
  s_statDirty = false;
  Serial.printf("[CMD] 统计: poll成功 %lu/失败 %lu | 执行 %lu 未知 %lu 重复 %lu 过期 %lu | ack成功 %lu/失败 %lu\n",
                (unsigned long)s_cntPollOk, (unsigned long)s_cntPollFail,
                (unsigned long)s_cntApplied, (unsigned long)s_cntUnknown,
                (unsigned long)s_cntDup, (unsigned long)s_cntExpired,
                (unsigned long)s_cntAckOk, (unsigned long)s_cntAckFail);
}

}  // namespace

// ============================================================
//  对外接口实现
// ============================================================

void cloudCommandInit() {
  // ---- 队列/锁先建好（必须在 cloudTask 启动之前、主 loop 第一次 Take 之前）----
  if (!s_mtx)  s_mtx  = xSemaphoreCreateMutex();
  if (!s_cmdQ) s_cmdQ = xQueueCreate(CLOUD_CMD_QUEUE_LEN, sizeof(CloudCmdMsg));
  if (!s_ackQ) s_ackQ = xQueueCreate(CLOUD_CMD_ACKQ_LEN, sizeof(AckMsg));

  // ---- 打印开关状态与推导出的命令 URL（免看代码就能确认链路）----
  CloudConfig cfg = cloudUploadGetConfig();
  String url = deriveCommandUrl(cfg.url);
  Serial.println("[CMD] ===== 远程命令(云→ESP32→STM32) 初始化 =====");
  Serial.printf("[CMD] 开关: %s (编译期 CLOUD_CMD_ENABLE=%d)\n",
                s_enabled ? "开启：按白名单执行远程命令"
                          : "关闭：连 poll 都不发(远程控制面完全切断)",
                (int)(CLOUD_CMD_ENABLE ? 1 : 0));
  Serial.printf("[CMD] 上传URL : %s\n", cfg.url.c_str());
  Serial.printf("[CMD] 命令URL : %s  ← %s\n", url.c_str(),
                (url == String(CLOUD_COMMAND_URL))
                    ? "编译期宏兜底(上传URL里没匹配到 deviceUpload)"
                    : "由上传URL推导(改 /cloud 的上传地址会跟着变)");
  Serial.printf("[CMD] 轮询间隔: %dms | 超时: 连接%dms/TLS%ds/响应%dms | 白名单: arm,start,stop,reset,mode_press,mode_rub,mode_slide\n",
                (int)CLOUD_CMD_POLL_MS, (int)CLOUD_CMD_CONNECT_TIMEOUT_MS,
                (int)CLOUD_CMD_TLS_HANDSHAKE_TIMEOUT_S, (int)CLOUD_CMD_RESPONSE_TIMEOUT_MS);
  Serial.printf("[CMD] 队列: 命令 %d 条 / 回执 %d 条 | 去重表 %d 条 | 本地过期兜底 %dms\n",
                (int)CLOUD_CMD_QUEUE_LEN, (int)CLOUD_CMD_ACKQ_LEN,
                (int)CLOUD_CMD_RECENT_LEN, (int)CLOUD_CMD_MAX_AGE_MS);
  if (!s_cmdQ || !s_ackQ) {
    Serial.println("[CMD] ! 队列创建失败(内存不足)：远程命令将不可用，既有功能不受影响");
  }
  if (cfg.token == "REPLACE_ME") {
    Serial.println("[CMD] ! token 仍是占位值 REPLACE_ME：云函数会拒绝，先用 /cloud 页写入真实 token");
  }
  if (!s_enabled) {
    Serial.println("[CMD] 提示: 如需开启远程控制，把 platformio.ini 的 -DCLOUD_CMD_ENABLE 设为 1（默认即 1）");
  }
}

void cloudCommandLoop() {
  statTick();                                    // 有活动时每 60s 一行统计（非阻塞，纯串口）

  // ---- ① 没联网：跳过（退避），联网后立刻恢复；回执留在队列里，等联网再补发 ----
  //      这样断网期间不会白白等 4s 连接超时，也不会把还没发出去的回执丢掉。
  if (!cloudUploadIsOnline()) {
    if (!s_offlineLogged) {
      s_offlineLogged = true;
      Serial.println("[CMD] 未联网(或配网中)：暂停远程命令轮询，联网后自动恢复");
    }
    s_nextPollMs = millis();                     // 联网恢复后立刻轮询，不背退避
    s_failCount  = 0;
    return;
  }
  if (s_offlineLogged) {
    s_offlineLogged = false;
    Serial.println("[CMD] 网络已恢复：继续轮询远程命令");
    s_nextPollMs = millis();
  }

  // ---- ② 先发回执：主 loop 已执行的命令，尽快 ack 回云端（云端据此把该命令标 done）----
  //      开关关闭时也照发：关的是"远程控制"，不是"已执行命令的结果回传"。
  drainAckQueue();

  // ---- ③ 开关关闭：连 poll 都不发（设备侧彻底没有远程控制面）----
  if (!cloudCommandEnabled()) return;

  // ---- ④ 到点才 poll（固定间隔 CLOUD_CMD_POLL_MS + 失败退避）----
  if (!timeReached(millis(), s_nextPollMs)) return;
  s_nextPollMs = millis() + CLOUD_CMD_POLL_MS;   // 先排下一次；失败时会被退避覆盖
  doPollOnce();                                  // 锁外有界阻塞（连接 4s / TLS 6s / 响应 4s）
}

bool cloudCommandTake(CloudCmdMsg &out) {
  if (!s_cmdQ) return false;
  return xQueueReceive(s_cmdQ, &out, 0) == pdTRUE;   // 非阻塞：主 loop 绝不被这里卡住
}

void cloudCommandAckResult(const char *id, const char *result) {
  // ⚠ 本函数在主 loop 里被调用：内部只把消息投进回执队列（xQueueSend 非阻塞、线程安全），
  //   不碰模块内部任何计数器/状态 → 模块状态仍然只被 cloudTask 一条任务读写。
  ackQueuePush(id, result, true);
}

bool cloudCommandEnabled() {
  if (!s_mtx) return s_enabled;                // 极早期(Init 之前)按无锁读处理
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  bool en = s_enabled;
  xSemaphoreGive(s_mtx);
  return en;
}

void cloudCommandSetEnabled(bool en) {
  if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
  bool changed = (s_enabled != en);
  s_enabled = en;
  if (s_mtx) xSemaphoreGive(s_mtx);
  if (changed) {
    Serial.printf("[CMD] 远程控制已%s\n", en ? "开启" : "关闭(不再轮询, 已排队的命令仍会执行)");
  }
}

String cloudCommandUrlText() {
  CloudConfig cfg = cloudUploadGetConfig();     // 内部自带互斥量，锁内取副本
  return deriveCommandUrl(cfg.url);
}

void cloudCommandSetStatus(const remote_status_t &status,bool alive){
 if(!s_mtx)return;
 xSemaphoreTake(s_mtx,portMAX_DELAY);
 s_motorStatus=status;s_motorAlive=alive;s_motorUpdated=millis();
 xSemaphoreGive(s_mtx);
}
