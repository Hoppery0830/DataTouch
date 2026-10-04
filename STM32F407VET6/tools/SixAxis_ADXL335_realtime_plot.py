"""六轴力传感器 + ADXL335 同窗口实时曲线。

配套 STM32 工程输出三类文本行：
F,time_ms,Fx_mN,Fy_mN,Fz_mN,Mx_mNm,My_mNm,Mz_mNm,mode,frames
A,time_ms,ax_mg,ay_mg,az_mg,vibration_rms_mg,blocks
S,time_ms,force_frames,crc_errors,uart_errors,ring_overflows,adxl_overruns,adc_errors

运行：
    py SixAxis_ADXL335_realtime_plot.py
端口不是 COM8 时：
    py SixAxis_ADXL335_realtime_plot.py --port COM5

按键：
    Z：六轴力无载时，用最近 2 秒数据软件清零
    R：恢复预置六轴零偏
    M：重新统计平均摩擦系数

关闭图窗后，会在本文件所在目录保存两份 CSV 和一张 PNG。
"""

from __future__ import annotations

import argparse
import csv
import math
import queue
import threading
import time
import traceback
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Optional, Sequence, Tuple

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import serial
from serial.tools import list_ports


DEFAULT_PORT = "COM8"
DEFAULT_BAUDRATE = 115200
DEFAULT_WINDOW_SECONDS = 20.0
DRAW_INTERVAL_MS = 50
FEATURE_WINDOW_SECONDS = 2.0
FEATURE_INTERVAL_SECONDS = 0.5
MAX_QUEUE_SIZE = 30000
TARE_WINDOW_SECONDS = 2.0
MIN_TARE_SAMPLES = 10
MIN_NORMAL_FORCE_N = 0.2
DEFAULT_HP_CUTOFF_HZ = 1.0
# The current sensor mounting uses Z as the normal direction.
# Override with --normal-axis x/y only when the mechanical mounting changes.
DEFAULT_NORMAL_AXIS = "z"

# 2026-09-09 无载静止实验的六轴平均零偏。
# 顺序：Fx/N、Fy/N、Fz/N、Mx/(N·m)、My/(N·m)、Mz/(N·m)
PRESET_ZERO_OFFSETS = (
    0.0,
    0.0,
    0.0,
    0.000725,
    0.001028,
    0.000182,
)

ForceSample = Tuple[float, int, float, float, float,
                    float, float, float, str, int]
AdxlSample = Tuple[float, int, float, float, float,
                    float, float, float, float, int]


def force_components(corrected: Sequence[float], normal_axis: str) -> Tuple[float, float]:
    """Return tangential magnitude and normal magnitude for the selected axis."""
    axis = "xyz".index(normal_axis)
    tangent = [corrected[index] for index in range(3) if index != axis]
    return math.hypot(*tangent), abs(corrected[axis])


def dominant_frequency(times: Sequence[float], values: Sequence[float]) -> float:
    """Estimate the dominant frequency of the sampled RMS envelope without numpy."""
    if len(values) < 8 or len(times) < 8:
        return 0.0
    duration = times[-1] - times[0]
    if duration <= 0.0:
        return 0.0
    sample_rate = (len(values) - 1) / duration
    mean_value = sum(values) / len(values)
    best_frequency = 0.0
    best_power = 0.0
    max_frequency = min(40.0, sample_rate / 2.0)
    frequency = max(0.5, 1.0 / duration)
    while frequency <= max_frequency:
        real = 0.0
        imag = 0.0
        for index, value in enumerate(values):
            phase = 2.0 * math.pi * frequency * index / sample_rate
            centred = value - mean_value
            real += centred * math.cos(phase)
            imag -= centred * math.sin(phase)
        power = real * real + imag * imag
        if power > best_power:
            best_frequency = frequency
            best_power = power
        frequency += max(0.5, 1.0 / duration)
    return best_frequency


