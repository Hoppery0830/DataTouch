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
#include <freertos/FreeRTOS.h>   // 并发重构: 任务/队列
#include <freertos/task.h>
#include <freertos/queue.h>
#include "protocol.h"
#include "cloud_upload.h"   // 设备上云: HTTPS 上传实验记录 (新增模块; 不改动现有逻辑)
#include "remote_link.h"
#include "cloud_command.h"  // 云端远程命令: 轮询 deviceCommand → 主 loop 执行 → ack (新增模块)

// ------------------- 用户配置 -------------------
#define CUSTOM_SSID   ""            // 开发用: 未配网也连它(仅 DEV_QUICK_WIFI=1 时生效); 产品请留空
#define CUSTOM_PASS   ""            // WiFi 密码
#define DEV_QUICK_WIFI 0            // 1=开发快捷连CUSTOM_SSID; 0=产品(未配网则进配网)
#define AP_SSID       "YD-ESP32S3"  // AP 热点名
#define AP_PASS       "12345678"    // AP 密码 (>=8 位)

#define STM32_RX_PIN  18            // 接 STM32 TX (ESP32-S3 UART2 默认 RX)
#define STM32_TX_PIN  17            // 接 STM32 RX
#define STM32_BAUD    115200
// UART2 接收缓冲: 默认仅 256 字节, 460800 波特率下约 5.5ms 就能灌满 →
// 主循环偶发卡顿(如网页请求/配置写入)就会丢帧。加大到 4096 字节提高抗突发能力。
// ⚠ 必须在 STM32.begin() 之前调用 setRxBufferSize()。
#define STM32_RX_BUF_SIZE 4096

#define WEB_PORT      80            // HTTP 网页
#define WS_PORT       81            // WebSocket

#define DEMO_MODE     false          // true: 内置模拟数据; false: 接真实 STM32
#define PUSH_HZ       20            // 推送频率 (10~20 Hz)

// ------------------- 设备上云并发配置 (FreeRTOS) -------------------
// 背景: 云端 HTTPS 是同步阻塞的(连接 6s / TLS 8s / 响应 6s)。重构前它在 loop() 里跑,
//       会把 loop 卡住 → UART 丢帧、WebSocket 推送停顿。
// 现在: "云"跑在独立任务 cloudTask 上, pin 到 core 0; loop() 默认跑在 core 1
//       (ARDUINO_RUNNING_CORE=1)，两者互不阻塞。
#define CLOUD_TASK_CORE       0     // 固定 core 0 (WiFi/lwIP 也在这核, 但优先级高于本任务)
#define CLOUD_TASK_STACK      10240 // 栈 10KB(≥8KB): HTTPS/TLS(mbedTLS) 握手比较吃栈
#define CLOUD_TASK_PRIO       1     // 适中优先级: 高于 idle(0), 远低于 WiFi(≈23)/lwIP(≈18)
#define CLOUD_TASK_PERIOD_MS  30    // 每轮 vTaskDelay 让出 CPU (20~50ms), 不忙等
// 云任务 → 主 loop 的"网页广播队列"(WebSocketsServer 只能单线程访问)
#define CLOUD_WS_QUEUE_LEN    16    // 队列深度(条)
#define CLOUD_WS_MSG_LEN      192   // 单条状态文本上限(cloud_upload 里已截断到 ~130)
#define CLOUD_WS_DRAIN_MAX    8     // 每轮 loop 最多广播几条, 避免一次 push 太多拖慢 loop
// --------------------------------------------------

// ------------------- 云端远程命令配置 (FreeRTOS) -------------------
// 轮询 deviceCommand 的 HTTPS 也在 core 0 的 cloudTask 里做(与上传**同一任务**,
// 两个调用串行执行 → 同一时刻只有一路 TLS, 栈/内存与既有上传完全一致)。
// 命令的"执行"(改 g_state / 广播 WebSocket / 给 STM32 发 FRAME_CMD)只在主 loop 做,
// 两者之间用 FreeRTOS 队列通信:
//   · 云任务 → 主 loop: 待执行命令队列 (cloudCommandTake)
//   · 主 loop → 云任务: 待发回执队列 (cloudCommandAckResult)
// 这样 WebSocketsServer / UART2 仍然只被 loopTask 一个线程访问(与 V2 约定一致)。
#define CLOUD_CMD_DRAIN_MAX   4     // 主 loop 每轮最多执行几条远程命令(防止一次挤占 loop)
// --------------------------------------------------

// ------------------- 全局对象 -------------------
WebServer       server(WEB_PORT);
WebSocketsServer webSocket(WS_PORT);
HardwareSerial  STM32(2);           // UART2

// 说明: 用值初始化 "= {}" 代替旧的 "= {0}" —— 语义完全相同(静态存储期, 全部成员清零),
//       但不触发 -Wextra 的 -Wmissing-field-initializers (部分成员显式初始化告警)。
TouchFrame      g_frame = {};
bool            g_frameNew = false;
MetricsFrame    g_metrics = {};   // 最新四维特征 (粗量纲)
bool            g_metricsNew = false;
float           g_score = 0.0f;    // 最新综合评分 (0~100, STM32 端给出)
bool            g_scoreNew = false;
volatile uint8_t g_state = STATE_IDLE;

// ---- 电机运动模式选择 (见 protocol.h 的 ActionMode / FRAME_ACTION) ----
// 语义: 只"选择/预置"模式, **不改变 g_state、不启动动作**;
//       真实模式下随命令下发一帧 FRAME_ACTION(0x06) 给 STM32(载荷 = 模式号)。
// 线程约定: 只在主 loop 读写(与 g_state 完全一致) —— 云任务只投命令队列,
//           真正的赋值/广播/UART 写都在 handleCommand() 里(主 loop 执行)。
volatile uint8_t g_actionMode = DEMO_MODE ? ACTION_PRESS : 255;   // 当前选中的模式: 0=PRESS 1=RUB 2=SLIDE
const char      *g_actionModeName = DEMO_MODE ? "PRESS" : "UNKNOWN";    // 当前模式文本(供网页广播/串口日志)

