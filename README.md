# YD-ESP32-S3 主控通信与手机端网关

> 模块：**ESP32-S3-WROOM-1-N16R8**（16MB Flash + 8MB Octal PSRAM）
> 框架：**Arduino**（PlatformIO / VSCode）
> 链路：**STM32 → UART2 → ESP32-S3 → WiFi + WebSocket → 手机/网页**

ESP32 是整个"主控通信与手机端"的**网关**，只做转发与显示，不做采集算法。STM32 负责采集、控制、时间戳。

---

## 一、项目结构

```
DataTouch\                     # 仓库根目录
├── platformio.ini      # 工程配置（N16R8 / 16MB / qio_opi / 库依赖）
├── README.md           # 本说明
├── docs\
│   └── 产品端方案对比.md    # 产品形态 / 配网方式对比
└── src\
    ├── main.cpp        # WiFi(配网/门户/mDNS) + WebSocket + UART 收发 + DEMO
    ├── protocol.h      # STM32<->ESP32 二进制帧协议 + CRC8
    ├── cloud_upload.h  # 【新增】设备上云（HTTPS 上传实验记录）接口/配置
    └── cloud_upload.cpp# 【新增】组包 + HTTPS POST + 断网环形队列/退避/幂等
```

## 二、第一次准备（等 PlatformIO 下完即可）

1. VSCode 装好 **PlatformIO IDE** 插件（首次会下载工具链，较慢，**耐心等**）。
2. 用 **File → Open Folder** 打开 `D:\Develop\ESP32CODE\DataTouch` 文件夹（**不要**再点 New Project）。
3. 等左下角 PlatformIO 显示 `Loading` 结束、环境就绪。

## 三、烧录前改两处

**1. 端口：** 在 `platformio.ini` 里取消 `upload_port` 注释，填成 CH340 口对应的 COM 号
   （设备管理器里看，通常是 `COMx`）。不填也可以，PlatformIO 有时能自动识别。

**2. WiFi（两种方式）：**
- **产品方式（推荐）**：不用填。设备未配网时自动开热点 `YD-ESP32S3`（密码 `12345678`），手机连上后在网页里输 WiFi 保存即可。
- **开发快捷方式**：在 `src/main.cpp` 顶部填 `CUSTOM_SSID` / `CUSTOM_PASS`，并把 `DEV_QUICK_WIFI` 改为 `1`，则未配网时也会自动连它。

> 连接烧录用 **CH340 那个 Type-C 口**；板上另一个 Type-C 是原生 USB-OTG，通常可不管。

## 四、硬件连接（接真实 STM32 时）

| STM32 (F4VET6) | ESP32-S3 | 说明 |
|------|------|------|
| TX  | GPIO18 | STM32 发送 → ESP32 接收 |
| RX  | GPIO17 | ESP32 发送 → STM32 接收 |
| GND | GND    | **必须共地** |
| 3.3V 输出 | 3V3 | 若传感器需要供电（可选） |

- 默认波特率 `460800`（已统一裁定；`Serial.begin(115200)` 是 ESP32 自己的 USB 调试口，不属于本链路），如 STM32 不同请改 `main.cpp` 里的 `STM32_BAUD`。
- 两边都是 3.3V 电平，可直接直连；如电平不同请加电平转换。

## 五、烧录与测试

1. 连接好 CH340 口，点 PlatformIO 左下角 **Upload**（编译+烧录）。
2. 打开 **Serial Monitor**（115200），应看到芯片信息与 `本机 IP:`。
3. 手机/电脑浏览器打开 `http://datatouch.local/`（手机与设备同一 WiFi），或串口打印的 IP，能看到实时曲线页 + ARM/START/STOP/RESET 按钮。

### 立刻看效果（无需 STM32）
`main.cpp` 里 `#define DEMO_MODE true` 默认开启，会**内置模拟"按压→保压→滑动→抬起"数据**，
刷新页面即可看到蓝=Fz、红=Fx、绿=Y 的曲线滚动。等接入 STM32 后改为 `false`。

### WiFi 配网 / 门户 / mDNS（产品化）
- **没连上网 → 开热点**：设备未保存 WiFi（或点了重新配网）时，开机进入**配网模式**，自动开热点 `YD-ESP32S3`（密码 `12345678`）。
  手机连上热点后，**门户自动弹出**配网页（`http://192.168.4.1/`）。
