# K230 织物纹理测评

本目录包含当前部署程序、冻结的 U_curve 算法、CanMV 原生模块及状态管理修改、部署脚本和主机回归测试。

## V2 区域纹理均匀度（PC）

[PC 版算法入口](pc_texture_uniformity_v2/regional_texture_uniformity.py)输出区域纹理均匀度评分 `S_uniformity`。它按标定权重合成五个尺度的结构差异及区域明暗差异。评分越高表示在相同成像条件下区域越一致。标定基于九张织物图的人工评分，适用域尚未用独立织物验证。评分采用经验标定单位。

在仓库根目录运行以下命令。输入应使用短边至少 896 像素且保有足够真实图像边界的原图。

```powershell
python -m pip install "numpy>=2" opencv-python
python K230/pc_texture_uniformity_v2/regional_texture_uniformity.py "path/to/fabric.jpg"
```

程序输出 JSON，含评分、各尺度结构差异、区域明暗差异和特征是否落在标定范围内。具备 CUDA 版 PyTorch 时可添加 `--gpu`。V2 在 PC 运行，下面的板端流程使用 `U_curve`。

## 运行与保存

- 默认生产模式由 F4 令牌请求触发一次测评（REMOTE_CAPTURE_ENABLED=True）；GPIO21 长按 3 秒仍可切换调试模式。模式保存在 `/sdcard/datatouch_mode.txt`。
- 生产测评顺序：F4 三轴停稳 → PD14 开灯等待 2 秒 → 令牌请求拍摄 → F4 关灯放行 → 计算并保存 U_curve → 发送令牌和指标，等待 F4 确认（最多 5 次）。完整链路见 [视觉实验结果联调](../docs/视觉实验结果联调_2026-11-14.md)。
- 相机使用 CSI1、1920×1080 RGB565，ROI 为 `(240, 135, 1440, 810)`，灰度测评输入为 256×256，焦点位置为 275。
- 日志为 `/sdcard/data_touch_results/u_curve.txt`，每行 `measurement_index,U_curve`，无表头，无时间戳。生产与调试测评共用连续序号。
- 调试入口 `debug_main.py` 支持预览、`capture_once()`、`evaluate_once()` 和 `capture_evaluate_save()`。测评照片采用与日志一致的 `measure_<index>.jpg` 编号。`capture_evaluate_save()` 在照片和日志保存完成后向F4发送评分；单独预览、拍照、试算不发送评分。
- IDE 连接和断开只改变连接状态；明确的停止或运行脚本操作才接管生产程序。

日志迁移、恢复、失败回滚和照片保存规则见 [DATA_SAVING.md](DATA_SAVING.md)。

## 硬件与固件

KEY 为 GPIO21，低电平有效，30 ms 消抖。K230 UART2 GPIO11/TX → STM32F407VET6 PA10/USART1 RX，GPIO12/RX ← F4 PA9/USART1 TX，共地，115200 8N1、3.3 V TTL。帧格式见 [stm32_uart_protocol.md](stm32_uart_protocol.md)。

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

主机回归覆盖按键、UART、日志迁移和失败恢复、照片与评分对应关系、生产与调试模式，以及固件和 IDE 状态管理。`host_tools/` 保留回归使用的定制 IDE 代码，见 [host_tools/README.md](host_tools/README.md)。主机检查不替代开发板实测。
