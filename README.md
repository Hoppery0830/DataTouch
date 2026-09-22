> 当前为三端电机联调固件：上电不运动，选择模式 → ARM → START；详见 [docs/电机联调.md](docs/电机联调.md)。以下旧演示说明不代表当前启动配置。

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
    ├── cloud_upload.cpp# 【新增】组包 + HTTPS POST + 断网环形队列/退避/幂等
    ├── cloud_command.h # 【新增】云端远程命令（轮询 deviceCommand/白名单/去重/ack）接口与参数
    └── cloud_command.cpp#【新增】poll + 解析 + 投队列(主 loop 执行) + ack 回执
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
| **自动补传** | 独立任务 **`cloudTask`（core 0）** 每 30ms 调用一次 `cloudUploadLoop()` 推进状态机；有网且到期时上传队首一条；一条成功后才出队。重启后剩余记录在开机约 3s 后自动继续补传。主 `loop()`（core 1）**不再**调用它。 |
| **指数退避** | 失败后 **5s → 15s → 60s → 300s**（封顶）再试；退避用 `millis()` 计时，**不在主 `loop()` 里 `delay()`**。离线时只每 2s 复查一次，不消耗退避档位。 |
| **幂等去重** | 记录在**入队时**就固定 `dedupId`（`deviceId-millis-seq`，`seq` 存 NVS 且单调递增，重启不重号），重传用的是**同一份 JSON 字节**；云端按 `dedupId` upsert → 网络抖动/半成功也不会产生重复记录。 |
| **超时有界** | 连接超时 6s、TLS 握手 8s、响应 6s。这些阻塞**全部发生在 core 0 的 `cloudTask` 里**，主 `loop()` 照常跑 UART 接收 + 20Hz WebSocket 推送 → 上传期间曲线不卡、不丢帧（V2 并发重构；详见 §6.9）。 |
| **可观测** | 串口 `[CLOUD] …` 全程日志；网页「上云」状态卡实时显示 `已入队/上传中/成功/失败/离线等待`。 |

### 6.9 并发模型（V2 重构：云端 HTTPS 不再阻塞主循环）

**问题**：V1 在 `loop()` 里直接调 `cloudUploadLoop()`，而它是**同步阻塞 HTTPS**（连接 6s / TLS 8s / 响应 6s）。
一旦云端慢或断网，单轮 `loop()` 会被占住几秒 → UART2 收帧被耽误（丢帧）、WebSocket 曲线推送停顿。

**改法**（`src/main.cpp` + `src/cloud_upload.*`）：

| 项 | 实现 |
|---|---|
| 独立任务 | `xTaskCreatePinnedToCore(cloudTask, "cloudTask", 10240, …, prio 1, core 0)`；任务内 `cloudUploadLoop()` + `vTaskDelay(30ms)`（不忙等）。Arduino `loop()` 在 core 1（`ARDUINO_RUNNING_CORE`），互不阻塞。 |
| 互斥保护 | `cloud_upload.cpp` 内一把 `SemaphoreHandle_t s_mtx`（`cloudUploadInit()` 里创建 → **先建锁、再起任务**）保护 配置 / 环形队列 + NVS / 状态机 / 最近结果。 |
| 临界区纪律 | 锁内只碰 RAM/NVS（微秒~毫秒），**绝不**在持锁期间做 HTTPS 或等 SNTP；统一"锁内取副本 → 解锁 → 发网络 → 再加锁回写状态"。 |
| WebSocket 单线程 | 云任务**不直接**碰 `WebSocketsServer`。状态回调改成只 `xQueueSend` 到一个 FreeRTOS 队列（`CLOUD_WS_QUEUE_LEN=16`），主 `loop()` 每轮 `drainCloudWsQueue()` 取出后 `broadcastTXT` → WS 永远只被 loopTask 访问。 |
| 数据快照 | `g_metrics/g_score` 在**主 loop 入队那一刻**快照进 JSON（`cloudUploadEnqueueCurrent()`），云任务只搬运已冻结的 JSON 字节，读不到"半更新"状态。 |
| NVS 串行化 | 队列落盘、`seq`、配置写入全部在互斥区内，避免两线程同时写 `Preferences`。 |
| 栈水位自证 | `cloudTask` 在开机 60s 后打印一次 `uxTaskGetStackHighWaterMark()`，用于确认 10KB 栈对 TLS 握手够用（>512 字节即安全）。 |
| UART 余量 | `STM32.setRxBufferSize(4096)`（在 `begin()` 之前）→ 460800 baud 下可容忍约 89ms 的循环抖动；串口每 5s 汇总一次 `[UART] 健康: … 帧 / 坏帧 / 单轮最大积压`，丢帧一眼可见。 |