- **输入 WiFi → 保存 → 关热点**：在配网页输入家里 WiFi 名称和密码点保存，设备保存到 NVS、连上该 WiFi 并**自动关闭热点**。
- **连接成功页 → 提示**：保存后页面会提示"设备已连接你的 WiFi，热点已关闭，请让手机连接同一 WiFi，然后打开 **`http://datatouch.local/`**"。
- **连上后显示**：设备连上 WiFi 后进入**显示模式**（仅 STA，无热点），手机与设备同一 WiFi 时用 `http://datatouch.local/`（mDNS）或串口显示的 IP 访问。
- **重新配网**：显示页右上角"**重新配网**"→ 清除已存 WiFi 并**置配网标志**，下次开机必定进配网模式（手机需手动连接热点 `YD-ESP32S3`）。
- 两个地址在不同网络：`192.168.4.1`（热点，配网时用）与 `datatouch.local`（连网后，同 WiFi 用）。
- 编译期 `CUSTOM_SSID/PASS` 仅用于开发快捷连网（`DEV_QUICK_WIFI=1` 时）；用户配置存进 NVS，掉电不丢。

## 六、设备上云（HTTPS 上传实验记录）

> 模块：`src/cloud_upload.h` / `src/cloud_upload.cpp`（**新增，只加不改**：`protocol.h` 零改动，WiFi 配网 / WebSocket / 内嵌显示页 / DEMO 逻辑都不动）
> 方案背景见 `docs\小程序与微信云开发落地方案.md`（§5 设备上云、§8 上传协议）。

### 6.1 用途

- **一次实验结束**（`g_state == STATE_DONE`）后，自动把**四维特征**（`softness / smoothness / roughness / rebound`，取自 `g_metrics`，已是 0~1 归一化）与**综合评分**（`composite`，取自 `g_score`，0~100）拼成 JSON，用 `WiFiClientSecure + HTTPClient` 以 **HTTPS POST** 上传到微信云开发 **HTTP 云函数 `deviceUpload`**，在 `experiments` 集合里新增一条 `source:"device"` 的记录。
- 分工不变：**本地毫秒级实时仍归设备网页（WebSocket）**；上云只负责**历史沉淀 / 远程查看**。两条路并存，互不替代。
- ESP32 **不做任何特征重算**：四维与评分直接取 STM32 给的值（职责边界不变）。

### 6.2 上传的 JSON（字段与云端对齐）

```json
{"token":"REPLACE_ME","deviceId":"esp32-dev01","fabricName":"未命名布料","operator":"operator",
 "softness":0.623,"smoothness":0.710,"roughness":0.240,"rebound":0.550,"composite":72.4,
 "metricsNormalized":true,
 "weights":{"softness":0.30,"smoothness":0.25,"roughness":0.20,"rebound":0.25},
 "curveRef":"","dedupId":"esp32-dev01-123456-7","startedAt":1756376463000,"source":"device"}
```

| 字段 | 来源 | 说明 |
|---|---|---|
| `token` | NVS 配置 | 云端校验用；默认占位 `REPLACE_ME` |
| `deviceId` | NVS 配置 | 默认 `esp32-dev01` |
| `fabricName` | NVS 配置 | 默认 `未命名布料`（可配置） |
| `operator` | 入队参数 | 默认 `operator`；「上传测试」传 `test` |
| `softness/smoothness/roughness/rebound` | `g_metrics` | 0~1，限幅保护（NaN/Inf → 0） |
| `composite` | `g_score` | 0~100，限幅保护 |
| `metricsNormalized` | 固定 `true` | 四维已归一化 |
| `weights` | 固定快照 | 仅记录本次口径，便于复现 |
| `curveRef` | 固定 `""` | MVP 只传摘要，不传曲线 |
| `dedupId` | 自动生成 | `deviceId-millis-seq`，**幂等键**（重传不产生重复记录） |
| `startedAt` | 实验开始时刻 | **JSON number**，13 位 epoch 毫秒（如 `1789360500123`）。语义 = **实验开始时刻**（`g_state` 进入 `STATE_RUNNING` 的状态沿记录）；从未运行过（如 DEMO 直接 STOP）则回退为**入队时刻**；SNTP 未同步时传 `0` 并串口告警，**绝不发错误数字** |
| `source` | 固定 `"device"` | 标记来源为设备上云 |

### 6.3 配置（NVS，掉电不丢）

