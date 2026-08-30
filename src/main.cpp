// ============================================================
//  YD-ESP32-S3 经典版 (N16R8) — 主控通信与手机端网关
//  角色:
//    STM32 --UART2--> ESP32-S3 --WebSocket--> 手机/网页
//  本文件提供:
//    1. WiFi: NVS 保存凭据, 优先连上次 WiFi; 连不上则开 AP + 门户配网(自动弹出)
//    2. mDNS: http://datatouch.local/ 免记 IP
//    3. WebSocket 服务器 (推实时曲线 + 收控制命令)
//    4. HTTP 服务器 (显示页 / 配网页 / 重新配网)
//    5. UART2 从 STM32 收帧 + 解析 (见 protocol.h)
//    6. DEMO_MODE: 无 STM32 时用内置模拟数据, 先看显示效果
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "protocol.h"

// ------------------- 用户配置 -------------------
#define CUSTOM_SSID   ""            // 开发用: 未配网也连它(仅 DEV_QUICK_WIFI=1 时生效); 产品请留空
#define CUSTOM_PASS   ""            // WiFi 密码
#define DEV_QUICK_WIFI 0            // 1=开发快捷连CUSTOM_SSID; 0=产品(未配网则进配网)
#define AP_SSID       "YD-ESP32S3"  // AP 热点名
#define AP_PASS       "12345678"    // AP 密码 (>=8 位)

#define STM32_RX_PIN  18            // 接 STM32 TX (ESP32-S3 UART2 默认 RX)
#define STM32_TX_PIN  17            // 接 STM32 RX
#define STM32_BAUD    115200

#define WEB_PORT      80            // HTTP 网页
#define WS_PORT       81            // WebSocket

#define DEMO_MODE     true          // true: 内置模拟数据; false: 接真实 STM32
#define PUSH_HZ       20            // 推送频率 (10~20 Hz)
// --------------------------------------------------

// ------------------- 全局对象 -------------------
WebServer       server(WEB_PORT);
WebSocketsServer webSocket(WS_PORT);
HardwareSerial  STM32(2);           // UART2

TouchFrame      g_frame = {0};
bool            g_frameNew = false;
volatile uint8_t g_state = STATE_IDLE;

// ---- WiFi 配网 / 门户 / mDNS 相关 ----
DNSServer    dnsServer;
Preferences  prefs;            // NVS: 保存 WiFi 凭据
const byte   DNS_PORT   = 53;
bool         g_provisioning = false;  // true = AP 配网模式(门户); false = STA 正常模式
IPAddress    g_bindIP;           // 当前绑定 IP (STA 或 AP)

// 前向声明 (pollUart 在下文才用到它们)
void dispatchFrame(uint8_t type, const uint8_t *pl, uint8_t len);
size_t frameLenByte();   // 读取缓冲第 4 字节(len), 用于判断是否收齐