> 仍未消除的唯一有界等待：入队时若 SNTP 尚未同步，`kickTimeSyncIfNeeded()` 会在**主 loop** 里最多等 1.5s
> （V1 既有设计，用于让 `startedAt` 拿到真实绝对时间）。它只在"开机后第一次入队且时间未同步"时发生，与云端快慢无关。


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
[UART] UART2 已启动: 460800 baud, RX 缓冲 4096 字节(请求 4096)             ← V2 新增
[CLOUD] ===== 设备上云(HTTPS) 初始化 =====
[CLOUD] URL      : https://.../deviceUpload
[CLOUD] deviceId : esp32-dev01 | fabricName: 未命名布料 | operator默认: operator
[CLOUD] token    : REPL****            ← 占位值会额外提示更换
[CLOUD] 队列     : 0/16 条待传，累计丢弃 0
[CLOUD] · SNTP 已同步：now=1789360500757 ms (epoch 毫秒)      ← 已同步时
[CLOUD] 云任务已启动: core=0 prio=1 stack=10240 周期=30ms                ← V2 新增
[CLOUD] 云任务栈余量(最低水位): 6216 字节 / 10240 (>512 即安全)          ← V2 新增(开机60s后)
[SNTP] ✓ 同步成功: epochMs=1789360500757 (startedAt 将使用该时间基准)
[CLOUD] 记录实验开始时刻: millis=42310  epochMs=1789360471000   ← 点 START 进入运行态时
[CLOUD] startedAt=实验开始时刻(回算): 1789360471000 ms (已过 12500 ms)  ← 入队时间START 12.5s
[CLOUD] ENQ: pending=1 esp32-dev01-123456-7 composite=72.4 startedAt=1789360471000
[CLOUD] SEND: 第1次尝试 esp32-dev01-123456-7 pending=1
[CLOUD] 云端响应(HTTP 200): {"code":0,"message":"ok","data":{"_id":"a1b2..."}}
[CLOUD] OK: HTTP 200 _id=a1b2... 耗时1234ms pending=0
[UART] 健康: 5s内 250 帧, 坏帧 0, 单轮最大积压 138/4096 字节          ← V2 新增(丢帧自检)
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
- **远程控制（§6.10）**：白名单只有 `arm/start/stop/reset`，且有编译期总开关 `CLOUD_CMD_ENABLE`；
  但**远程命令有延迟（≥ 轮询间隔 2s + 一次 HTTPS），不是实时控制 —— 现场操作永远优先**；
  拿到 `token` 即等于能远程控制设备，切勿外泄。

### 6.10 远程控制（云端命令：小程序/网页 → 云函数 → ESP32 → STM32）

> 一句话：设备在 **core 0 的 `cloudTask`** 里每 **2s** 向云函数 `deviceCommand` 发一次 `action=poll`，
> 拿到小程序/网页下发的命令后由**主 `loop()`** 执行（改 `g_state`、广播 WebSocket、非 DEMO 时给 STM32 发 `FRAME_CMD`），
> 执行完再发 `action=ack` 回执。**只新增文件 `cloud_command.*`，既有功能零改动。**

#### 6.10.1 接口约定（与云函数/小程序同一份约定）

| 方向 | 请求 | 响应 |
|---|---|---|
| 轮询 | `POST {"action":"poll","token":…,"deviceId":…}` | `{"ok":true,"commands":[{"id":"…","cmd":"…"}],"serverTime":…}` |
| 回执 | `POST {"action":"ack","token":…,"deviceId":…,"id":…,"result":"ok"}` | `{"ok":true}` |

- **命令白名单（只有 4 个）**：`arm` / `start` / `stop` / `reset`；未知命令**不执行**，只回 `result:"unknown"`。
- 命令 **60s 过期由云端保证**（不下发过期命令）；设备侧另做本地兜底（见 6.10.4）。
- 解析容错：`commands` 数组只要能被找到即可，扁平 `{"commands":[…]}` 或嵌套 `{"data":{"commands":[…]}}` 都支持；
  `id` 支持字符串（Mongo `_id`）与裸数字；可选认 `ts` / `createdAt` 作为时间戳。

#### 6.10.2 命令 URL 怎么来的（**不用新增配置项**）

命令 URL = 把**上传 URL**里的 `deviceUpload` 换成 `deviceCommand`：

| 上传 URL（可在 `/cloud` 页改，写 NVS） | 推导出的命令 URL |
|---|---|
| `https://<env>.tcloudbase.com/deviceUpload` | `https://<env>.tcloudbase.com/deviceCommand` |
| `https://host/api/deviceUpload?v=1`（带 query） | `https://host/api/deviceCommand?v=1` |
| 匹配不到 `deviceUpload` 的地址 | 编译期兜底宏 `CLOUD_COMMAND_URL`（`src/cloud_command.h`，默认同域名 `/deviceCommand`） |

