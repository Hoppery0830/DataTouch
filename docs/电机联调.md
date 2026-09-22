# 小程序—ESP32—STM32 电机联调

## 当前行为

三端使用同一模式编号：0 按压、1 揉搓、2 滑动。STM32 上电不自动运动。小程序操作顺序：

1. 设备状态显示在线；选择模式，等待“STM32 已确认模式”。
2. 点 ARM，等待“准备完成，可 START”。程序等待参与轴在线，逐轴使能；只有原点无效的轴才将当前位置设零，有效原点保持，不重新寻零。
3. 点 START，执行一次所选动作，完成后显示“动作完成”。再次执行需 ARM → START。
4. STOP 可随时下发，优先于尚未执行的命令；不自动退回原点。RESET 先停止，再清理运行会话，保留模式与已有有效坐标，不复位芯片、不清除硬件故障。更换模式后必须重新 ARM。

当前实机参数保留：Z 压入 3mm、1mm/s、停留 300ms；揉搓 Yaw ±10° 往复 3 轮；滑动 X +10mm，先退 Z 再退 X。参数在 STM32 `source/task/Motor_Task/motor_config.c`。设零不是机械寻零。

## 接线与固件

| ESP32-S3 | STM32F407 |
|---|---|
| GPIO17 TX | PB11 / USART3_RX |
| GPIO18 RX | PB10 / USART3_TX |
| GND | GND |

两端 115200、8N1。STM32 USART3 使用字节接收中断、环形缓冲和中断发送，独立于三路电机 DMA。RemoteTask 为静态任务（栈 3072 字节，Normal），MotorTask 仍独占电机对象。

- STM32：`cmake --build --preset Debug -j 6`；Ozone 下载 `build/Debug/DataTouchCode.elf`，继续运行。
- ESP32：PlatformIO `pio run`，再通过原上传流程烧录整个工程；不要只把应用 bin 写到地址 0。生成物 `.pio/build/esp32-s3-devkitc-1/firmware.bin`。
- ESP32 `DEMO_MODE=false`，STM32 `APP_MOTOR_AUTOSTART=0`、`APP_MOTOR_SELFTEST=0`。
- 两端必须一起更新。旧 ESP32 的 0x05/0x06 无序号控制帧不会驱动新 STM32，避免无确认命令误动作。

## 小程序与云端部署

本次只修改本地代码，未上传云函数、发布小程序或烧录硬件。

1. 在原云环境创建 `device_status` 集合，保留现有 `commands` 集合。`device_status` 通过云函数读写即可，不需要客户端写权限；设备号使用英文字母、数字、下划线或短横线（例如 esp32-dev01）。
2. 微信开发者工具中分别对 `cloudfunctions/sendCommand`、`cloudfunctions/deviceCommand` 执行“上传并部署：云端安装依赖”。保留已有 deviceCommand HTTP 访问服务绑定。
3. 若查询提示缺少索引，按云数据库返回提示为 commands 添加对应索引。查询包括 deviceId/state/expiresAt 与 createdAt 排序；STOP 优先查询额外过滤 cmd，超时查询过滤 sentAt。
4. 重新编译/预览小程序，打开远程控制页。ESP32 云配置里的 deviceId、token、URL 和原云环境保持一致。
5. 页面每 2 秒更新命令列表和设备快照。STM32 状态经 ESP32 轮询上报；超过 15 秒没有云端新快照显示离线。网络延迟时不要根据按钮点击推断电机已经动作。

`device_status` 创建遗漏会使页面拿不到状态，但云函数仍允许 STOP 通道工作。状态示例包含 alive、state、mode、stage、online/fault 轴掩码及 updatedAt，不包含伪造传感器数据。

## 协议

`AA 55 type len payload crc8`，CRC-8 多项式 0x31、初值 0xFF，覆盖同步头至 payload 全部字节，多字节整数为小端。STM32 与 ESP32 各有相同的 remote_protocol.h。

| type | payload |
|---|---|
| 0x10 请求 | seq:u32, op:u8, mode:u8，共 6 字节 |
| 0x11 回复 | seq:u32, result:u8，共 5 字节 |
| 0x12 状态 | state, mode, stage, online_mask, fault_mask，共 5 字节 |
| 0x13 心跳 | 空 payload |

op：0 SELECT、1 ARM、2 START、3 STOP、4 RESET。mode：0/1/2；状态回传中 255 表示尚未选模式。
result：0 已接受、1 已完成、2 忙、3 参数非法、4 未准备、5 故障、6 已取消。
state：0 空闲、1 已准备、2 运行、3 完成、4 故障、5 准备中、6 停止中。
轴掩码：bit0=X、bit1=Z、bit2=Yaw。

ESP32 每 500ms 发送心跳；STM32 每 250ms 回状态。STM32 准备/待命/运行期间超过 2 秒收不到有效心跳或请求，停止动作并进入错误流程。串口确认丢失时 ESP32 只重发同一序号，STM32 缓存最近 16 条请求与结果，重复 START 不会重新执行；相同序号但不同参数被拒绝。此缓存不是跨断电持久存储，ESP32 启动序号取随机值。云端已下发的命令不自动重投，失去回执最终标“结果未知”，不以重放动作恢复。

## 回执与停止

命令状态：pending 待发送 → sent 已下发 → accepted STM32 已接受 → done 已完成。busy/fault 等显示 failed；中止显示 cancelled；确认超时显示 unknown。迟到的 accepted 不覆盖终态。

ARM 的完成表示参与轴准备成功，START 的完成表示整套动作完成，STOP 的完成表示参与轴停止事务已获成功确认。串口失联时不能据此保证物理停机，会报告错误。STOP 会取消旧普通命令和当前流程；云端优先取 STOP/RESET，取消其之前尚未取走的命令，ESP32/STM32 同样优先处理。

云控制存在轮询和 HTTPS 延迟，不是硬实时急停。ESP32 与 STM32 之间的心跳只检测串口链路，不等价于云网络连通；现场紧急停止仍应使用硬件措施。短时云断网后未过期的 pending 命令仍可能执行。

当前真实模式关闭 DONE 后实验上传与手动评分上传，避免把未采集的零值/旧值当成实验结果；采集、评分链路留待后续接入。

## 排查与验证

- 页面离线：检查 ESP32 串口日志、共地与 TX/RX 交叉接线、两端固件、115200、device_status 集合及云函数部署。
- ARM 失败：查看 STM32 `published.remote`、`published.fault`、`published.online`；电机 UART 波特率/地址继续使用原配置。故障锁存不会被 RESET 自动清除，排除硬件问题后用已有电机故障恢复接口或重新上电。
- 状态 running 卡住：看 action_stage、path_status、missed_ticks。断点停住期间可能触发超时，不宜单步跨越运动时限。
- 串口日志 `[STM32] seq=... op=... result=...` 区分 accepted 和 ok；小程序云历史同时展示失败和未知结果。

自动验证：STM32 原生 HAL 桩测试覆盖三种模式完整流程、CRC/半帧、准备新反馈、保留原点、重复请求、STOP 和断联；ESP32 `python test/remote/run.py` 验证相同序号重试、旧 ACK、STOP 插队与超时；小程序 `node tests/remote_control.test.js` / `node tests/control_page.test.js` 验证云端与页面行为。两端固件构建通过不等于实际云环境、接线和机械联调完成。