// ------------------- 内嵌网页 -------------------
// 手机浏览器打开 http://<ip>/ 即可看到实时曲线
const char INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>织物触觉实时显示</title>
<style>
body{font-family:sans-serif;padding:12px;background:#111;color:#ddd}
canvas{background:#000;width:100%;height:58vh;border:1px solid #333;border-radius:6px}
.row{display:flex;gap:14px;flex-wrap:wrap;margin-bottom:8px}
.card{background:#1a1a1a;padding:8px 14px;border-radius:6px;min-width:88px}
.big{font-size:22px;font-weight:bold}
.hint{color:#888;font-size:12px}
button{background:#2a6;border:0;color:#fff;padding:8px 14px;border-radius:6px;margin:2px;font-size:14px}
</style>
</head>
<body>
<div style="text-align:right;margin-bottom:6px"><a href="/reset" style="color:#888;font-size:12px">重新配网</a></div>
<div class="row">
  <div class="card">状态 <span id="st" class="big">--</span></div>
  <div class="card">评分 <span id="sc" class="big">--</span></div>
</div>
<div class="row">
  <div class="card">Fx <span id="fx" class="big">0.00</span> N</div>
  <div class="card">Fy <span id="fy" class="big">0.00</span> N</div>
  <div class="card">Fz <span id="fz" class="big">0.00</span> N</div>
</div>
<div class="row">
  <div class="card">Mx <span id="mx" class="big">0.0</span> N·mm</div>
  <div class="card">My <span id="my" class="big">0.0</span> N·mm</div>
  <div class="card">Mz <span id="mz" class="big">0.0</span> N·mm</div>
</div>
<div class="row">
  <div class="card">X <span id="x" class="big">0.0</span> mm</div>
  <div class="card">Y <span id="y" class="big">0.0</span> mm</div>
  <div class="card">Z <span id="z" class="big">0.0</span> mm</div>
</div>
<div><button onclick="cmd('arm')">ARM</button>
       <button onclick="cmd('start')">START</button>
       <button onclick="cmd('stop')">STOP</button>
       <button onclick="cmd('reset')">RESET</button></div>
<p class="hint">曲线: 蓝=Fz(法向力) 红=Fx(切向力) 绿=Y(位移)</p>
<canvas id="cv"></canvas>
<script>
const MAX=240, fz=[], fx=[], posY=[];
const ws=new WebSocket("ws://"+location.hostname+":81");
ws.onopen =()=>{document.getElementById('st').textContent='已连接';};
ws.onclose=()=>{document.getElementById('st').textContent='断开';};
function cmd(c){ if(ws.readyState==1) ws.send(c); }
ws.onmessage=e=>{
  const p=e.data.split(',');
  if(p[0]==='DATA'){
    document.getElementById('fx').textContent=(+p[5]).toFixed(2);
    document.getElementById('fy').textContent=(+p[6]).toFixed(2);
    document.getElementById('fz').textContent=(+p[7]).toFixed(2);
    document.getElementById('mx').textContent=(+p[8]).toFixed(1);
    document.getElementById('my').textContent=(+p[9]).toFixed(1);
    document.getElementById('mz').textContent=(+p[10]).toFixed(1);
    document.getElementById('x').textContent=(+p[2]).toFixed(1);
    document.getElementById('y').textContent=(+p[3]).toFixed(1);
    document.getElementById('z').textContent=(+p[4]).toFixed(1);
    fz.push(+p[7]); fx.push(+p[5]); posY.push(+p[3]);
    if(fz.length>MAX){fz.shift();fx.shift();posY.shift();}
    draw();
  } else if(p[0]==='STATUS'){
    const m={0:'空闲',1:'已ARM',2:'采集中',3:'完成',4:'错误'};
    document.getElementById('st').textContent=m[p[1]]||p[1];
  } else if(p[0]==='SCORE'){
    document.getElementById('sc').textContent=(+p[1]).toFixed(0);
  }
};
function draw(){
  const c=document.getElementById('cv'), ctx=c.getContext('2d');
  const dpr=window.devicePixelRatio||1, w=c.clientWidth, h=c.clientHeight;
  c.width=w*dpr; c.height=h*dpr; ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  let max=1; fz.forEach(v=>{let a=Math.abs(v);if(a>max)max=a;});
  fx.forEach(v=>{let a=Math.abs(v);if(a>max)max=a;});
  posY.forEach(v=>{let a=Math.abs(v);if(a>max)max=a;});
  const n=fz.length, px=i=>i/(MAX-1)*w, py=v=>h/2-(v/max)*(h/2-8);
  ctx.strokeStyle='#3af';ctx.lineWidth=2;ctx.beginPath();
  for(let i=0;i<n;i++){i?ctx.lineTo(px(i),py(fz[i])):ctx.moveTo(px(i),py(fz[i]));}
  ctx.stroke();
  ctx.strokeStyle='#f55';ctx.beginPath();
  for(let i=0;i<n;i++){i?ctx.lineTo(px(i),py(fx[i])):ctx.moveTo(px(i),py(fx[i]));}
  ctx.stroke();
  ctx.strokeStyle='#3c3';ctx.beginPath();
  for(let i=0;i<n;i++){i?ctx.lineTo(px(i),py(posY[i])):ctx.moveTo(px(i),py(posY[i]));}
  ctx.stroke();
}
</script>
</body>
</html>)rawliteral";

// ------------------- 内嵌配网页 (门户) -------------------
// AP 配网模式下显示, 让用户输入家里 WiFi
const char CONFIG_HTML[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>设备配网</title>
<style>
body{font-family:sans-serif;padding:20px;background:#111;color:#ddd;max-width:420px;margin:0 auto}
h2{margin-top:8px}
label{display:block;margin-top:14px;font-size:14px;color:#bbb}
input{width:100%;padding:11px;margin-top:5px;box-sizing:border-box;font-size:16px;border-radius:6px;border:1px solid #444;background:#1a1a1a;color:#fff}
button{width:100%;margin-top:18px;padding:13px;background:#2a6;color:#fff;border:0;border-radius:8px;font-size:16px}
.ok{color:#3c3;margin-top:12px;font-size:14px}
.hint{color:#888;font-size:12px;margin-top:14px}
</style>
</head>
<body>
<h2>🔧 设备配网</h2>
<p class="hint">为让设备上网,请填写你家 WiFi 的名称和密码：</p>
<form method="POST" action="/config">
  <label>WiFi 名称 (SSID)
    <input name="ssid" id="ssid" required autocomplete="off" placeholder="例如 MyHome_WiFi">
  </label>
  <label>WiFi 密码
    <input name="pass" type="password" id="pass" autocomplete="off" placeholder="家里WiFi密码">
  </label>
  <button type="submit">保存并连接</button>
</form>
<div class="ok" id="msg"></div>
<script>
  // 浏览器出于安全无法读取手机当前 WiFi 名, 请手动输入
  document.getElementById('msg').textContent = '如果连接失败, 请检查密码后重试。';
</script>
</body>
</html>)rawliteral";

// ------------------- 工具函数 -------------------
static void pushState() {
  char buf[32];
  snprintf(buf, sizeof(buf), "STATUS,%u", (unsigned)g_state);
  webSocket.broadcastTXT(buf);
}

static void pushFrame() {
  char buf[160];
  snprintf(buf, sizeof(buf),
           "DATA,%.3f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%u",
           g_frame.t, g_frame.x, g_frame.y, g_frame.z,
           g_frame.Fx, g_frame.Fy, g_frame.Fz,
           g_frame.Mx, g_frame.My, g_frame.Mz,
           (unsigned)g_frame.contact);
  webSocket.broadcastTXT(buf);
}

static void handleCommand(const String &cmd) {
  uint8_t st = g_state;
  if      (cmd == "arm")   st = STATE_ARMED;
  else if (cmd == "start") st = STATE_RUNNING;
  else if (cmd == "stop")  st = STATE_DONE;
  else if (cmd == "reset") st = STATE_IDLE;
  else return;

  g_state = st;
  pushState();

  // 转发给 STM32 (FRAME_CMD), 真实模式下启用
  if (!DEMO_MODE) {
    uint8_t buf[8];
    size_t n = build_frame(buf, FRAME_CMD, &st, 1);
    STM32.write(buf, n);
  }
  Serial.printf("[CMD] -> %s (state=%u)\n", cmd.c_str(), st);
}

// ------------------- WebSocket 事件 -------------------
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] #%u connected\n", num);
      pushState();
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] #%u disconnected\n", num);
      break;
    case WStype_TEXT: {
      String s((const char*)payload, length);
      s.trim();
      handleCommand(s);
      break;
    }
    default:
      break;
  }
}

// ------------------- UART2 帧接收 (逐字节状态机) -------------------
enum { ST_SYNC0, ST_SYNC1, ST_TYPE, ST_LEN, ST_PAYLOAD, ST_CRC };
uint8_t  rxBuf[64];
size_t   rxIdx = 0;
uint8_t  rxState = ST_SYNC0;
uint8_t  rxType = 0, rxLen = 0;

size_t frameLenByte() { return rxLen; }   // 占位, 兼容旧调用

void pollUart() {
  while (STM32.available() > 0) {
    uint8_t b = (uint8_t)STM32.read();
    switch (rxState) {
      case ST_SYNC0:
        if (b == FRAME_SYNC0) { rxState = ST_SYNC1; rxBuf[rxIdx++] = b; }
        break;
      case ST_SYNC1:
        if (b == FRAME_SYNC1) { rxState = ST_TYPE; rxBuf[rxIdx++] = b; }
        else { rxState = ST_SYNC0; rxIdx = 0; }
        break;
      case ST_TYPE:
        rxType = b; rxBuf[rxIdx++] = b; rxState = ST_LEN; break;
      case ST_LEN:
        rxLen = b; rxBuf[rxIdx++] = b; rxState = ST_PAYLOAD; break;
      case ST_PAYLOAD:
        rxBuf[rxIdx++] = b;
        if (rxIdx >= (size_t)(4 + rxLen)) rxState = ST_CRC;
        break;
      case ST_CRC:
        rxBuf[rxIdx++] = b;
        if (rxIdx >= (size_t)(5 + rxLen)) {   // 4 头 + payload + 1 CRC
          uint8_t crc = rxBuf[rxIdx - 1];
          if (crc8_over(rxBuf, rxIdx - 1) == crc) {
            dispatchFrame(rxType, rxBuf + 4, rxLen);
          } else {
            Serial.println("[UART] bad crc");
          }
          rxState = ST_SYNC0; rxIdx = 0;
        }
        break;
      default:
        rxState = ST_SYNC0; rxIdx = 0;
    }
  }
}

// 分发帧
void dispatchFrame(uint8_t type, const uint8_t *pl, uint8_t len) {
  switch (type) {
    case FRAME_DATA: {
      if (len >= sizeof(TouchFrame)) {
        memcpy(&g_frame, pl, sizeof(TouchFrame));
        g_frameNew = true;
      }
      break;
    }
    case FRAME_STATUS:
      if (len >= 1) g_state = pl[0];
      break;
    case FRAME_METRICS:
      // 预留: 分项指标, 后续再定义结构
      break;
    case FRAME_SCORE:
      if (len >= 4) {
        float sc; memcpy(&sc, pl, 4);
        char buf[32]; snprintf(buf, sizeof(buf), "SCORE,%.1f", sc);
        webSocket.broadcastTXT(buf);
      }
      break;
    default:
      break;
  }
}

// ------------------- 仿真数据 (DEMO_MODE) -------------------
// 模拟一次"按压->保压->滑动->抬起"
void demoStep() {
  static float t = 0, z = 0, fz = 0, x = 0;
  const float dt = 1.0f / PUSH_HZ;
  t += dt;

  float tFz = 0, tX = 0;
  if      (t < 1.0f) { tFz = 2.0f * (t / 1.0f); }
  else if (t < 3.0f) { tFz = 2.0f; }
  else if (t < 6.0f) { tFz = 2.0f; tX = (t - 3.0f) / 3.0f * 30.0f; }
  else if (t < 6.4f) { tFz = 2.0f * (1 - (t - 6.0f) / 0.4f); }
  else { t = 0; x = 0; z = 0; fz = 0; return; }

  fz += (tFz - fz) * 0.2f;         // 一阶低通
  x  += (tX  - x ) * 0.3f;
  z  = fz * 0.8f;

  float fx = 0.35f * fz + 0.05f * sinf(2 * PI * 8 * t);
  float fy = 0.02f * sinf(2 * PI * 3 * t);

  g_frame.t = t; g_frame.x = x; g_frame.y = 0.0f; g_frame.z = z;
  g_frame.Fx = fx; g_frame.Fy = fy; g_frame.Fz = fz;
  g_frame.Mx = 0.0f; g_frame.My = 0.0f; g_frame.Mz = 0.0f; // 力矩默认 0
  g_frame.contact = (fz > 0.1f) ? 1 : 0;
  g_frameNew = true;
}

// ------------------- WiFi 配置处理 -------------------
void handleConfig() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() == 0) {
    server.send(200, "text/html", "<meta charset=utf-8><h3>WiFi 名不能为空，请返回重试</h3>");
    return;
  }
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putBool("force", false);   // 清除强制配网标志, 下次开机正常连网
  prefs.end();
  Serial.printf("[CONFIG] 已保存 WiFi: %s, 正在重启连接...\n", ssid.c_str());
  // 保存成功后: 设备会连上该 WiFi 并自动关闭热点, 所以这里无法跨网络自动跳转, 用提示
  server.send(200, "text/html",
              "<!DOCTYPE html><html><head><meta charset=utf-8>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<style>body{font-family:sans-serif;background:#111;color:#ddd;padding:30px;text-align:center}"
              "h3{font-size:30px;color:#3c3;margin:6px 0 14px}"
              "p{font-size:22px;line-height:1.8;margin:6px 0}"
              ".big{font-size:36px;color:#fff;font-weight:bold;word-break:break-all;margin:20px 0}"
              ".sub{color:#999;font-size:18px}</style></head><body>"
              "<h3>✅ 已保存，正在连接…</h3>"
              "<p>设备已连接你的 WiFi，<b>热点已关闭</b>。</p>"
              "<p>请让手机连接<b>同一个 WiFi</b>，然后打开：</p>"
              "<div class='big'>http://datatouch.local/</div>"
              "<p class='sub'>若打不开，请用设备串口显示的 IP 访问。</p>"
              "</body></html>");
  delay(800);
  ESP.restart();
}

void handleReset() {
  prefs.begin("wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.putBool("force", true);   // 强制下次进入配网模式
  prefs.end();
  Serial.println("[RESET] 已清除 WiFi, 将返回配网模式");
  server.send(200, "text/html",
              "<!DOCTYPE html><html><head><meta charset=utf-8>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<style>body{font-family:sans-serif;background:#111;color:#ddd;padding:30px;text-align:center}"
              "h3{font-size:30px;color:#f80;margin:6px 0 16px}"
              "p{font-size:24px;line-height:1.8;margin:8px 0}"
              ".b{color:#fff;font-weight:bold}"
              ".sub{color:#999;font-size:18px}</style></head><body>"
              "<h3>已清除，正在返回配网模式...</h3>"
              "<p>请到手机 <b>WiFi 设置</b>中连接热点<br>"
              "<span class='b' style='font-size:32px'>YD-ESP32S3</span>&nbsp;&nbsp;密码 <span class='b'>12345678</span></p>"
              "<p class='sub'>连接成功后，配网页会自动弹出。</p>"
              "</body></html>");
  delay(800);
  ESP.restart();
}

// ------------------- setup -------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== YD-ESP32-S3 网关启动 ===");
  Serial.printf("芯片: %s | 频率: %u MHz\n", ESP.getChipModel(), ESP.getCpuFreqMHz());

  STM32.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  // ---- 读取 NVS: 保存的 WiFi 与 "强制配网" 标志 (/reset 会置位) ----
  prefs.begin("wifi", false);
  bool forceProv = prefs.getBool("force", false);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  bool staOk = false;
  if (!forceProv) {
    // 先以 STA 模式连已保存的 WiFi (能连上就不开热点)
    WiFi.mode(WIFI_STA);
    if (savedSsid.length() > 0) {
      WiFi.begin(savedSsid.c_str(), savedPass.c_str());
      Serial.printf("尝试连接 WiFi: %s", savedSsid.c_str());
      unsigned t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
        delay(300); Serial.print(".");
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) staOk = true;
    }
#if DEV_QUICK_WIFI
    // 开发用: 未连上时尝试 CUSTOM_SSID (产品请保持 0)
    if (!staOk && strlen(CUSTOM_SSID) > 0) {
      WiFi.begin(CUSTOM_SSID, CUSTOM_PASS);
      Serial.printf("尝试连接(开发): %s", CUSTOM_SSID);
      unsigned t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
        delay(300); Serial.print(".");
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) staOk = true;
    }
#endif
  } else {
    Serial.println("检测到强制配网标志, 进入配网模式");
  }

  IPAddress ip;
  if (staOk) {
    // ---- 已连上: 仅 STA, 关闭热点. 显示页 + mDNS ----
    g_provisioning = false;
    ip = WiFi.localIP();
    g_bindIP = ip;
    Serial.print("本机 IP (STA): ");
    Serial.println(ip);
    Serial.println("已连接 WiFi, 热点已关闭");
    if (MDNS.begin("datatouch")) {
      Serial.println("mDNS: http://datatouch.local/");
    } else {
      Serial.println("警告: mDNS 启动失败");
    }
  } else {
    // ---- 未连上: 开热点 + 门户配网 ----
    g_provisioning = true;
    WiFi.mode(WIFI_AP);
    bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("AP 已开: %s (SSID=%s 密码=%s)\n",
                  apOk ? "OK" : "失败", AP_SSID, AP_PASS);
    ip = WiFi.softAPIP();
    unsigned apT0 = millis();
    while (ip == IPAddress(0, 0, 0, 0) && millis() - apT0 < 3000) {
      delay(100); ip = WiFi.softAPIP();
    }
    g_bindIP = ip;
    Serial.print("配网地址(AP): http://");
    Serial.println(ip);
    // 门户 DNS: 所有域名解析到本机, 手机连上热点后自动弹出配网页
    dnsServer.start(DNS_PORT, "*", ip);
  }

  // ---- WebSocket ----
  webSocket.begin();
  webSocket.onEvent(onWsEvent);

  // ---- HTTP 网页 ----
  // / 按模式返回: AP->配网页, STA->显示页
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", g_provisioning ? CONFIG_HTML : INDEX_HTML);
  });
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/reset", HTTP_GET, handleReset);
  // 门户探测(苹果/安卓 captive portal 检测URL)重定向到本机首页 -> 自动弹出
  server.onNotFound([]() {
    String url = String("http://") + g_bindIP.toString() + "/";
    server.sendHeader("Location", url, true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.printf("配网/显示地址: http://%s/  |  WebSocket: ws://%s:%d\n",
                ip.toString().c_str(),
                ip.toString().c_str(), WS_PORT);
}

// ------------------- loop -------------------
void loop() {
  if (g_provisioning) dnsServer.processNextRequest(); // 门户 DNS
  webSocket.loop();
  server.handleClient();

  if (DEMO_MODE) {
    demoStep();
  } else {
    pollUart();
  }

  static uint32_t lastPush = 0;
  uint32_t now = millis();
  uint32_t period = 1000UL / PUSH_HZ;
  if (g_frameNew && (now - lastPush) >= period) {
    lastPush = now;
    pushFrame();
    g_frameNew = false;
  }
}