| 编译期默认宏（`src/cloud_upload.h`） | 默认值 | NVS 键 |
|---|---|---|
| `CLOUD_UPLOAD_URL` | `https://cloud1-d5gncnkxj2606d114-1487403371.ap-shanghai.app.tcloudbase.com/deviceUpload` | `url` |
| `CLOUD_TOKEN` | `REPLACE_ME`（占位） | `token` |
| `DEVICE_ID` | `esp32-dev01` | `devid` |
| `FABRIC_NAME` | `未命名布料` | `fabric` |

改动方式（任选）：

1. **设备网页（推荐，免重新烧录）**：浏览器打开 `http://<设备IP>/cloud`（设备页右上角也有 **「上云配置」** 入口），
   直接改 `token` / `fabricName` / `deviceId` → 点「保存」→ 写入 NVS **立即生效**，**无需重启**。详见 6.5。
2. **编译期**：改 `src/cloud_upload.h` 顶部默认值，或在 `platformio.ini` 里加
   `build_flags = -DCLOUD_TOKEN=\"你的真实token\"`（注意 ⚠ 别把真实 token 提交进 Git）。
3. **代码运行期**：调用
   `cloudUploadSetConfig(url, token, deviceId, fabricName)` → 立刻落 NVS，串口会打印
   `[CLOUD] 配置已保存到 NVS: ... token=abcd****`（token 只显示前 4 位）。
4. **读回**：`cloudUploadGetConfig()`；队列/结果：`cloudUploadPendingCount()` / `cloudUploadDroppedCount()` / `cloudUploadLastResult()`。

### 6.4 用网页「上传测试」按钮验证（最快路径）

1. 手机/电脑打开设备页 `http://datatouch.local/`（或串口打印的 IP）。
2. 页面顶部多了一个 **「上云」** 状态卡；按钮行多了一个 **「上传测试」**（蓝色）按钮。
3. 点一下 → 浏览器通过 WebSocket 发 `upload` 命令 → 设备端 `cloudUploadTestNow()` 用**当前** `g_metrics/g_score` 立刻 POST 一条测试记录。
4. 预期看到：
   - 网页「上云」状态依次变成 `已请求` → `已入队 pending=1 …` → `上传中 …` → **`✓ 成功 HTTP 200 _id=… 耗时…ms pending=0`**；
   - 串口打印 `[CLOUD] ===== 手动测试上传 =====`、`[CLOUD] 组包(当前值): soft=… composite=…`、`[CLOUD] ENQ:`、`[CLOUD] SEND:`、`[CLOUD] OK: HTTP 200 …`，以及云端原始响应 `[CLOUD] 云端响应(HTTP 200): {"code":0,...}`；
   - 云开发控制台 `experiments` 集合**多一条**记录（`source:"device"`、`deviceId`、`dedupId`、四维与 `composite` 与网页显示一致）。
5. 若失败：网页显示 `✗ 失败 HTTP …`，串口给具体原因与下次重试时间；按 6.5 的退避自动重试。

> 「上传测试」**不影响状态机**（不改 `g_state`、不转发 `FRAME_CMD` 给 STM32），可随时点。

### 6.5 设备网页「上云配置」页（免重烧改 token / fabricName / deviceId）

设备页（`INDEX_HTML`）右上角，**「重新配网」** 左边多了 **「上云配置」** 链接 → `GET /cloud`：

| 方法 | 路径 | 作用 |
|---|---|---|
| `GET` | `/cloud` | 返回配网页 `CLOUD_CFG_HTML`（风格同配网页：深色卡片 / 大字号 / 移动端友好）。表单含 `token`（密码框，回显当前值）、`fabricName`、`deviceId`，并显示当前 token 掩码与固定上传地址 |
| `POST` | `/cloud/config` | 解析表单 → `cloudUploadSetConfig(cur.url, token, deviceId, fabricName)` 写 NVS → 返回**「已保存」提示页**（新值回显，token 脱敏）。**无需重启，立即生效** |
| `GET` | `/` | 主显示页（右上角新增「上云配置」入口链接） |
| `POST` | `/config` | WiFi 配网（**未改动**） |
| `GET` | `/reset` | 重新配网（**未改动**） |
| — | `onNotFound` | 门户重定向（**未改动**）；新路由只是 `server.on` 追加，且在 `onNotFound` 之前注册，所以 `/cloud` 不会被门户重定向吞掉 |

- **token 留空 = 不修改**（浏览器不回显密码时的常见做法，若故意要清空 token 可先填新值）。
  实际上 `cloudUploadSetConfig()` 对空值会回落编译期默认值，所以留空时直接用读回的旧值提交。