def calculate_features(force_times: Sequence[float],
                       corrected_force: Sequence[Sequence[float]],
                       adxl_times: Sequence[float],
                       adxl_rms_values: Sequence[float],
                       normal_axis: str,
                       minimum_normal_force: float) -> dict:
    friction = []
    normal = []
    ratios = []
    for values in zip(*corrected_force):
        friction_value, normal_value = force_components(values, normal_axis)
        if normal_value >= minimum_normal_force:
            friction.append(friction_value)
            normal.append(normal_value)
            ratios.append(friction_value / normal_value)

    if friction:
        mean_friction = sum(friction) / len(friction)
        mean_normal = sum(normal) / len(normal)
        mean_mu = mean_friction / mean_normal if mean_normal else 0.0
        friction_sd = math.sqrt(
            sum((value - mean_friction) ** 2 for value in friction) /
            len(friction)
        )
        threshold = mean_friction + 2.0 * friction_sd
        stick_slip_events = sum(
            1 for previous, current in zip(friction, friction[1:])
            if current >= threshold and previous < threshold
        )
        force_features = {
            "normal_mean_N": mean_normal,
            "tangential_mean_N": mean_friction,
            "mu_static": max(ratios),
            "mu_dynamic": mean_mu,
            "tangential_sd_N": friction_sd,
            "tangential_peak_to_peak_N": max(friction) - min(friction),
            "stick_slip_events": stick_slip_events,
            "valid_force_samples": len(friction),
        }
    else:
        force_features = {
            "normal_mean_N": 0.0, "tangential_mean_N": 0.0,
            "mu_static": 0.0, "mu_dynamic": 0.0,
            "tangential_sd_N": 0.0, "tangential_peak_to_peak_N": 0.0,
            "stick_slip_events": 0, "valid_force_samples": 0,
        }

    rms_values = list(adxl_rms_values)
    if rms_values:
        vibration_features = {
            "vibration_rms_mg": sum(rms_values) / len(rms_values),
            "vibration_peak_mg": max(rms_values),
            "vibration_main_frequency_Hz": dominant_frequency(
                adxl_times, rms_values),
        }
    else:
        vibration_features = {
            "vibration_rms_mg": 0.0, "vibration_peak_mg": 0.0,
            "vibration_main_frequency_Hz": 0.0,
        }
    return {**force_features, **vibration_features}


def configure_chinese_font() -> None:
    plt.rcParams["font.sans-serif"] = [
        "Microsoft YaHei", "SimHei", "Arial Unicode MS", "DejaVu Sans"
    ]
    plt.rcParams["axes.unicode_minus"] = False


def available_ports_text() -> str:
    ports = [item.device for item in list_ports.comports()]
    return ", ".join(ports) if ports else "未发现串口"


def subtract_offsets(values: Sequence[float],
                     offsets: Sequence[float]) -> Tuple[float, ...]:
    return tuple(value - offset for value, offset in zip(values, offsets))


def elapsed_seconds(sensor_ms: int, first_ms: int) -> float:
    """允许首批跨传感器样本小幅乱序，同时处理32位tick回绕。

    ADC块时间可能早于先收到的力帧，不能把负几毫秒解释成49.7天。
    使用有符号模差；一次记录相对起点须小于2**31毫秒（约24.9天）。
    """
    delta = (sensor_ms - first_ms) & 0xFFFFFFFF
    if delta >= 0x80000000:
        delta -= 0x100000000
    return delta / 1000.0


def set_reasonable_ylim(axis, series, minimum_span: float,
                        nonnegative: bool = False) -> None:
    """纵轴范围与分度值均由 Matplotlib 随实时数据自动确定。"""
    values = [value for one_series in series for value in one_series]
    if not values:
        return

    if nonnegative:
        high = max(values)
        axis.set_ylim(0.0, max(high, minimum_span) * 1.12)
        return

    low = min(values)
    high = max(values)
    span = max(high - low, minimum_span)
    margin = span * 0.12
    centre = (high + low) / 2.0
    axis.set_ylim(centre - span / 2.0 - margin,
                  centre + span / 2.0 + margin)


