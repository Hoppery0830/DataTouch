# ESP32 命令通知服务

小程序仍调用 sendCommand。命令入库后，该函数用 HTTPS POST /notify 通知服务。服务按设备身份在现有 WebSocket 连接发送文本 wake。ESP32 在云通信任务下次可用时调用原 deviceCommand/poll，复用命令过期、去重、回执和设备状态逻辑。设备忙于其他 HTTPS 请求或正在失败退避时仍可能延迟。实验结果和小程序页面刷新周期沿用原流程。

## 部署到 cloud1

1. 将代码提交到部署使用的仓库分支。云托管创建服务时选择项目目录 `cloudrun/command-wake`，用目录里的 Dockerfile 构建，容器监听 8080，健康检查 GET /health。
2. 第一版为一台设备、一个实例设计。将最大实例数设为 **1**，避免 HTTP 通知路由到另一实例。联调期间如选择最小实例数 1，请核对常驻费用。开放可从设备访问的 HTTPS/WSS 入口，网关负责 TLS。
3. 在服务环境变量设置 `DEVICE_ID=esp32-dev01`、`DEVICE_WS_KEY`、`WAKE_NOTIFY_KEY`。两个密钥分别用 `node -p "require('crypto').randomBytes(32).toString('hex')"` 生成，设为不同值。不要提交密钥。
4. 将 `WAKE_NOTIFY_URL=https://实际服务域名/notify` 和相同的 `WAKE_NOTIFY_KEY` 配到云函数 **sendCommand**，重新上传整个函数目录（含 wake.js）。不要把通知密钥放入小程序或设备。
5. ESP32：将 `ESP32/include/wake_config.example.h` 复制成同目录的 `wake_config.local.h`（已忽略），填写 `WAKE_HOST`（仅域名），`WAKE_DEVICE_KEY`（服务的 DEVICE_WS_KEY），`WAKE_CA_PEM`（服务证书链对应的可信根 CA，使用 C++ 原始字符串 `R"PEM(...证书 PEM 全文...)PEM"`）。设备的上云 deviceId 必须与 DEVICE_ID 相同。编译烧录 ESP32，联网、同步时间后串口应显示 `[WAKE] WSS connected`。未配置时自动沿用原轮询。
6. 验证：点一次选模式，核对 commands 中该命令的 createdAt、sentAt 和 STM32 回执。设备保持 Wi-Fi 连接时暂时停掉推送服务，命令应能通过原轮询送达。恢复服务后检查串口重连。STM32 不用重烧。

本地服务测试：在此目录运行 `npm ci`、`npm test`。204 仅表示通知已发送，动作结果仍以 STM32 回执为准。