// 模式号 → 文本 (越界一律回落 PRESS; 只做查表, 无副作用)
static const char *actionModeName(int m) {
  switch (m) {
    case ACTION_RUB:   return "RUB";
    case ACTION_SLIDE: return "SLIDE";
    case ACTION_PRESS: return "PRESS";
    default:           return "UNKNOWN";
  }
}

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
body{font-family:sans-serif;padding:12px;background:#111;color:#ddd;max-width:1000px;margin:0 auto}
canvas{background:#000;border:1px solid #333;border-radius:6px}
#cv{width:100%;height:46vh}
.row{display:flex;gap:14px;flex-wrap:wrap;margin-bottom:8px;align-items:flex-start}
.card{background:#1a1a1a;padding:8px 14px;border-radius:6px;min-width:96px;box-sizing:border-box}
.big{font-size:22px;font-weight:bold;font-variant-numeric:tabular-nums;display:inline-block;min-width:4ch;text-align:right}
.md{font-size:16px;font-weight:bold;font-variant-numeric:tabular-nums;display:inline-block;min-width:4ch;text-align:right}
.hint{color:#888;font-size:12px}
button{background:#2a6;border:0;color:#fff;padding:8px 14px;border-radius:6px;margin:2px;font-size:14px}
.modebtn{background:#333;border:1px solid #444}
.modebtn.act{background:#2a6;border-color:#2a6}
.radarwrap{text-align:center;margin-top:8px}
#radar{width:300px;height:300px;display:inline-block}
</style>
</head>
<body>
<div style="text-align:right;margin-bottom:6px"><a href="/cloud" style="color:#2a6;font-size:12px;margin-right:10px">上云配置</a><a href="/reset" style="color:#888;font-size:12px">重新配网</a></div>
<div class="row">
  <div class="card">状态 <span id="st" class="big">--</span></div>
  <div class="card">综合评分 <span id="composite" class="big">--</span></div>
  <div class="card">设备评分 <span id="sc" class="md">--</span></div>
  <div class="card">上云 <span id="cloud" class="md">--</span></div>
</div>
<div class="row">
  <div class="card">柔软度 <span id="d_softness" class="big">--</span></div>
  <div class="card">顺滑度 <span id="d_smoothness" class="big">--</span></div>
  <div class="card">细腻度 <span id="d_roughness" class="big">--</span></div>
  <div class="card">回弹 <span id="d_rebound" class="big">--</span></div>
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
<div class="row">
  <div class="card">运动模式 <span id="mode" class="md">--</span></div>
  <div>
    <button id="m0" class="modebtn" onclick="cmd('mode_press')">按压</button>
    <button id="m1" class="modebtn" onclick="cmd('mode_rub')">揉搓</button>
    <button id="m2" class="modebtn" onclick="cmd('mode_slide')">滑动</button>
  </div>
</div>
<div><button onclick="cmd('arm')">ARM</button>
       <button onclick="cmd('start')">START</button>
       <button onclick="cmd('stop')">STOP</button>
       <button onclick="cmd('reset')">RESET</button>
       <button disabled style="background:#777">电机联调：评分上传关闭</button></div>
<p class="hint" id="upHint">电机联调仅回传控制状态，尚未接入采集与评分，不上传实验记录。</p>
<p class="hint">运动模式：点「按压/揉搓/滑动」即通过 WebSocket 发 mode_press/mode_rub/mode_slide（与 ARM/START/STOP 同一条路径）；设备端非 DEMO 时会下发 FRAME_ACTION(0x06) 给 STM32，DEMO 下只切换显示</p>
<p class="hint">曲线: 蓝=Fz(法向力) 红=Fx(切向力) 绿=Y(位移) ｜ 细腻度: 值越小越细腻, 评分自动取反 ｜ 综合评分由前端按权重融合四维特征得出</p>
<canvas id="cv"></canvas>
<div class="radarwrap"><canvas id="radar"></canvas></div>
<script>
// ===== 权重与归一化分段 (在线调评分只改这里即可) =====
const WEIGHTS={softness:0.30,smoothness:0.25,roughness:0.20,rebound:0.25};
const RANGE={
  softness:[0.0,1.0],    // 原始粗量纲 -> 0~100 分段; 越大越软
  smoothness:[0.0,1.0],  // 越大越顺滑
  roughness:[0.0,1.0],   // 越小越细腻, 归一化时取反
  rebound:[0.0,1.0]      // 越大回弹越好
};

const MAX=240, fz=[], fx=[], posY=[];
let composite=0;   // 前端加权融合出的"综合评分"
const ws=new WebSocket("ws://"+location.hostname+":81");
ws.onopen =()=>{document.getElementById('st').textContent='已连接';};
ws.onclose=()=>{document.getElementById('st').textContent='断开';};
function cmd(c){ if(ws.readyState==1) ws.send(c); }
// 运动模式: 0=PRESS(按压) 1=RUB(揉搓) 2=SLIDE(滑动)
// 点击按钮 -> WS 发 mode_press/mode_rub/mode_slide; 设备收到后广播 ACTION,<mode>,<name> 再高亮
const MODENAMES={0:'PRESS',1:'RUB',2:'SLIDE'};
function setMode(m){
  document.getElementById('mode').textContent=MODENAMES[m]||'未确认';
  [0,1,2].forEach(i=>{const b=document.getElementById('m'+i); if(b) b.classList.toggle('act', i===m);});
}
// 设备上云: 手动上传测试 —— 设备端收到 'upload' 后立刻用当前四维/评分 POST 一条测试记录
function upTest(){
  cmd('upload');
  const el=document.getElementById('cloud');
  el.textContent='已请求'; el.style.color='#ddd';
}

function clamp01(v){ return v<0?0:(v>1?1:v); }
// 把某维原始特征映射到 0~100
function normalize(dim,v){
  const seg=RANGE[dim]||[0,1];
  let n=clamp01((v-seg[0])/(seg[1]-seg[0]));
  if(dim==='roughness') n=1-n;   // 越小越细腻 -> 取反
  return n*100;
}
// 按权重融合四维 -> 综合评分 (返回各维 0~100 得分, 并写入全局 composite)
function computeScore(m){
  const dims=['softness','smoothness','roughness','rebound'];
  const s={}; let sum=0, wsum=0;
  dims.forEach(d=>{ const sc=normalize(d,m[d]); s[d]=sc; sum+=sc*WEIGHTS[d]; wsum+=WEIGHTS[d]; });
  composite = wsum>0 ? sum/wsum : 0;
  return s;
}
// 五边形雷达图: 柔软度/顺滑度/细腻度/回弹 + 综合(第5边)
function drawRadar(s){
  const c=document.getElementById('radar'), ctx=c.getContext('2d');
  const dpr=window.devicePixelRatio||1, w=c.clientWidth, h=c.clientHeight;
  c.width=w*dpr; c.height=h*dpr; ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  const cx=w/2, cy=h/2, R=Math.min(w,h)/2-22, maxV=100;
  const labels=['柔软度','顺滑度','细腻度','回弹','综合'];
  const vals=[s.softness,s.smoothness,s.roughness,s.rebound,composite];
  const n=vals.length, ang=i=>(-Math.PI/2 + i*2*Math.PI/n);
  // 网格环
  for(let ring=4;ring>=1;ring--){
    ctx.strokeStyle='rgba(255,255,255,0.12)'; ctx.lineWidth=1; ctx.beginPath();
    for(let i=0;i<=n;i++){ const a=ang(i%n), rr=R*ring/4;
      const x=cx+rr*Math.cos(a), y=cy+rr*Math.sin(a);
      i?ctx.lineTo(x,y):ctx.moveTo(x,y); }
    ctx.stroke();
  }
  // 轴线
  for(let i=0;i<n;i++){ const a=ang(i);
    ctx.strokeStyle='rgba(255,255,255,0.2)'; ctx.beginPath();
    ctx.moveTo(cx,cy); ctx.lineTo(cx+R*Math.cos(a), cy+R*Math.sin(a)); ctx.stroke(); }
  // 数据多边形
  ctx.strokeStyle='#4af'; ctx.fillStyle='rgba(68,170,255,0.25)'; ctx.lineWidth=2; ctx.beginPath();
  for(let i=0;i<=n;i++){ const a=ang(i%n), rr=R*vals[i%n]/maxV;
    const x=cx+rr*Math.cos(a), y=cy+rr*Math.sin(a);
    i?ctx.lineTo(x,y):ctx.moveTo(x,y); }
  ctx.closePath(); ctx.stroke(); ctx.fill();
  // 维度标签
  ctx.fillStyle='#ddd'; ctx.font='12px sans-serif'; ctx.textAlign='center'; ctx.textBaseline='middle';
  for(let i=0;i<n;i++){ const a=ang(i), x=cx+(R+16)*Math.cos(a), y=cy+(R+16)*Math.sin(a);
    ctx.fillText(labels[i], x, y); }
}

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
    const m={0:'空闲',1:'准备完成',2:'运动中',3:'完成',4:'故障/离线',5:'准备电机中',6:'停止中'};
    document.getElementById('st').textContent=m[p[1]]||p[1];
  } else if(p[0]==='SCORE'){
    // 设备端给出的 0~100 综合评分 (仅作参考显示)
    document.getElementById('sc').textContent=(+p[1]).toFixed(0);
  } else if(p[0]==='METRICS'){
    // METRICS,softness,smoothness,roughness,rebound,flags
    const m={softness:+p[1],smoothness:+p[2],roughness:+p[3],rebound:+p[4]};
    const s=computeScore(m);
    document.getElementById('d_softness').textContent=s.softness.toFixed(0);
    document.getElementById('d_smoothness').textContent=s.smoothness.toFixed(0);
    document.getElementById('d_roughness').textContent=s.roughness.toFixed(0);
    document.getElementById('d_rebound').textContent=s.rebound.toFixed(0);
    document.getElementById('composite').textContent=composite.toFixed(0);
    drawRadar(s);
  } else if(p[0]==='ACTION'){
    // 运动模式: ACTION,<mode>,<name>  (设备收到 mode_* 命令后广播 → 高亮当前模式)
    setMode(+p[1]);
  } else if(p[0]==='CLOUD'){
    // 设备上云状态: CLOUD,<事件>,<说明>  事件 = CFG/ENQ/SEND/OK/FAIL/OFF/DROP
    const cm={CFG:'已配置',ENQ:'已入队',SEND:'上传中',OK:'✓ 成功',FAIL:'✗ 失败',OFF:'离线等待',DROP:'队列满'};
    let d=p[2]||''; if(d.length>44) d=d.slice(0,44)+'…';
    const el=document.getElementById('cloud');
    el.textContent=(cm[p[1]]||p[1])+(d?(' '+d):'');
    el.style.color = (p[1]==='OK') ? '#3c3' : (p[1]==='FAIL' ? '#f88' : '#ddd');
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

// ------------------- 内嵌"上云配置"页 -------------------
// GET /cloud 返回: 免重新烧录即可改 token / fabricName / deviceId (写 NVS, 立即生效)
// 风格与 CONFIG_HTML 保持一致: 深色卡片 + 大字号 + 移动端友好
// %TOKEN% / %TOKENMASK% / %FABRIC% / %DEVICE% 由 handleCloudPage() 替换后下发
const char CLOUD_CFG_HTML[] = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>上云配置</title>
<style>
body{font-family:sans-serif;padding:20px;background:#111;color:#ddd;max-width:420px;margin:0 auto}
h2{margin-top:8px}
label{display:block;margin-top:14px;font-size:14px;color:#bbb}
input{width:100%;padding:11px;margin-top:5px;box-sizing:border-box;font-size:16px;border-radius:6px;border:1px solid #444;background:#1a1a1a;color:#fff}
button{width:100%;margin-top:18px;padding:13px;background:#2a6;color:#fff;border:0;border-radius:8px;font-size:16px}
a{color:#2a6}
.hint{color:#888;font-size:12px;margin-top:6px}
.top{text-align:right;margin-bottom:4px}
</style>
</head>
<body>
<div class="top"><a href="/">← 返回主页</a></div>
<h2>☁ 上云配置</h2>
<p class="hint">改完点保存即写入设备 NVS，立即生效，无需重新烧录、无需重启。</p>
<form method="POST" action="/cloud/config">
  <label>Token（云端校验密钥）
    <input name="token" type="password" value="%TOKEN%" autocomplete="off" placeholder="留空则保持原值">
  </label>
  <p class="hint">当前 token：%TOKENMASK%</p>
  <label>布料名称 fabricName
    <input name="fabricName" value="%FABRIC%" autocomplete="off" placeholder="例如 莫代尔-180g">
  </label>
  <label>设备 ID deviceId
    <input name="deviceId" value="%DEVICE%" autocomplete="off" placeholder="例如 esp32-dev01">
  </label>
  <p class="hint">上传地址(固定, 编译期配置)：<span id="u"></span></p>
  <p class="hint">远程命令地址(由上传地址推导 deviceUpload→deviceCommand)：<br>%CMDURL%<br>远程控制：%CMDSTATE%</p>
  <button type="submit">保存</button>
</form>
</body>
</html>)rawliteral";

// token 脱敏: 只显示前 4 位
static String maskTokenForHtml(const String &t) {
  if (t.length() == 0) return "(未设置)";
  if (t.length() <= 4) return "****";
  return t.substring(0, 4) + "****";
}

// HTML 属性值转义 (配置里可能出现引号/尖括号)
static String htmlEsc(const String &s) {
  String o = s;
  o.replace("&", "&amp;");
  o.replace("\"", "&quot;");
  o.replace("<", "&lt;");
  o.replace(">", "&gt;");
  return o;
}

// GET /cloud —— 下发配置页(表单回显当前值)
void handleCloudPage() {
  CloudConfig c = cloudUploadGetConfig();      // 读回当前配置
  String html;
  html.reserve(strlen(CLOUD_CFG_HTML) + 256);  // 预留, 避免多次重分配
  html = CLOUD_CFG_HTML;
  html.replace("%TOKEN%",      htmlEsc(c.token));
  html.replace("%TOKENMASK%",  maskTokenForHtml(c.token));
  html.replace("%FABRIC%",     htmlEsc(c.fabricName));
  html.replace("%DEVICE%",     htmlEsc(c.deviceId));
  html.replace("<span id=\"u\"></span>", "<span id=\"u\">" + htmlEsc(c.url) + "</span>");
  // 云端远程命令: 只读展示"推导出的命令 URL + 远程控制开关状态"(便于现场核对链路是否一致)
  html.replace("%CMDURL%",   htmlEsc(cloudCommandUrlText()));
  html.replace("%CMDSTATE%", cloudCommandEnabled()
                                 ? String("已开启(白名单 arm/start/stop/reset)")
                                 : String("已关闭(编译期 CLOUD_CMD_ENABLE=0)"));
  server.send(200, "text/html", html);
}

// POST /cloud/config —— 解析表单 → 写 NVS → 返回"已保存"提示页(新值回显, token 脱敏)
void handleCloudConfigSave() {
  CloudConfig cur = cloudUploadGetConfig();    // 保留原有 url / deviceId / fabricName
  String token  = server.arg("token");
  String fabric = server.arg("fabricName");
  String devid  = server.arg("deviceId");
  token.trim();
  fabric.trim();
  devid.trim();

  // token 密码框: 留空表示"不修改"(浏览器不回显密码时的常见做法)
  if (token.length() == 0) token = cur.token;
  if (fabric.length() == 0) fabric = cur.fabricName;
  if (devid.length() == 0)  devid  = cur.deviceId;

  cloudUploadSetConfig(cur.url, token, devid, fabric);   // 写 NVS + 立即生效

  CloudConfig now = cloudUploadGetConfig();              // 再读回, 保证回显=真实生效值
  String html;
  html.reserve(1100);                                    // 预留, 避免多次重分配
  html = String("<!DOCTYPE html><html><head><meta charset='utf-8'>") +
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;background:#111;color:#ddd;padding:30px;text-align:center}"
    "h3{font-size:30px;color:#3c3;margin:6px 0 14px}p{font-size:22px;line-height:1.8;margin:6px 0}"
    ".b{color:#fff;font-weight:bold;font-size:20px;word-break:break-all}"
    ".sub{color:#999;font-size:18px}a{color:#2a6;font-size:22px}</style></head><body>"
    "<h3>✅ 上云配置已保存</h3>"
    "<p>布料名称<br><span class='b'>" + htmlEsc(now.fabricName) + "</span></p>"
    "<p>设备 ID<br><span class='b'>" + htmlEsc(now.deviceId) + "</span></p>"
    "<p>Token<br><span class='b'>" + maskTokenForHtml(now.token) + "</span></p>"
    "<p class='sub'>已写入 NVS，无需重启；下一次上传即使用新配置。<br>"
    "当前电机联调不上传评分，可在远程控制页查看设备状态。</p>"
    "<p><a href='/'>返回主页</a> ｜ <a href='/cloud'>继续修改</a></p>"
    "</body></html>";
  server.send(200, "text/html", html);
}

// ------------------- 工具函数 -------------------
// 把浮点限幅到 [0,1], 供 DEMO 模拟特征/评分使用
static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

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

// 推送四维特征 (CSV): METRICS,softness,smoothness,roughness,rebound,flags
static void pushMetrics() {
  char buf[80];
  snprintf(buf, sizeof(buf), "METRICS,%.3f,%.3f,%.3f,%.3f,%u",
           g_metrics.softness, g_metrics.smoothness,
           g_metrics.roughness, g_metrics.rebound,
           (unsigned)g_metrics.flags);
  webSocket.broadcastTXT(buf);
}

// 推送综合评分 (CSV): SCORE,<0-100>
static void pushScore() {
  char buf[32];
  snprintf(buf, sizeof(buf), "SCORE,%.1f", g_score);
  webSocket.broadcastTXT(buf);
}

// 推送当前运动模式 (CSV): ACTION,<mode>,<name>
// 网页据此高亮"按压/揉搓/滑动"分段按钮; 与 pushState 一样只在主 loop 被调用。
static void pushActionMode() {
  char buf[32];
  snprintf(buf, sizeof(buf), "ACTION,%u,%s", (unsigned)g_actionMode, g_actionModeName);
  webSocket.broadcastTXT(buf);
}

// STM32 state is authoritative in real mode; cloud snapshot uses a short mutex.
void remoteWireSend(const uint8_t *data,size_t len) { STM32.write(data,len); }
void remoteApplyStatus(const remote_status_t &s,bool alive) {
  uint8_t state=alive?s.state:(uint8_t)STATE_ERROR;
  bool changed=g_state!=state;
  g_state=state;
  if(changed)pushState();
  uint8_t mode=alive?s.mode:255;
  if(g_actionMode!=mode){g_actionMode=mode;g_actionModeName=actionModeName(mode);pushActionMode();}
  cloudCommandSetStatus(s,alive);
}

// 处理一条控制命令 (网页按钮/小程序 WebSocket 直连, 以及云端远程命令)
//   fromCloud=false(默认): 既有路径 —— WebSocket 文本命令, 行为与改动前**完全一致**
//   fromCloud=true       : 来自云端远程命令(cloudTask 轮询 → 队列 → 主 loop)。
//                          这里仍在主 loop 线程执行, 因此 pushState()/STM32.write() 安全;
//                          执行完把回执投回"回执队列"(由云任务 POST action=ack)。
//   reqId: 云端命令 id(仅回执用), 可为 nullptr
// ⚠ 参数带默认值 → 既有调用点 handleCommand(s) 无需改动。
static void handleCommand(const String &cmd, bool fromCloud = false, const char *reqId = nullptr) {
  // 设备上云: 网页「上传测试」按钮 -> 用当前 g_metrics/g_score 立刻传一条测试记录
  // (不影响状态机, 不改变 g_state, 也不转发给 STM32)
  if (cmd == "upload") { if (DEMO_MODE) cloudUploadTestNow(); return; }

  // Real control is acknowledged by STM32, never by optimistic local state changes.
  if (!DEMO_MODE) { remoteLinkSubmit(cmd, fromCloud ? reqId : nullptr); return; }

  // ---- 运动模式选择: mode_press / mode_rub / mode_slide (云端远程 与 网页按钮 同一条路径) ----
  // 只"选模式", **不改变 g_state**(与 arm/start/stop/reset 互不影响), 也不启动动作。
  // 非 DEMO 时给 STM32 发一帧 FRAME_ACTION(0x06, 载荷 = 1 字节模式号), 写法与下面发 FRAME_CMD 一致;
  // DEMO 模式下只更新/显示模式 + 打日志, **不发串口**(与 FRAME_CMD 的 DEMO 行为一致)。
  int mode = -1;                                  // -1 = 不是模式命令
  if      (cmd == "mode_press") mode = ACTION_PRESS;   // 0 = 按压
  else if (cmd == "mode_rub")   mode = ACTION_RUB;     // 1 = 揉搓
  else if (cmd == "mode_slide") mode = ACTION_SLIDE;   // 2 = 滑动
  if (mode >= 0) {
    g_actionMode     = (uint8_t)mode;
    g_actionModeName = actionModeName(mode);
    pushActionMode();                             // 广播给网页(WebSocket 只在主 loop 访问)
    if (!DEMO_MODE) {
      uint8_t payload = (uint8_t)mode;
      uint8_t buf[8];
      size_t n = build_frame(buf, FRAME_ACTION, &payload, 1);
      STM32.write(buf, n);
      Serial.printf("[UART] 已下发 FRAME_ACTION mode=%u\n", (unsigned)payload);
    }
    Serial.printf("[CMD] 运动模式: %s(%d)%s\n",
                  g_actionModeName, mode, fromCloud ? " [云端远程]" : "");
    if (fromCloud) cloudCommandAckResult(reqId, "ok");
    return;
  }

  uint8_t st = g_state;
  bool known = true;
  if      (cmd == "arm")   st = STATE_ARMED;
  else if (cmd == "start") st = STATE_RUNNING;
  else if (cmd == "stop")  st = STATE_DONE;
  else if (cmd == "reset") st = STATE_IDLE;
  else known = false;

  if (!known) {
    // 未知命令: 网页路径保持原有的"直接忽略"行为; 云端路径额外回一个 unknown 回执
    // (白名单校验在 cloud_command 里已做一次, 这里是第二道防线)
    if (fromCloud) {
      Serial.printf("[CMD] 远程命令: %s (id=%s) → 不在白名单, 已忽略\n",
                    cmd.c_str(), reqId ? reqId : "?");
      cloudCommandAckResult(reqId, "unknown");
    }
    return;
  }
  if (fromCloud) {
    Serial.printf("[CMD] 远程命令: %s (id=%s)\n", cmd.c_str(), reqId ? reqId : "?");
  }

  g_state = st;
  pushState();

  // 转发给 STM32 (FRAME_CMD), 真实模式下启用
  if (!DEMO_MODE) {
    uint8_t buf[8];
    size_t n = build_frame(buf, FRAME_CMD, &st, 1);
    STM32.write(buf, n);
  }
  Serial.printf("[CMD] -> %s (state=%u)%s\n", cmd.c_str(), st, fromCloud ? " [云端远程]" : "");
  if (fromCloud) cloudCommandAckResult(reqId, "ok");
}

// ------------------- WebSocket 事件 -------------------
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] #%u connected\n", num);
      pushState();
      pushActionMode();   // 新连上的页面立刻显示"当前选中模式"(高亮分段按钮)
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
#define RX_BUF_SIZE 64                 // 帧解析软件缓冲: 一帧 = 4 头 + payload + 1 CRC
uint8_t  rxBuf[RX_BUF_SIZE];
size_t   rxIdx = 0;
uint8_t  rxState = ST_SYNC0;
uint8_t  rxType = 0, rxLen = 0;

// ---- 帧长合法性上限 (修越界: len 是外部来的 uint8_t, 可达 255) ----
// 现网各帧类型的负载长度: DATA=sizeof(TouchFrame)=41, METRICS=17, SCORE=4, STATUS=1。
// 这里取"缓冲能容纳的最大负载"59 作为硬上限 (4 头 + 59 + 1 CRC = 64 = RX_BUF_SIZE),
// 而不是收紧到 41: dispatchFrame() 里用的是 "len >= sizeof(...)" 这种允许负载略大于
// 结构体的既有宽容写法, 收紧到 41 会改变这类帧的既有行为。取 59 既不改变任何合法帧的
// 解析结果, 又从结构上保证 4+len+1 永远不超过缓冲容量。
static constexpr size_t FRAME_MAX_PAYLOAD = 59;
static_assert(FRAME_MAX_PAYLOAD >= sizeof(TouchFrame), "FRAME_MAX_PAYLOAD 必须能容纳 TouchFrame");
static_assert(FRAME_MAX_PAYLOAD + 5 <= RX_BUF_SIZE, "FRAME_MAX_PAYLOAD 超出 rxBuf 容量");

// ---- UART2 接收健康度统计 (只统计/打印, 不改变任何收发行为) ----
// 用途: 上云等后台活动变重时, 一眼看出主循环有没有被拖慢(UART 丢帧)。
// 判据: ① 坏帧(CRC 错 / len 越界)数增长 → 真的丢了字节或线上有噪声; ② 单轮 loop 积压字节数偏高 → loop 被卡过。
// 460800 baud ≈ 46 字节/ms, 正常轮询(几 ms)积压只有几百字节。
static uint32_t s_uartFrames5s     = 0;   // 窗口内成功解析帧数
static uint32_t s_uartBadCrc5s     = 0;   // 窗口内坏帧数 (CRC 校验失败)
static uint32_t s_uartBadLen5s     = 0;   // 窗口内坏帧数 (len 超限 / 缓冲越界被丢弃)
static size_t   s_uartPeakBacklog  = 0;   // 窗口内单轮最大积压字节数

size_t frameLenByte() { return rxLen; }   // 占位, 兼容旧调用

// ---- 解析辅助: 丢弃当前帧、重新找帧头 ----
// countBad=true 时计入"长度越界/缓冲越界"坏帧统计。
// 只重置状态机变量, 不触碰 g_frame/g_metrics 等业务数据, 与改动前的重置行为一致。
static inline void rxReset(bool countBad) {
  if (countBad) s_uartBadLen5s++;
  rxState = ST_SYNC0;
  rxIdx   = 0;
  rxLen   = 0;
  rxType  = 0;
}

// ---- 解析辅助: 安全入缓冲 (把"理论安全"变成"结构性安全") ----
// 任何 rxBuf[rxIdx++] 之前都先确认 rxIdx < RX_BUF_SIZE; 返回 false = 缓冲已满,
// 调用方据此丢帧重同步。正常情况下 len 判据已兜底, 这里是最后一道结构防线。
static inline bool rxPush(uint8_t b) {
  if (rxIdx >= RX_BUF_SIZE) return false;
  rxBuf[rxIdx++] = b;
  return true;
}

// 每 5s 汇总一次 (只在"有帧/有坏帧/积压偏高"时打印, 避免刷屏拖慢串口)
static void uartHealthTick() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (lastMs == 0) { lastMs = now; return; }
  if ((uint32_t)(now - lastMs) < 5000UL) return;
  uint32_t frames = s_uartFrames5s, badCrc = s_uartBadCrc5s, badLen = s_uartBadLen5s;
  size_t   peak   = s_uartPeakBacklog;
  s_uartFrames5s = 0; s_uartBadCrc5s = 0; s_uartBadLen5s = 0; s_uartPeakBacklog = 0;
  lastMs = now;
  bool backlogHigh = peak > (size_t)(STM32_RX_BUF_SIZE / 2);
  uint32_t bad = badCrc + badLen;
  if (frames || bad || backlogHigh) {
    Serial.printf("[UART] 健康: 5s内 %lu 帧, 坏帧 %lu (CRC错 %lu / 长度越界 %lu), 单轮最大积压 %u/%d 字节%s\n",
                  (unsigned long)frames, (unsigned long)bad,
                  (unsigned long)badCrc, (unsigned long)badLen,
                  (unsigned)peak, (int)STM32_RX_BUF_SIZE,
                  backlogHigh ? "  ← 主循环曾被卡顿(接近半缓冲)" : "");
  }
}

void pollUart() {
  // 进入本轮时的积压字节数 = 上一次 pollUart 到现在 UART 攒下的数据量,
  // 是"主循环延迟"的最直观指标 (被 Cloud HTTPS 卡住时这里会飙到几千)。
  int pending = STM32.available();
  if (pending > 0 && (size_t)pending > s_uartPeakBacklog) s_uartPeakBacklog = (size_t)pending;

  while (STM32.available() > 0) {
    uint8_t b = (uint8_t)STM32.read();
    switch (rxState) {
      case ST_SYNC0:
        if (b == FRAME_SYNC0) { if (rxPush(b)) rxState = ST_SYNC1; else rxReset(false); }
        break;
      case ST_SYNC1:
        if (b == FRAME_SYNC1) { if (rxPush(b)) rxState = ST_TYPE; else rxReset(false); }
        else { rxReset(false); }              // 同步字不匹配 → 重新找帧头 (同原行为)
        break;
      case ST_TYPE:
        rxType = b;
        if (rxPush(b)) rxState = ST_LEN; else rxReset(false);
        break;
      case ST_LEN:
        // ★ 越界判据: len 是外部字节, 合法值必须 <= FRAME_MAX_PAYLOAD。
        //   超限即判为坏帧, 立刻丢帧重新同步, 不再继续按该 len 收字节。
        //   这里不逐帧打印(畸形/噪声帧可能成串出现, 打印会拖慢 loop), 由 5s 健康行汇报。
        if ((size_t)b > FRAME_MAX_PAYLOAD) { rxReset(true); break; }
        rxLen = b;
        if (rxPush(b)) rxState = ST_PAYLOAD; else rxReset(false);
        break;
      case ST_PAYLOAD:
        if (!rxPush(b)) { rxReset(true); break; }   // 结构性兜底: 缓冲满 → 丢帧重同步
        if (rxIdx >= (size_t)(4 + rxLen)) rxState = ST_CRC;
        break;
      case ST_CRC:
        if (!rxPush(b)) { rxReset(true); break; }   // 结构性兜底: 缓冲满 → 丢帧重同步
        if (rxIdx >= (size_t)(5 + rxLen)) {   // 4 头 + payload + 1 CRC
          uint8_t crc = rxBuf[rxIdx - 1];
          if (crc8_over(rxBuf, rxIdx - 1) == crc) {
            dispatchFrame(rxType, rxBuf + 4, rxLen);
            s_uartFrames5s++;
          } else {
            s_uartBadCrc5s++;
            Serial.println("[UART] bad crc");
          }
          rxReset(false);                     // 原: rxState = ST_SYNC0; rxIdx = 0;
        }
        break;
      default:
        rxReset(false);
    }
  }

  uartHealthTick();
}

// 分发帧
void dispatchFrame(uint8_t type, const uint8_t *pl, uint8_t len) {
  remoteLinkFrame(type, pl, len);
  switch (type) {
    case FRAME_DATA: {
      if (len >= sizeof(TouchFrame)) {
        memcpy(&g_frame, pl, sizeof(TouchFrame));
        g_frameNew = true;
      }
      break;
    }
    case FRAME_STATUS:
      if (DEMO_MODE && len >= 1) { g_state = pl[0]; pushState(); }
      break;
    case FRAME_METRICS:
      // 四维特征向量: 存最新值, 由推送循环统一广播 (METRICS 消息)
      if (len >= sizeof(MetricsFrame)) {
        memcpy(&g_metrics, pl, sizeof(MetricsFrame));
        g_metricsNew = true;
      }
      break;
    case FRAME_SCORE:
      // 综合评分 0~100: 存最新值, 由推送循环统一广播 (SCORE 消息)
      if (len >= sizeof(float)) {
        memcpy(&g_score, pl, sizeof(float));
        g_scoreNew = true;
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
  static uint32_t lastMs = 0;
  uint32_t nowMs = millis();
  float dt = (float)(nowMs - lastMs) / 1000.0f;
  if (lastMs == 0) dt = 1.0f / PUSH_HZ;  // 首次
  lastMs = nowMs;
  if (dt > 0.2f) dt = 0.2f;              // 防大跳变
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

  // ---- 模拟四维特征 + 综合评分 (供网页实时画雷达图/评分) ----
  // 值随"按压->保压->滑动->抬起"过程连续变化, 量纲为 0~1 的粗量纲, 归一化在 JS 端做。
  g_metrics.softness   = clamp01(0.55f + 0.35f * (fz / 2.0f));          // 按压越深越"软"
  g_metrics.smoothness = clamp01(0.90f - 0.55f * (fx / (fz + 0.05f)));  // 摩擦越小越顺滑
  g_metrics.roughness  = clamp01(0.25f + 0.35f * fabsf(sinf(2 * PI * 8 * t))); // 力纹波越大越粗糙(值小=细腻)
  g_metrics.rebound    = clamp01(0.45f + 0.45f * (z / 2.0f));           // 回弹贴合随压缩度升高
  g_metrics.flags      = 0x01;                                          // bit0: 数据有效
  g_metricsNew = true;

  // 综合评分 (STM32 端给出的 0~100; 此处按四维权重做近似, 让数字可信)
  // 注意: 网页端(JS)会再按它自己的权重做一次融合, 这里的 g_score 为"设备端评分"参考。
  float fused = 0.30f * g_metrics.softness
              + 0.25f * g_metrics.smoothness
              + 0.20f * (1.0f - g_metrics.roughness)   // 细腻度高(粗糙度小) → 加分
              + 0.25f * g_metrics.rebound;
  g_score = fused * 100.0f;
  g_scoreNew = true;
}

// ============================================================
//  设备上云: 独立 FreeRTOS 任务 (core 0) + "网页广播队列"
//  ------------------------------------------------------------
//  目的: 把同步阻塞的云端 HTTPS 从 loop() 里挪走, 消除
//        "云上传 → 主循环卡顿 → UART 丢帧 / WebSocket 推送停顿"的隐患。
//
//  线程模型:
//    · loop() (core 1, loopTask)  : 排空队列 + webSocket.broadcastTXT (独占 WS)
//    · cloudTask (core 0)         : 周期 cloudUploadLoop() → 内部做 HTTPS
//    · 两者之间只通过 FreeRTOS 队列通信, 没有任何共享的可变状态
//
//  WebSocket 单线程保证:
//    cloud_upload 的通知回调运行在 cloudTask 线程, 里面 **只** 做 xQueueSend
//    (非阻塞, 线程安全), 真正的 broadcastTXT 全部在主 loop 里执行,
//    因此 WebSocketsServer 始终只被 loopTask 访问。
// ============================================================

// 一条待广播的状态文本 (POD, 供 FreeRTOS 队列按值拷贝)
struct CloudWsMsg {
  char text[CLOUD_WS_MSG_LEN];
};

static QueueHandle_t g_cloudWsQ = nullptr;   // 云任务 → 主 loop 的状态消息队列

// 云任务: 只做"推进上传状态机", 网络超时全部在这条任务里消化(不碰 WebSocket/UART)
static void cloudTask(void *arg) {
  (void)arg;
  Serial.printf("[CLOUD] 云任务已启动: core=%d prio=%d stack=%d 周期=%dms\n",
                CLOUD_TASK_CORE, CLOUD_TASK_PRIO, CLOUD_TASK_STACK, CLOUD_TASK_PERIOD_MS);
  const uint32_t t0 = millis();
  bool stackReported = false;
  for (;;) {
    cloudUploadLoop();                        // 内部有界阻塞(HTTPS ≤ 约 6~20s), 也含退避/队列推进
    cloudCommandLoop();                       // 云端远程命令: 先补发 ack, 再按 2s 间隔 poll 一次
                                              // (同一任务内串行 → 同一时刻只有一路 TLS)
    // 开机 60s 后报一次"栈余量最低水位": 用于现场确认 10KB 栈对 TLS 握手够用
    // ⚠ ESP-IDF 的 uxTaskGetStackHighWaterMark() 返回单位是 **字节**(不是 vanilla FreeRTOS 的字),
    //   所以这里直接用它, 不要再乘 sizeof(StackType_t), 否则会把危险值放大 4 倍。
    if (!stackReported && (uint32_t)(millis() - t0) > 60000UL) {
      stackReported = true;
      Serial.printf("[CLOUD] 云任务栈余量(最低水位): %u 字节 / %d (>512 即安全)\n",
                    (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                    CLOUD_TASK_STACK);
    }
    vTaskDelay(pdMS_TO_TICKS(CLOUD_TASK_PERIOD_MS));   // 主动让出 core 0, 不忙等
  }
}

// cloud_upload 的"状态通知"回调 (可能在 cloudTask 线程被调用!)
// ⚠ 这里绝不能调用 webSocket.broadcastTXT() —— 只把文本投进队列, 交给主 loop 广播。
static void cloudNotifyEnqueue(const char *msg) {
  if (!g_cloudWsQ || !msg) return;
  CloudWsMsg m;
  strncpy(m.text, msg, sizeof(m.text) - 1);
  m.text[sizeof(m.text) - 1] = '\0';
  // 非阻塞投递: 队列满时丢"最旧"的一条, 保证网页看到的是最新状态(绝不阻塞云任务)
  if (xQueueSend(g_cloudWsQ, &m, 0) != pdTRUE) {
    CloudWsMsg old;
    xQueueReceive(g_cloudWsQ, &old, 0);
    xQueueSend(g_cloudWsQ, &m, 0);
  }
}

// 主 loop 每轮排空队列并广播 (WebSocket 只在这里被写 → 单线程安全)
static void drainCloudWsQueue() {
  if (!g_cloudWsQ) return;
  CloudWsMsg m;
  int budget = CLOUD_WS_DRAIN_MAX;
  while (budget-- > 0 && xQueueReceive(g_cloudWsQ, &m, 0) == pdTRUE) {
    webSocket.broadcastTXT(m.text);
  }
}

// ============================================================
//  云端远程命令: 主 loop 侧的"执行"入口
//  ------------------------------------------------------------
//  云任务(core 0) 轮询 deviceCommand 拿到命令后, 只把 {id,cmd} 投进命令队列;
//  这里在主 loop 取出并交给 handleCommand(..., fromCloud=true) —— 与网页按钮
//  **同一条处理路径**(改 g_state + pushState + 非 DEMO 时给 STM32 发 FRAME_CMD),
//  执行完由 handleCommand 调 cloudCommandAckResult() 把回执投回云任务。
//  因此 WebSocketsServer / UART2 仍然只被 loopTask 访问, 与既有 V2 约定一致。
// ============================================================
static void drainCloudCmdQueue() {
  CloudCmdMsg m;
  int budget = CLOUD_CMD_DRAIN_MAX;
  while (budget-- > 0 && cloudCommandTake(m)) {   // 非阻塞
    String c(m.cmd);
    c.trim();
    if (c.length() == 0) continue;
    handleCommand(c, true, m.id);
  }
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
  Serial.printf("[CONFIG] 已保存 WiFi: %s\n", ssid.c_str());

  // 就地切换为 APSTA(保持热点, 手机仍能访问), 同时连接 WiFi, 以便把"真实网站地址(IP)"显示给用户
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("连接中: ");
  unsigned t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
    delay(200); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    g_provisioning = false;
    String ip = WiFi.localIP().toString();
    g_bindIP = WiFi.localIP();
    Serial.print("已连接, 网站地址: http://");
    Serial.println(ip);
    MDNS.begin("datatouch");   // 顺带让 datatouch.local 也可用(用户可选, 非必须)

    // 保存成功页: 直接显示真实 IP, 用户照着打开即可
    server.send(200, "text/html",
                "<!DOCTYPE html><html><head><meta charset=utf-8>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#111;color:#ddd;padding:30px;text-align:center}"
                "h3{font-size:30px;color:#3c3;margin:6px 0 14px}p{font-size:22px;line-height:1.8;margin:6px 0}"
                ".big{font-size:36px;color:#fff;font-weight:bold;word-break:break-all;margin:20px 0}"
                ".sub{color:#999;font-size:18px}</style></head><body>"
                "<h3>✅ 已连接到 WiFi</h3>"
                "<p>请让你的手机连接<b>同一个 WiFi</b>，然后打开：</p>"
                "<div class='big'>http://" + ip + "/</div>"
                "<p class='sub'>热点已关闭；若打不开，也可试 http://datatouch.local/</p>"
                "</body></html>");
    // 稍等让页面发出, 再关闭热点 -> 设备进入纯 STA
    delay(1200);
    WiFi.mode(WIFI_STA);
  } else {
    // 连不上: 保持配网(热点开着), 提示重试
    g_provisioning = true;
    Serial.println("连接失败, 保持配网模式");
    server.send(200, "text/html",
                "<!DOCTYPE html><html><head><meta charset=utf-8>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#111;color:#ddd;padding:30px;text-align:center}"
                "h3{font-size:30px;color:#f80;margin:6px 0 14px}p{font-size:22px;line-height:1.8;margin:6px 0}"
                ".sub{color:#999;font-size:18px}</style></head><body>"
                "<h3>❌ 连接失败</h3>"
                "<p>请检查 WiFi 名称和密码后，返回重新配网。</p>"
                "</body></html>");
  }
  // 不调用 ESP.restart(), 就地完成切换
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

  // ---- UART2: 加大接收缓冲(默认 256B 太小) ----
  // ⚠ setRxBufferSize() 必须在 begin() 之前调用才生效(返回实际生效的字节数, 0=失败)。
  //   460800 baud 下 256B 只需约 5.5ms 就灌满, 主循环偶发卡顿即丢帧;
  //   4096B 可容忍约 89ms 的循环抖动(云上传已挪到 core 0, 这里的余量是双保险)。
  size_t rxBufGot = STM32.setRxBufferSize(STM32_RX_BUF_SIZE);
  STM32.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  remoteLinkBegin();
  Serial.printf("[UART] UART2 已启动: %d baud, RX 缓冲 %u 字节(请求 %d)%s\n",
                STM32_BAUD, (unsigned)rxBufGot, (int)STM32_RX_BUF_SIZE,
                (rxBufGot == STM32_RX_BUF_SIZE) ? "" : "  ← 未按预期生效!");

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
  // ---- 新增: 设备网页上的「上云配置」入口 (免重烧改 token/fabricName/deviceId) ----
  // 只"追加"路由, 上面 / 与 /config、下面的 onNotFound 门户重定向逻辑均未改动
  server.on("/cloud", HTTP_GET, handleCloudPage);
  server.on("/cloud/config", HTTP_POST, handleCloudConfigSave);
  // 门户探测(苹果/安卓 captive portal 检测URL)重定向到本机首页 -> 自动弹出
  server.onNotFound([]() {
    String url = String("http://") + g_bindIP.toString() + "/";
    server.sendHeader("Location", url, true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  // ---- 设备上云 (HTTPS 上传实验记录) ----
  // 只新增调用: 读 NVS 配置 / 恢复断网队列 / 启动 SNTP; 不影响配网、WebSocket、显示页
  // 状态回调: 回调可能运行在 cloudTask(core 0) 线程, 所以这里只"投递队列",
  //           真正的 webSocket.broadcastTXT() 在主 loop 的 drainCloudWsQueue() 里做。
  g_cloudWsQ = xQueueCreate(CLOUD_WS_QUEUE_LEN, sizeof(CloudWsMsg));
  if (!g_cloudWsQ) {
    Serial.println("[CLOUD] ! 网页广播队列创建失败(内存不足): 上云仍可工作, 但网页看不到上云状态");
  }
  cloudUploadSetNotifyCallback(cloudNotifyEnqueue);
  cloudUploadInit();   // 内部创建互斥量 → 必须早于下面的 cloudTask

  // ---- 云端远程命令 (云函数 deviceCommand → ESP32 → STM32) ----
  // 只新增调用: 创建"命令/回执"两个 FreeRTOS 队列并读一次配置打印命令 URL;
  // 命令 URL 由"上传 URL"推导(deviceUpload → deviceCommand), 不新增任何配置项。
  // ⚠ 必须早于 cloudTask 创建(先建队列再起任务), 也要早于 loop() 第一次 drainCloudCmdQueue()。
  cloudCommandInit();

  // ---- SNTP 时间同步 (startedAt 需要 epoch 毫秒; 未同步时不发错误数字) ----
  // 放在 WiFi 模式确定之后: 已连 WiFi(STA/APSTA) 时发起同步; 纯 AP 配网模式下没有外网,
  // 但 configTime() 本身无害, 等用户在 /config 里配好 WiFi 连上后会自动同步成功。
  cloudUploadSntpSync();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[SNTP] 已发起时间同步(ntp.aliyun.com/ntp.ntsc.ac.cn/pool.ntp.org)，"
                   "约 1~3 秒后生效；同步状态见下方循环日志与 [CLOUD] 初始化日志");
  } else {
    Serial.println("[SNTP] 当前未连 WiFi(配网中)：暂无法同步时间，配好网后自动重试");
  }

  Serial.printf("配网/显示地址: http://%s/  |  WebSocket: ws://%s:%d\n",
                ip.toString().c_str(),
                ip.toString().c_str(), WS_PORT);
  Serial.printf("上云配置页  : http://%s/cloud  (免重烧改 token/fabricName/deviceId)\n",
                ip.toString().c_str());

  // ---- 启动"云任务"(core 0) ----
  // 必须放在 cloudUploadInit() 之后: 互斥量在 Init 里创建, 先建锁再起任务, 避免竞态。
  // loop() 跑在 core 1, 本任务 pin 到 core 0 → 云端 HTTPS 阻塞影响不到 UART/WebSocket。
  BaseType_t taskOk = xTaskCreatePinnedToCore(
      cloudTask,            // 任务函数: 周期推进 cloudUploadLoop()
      "cloudTask",          // 任务名(便于调试/看栈使用)
      CLOUD_TASK_STACK,     // 栈: 10240 字节
      nullptr,              // 参数
      CLOUD_TASK_PRIO,      // 优先级 1(适中: 高于 idle, 低于 WiFi/lwIP)
      nullptr,              // 不需要任务句柄
      CLOUD_TASK_CORE);     // 固定 core 0
  if (taskOk != pdPASS) {
    Serial.println("[CLOUD] ! 云任务创建失败: 上云将停摆(入队仍可用), 请加大堆或减小 CLOUD_TASK_STACK");
  }
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
    remoteLinkTick();
  }

  static uint32_t lastPush = 0;
  uint32_t now = millis();
  uint32_t period = 1000UL / PUSH_HZ;
  // 按 PUSH_HZ 节拍, 把最新的 曲线/特征/评分 广播给网页 (有新数据才发)
  if ((now - lastPush) >= period) {
    lastPush = now;
    if (g_frameNew)   { pushFrame();   g_frameNew = false; }
    if (g_metricsNew) { pushMetrics(); g_metricsNew = false; }
    if (g_scoreNew)   { pushScore();   g_scoreNew = false; }
  }

  // ---- 设备上云: 只做"轻量入队"(不再在这里跑阻塞的云网络) ----
  // 状态沿检测: 只在 g_state "变化" 的那一拍做一次, 不会每轮 loop 重复触发
  static uint8_t lastCloudState = STATE_IDLE;
  if (g_state != lastCloudState) {
    // 进入运行态的状态沿: 记录"实验开始时刻"(之后上传的 startedAt 用它)
    if (g_state == STATE_RUNNING) {
      cloudUploadNoteRunningEdge();
    }
    lastCloudState = g_state;
    if (g_state == STATE_DONE && DEMO_MODE) {
      Serial.println("[CLOUD] 检测到实验结束(STATE_DONE), 组包入队");
      cloudUploadEnqueueCurrent();   // 用当前 g_metrics/g_score 快照组包入队 (由 cloudTask 上传)
    }
  }

  // ---- 设备上云: 排空"网页广播队列" ----
  // 云任务(core 0)把状态文本投进队列, 这里在主 loop 里广播 →
  // WebSocketsServer 永远只被 loopTask 访问(单线程安全)。
  drainCloudWsQueue();

  // ---- 云端远程命令: 取出云任务投递的命令 → 与网页按钮同一条处理路径 ----
  // (改 g_state / pushState 广播 / 非 DEMO 时给 STM32 发 FRAME_CMD; 执行完自动回 ack)
  drainCloudCmdQueue();

  // ---- 设备上云状态机已挪到 cloudTask(core 0) ----
  // 主 loop 只做"轻量入队"(上面 STATE_DONE 状态沿) + 广播队列排空,
  // 不再调用 cloudUploadLoop() → HTTPS 的 6s/8s/6s 阻塞不会拖慢 UART 接收与 WebSocket 推送。
  // (原第 903 行的 cloudUploadLoop() 已移除)

  // ---- SNTP 同步状态: 同步成功时打印一次(13 位 epoch 毫秒), 之后每 60s 报一次当前时间 ----
  // (便于串口确认 startedAt 的基准是真实绝对时间; 不影响任何既有功能)
  // 若开机时是 AP 配网模式, 用户配好网连上后这里会再发起一次同步(每 10s 最多一次, 内部限流)
  static bool     sntpOkLogged = false;
  static uint32_t sntpLogMs    = 0;
  static bool     sntpAfterWifi = false;
  if (cloudUploadTimeSynced()) {
    if (!sntpOkLogged) {
      sntpOkLogged = true;
      Serial.printf("[SNTP] ✓ 同步成功: epochMs=%llu (startedAt 将使用该时间基准)\n",
                    (unsigned long long)cloudUploadNowMs());
      sntpLogMs = now;
    } else if ((now - sntpLogMs) >= 60000UL) {
      sntpLogMs = now;
      Serial.printf("[SNTP] 当前时间 epochMs=%llu\n",
                    (unsigned long long)cloudUploadNowMs());
    }
  } else if (cloudUploadIsOnline()) {
    // 时间还无效但有网了(例如刚配网成功): 再发起一次同步; 成功后 _sntpSyncLogged 会打印出来
    if (!sntpAfterWifi) {
      sntpAfterWifi = true;
      Serial.println("[SNTP] 检测到网络已就绪但时间未同步, 重新发起 SNTP 同步");
    }
    cloudUploadSntpSync();     // 内部限流(最短 10s), 不会频繁重启 SNTP
  }
}