def parse_force(parts, first_ms: Optional[int]):
    """解析带 F 前缀的新格式，也兼容以前的 9 列六轴格式。"""
    if parts and parts[0] == "F":
        fields = parts[1:]
    elif len(parts) == 9:
        fields = parts
    else:
        return None, first_ms

    if len(fields) != 9:
        return None, first_ms

    try:
        sensor_ms = int(fields[0])
        force_mn = [int(value) for value in fields[1:4]]
        torque_mnm = [int(value) for value in fields[4:7]]
        mode = fields[7]
        frame_count = int(fields[8])
    except ValueError:
        return None, first_ms

    if first_ms is None:
        first_ms = sensor_ms
    sample: ForceSample = (
        elapsed_seconds(sensor_ms, first_ms), sensor_ms,
        force_mn[0] / 1000.0,
        force_mn[1] / 1000.0,
        force_mn[2] / 1000.0,
        torque_mnm[0] / 1000.0,
        torque_mnm[1] / 1000.0,
        torque_mnm[2] / 1000.0,
        mode, frame_count,
    )
    return sample, first_ms


def parse_adxl(parts, first_ms: Optional[int]):
    if len(parts) != 7 or parts[0] != "A":
        return None, first_ms
    try:
        sensor_ms = int(parts[1])
        ax_mg = float(parts[2])
        ay_mg = float(parts[3])
        az_mg = float(parts[4])
        vibration_rms_mg = float(parts[5])
        block_count = int(parts[6])
    except ValueError:
        return None, first_ms

    if first_ms is None:
        first_ms = sensor_ms
    return (sensor_ms, ax_mg, ay_mg, az_mg,
            vibration_rms_mg, block_count), first_ms