- `fabricName` 里的空格/中文会由 `WebServer::urlDecode()` 正确还原（表单 `+` → 空格）。
- 配置立即生效：下一次「上传测试」或 `STATE_DONE` 入队就用新值，**不用开热点、不用重烧**。

### 6.6 断网 / 失败行为（环形队列 + 退避 + 幂等）

| 机制 | 实现 |
|---|---|
| **环形队列** | 默认 **16 条**（`CLOUD_QUEUE_SIZE`）；每条入队时**立刻写 NVS**（`q0..q15`），**掉电不丢**；队列满则丢弃最旧一条并累计 `droppedCount`（串口/网页会提示 `DROP`）。 |
| **自动补传** | `loop()` 每轮调用 `cloudUploadLoop()` 推进状态机；有网且到期时上传队首一条；一条成功后才出队。重启后剩余记录在开机约 3s 后自动继续补传。 |
| **指数退避** | 失败后 **5s → 15s → 60s → 300s**（封顶）再试；退避用 `millis()` 计时，**不在 `loop()` 里 `delay()`**。离线时只每 2s 复查一次，不消耗退避档位。 |
| **幂等去重** | 记录在**入队时**就固定 `dedupId`（`deviceId-millis-seq`，`seq` 存 NVS 且单调递增，重启不重号），重传用的是**同一份 JSON 字节**；云端按 `dedupId` upsert → 网络抖动/半成功也不会产生重复记录。 |
| **超时有界** | 连接超时 6s、TLS 握手 8s、响应 6s；每轮 `loop()` **最多发一条**，网页/WebSocket 正常推送不受影响（极端情况单轮 loop 会被 HTTP 超时占用最多几秒，这是 MVP 的有意取舍；后续要彻底非阻塞可挪到独立 FreeRTOS 任务）。 |
| **可观测** | 串口 `[CLOUD] …` 全程日志；网页「上云」状态卡实时显示 `已入队/上传中/成功/失败/离线等待`。 |

### 6.6 `startedAt` 与 SNTP 时间（原「时间戳不对」问题的修法）

**根因**：`cloud_upload.cpp` 里 `nowEpochMs()` 返回 **`uint32_t`**：
`(uint32_t)t * 1000UL`，其中 `t` 已是 epoch **秒**（约 1.79e9），乘 1000 得到约 **1.79e12**，
远超 `uint32_t` 上限 4294967295 → 按 2³² 取模，于是串口出现 `startedAt=2693757455`（≈24 位以内的乱数，不是 13 位 epoch 毫秒）。

**修法**：

| 项 | 改法 |
|---|---|
| 类型 | `nowEpochMs()` / `CloudRecord::startEpochMs` / `buildBody()` 的 `startedAt` 参数全部改为 **`uint64_t`**；JSON 用 `snprintf("%llu")` 拼数字（仍是 JSON **number**） |
| 子秒精度 | 不再用 `millis() % 1000`（毫秒会在"秒"的任意相位跳变、最大差近 1 秒），改用开机以来单调时钟 `esp_timer_get_time()` 推算 |
| 语义 | `main.cpp` 在 `g_state` 进入 **`STATE_RUNNING` 的状态沿**调 `cloudUploadNoteRunningEdge()` 记录 `millis()`；入队时 `cloudUploadResolveStartedAt()` 用 `当前epoch - 已过时长` 回算出**实验开始那一刻**的绝对时间 |
| 回退 | 从未进入运行态（如 DEMO 直接 STOP）→ 用**入队时刻**；`elapsed > 1 天`（跨上电残留的过期记录）也按入队时刻处理 |
| 有效性 | 判据 `epoch 秒 ≥ 1600000000`（2020-09）／`epoch 毫秒 > 1600000000000`，避免把 1970 年或开机秒数当真实时间 |
| 未同步策略 | **择一：先尝试同步 + 有界短等（1.5s），仍失败才传 `0` 并串口告警**（不选"无限等待"，因为 `loop()` 不能被长时间阻塞）。`configTime()` 不阻塞，同步由系统后台任务完成 |
| SNTP 启动 | `setup()` 在 WiFi 模式确定后调 `cloudUploadSntpSync()`（服务器 `ntp.aliyun.com` / `ntp.ntsc.ac.cn` / `pool.ntp.org`）；`cloudUploadInit()` 内也发起一次 |

> 说明：入队时 JSON 就已**冻结**（保证重传幂等），所以如果某条记录是在时间无效时入队的，它的 `startedAt` 就是 `0`，
> 之后同步成功也不会改写那一条；**之后的新记录**都会是正常的 13 位 epoch 毫秒。