规则在 `cloud_command.cpp` 的 `deriveCommandUrl()`：① 先按 `/deviceUpload` **结尾后缀**替换 →
② 退化为"整串最后一次出现"替换（兼容带 query 的地址）→ ③ 都不匹配才用编译期宏兜底。
token / deviceId 同样复用既有配置（`cloudUploadGetConfig()`），所以**改 `/cloud` 页的上传地址，命令地址自动跟着变**。
`GET /cloud` 页现在会直接显示推导出的命令 URL 与远程控制开关状态，便于现场核对。

#### 6.10.3 轮询 / 应用 / ack 流程与线程安全

| 环节 | 在哪跑 | 说明 |
|---|---|---|
| 轮询 `action=poll` | **`cloudTask`（core 0）**，`cloudCommandLoop()` | 固定间隔 `CLOUD_CMD_POLL_MS=2000`（可宏调）；`WiFiClientSecure.setInsecure()` + `HTTPClient`，超时**连接 4s / TLS 6s / 响应 4s**；失败退避 **2s→4s→8s→16s→30s**（成功清零），离线只提示一次 → 不刷屏。与上传是**同一任务串行**执行 → 同一时刻只有一路 TLS。 |
| 解析 + 校验 | `cloudTask` | 逐条：白名单 → 本地过期兜底 → 同一 `id` 去重；通过后把 `{id,cmd}` 投进**命令队列**（`xQueueSend` 非阻塞）。 |
| 执行 | **主 `loop()`（core 1）**，`drainCloudCmdQueue()` | 取出命令 → `handleCommand(cmd, true, id)`（**与网页 ARM/START/STOP/RESET 完全同一条处理路径**）：`g_state=…` + `pushState()` 广播 + 非 `DEMO_MODE` 时 `STM32.write(FRAME_CMD)`。 |
| 回执 `action=ack` | 主 loop 投队列 → `cloudTask` 发 | `cloudCommandAckResult(id,"ok")` 只做 `xQueueSend` 进**回执队列**；云任务下一轮取出并 POST ack。**WebSocket 广播与 UART2 写永远只在主 loop**，与 6.9 的 V2 约定一致。 |
| 互斥量与纪律 | — | 配置只经 `cloudUploadGetConfig()`（cloud_upload 那把既有互斥量）**锁内取副本 → 解锁做网络**；模块自身状态（轮询计时 / 退避 / 去重表 / 统计）只被 `cloudTask` 一条任务读写，跨线程只走两个 FreeRTOS 队列 → 不需要新的长临界区，**没有任何持锁网络 I/O**。 |

> 队列深度：命令 8 条 / 回执 8 条；主 loop 每轮最多执行 `CLOUD_CMD_DRAIN_MAX=4` 条，
> 云任务每轮最多发 `CLOUD_CMD_ACK_MAX_PER_LOOP=2` 条 ack（都是"防挤占"上限）。
> `cloudTask` 栈 10KB 不变：轮询与上传在同一任务内串行，峰值栈与改造前一致。

#### 6.10.4 安全措施

| 措施 | 实现 |
|---|---|
| **白名单** | 只认 `arm/start/stop/reset`（`cloudCommandIsWhitelisted()`；云任务与主 loop **双重校验**），其余不执行并回 `result:"unknown"`。 |
| **总开关** | 编译期 `CLOUD_CMD_ENABLE`（`src/cloud_command.h`，**默认 1 = 开启**）；`platformio.ini` 加 `-DCLOUD_CMD_ENABLE=0` 即关闭。**选择"关闭时直接不 poll"**（不是"poll 但不执行"）：设备侧彻底没有远程控制面，也不产生无意义流量与日志；云端未消费的命令在其 60s 过期窗口内自然作废。运行期还可用 `cloudCommandSetEnabled(false)` 临时关（不落 NVS，避免多一份掉电状态）。⚠ 关闭后**已排队命令的回执仍会补发**（执行结果要让云端知道）。 |
| **去重（同一 id 只执行一次）** | 环形表 `CLOUD_CMD_RECENT_LEN=8` 记录最近处理过的 `id`：云端重投（ack 丢了 / 网络抖动）时**只补 ack，不重复执行**；只有"真正排进命令队列"的才记入去重表 → 极端队列满也不会静默吃掉命令。 |
| **60s 过期（本地兜底）** | 云端保证不下发过期命令；设备侧仅当响应带 `ts`/`createdAt` **且本地 SNTP 已同步**时才判断（兼容 10 位秒 / 13 位毫秒；未来时间戳、不合理值一律不判），超 `CLOUD_CMD_MAX_AGE_MS=60000` 的只回 `result:"expired"` 不执行。**判不准就不判 → 不会误杀合法命令**。 |
| **ack 失败自愈** | ack 失败不回队、不重试：云端在 60s 窗口内重投该命令 → 去重表命中 → 再补一次 ack。 |