def serial_reader(ser, data_queue, stop_event,
                  force_writer, force_file,
                  adxl_writer, adxl_file,
                  reader_state, zero_state, zero_lock,
                  hp_cutoff_hz: float, normal_axis: str) -> None:
    first_ms: Optional[int] = None
    last_flush = time.monotonic()
    hp_previous_input = None
    hp_previous_output = [0.0, 0.0, 0.0]
    hp_previous_ms = None

    try:
        while not stop_event.is_set():
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("ascii", errors="ignore").strip()
            parts = [part.strip() for part in line.split(",")]

            if parts and parts[0] == "S" and len(parts) == 8:
                try:
                    reader_state["force_frames"] = int(parts[2])
                    reader_state["crc_errors"] = int(parts[3])
                    reader_state["uart_errors"] = int(parts[4])
                    reader_state["ring_overflows"] = int(parts[5])
                    reader_state["adxl_overruns"] = int(parts[6])
                    reader_state["adc_errors"] = int(parts[7])
                except ValueError:
                    reader_state["ignored_lines"] += 1
                continue

            force_sample, first_ms_after = parse_force(parts, first_ms)
            if force_sample is not None:
                first_ms = first_ms_after
                (elapsed_s, sensor_ms, fx, fy, fz,
                 mx, my, mz, mode, frame_count) = force_sample
                raw_values = (fx, fy, fz, mx, my, mz)

                with zero_lock:
                    offsets = tuple(zero_state["offsets"])
                    corrected = subtract_offsets(raw_values, offsets)
                    friction_n, normal_force_n = force_components(
                        corrected, normal_axis)
                    if normal_force_n >= MIN_NORMAL_FORCE_N:
                        mu_instant = friction_n / normal_force_n
                        zero_state["mu_sum"] += mu_instant
                        zero_state["mu_count"] += 1
                    else:
                        mu_instant = None
                    mu_average = (
                        zero_state["mu_sum"] / zero_state["mu_count"]
                        if zero_state["mu_count"] else None
                    )
                    revision = zero_state["revision"]
                    source = zero_state["source"]

                force_writer.writerow([
                    datetime.now().isoformat(timespec="milliseconds"),
                    sensor_ms, f"{elapsed_s:.4f}",
                    *(f"{v:.6f}" for v in raw_values),
                    *(f"{v:.6f}" for v in corrected),
                    f"{friction_n:.6f}",
                    f"{normal_force_n:.6f}",
                    "" if mu_instant is None else f"{mu_instant:.6f}",
                    "" if mu_average is None else f"{mu_average:.6f}",
                    *(f"{v:.6f}" for v in offsets),
                    revision, source, mode, frame_count,
                ])
                try:
                    data_queue.put_nowait(("F", force_sample))
                except queue.Full:
                    reader_state["queue_drops"] += 1
                reader_state["force_lines"] += 1
                reader_state["mode"] = mode
                reader_state["force_frames"] = frame_count
                now = time.monotonic()
                if now - last_flush >= 1.0:
                    force_file.flush()
                    adxl_file.flush()
                    last_flush = now
                continue

            parsed_adxl, first_ms_after = parse_adxl(parts, first_ms)
            if parsed_adxl is not None:
                first_ms = first_ms_after
                (sensor_ms, ax, ay, az,
                 vibration_rms_mg, block_count) = parsed_adxl
                elapsed_s = elapsed_seconds(sensor_ms, first_ms)
                current_input = [ax, ay, az]

                if hp_previous_input is None or hp_previous_ms is None:
                    dynamic = [0.0, 0.0, 0.0]
                else:
                    dt = ((sensor_ms - hp_previous_ms) & 0xFFFFFFFF) / 1000.0
                    dt = min(max(dt, 0.001), 0.2)
                    rc = 1.0 / (2.0 * math.pi * hp_cutoff_hz)
                    alpha = rc / (rc + dt)
                    dynamic = [
                        alpha * (hp_previous_output[index]
                                 + current_input[index]
                                 - hp_previous_input[index])
                        for index in range(3)
                    ]

                hp_previous_input = current_input
                hp_previous_output = dynamic
                hp_previous_ms = sensor_ms

                sample: AdxlSample = (
                    elapsed_s, sensor_ms,
                    ax, ay, az,
                    dynamic[0], dynamic[1], dynamic[2],
                    vibration_rms_mg, block_count,
                )
                adxl_writer.writerow([
                    datetime.now().isoformat(timespec="milliseconds"),
                    sensor_ms, f"{elapsed_s:.4f}",
                    f"{ax:.3f}", f"{ay:.3f}", f"{az:.3f}",
                    f"{dynamic[0]:.3f}",
                    f"{dynamic[1]:.3f}",
                    f"{dynamic[2]:.3f}",
                    f"{vibration_rms_mg:.3f}", block_count,
                ])
                try:
                    data_queue.put_nowait(("A", sample))
                except queue.Full:
                    reader_state["queue_drops"] += 1
                reader_state["adxl_lines"] += 1
                reader_state["adxl_blocks"] = block_count
                now = time.monotonic()
                if now - last_flush >= 1.0:
                    force_file.flush()
                    adxl_file.flush()
                    last_flush = now
                continue

            reader_state["ignored_lines"] += 1

            now = time.monotonic()
            if now - last_flush >= 1.0:
                force_file.flush()
                adxl_file.flush()
                last_flush = now

    except serial.SerialException as exc:
        reader_state["error"] = str(exc)
        stop_event.set()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="六轴力传感器与ADXL335同窗口实时曲线"
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUDRATE)
    parser.add_argument("--window", type=float, default=DEFAULT_WINDOW_SECONDS)
    parser.add_argument("--hp-cutoff", type=float,
                        default=DEFAULT_HP_CUTOFF_HZ,
                        help="ADXL动态曲线高通截止频率，默认1 Hz")
    parser.add_argument("--normal-axis", choices=("x", "y", "z"),
                        default=DEFAULT_NORMAL_AXIS,
                        help="法向力轴；默认 z（当前传感器安装方向）")
    args = parser.parse_args()
    if args.window <= 0 or args.hp_cutoff <= 0:
        raise SystemExit("--window 和 --hp-cutoff 必须大于0")

    output_dir = Path(__file__).resolve().parent
    run_stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    force_csv_path = output_dir / f"integrated_force_{run_stamp}.csv"
    adxl_csv_path = output_dir / f"integrated_adxl_{run_stamp}.csv"
    feature_csv_path = output_dir / f"fabric_features_{run_stamp}.csv"
    png_path = output_dir / f"integrated_plot_{run_stamp}.png"

    print(f"正在打开 {args.port}，{args.baud} bps ...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except serial.SerialException as exc:
        print(f"无法打开串口：{exc}")
        print("请关闭SSCOM，并检查USB转TTL的端口号。")
        print(f"当前可用串口：{available_ports_text()}")
        raise SystemExit(1) from exc
    ser.reset_input_buffer()

    force_file = force_csv_path.open("w", newline="", encoding="utf-8-sig")
    adxl_file = adxl_csv_path.open("w", newline="", encoding="utf-8-sig")
    feature_file = feature_csv_path.open("w", newline="", encoding="utf-8-sig")
    force_writer = csv.writer(force_file)
    adxl_writer = csv.writer(adxl_file)
    feature_writer = csv.writer(feature_file)
    force_writer.writerow([
        "computer_time", "sensor_time_ms", "elapsed_s",
        "Fx_raw_N", "Fy_raw_N", "Fz_raw_N",
        "Mx_raw_Nm", "My_raw_Nm", "Mz_raw_Nm",
        "Fx_zero_N", "Fy_zero_N", "Fz_zero_N",
        "Mx_zero_Nm", "My_zero_Nm", "Mz_zero_Nm",
        "Ft_N", "normal_force_N", "mu_instant", "mu_average",
        "Fx_offset_N", "Fy_offset_N", "Fz_offset_N",
        "Mx_offset_Nm", "My_offset_Nm", "Mz_offset_Nm",
        "zero_revision", "zero_source", "mode", "frame_count",
    ])
    adxl_writer.writerow([
        "computer_time", "sensor_time_ms", "elapsed_s",
        "ax_mg", "ay_mg", "az_mg",
        "ax_dynamic_mg", "ay_dynamic_mg", "az_dynamic_mg",
        "vibration_rms_mg", "block_count",
    ])
    feature_writer.writerow([
        "computer_time", "elapsed_s", "normal_axis",
        "normal_mean_N", "tangential_mean_N", "mu_static", "mu_dynamic",
        "tangential_sd_N", "tangential_peak_to_peak_N",
        "stick_slip_events", "valid_force_samples",
        "vibration_rms_mg", "vibration_peak_mg",
        "vibration_main_frequency_Hz",
    ])
    force_file.flush()
    adxl_file.flush()
    feature_file.flush()

    data_queue = queue.Queue(MAX_QUEUE_SIZE)
    stop_event = threading.Event()
    reader_state = {
        "force_lines": 0, "adxl_lines": 0, "ignored_lines": 0,
        "queue_drops": 0, "mode": "WAITING", "force_frames": 0,
        "adxl_blocks": 0, "crc_errors": 0, "uart_errors": 0,
        "ring_overflows": 0, "adxl_overruns": 0, "adc_errors": 0,
        "error": "", "notice": "", "notice_until": 0.0,
    }
    zero_lock = threading.Lock()
    zero_state = {
        "offsets": list(PRESET_ZERO_OFFSETS),
        "revision": 0, "source": "预置静止零偏",
        "mu_sum": 0.0, "mu_count": 0,
    }

    reader = threading.Thread(
        target=serial_reader,
        args=(ser, data_queue, stop_event,
              force_writer, force_file, adxl_writer, adxl_file,
              reader_state, zero_state, zero_lock,
              args.hp_cutoff, args.normal_axis),
        daemon=True,
    )
    reader.start()

    configure_chinese_font()
    figure, axes = plt.subplots(4, 1, figsize=(14, 12), sharex=True)
    force_axis, friction_axis, torque_axis, adxl_axis = axes
    figure.canvas.manager.set_window_title("六轴力 + ADXL335 实时曲线")

    force_times = deque()
    force_values = [deque() for _ in range(6)]
    adxl_times = deque()
    adxl_dynamic_values = [deque() for _ in range(3)]
    adxl_rms_values = deque()

    force_lines = [
        force_axis.plot([], [], label="Fx", color="#d62728", lw=1.0)[0],
        force_axis.plot([], [], label="Fy", color="#2ca02c", lw=1.0)[0],
        force_axis.plot([], [], label="Fz", color="#1f77b4", lw=1.0)[0],
    ]
    friction_line = friction_axis.plot(
        [], [], label=r"$F_t$ (切向力)",
        color="#111111", lw=1.2
    )[0]
    torque_lines = [
        torque_axis.plot([], [], label="Mx", color="#9467bd", lw=1.0)[0],
        torque_axis.plot([], [], label="My", color="#ff7f0e", lw=1.0)[0],
        torque_axis.plot([], [], label="Mz", color="#17becf", lw=1.0)[0],
    ]
    adxl_lines = [
        adxl_axis.plot([], [], label="X动态", color="#d62728", lw=0.9)[0],
        adxl_axis.plot([], [], label="Y动态", color="#2ca02c", lw=0.9)[0],
        adxl_axis.plot([], [], label="Z动态", color="#1f77b4", lw=0.9)[0],
    ]
    adxl_rms_line = adxl_axis.plot(
        [], [], label="块内振动RMS", color="#111111", lw=1.2, alpha=0.85
    )[0]

    force_axis.set_title("三轴力（软件清零）")
    force_axis.set_ylabel("力 / N")
    friction_axis.set_title("实时摩擦力与平均摩擦系数")
    friction_axis.set_ylabel("Ft / N")
    torque_axis.set_title("三轴力矩（软件清零）")
    torque_axis.set_ylabel("力矩 / N·m")
    adxl_axis.set_title(
        f"ADXL335动态加速度（{args.hp_cutoff:g} Hz高通，无需固定静止基线）"
    )
    adxl_axis.set_ylabel("加速度 / mg")
    adxl_axis.set_xlabel("时间 / s")
    for axis in axes:
        axis.grid(True, alpha=0.3)
    force_axis.legend(loc="upper right", ncol=3)
    friction_axis.legend(loc="upper right")
    torque_axis.legend(loc="upper right", ncol=3)
    adxl_axis.legend(loc="upper right", ncol=4)

    mu_text = friction_axis.text(
        0.01, 0.92,
        f"平均 μ：等待 |F{args.normal_axis.upper()}|≥{MIN_NORMAL_FORCE_N:.1f} N",
        transform=friction_axis.transAxes, ha="left", va="top",
        bbox={"boxstyle": "round", "facecolor": "white", "alpha": 0.85},
    )
    feature_text = friction_axis.text(
        0.01, 0.72, "织物特征：等待数据",
        transform=friction_axis.transAxes, ha="left", va="top",
        fontsize=9,
        bbox={"boxstyle": "round", "facecolor": "white", "alpha": 0.75},
    )
    status_text = figure.suptitle(
        f"等待 {args.port} 数据 | PC串口 {args.baud} bps"
    )
    figure.tight_layout(rect=(0, 0, 1, 0.965))
    last_feature_write = [float("-inf")]

    def set_notice(message: str) -> None:
        reader_state["notice"] = message
        reader_state["notice_until"] = time.monotonic() + 4.0
        print(message)

    def remove_old_data(newest: float) -> None:
        cutoff = newest - args.window
        while force_times and force_times[0] < cutoff:
            force_times.popleft()
            for values in force_values:
                values.popleft()
        while adxl_times and adxl_times[0] < cutoff:
            adxl_times.popleft()
            for values in adxl_dynamic_values:
                values.popleft()
            adxl_rms_values.popleft()

    def update_plot(_frame_number):
        newest_time = None
        while True:
            try:
                kind, sample = data_queue.get_nowait()
            except queue.Empty:
                break
            if kind == "F":
                force_times.append(sample[0])
                for index in range(6):
                    force_values[index].append(sample[index + 2])
                newest_time = sample[0]
            else:
                adxl_times.append(sample[0])
                adxl_dynamic_values[0].append(sample[5])
                adxl_dynamic_values[1].append(sample[6])
                adxl_dynamic_values[2].append(sample[7])
                adxl_rms_values.append(sample[8])
                newest_time = sample[0]

        if newest_time is not None:
            remove_old_data(newest_time)

        with zero_lock:
            offsets = tuple(zero_state["offsets"])
            mu_count = zero_state["mu_count"]
            mu_average = (zero_state["mu_sum"] / mu_count
                          if mu_count else None)

        feature_corrected_force = None
        if force_times:
            force_time_list = list(force_times)
            corrected_force = [
                [value - offsets[index] for value in force_values[index]]
                for index in range(3)
            ]
            feature_corrected_force = corrected_force
            corrected_torque = [
                [value - offsets[index + 3]
                 for value in force_values[index + 3]]
                for index in range(3)
            ]
            friction = []
            normal = []
            for values in zip(*corrected_force):
                friction_value, normal_value = force_components(
                    values, args.normal_axis)
                friction.append(friction_value)
                normal.append(normal_value)
            for line, values in zip(force_lines, corrected_force):
                line.set_data(force_time_list, values)
            friction_line.set_data(force_time_list, friction)
            for line, values in zip(torque_lines, corrected_torque):
                line.set_data(force_time_list, values)
            set_reasonable_ylim(force_axis, corrected_force, 0.02)
            set_reasonable_ylim(friction_axis, [friction], 0.02, True)
            set_reasonable_ylim(torque_axis, corrected_torque, 0.002)

        if adxl_times:
            adxl_time_list = list(adxl_times)
            dynamic_lists = [list(values) for values in adxl_dynamic_values]
            rms_list = list(adxl_rms_values)
            for line, values in zip(adxl_lines, dynamic_lists):
                line.set_data(adxl_time_list, values)
            adxl_rms_line.set_data(adxl_time_list, rms_list)
            set_reasonable_ylim(adxl_axis,
                                dynamic_lists + [rms_list],
                                minimum_span=20.0)

        features = None
        if force_times or adxl_times:
            latest = max(force_times[-1] if force_times else 0.0,
                         adxl_times[-1] if adxl_times else 0.0)
            feature_cutoff = latest - FEATURE_WINDOW_SECONDS
            if feature_corrected_force is not None:
                force_start = next(
                    (index for index, value in enumerate(force_times)
                     if value >= feature_cutoff), len(force_times)
                )
                feature_force_times = list(force_times)[force_start:]
                feature_force = [values[force_start:]
                                 for values in feature_corrected_force]
            else:
                feature_force_times = []
                feature_force = [[], [], []]
            adxl_start = next(
                (index for index, value in enumerate(adxl_times)
                 if value >= feature_cutoff), len(adxl_times)
            )
            features = calculate_features(
                feature_force_times, feature_force,
                list(adxl_times)[adxl_start:],
                list(adxl_rms_values)[adxl_start:],
                args.normal_axis, MIN_NORMAL_FORCE_N,
            )
            if latest - last_feature_write[0] >= FEATURE_INTERVAL_SECONDS:
                feature_writer.writerow([
                    datetime.now().isoformat(timespec="milliseconds"),
                    f"{latest:.4f}", args.normal_axis.upper(),
                    f"{features['normal_mean_N']:.6f}",
                    f"{features['tangential_mean_N']:.6f}",
                    f"{features['mu_static']:.6f}",
                    f"{features['mu_dynamic']:.6f}",
                    f"{features['tangential_sd_N']:.6f}",
                    f"{features['tangential_peak_to_peak_N']:.6f}",
                    features["stick_slip_events"],
                    features["valid_force_samples"],
                    f"{features['vibration_rms_mg']:.3f}",
                    f"{features['vibration_peak_mg']:.3f}",
                    f"{features['vibration_main_frequency_Hz']:.3f}",
                ])
                feature_file.flush()
                last_feature_write[0] = latest

        all_latest = []
        if force_times:
            all_latest.append(force_times[-1])
        if adxl_times:
            all_latest.append(adxl_times[-1])
        if all_latest:
            right = max(all_latest)
            left = max(0.0, right - args.window)
            for axis in axes:
                axis.set_xlim(left, max(args.window, right))

        if mu_average is None:
            mu_text.set_text(
                f"平均 μ：等待 |F{args.normal_axis.upper()}|≥{MIN_NORMAL_FORCE_N:.1f} N"
            )
        else:
            mu_text.set_text(f"平均摩擦系数 μ={mu_average:.3f}  有效点={mu_count}")

        if features is not None:
            feature_text.set_text(
                f"{FEATURE_WINDOW_SECONDS:g}s窗口 | N={features['normal_mean_N']:.3f} N | "
                f"Ft={features['tangential_mean_N']:.3f} N\n"
                f"μs={features['mu_static']:.3f} | μk={features['mu_dynamic']:.3f} | "
                f"Ft标准差={features['tangential_sd_N']:.3f} N | "
                f"峰峰={features['tangential_peak_to_peak_N']:.3f} N\n"
                f"粘滑={features['stick_slip_events']}次 | "
                f"振动RMS={features['vibration_rms_mg']:.1f} mg | "
                f"振动峰值={features['vibration_peak_mg']:.1f} mg | "
                f"包络主频={features['vibration_main_frequency_Hz']:.1f} Hz"
            )

        if reader_state["error"]:
            status_text.set_text(f"串口错误：{reader_state['error']}")
        elif (reader_state["notice"] and
              time.monotonic() < reader_state["notice_until"]):
            status_text.set_text(reader_state["notice"])
        else:
            status_text.set_text(
                f"六轴力 + ADXL335 | {args.port} {args.baud} bps | "
                f"力帧 {reader_state['force_frames']} | "
                f"ADXL块 {reader_state['adxl_blocks']} | "
                f"CRC错 {reader_state['crc_errors']} | "
                f"ADC溢出 {reader_state['adxl_overruns']}"
            )

        return (*force_lines, friction_line, *torque_lines,
                *adxl_lines, adxl_rms_line, mu_text, feature_text, status_text)

    def key_press(event) -> None:
        key = (event.key or "").lower()
        if key == "z":
            if not force_times:
                set_notice("尚未收到六轴力数据，不能清零。")
                return
            cutoff = force_times[-1] - TARE_WINDOW_SECONDS
            indices = [i for i, value in enumerate(force_times)
                       if value >= cutoff]
            if len(indices) < MIN_TARE_SAMPLES:
                set_notice(f"清零失败：最近数据只有 {len(indices)} 点。")
                return
            new_offsets = []
            for series in force_values:
                selected = [series[index] for index in indices]
                new_offsets.append(sum(selected) / len(selected))
            with zero_lock:
                zero_state["offsets"] = new_offsets
                zero_state["revision"] += 1
                zero_state["source"] = f"手动#{zero_state['revision']}"
                zero_state["mu_sum"] = 0.0
                zero_state["mu_count"] = 0
            set_notice(f"六轴清零成功：采用最近 {len(indices)} 点平均值。")
        elif key == "r":
            with zero_lock:
                zero_state["offsets"] = list(PRESET_ZERO_OFFSETS)
                zero_state["revision"] += 1
                zero_state["source"] = "预置静止零偏"
                zero_state["mu_sum"] = 0.0
                zero_state["mu_count"] = 0
            set_notice("已恢复六轴预置零偏。")
        elif key == "m":
            with zero_lock:
                zero_state["mu_sum"] = 0.0
                zero_state["mu_count"] = 0
            set_notice("平均摩擦系数已重新开始统计。")

    figure.canvas.mpl_connect("close_event", lambda _event: stop_event.set())
    figure.canvas.mpl_connect("key_press_event", key_press)
    animation = FuncAnimation(
        figure, update_plot, interval=DRAW_INTERVAL_MS,
        blit=False, cache_frame_data=False,
    )
    _ = animation

    print("同窗口实时曲线已启动。")
    print("Z=六轴软件清零，R=恢复预置零偏，M=重置平均摩擦系数。")
    print("ADXL曲线采用连续高通，不要求先静止固定秒数。")
    print(f"六轴CSV：{force_csv_path}")
    print(f"ADXL CSV：{adxl_csv_path}")
    print(f"织物特征CSV：{feature_csv_path}")

    try:
        plt.show()
    finally:
        stop_event.set()
        reader.join(timeout=1.0)
        force_file.flush()
        adxl_file.flush()
        feature_file.flush()
        force_file.close()
        adxl_file.close()
        feature_file.close()
        if ser.is_open:
            ser.close()
        try:
            figure.savefig(png_path, dpi=180, bbox_inches="tight")
            print(f"图片已保存：{png_path}")
        except Exception as exc:
            print(f"PNG保存失败：{exc}")
        print(f"六轴有效行：{reader_state['force_lines']}")
        print(f"ADXL有效行：{reader_state['adxl_lines']}")
        print(f"忽略行：{reader_state['ignored_lines']}")
        print(f"绘图队列丢弃：{reader_state['queue_drops']}")


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:
        traceback.print_exc()
        input("程序发生错误。按回车键关闭窗口……")