### 6.7 串口日志速查（115200）

```text
[CLOUD] ===== 设备上云(HTTPS) 初始化 =====
[CLOUD] URL      : https://.../deviceUpload
[CLOUD] deviceId : esp32-dev01 | fabricName: 未命名布料 | operator默认: operator
[CLOUD] token    : REPL****            ← 占位值会额外提示更换
[CLOUD] 队列     : 0/16 条待传，累计丢弃 0
[CLOUD] · SNTP 已同步：now=1789360500757 ms (epoch 毫秒)      ← 已同步时
[SNTP] ✓ 同步成功: epochMs=1789360500757 (startedAt 将使用该时间基准)
[CLOUD] 记录实验开始时刻: millis=42310  epochMs=1789360471000   ← 点 START 进入运行态时
[CLOUD] startedAt=实验开始时刻(回算): 1789360471000 ms (已过 12500 ms)  ← 入队时间START 12.5s
[CLOUD] ENQ: pending=1 esp32-dev01-123456-7 composite=72.4 startedAt=1789360471000
[CLOUD] SEND: 第1次尝试 esp32-dev01-123456-7 pending=1
[CLOUD] 云端响应(HTTP 200): {"code":0,"message":"ok","data":{"_id":"a1b2..."}}
[CLOUD] OK: HTTP 200 _id=a1b2... 耗时1234ms pending=0
[CLOUD] ! 等待 SNTP 同步超时(1500 ms)：本次不改时间，startedAt 按规则回退/传 0  ← 未同步时
[CLOUD] ENQ: ... startedAt=0(时间无效)                                       ← 未同步时传 0
[CLOUD] ! 警告：SNTP 未同步，本条 startedAt 只能传 0（不发送错误时间戳）
[CLOUD] FAIL: HTTP -1 网络或超时(HTTPClient=-1) 耗时6001ms; 5s 后重试 pending=1   ← 断网时
[CLOUD] OFF: 离线(等联网) pending=1                                            ← 无 WiFi 时
```

### 6.8 ⚠ 安全提示（MVP 取舍，正式版要改）

- HTTPS 目前用 `client.setInsecure()`（**不校验证书链**，先跑通链路）。正式版应改为内置根 CA：
  `client.setCACert(ROOT_CA_PEM);` 或用 `client.setFingerprint("AA:BB:...")` 做指纹校验（代码中已留注释）。
- `token` 是**明文共享密钥**：不要提交进 Git；建议后续升级为每设备独立密钥 + HMAC 签名（见 `docs\小程序与微信云开发落地方案.md` §5.3）。
- 上云只传**摘要**（四维 + 评分 + 元数据，约 300 字节），不传曲线/原始数据，省云端额度。

## 七、协议（src/protocol.h）

帧格式（二进制，小端）：
```
[sync0=0xAA][sync1=0x55][type][len][payload...][crc8]
```
- 字段与任务书 CSV 一致：`t, x, y, z, Fx, Fy, Fz, Mx, My, Mz, contact`（y 为第二阶段扩展轴，当前恒为 0）
- `FRAME_DATA` 载荷 = `TouchFrame`（10 个 float + 1 字节 contact）≈ 41 字节
- `FRAME_CMD`(5)：手机端 ARM/START/STOP/RESET 转发给 STM32
- `FRAME_STATUS`(2) / `FRAME_SCORE`(4)：预留

STM32 侧按同样格式组帧即可被 ESP32 解析。校验：`crc8_over()`（多项式 0x31，初值 0xFF）。

## 八、当前状态 / 待办

- [x] 工程可编译/烧录（含 DEMO 数据）
- [x] 设备上云模块（`cloud_upload.*`）：STATE_DONE 自动入队 + 网页「上传测试」+ 断网环形队列/退避/幂等
- [x] `startedAt` 修正为 13 位 epoch 毫秒（`uint64_t`；语义 = 实验开始时刻，回退入队时刻；SNTP 未同步传 0 + 告警）
- [x] 设备网页「上云配置」页（`GET /cloud` + `POST /cloud/config`）：免重烧改 `token`/`fabricName`/`deviceId`，写 NVS 立即生效
- [ ] 云端 `deviceUpload` 联调：把 `token` 换成真实值（NVS），确认 `experiments` 落库 `source:"device"`
- [ ] 手机端真实 STM32（`DEMO_MODE=false`）
- [ ] 帧协议与 STM32 联调（`FRAME_METRICS` 分项指标结构待定）