> ⚠ **安全提示**：远程命令**有延迟**（最坏 ≈ 轮询间隔 2s + 一次 HTTPS + 队列），**不是实时控制**；
> **现场操作优先**（设备页/小程序 WebSocket 直连的延迟远小于云端下发）。只开放这 4 个**安全命令**
> （不含标定 / 擦除 / 改参数 / 改 WiFi 等危险动作）。`token` 是明文共享密钥，泄露即等于可远程控制设备；
> 远程命令**未做重放保护**（只靠 60s 过期 + 去重），正式版建议升级为每设备密钥 + 签名 + nonce。

#### 6.10.5 怎么验证（云函数 + 路由就绪后）

1. 云开发控制台部署云函数 `deviceCommand`（HTTP 访问服务路由 `/deviceCommand`，与上传同环境），
   token 与设备一致（用 `/cloud` 页写入 NVS；串口若出现 `[CMD] ! token 仍是占位值 REPLACE_ME` 就是没配）。
2. 设备上电，串口应出现（`src/cloud_command.cpp` 的初始化日志）：

   ```text
   [CMD] ===== 远程命令(云→ESP32→STM32) 初始化 =====
   [CMD] 开关: 开启：按白名单执行远程命令 (编译期 CLOUD_CMD_ENABLE=1)
   [CMD] 上传URL : https://.../deviceUpload
   [CMD] 命令URL : https://.../deviceCommand  ← 由上传URL推导(改 /cloud 的上传地址会跟着变)
   [CMD] 轮询间隔: 2000ms | 超时: 连接4000ms/TLS6s/响应4000ms | 白名单: arm,start,stop,reset
   [CMD] 队列: 命令 8 条 / 回执 8 条 | 去重表 8 条 | 本地过期兜底 60000ms
   ```

3. 小程序点 **START** → 串口应出现（标签就是 `[CMD] 远程命令`）：

   ```text
   [CMD] 远程命令入队: start (id=66f0c1a2...) → 等主 loop 执行
   [CMD] 远程命令: start (id=66f0c1a2...)
   [CMD] -> start (state=2) [云端远程]
   [CMD] ack 已发送: id=66f0c1a2... result=ok (HTTP 200, 412ms)
   ```

   - `state=2` = `STATE_RUNNING`；设备网页状态卡同步变「采集中」（`pushState` 已广播）。
   - **`DEMO_MODE=true`（默认）时不会给 STM32 发帧**——与网页按钮行为完全一致；
     要看到真正转发给 STM32，按 §三把 `DEMO_MODE` 改成 `false`。届时日志同上，STM32 会收到 `FRAME_CMD`。
4. 云端：命令集合里该条记录被 ack 标成 **done**（`result:"ok"`）。
5. 异常路径自测：

   | 操作 | 期望（串口） |
   |---|---|
   | 发不在白名单的命令（如 `reboot`） | `[CMD] 忽略未知命令: 'reboot' … → ack unknown`，设备状态不变 |
   | 同一 `id` 连点两次 | 第二次 `[CMD] 忽略重复命令 … → 补 ack ok`，**不重复执行** |
   | 断网 / 配网中 | `[CMD] 未联网(或配网中)：暂停远程命令轮询…`（只一次），联网后自动继续 |
   | token 填错 | `[CMD] ! 轮询失败(业务失败 …)，2s 后重试`（每 10 次失败才再打印，不刷屏） |
   | 想彻底关掉远程控制 | `-DCLOUD_CMD_ENABLE=0` 重新编译 → `[CMD] 开关: 关闭…`，且**没有任何 poll 请求** |

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
- [x] **并发重构（V2）**：云端 HTTPS 移到 `cloudTask`（core 0）+ 互斥量保护共享状态 + 状态消息队列（WS 单线程）+ UART2 RX 缓冲 4096（详见 §6.9）
- [x] **云端远程命令（`cloud_command.*`）**：`deviceCommand` 轮询（2s）+ 白名单 4 命令 + 同一 id 去重 + 60s 过期兜底 + ack 回执；命令 URL 由上传 URL 推导，开关 `CLOUD_CMD_ENABLE`（详见 §6.10）
- [ ] 云端 `deviceCommand` 联调：部署云函数 + 小程序命令按钮，确认 ESP32 串口出现 `[CMD] 远程命令` 且云库该条变 done
- [ ] 云端 `deviceUpload` 联调：把 `token` 换成真实值（NVS），确认 `experiments` 落库 `source:"device"`
- [ ] 手机端真实 STM32（`DEMO_MODE=false`）
- [ ] 帧协议与 STM32 联调（`FRAME_METRICS` 分项指标结构待定）
