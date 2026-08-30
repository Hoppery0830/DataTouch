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
│   └── 产品端方案对比.md    # 手机端/配网方案对比
└── src\
    ├── main.cpp        # WiFi(配网/门户/mDNS) + WebSocket + UART 收发 + DEMO
    └── protocol.h      # STM32<->ESP32 二进制帧协议 + CRC8
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

- 默认波特率 `115200`，如 STM32 不同请改 `main.cpp` 里的 `STM32_BAUD`。
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

## 六、协议（src/protocol.h）

帧格式（二进制，小端）：
```
[sync0=0xAA][sync1=0x55][type][len][payload...][crc8]
```
- 字段与任务书 CSV 一致：`t, x, y, z, Fx, Fy, Fz, Mx, My, Mz, contact`（y 为第二阶段扩展轴，当前恒为 0）
- `FRAME_DATA` 载荷 = `TouchFrame`（10 个 float + 1 字节 contact）≈ 41 字节
- `FRAME_CMD`(5)：手机端 ARM/START/STOP/RESET 转发给 STM32
- `FRAME_STATUS`(2) / `FRAME_SCORE`(4)：预留

STM32 侧按同样格式组帧即可被 ESP32 解析。校验：`crc8_over()`（多项式 0x31，初值 0xFF）。

## 七、当前状态 / 待办

- [x] 工程可编译/烧录（含 DEMO 数据）
- [ ] 手机端真实 STM32（`DEMO_MODE=false`）
- [ ] 帧协议与 STM32 联调（`FRAME_METRICS` 分项指标结构待定）
