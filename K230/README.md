# K230 织物纹理测评

本目录包含当前部署程序、冻结的 U_curve 算法、CanMV 原生模块及状态管理修改、部署脚本和主机回归测试。

## 运行与保存

- 默认进入生产模式；GPIO21 短按并释放后触发一次测评，长按 3 秒切换到调试模式并重启。模式保存在 `/sdcard/datatouch_mode.txt`。
- 生产测评顺序：STM32 补光开启并 ACK → 等待 2 秒 → 拍摄 → 保持补光至 5 秒 → 关闭补光 → 计算 U_curve → 保存评分 → 向 STM32 发送 float32 评分并等待 ACK。
- 相机使用 CSI1、1920×1080 RGB565，ROI 为 `(240, 135, 1440, 810)`，灰度测评输入为 256×256，焦点位置为 275。
- 日志为 `/sdcard/data_touch_results/u_curve.txt`，每行 `measurement_index,U_curve`，无表头，无时间戳。生产与调试测评共用连续序号。
- 调试入口 `debug_main.py` 支持预览、`capture_once()`、`evaluate_once()` 和 `capture_evaluate_save()`。测评照片采用与日志一致的 `measure_<index>.jpg` 编号。
- IDE 连接和断开只改变连接状态；明确的停止或运行脚本操作才接管生产程序。

日志迁移、恢复、失败回滚和照片保存规则见 [DATA_SAVING.md](DATA_SAVING.md)。

## 硬件与固件

KEY 为 GPIO21，低电平有效，30 ms 消抖。K230 UART2 GPIO11/TX → STM32F1 PA3/USART2 RX，GPIO12/RX ← PA2/USART2 TX，共地，115200 8N1、3.3 V TTL。帧格式见 [stm32_uart_protocol.md](stm32_uart_protocol.md)。

当前程序依赖自定义 CanMV v1.8 固件：内置 `gunay_native`，并提供 `machine.ide_connected()` 及新的 IDE 状态管理。原生模块源文件、三个修改后的 CanMV port 文件及补丁在 `native/`；重建方法见 [native/build_environment.md](native/build_environment.md)。仅上传 Python 文件不能补齐固件接口。

## 部署

首次部署先将 `texture_structure_tensor_v1/` 放到设备 `/sdcard/texture_structure_tensor_v1/`，并安装上述自定义固件。随后在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\K230\upload_k230_final_deploy.ps1 -Port COM14
powershell -ExecutionPolicy Bypass -File .\K230\start_k230_final_deploy.ps1 -Port COM14 -TimeoutSeconds 30
```

脚本以自身目录作为默认源目录。普通代码上传保留已有日志；显式恢复日志时才指定 `-ResultBackupPath`。可在 IDE 中运行 `set_debug_mode.py` 或 `set_production_mode.py` 切换持久模式。

## 主机验证

在仓库根目录执行：

```powershell
python -B -m unittest discover -s K230/tests -p "test_*.py" -v
```

61 项回归覆盖按键、UART、日志迁移和失败恢复、照片与评分对应关系、生产与调试模式，以及固件和 IDE 状态管理。`host_tools/` 保留回归使用的定制 IDE 代码，见 [host_tools/README.md](host_tools/README.md)。主机检查不替代开发板实测。
