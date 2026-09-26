"""Frozen full-curve multivariate Gunay U_curve evaluator for CanMV.

The port retains the PC geometry: all valid direct binary rotated windows over
uninterpolated image coordinates.  It streams one tensor scale and one
T/Q/U channel at a time.  The multivariate covariance trace is the sum of the
15 channel variances, so cross-channel covariance storage is not required.
"""

import math
from array import array

from ulab import numpy as np

try:
    import gunay_native
except ImportError:
    gunay_native = None

from config import ROI_HEIGHT, ROI_WIDTH, SIGMAS
from multivariate_gunay_reference import (
    AREA_SPAN,
    CREF_SQUARE,
    DREF,
    EPS,
    MREF,
    NOMINAL_AREAS,
    SHORT_SIDES,
)
from st_utils import collect, elapsed_ms, finite, mem_free, now_ms
from structure_tensor_gunay import (
    gaussian_blur,
    gaussian_workspace,
    scharr,
    scharr_workspace,
)


N = ROI_WIDTH
ASPECT = 6
CB_EPS = 1.0e-10

# This cached row-prefix implementation was numerically verified against the
# frozen K230 baseline.  It remains the safe fallback until a native kernel is
# built and independently qualified.
ROW_PREFIX = np.zeros((N, N), dtype=np.float)
for _row in range(N):
    ROW_PREFIX[_row, :_row + 1] = 1.0


def _tensor(gray, sigma):
    workspace = gaussian_workspace(sigma)
    t0 = now_ms()
    smooth = gaussian_blur(gray, sigma, workspace)
    gaussian_ms = elapsed_ms(t0)
    t1 = now_ms()
    derivative_workspace = scharr_workspace()
    ix, iy = scharr(smooth, derivative_workspace)
    scharr_ms = elapsed_ms(t1)
    del smooth
    del derivative_workspace
    t2 = now_ms()
    jxx_raw = ix * ix
    jxy_raw = ix * iy
    jyy_raw = iy * iy
    del ix
    del iy
    jxx = gaussian_blur(jxx_raw, sigma, workspace)
    del jxx_raw
    jxy = gaussian_blur(jxy_raw, sigma, workspace)
    del jxy_raw
    jyy = gaussian_blur(jyy_raw, sigma, workspace)
    del jyy_raw
    tensor_ms = elapsed_ms(t2)
    del workspace
    return jxx, jxy, jyy, {
        "gaussian_ms": gaussian_ms,
        "scharr_ms": scharr_ms,
        "structure_tensor_ms": tensor_ms,
    }


def global_main_orientation(gray):
    """The same summed-scale Structure-Tensor axis as the frozen PC path."""
    a = 0.0
    b = 0.0
    trace = 0.0
    timing = []
    for sigma in SIGMAS:
        jxx, jxy, jyy, stats = _tensor(gray, sigma)
        a += float(np.mean(jxx - jyy))
        b += float(np.mean(2.0 * jxy))
        trace += float(np.mean(jxx + jyy))
        timing_row = {"sigma": sigma}
        timing_row.update(stats)
        timing.append(timing_row)
        del jxx
        del jxy
        del jyy
        collect()
    gradient = (0.5 * math.degrees(math.atan2(b, a))) % 180.0
    line = (gradient + 90.0) % 180.0
    confidence = math.sqrt(a * a + b * b) / (trace + CB_EPS)
    return line, gradient, confidence, timing


def _geometry(short_side, shape):
    nominal = float(ASPECT * short_side * short_side)
    if shape == "SQUARE":
        side = max(1, int(round(math.sqrt(nominal))))
        return side, side, nominal
    return ASPECT * short_side, short_side, nominal


def _rotated_plan(length, width, theta_deg):
    theta = math.radians(theta_deg)
    ct = math.cos(theta)
    st = math.sin(theta)
    rx = int(math.ceil((abs(length * ct) + abs(width * st)) / 2.0)) + 1
    ry = int(math.ceil((abs(length * st) + abs(width * ct)) / 2.0)) + 1
    runs = []
    count = 0
    for dy in range(-ry, ry + 1):
        selected = []
        for dx in range(-rx, rx + 1):
            u = dx * ct + dy * st
            v = -dx * st + dy * ct
            if abs(u) <= length / 2.0 and abs(v) <= width / 2.0:
                selected.append(dx)
        if selected:
            first = selected[0]
            last = selected[-1]
            if last - first + 1 != len(selected):
                raise RuntimeError("non-contiguous rotated scanline")
            runs.append((dy, first, last))
            count += len(selected)
    if count < 2:
        raise RuntimeError("degenerate rotated window")
    return {
        "rx": rx,
        "ry": ry,
        "runs": runs,
        "actual_pixel_count": count,
        "target_area_px": int(length * width),
        "area_error_px": int(count - length * width),
        "area_error_relative": float((count - length * width) / float(length * width)),
        "valid_center_count": int((N - 2 * rx) * (N - 2 * ry)),
        "mask_width_px": int(2 * rx + 1),
        "mask_height_px": int(2 * ry + 1),
    }


def build_plans(theta_main_deg):
    plans = []
    for shape in ("SQUARE", "DIR1", "DIR2"):
        theta = theta_main_deg if shape in ("SQUARE", "DIR1") else (theta_main_deg + 90.0) % 180.0
        for point_index, short_side in enumerate(SHORT_SIDES):
            length, width, nominal = _geometry(int(short_side), shape)
            plan = _rotated_plan(length, width, theta)
            plan.update({
                "shape": shape,
                "point_index": point_index,
                "short_side_px": int(short_side),
                "nominal_area_px": nominal,
                "length_px": int(length),
                "width_px": int(width),
                "length_over_width": float(length / width),
                "window_theta_deg": float(theta),
            })
            plans.append(plan)
    return plans


def _pack_native_plans(plans):
    """Pack frozen geometry once for the single-call native ABI."""
    offsets = array("I", [0])
    spans = array("h")
    meta = array("H")
    span_count = 0
    for plan in plans:
        rx = int(plan["rx"])
        ry = int(plan["ry"])
        pixel_count = int(plan["actual_pixel_count"])
        if rx > 65535 or ry > 65535 or pixel_count > 65535:
            raise RuntimeError("native plan metadata exceeds uint16")
        meta.append(rx)
        meta.append(ry)
        meta.append(pixel_count)
        for dy, xlo, xhi in plan["runs"]:
            spans.append(int(dy))
            spans.append(int(xlo))
            spans.append(int(xhi))
            span_count += 1
        offsets.append(span_count)
    return offsets, spans, meta


def _row_prefix(source):
    output = np.zeros((N, N + 1), dtype=np.float)
    output[:, 1:] = np.dot(source, ROW_PREFIX.T)
    return output


def regional_mean_variance(prefix, plan, profile):
    """All-valid direct-mask regional means from a cached channel row prefix."""
    rx = int(plan["rx"])
    ry = int(plan["ry"])
    out_height = N - 2 * ry
    out_width = N - 2 * rx
    accumulation_start = now_ms()
    sums = np.zeros((out_height, out_width), dtype=np.float)
    for dy, xlo, xhi in plan["runs"]:
        row_start = ry + dy
        row_end = N - ry + dy
        left = rx + xlo
        right = rx + xhi + 1
        sums += (
            prefix[row_start:row_end, right:right + out_width]
            - prefix[row_start:row_end, left:left + out_width]
        )
    profile["region_accumulation_ms"] += elapsed_ms(accumulation_start)
    statistics_start = now_ms()
    means = sums / float(plan["actual_pixel_count"])
    unit_count = int(plan["valid_center_count"])
    mean = float(np.mean(means))
    std_population = float(np.std(means))
    variance_sample = (
        std_population * std_population * float(unit_count) / float(unit_count - 1)
        if unit_count > 1 else float("nan")
    )
    profile["cb_statistics_ms"] += elapsed_ms(statistics_start)
    del sums
    del means
    return mean, variance_sample


def _curve_accumulator(plans):
    return [
        {
            "sum_channel_variance": 0.0,
            "sum_channel_mean_squared": 0.0,
        }
        for _ in plans
    ]


def _accumulate_channel(source, plans, accumulator, profile):
    prefix_start = now_ms()
    prefix = _row_prefix(source)
    profile["row_prefix_ms"] += elapsed_ms(prefix_start)
    for plan_index, plan in enumerate(plans):
        mean, variance = regional_mean_variance(prefix, plan, profile)
        accumulator[plan_index]["sum_channel_variance"] += variance
        accumulator[plan_index]["sum_channel_mean_squared"] += mean * mean
    del prefix


def _accumulate_native_channels(t_channel, q_channel, u_channel, packed_plans, accumulator, profile):
    """Accumulate all T/Q/U windows in one native call for this scale."""
    native_start = now_ms()
    if hasattr(gunay_native, "region_stats_fused_span_query"):
        native_output = gunay_native.region_stats_fused_span_query(
            t_channel,
            q_channel,
            u_channel,
            packed_plans[0],
            packed_plans[1],
            packed_plans[2],
        )
        statistics = native_output[0]
        profile["prefix_build_ms"] += int(native_output[1])
        profile["span_query_ms"] += int(native_output[2])
        profile["prefix_scratch_bytes"] = int(native_output[3])
        profile["span_visits"] += int(native_output[4])
        profile["channel_traversals"] = 1
        profile["row_prefix_ms"] += int(native_output[1])
        profile["region_accumulation_ms"] += int(native_output[2])
        del native_output
    elif hasattr(gunay_native, "region_stats_row_prefix"):
        native_output = gunay_native.region_stats_row_prefix(
            t_channel,
            q_channel,
            u_channel,
            packed_plans[0],
            packed_plans[1],
            packed_plans[2],
        )
        statistics = native_output[0]
        profile["prefix_build_ms"] += int(native_output[1])
        profile["span_query_ms"] += int(native_output[2])
        profile["prefix_scratch_bytes"] = int(native_output[3])
        profile["channel_traversals"] = 3
        profile["row_prefix_ms"] += int(native_output[1])
        profile["region_accumulation_ms"] += int(native_output[2])
        del native_output
    else:
        statistics = gunay_native.region_stats(
            t_channel,
            q_channel,
            u_channel,
            packed_plans[0],
            packed_plans[1],
            packed_plans[2],
        )
    profile["native_kernel_ms"] += elapsed_ms(native_start)
    if len(statistics) != len(accumulator):
        raise RuntimeError("native region-stat result count mismatch")
    for plan_index, values in enumerate(statistics):
        if len(values) != 6:
            raise RuntimeError("native region-stat tuple width mismatch")
        sum_variance = float(values[1]) + float(values[3]) + float(values[5])
        sum_mean_squared = (
            float(values[0]) * float(values[0])
            + float(values[2]) * float(values[2])
            + float(values[4]) * float(values[4])
        )
        accumulator[plan_index]["sum_channel_variance"] += sum_variance
        accumulator[plan_index]["sum_channel_mean_squared"] += sum_mean_squared
    return statistics


def _curve_rows(plans, accumulator):
    rows = []
    by_shape = {}
    for plan_index, plan in enumerate(plans):
        data = accumulator[plan_index]
        cb = 100.0 * math.sqrt(
            data["sum_channel_variance"] / (data["sum_channel_mean_squared"] + CB_EPS)
        )
        row = dict(plan)
        row.update({
            "covariance_trace": float(data["sum_channel_variance"]),
            "mean_vector_norm_sq": float(data["sum_channel_mean_squared"]),
            "CB_MV_percent": float(cb),
            "CB_MV_finite": finite(cb),
        })
        rows.append(row)
        by_shape.setdefault(plan["shape"], []).append(row)
    for shape in by_shape:
        points = sorted(by_shape[shape], key=lambda row: row["point_index"])
        cb1 = float(points[0]["CB_MV_percent"])
        for row in points:
            row["CB_MV_A1_percent"] = cb1
            row["CB_MV_norm"] = float(row["CB_MV_percent"] / cb1)
    return rows


def _trapz(values):
    total = 0.0
    for index in range(len(values) - 1):
        delta = float(NOMINAL_AREAS[index + 1]) - float(NOMINAL_AREAS[index])
        total += 0.5 * delta * (float(values[index]) + float(values[index + 1]))
    return total


def _functional_score(rows):
    lookup = dict((row["shape"], [None] * len(SHORT_SIDES)) for row in rows)
    for row in rows:
        lookup[row["shape"]][int(row["point_index"])] = float(row["CB_MV_norm"])
    square = lookup["SQUARE"]
    dir1 = lookup["DIR1"]
    dir2 = lookup["DIR2"]
    m = [(dir1[index] + dir2[index]) / 2.0 for index in range(len(SHORT_SIDES))]
    d = [abs(dir1[index] - dir2[index]) / 2.0 for index in range(len(SHORT_SIDES))]
    e_square = [(square[index] - float(CREF_SQUARE[index])) ** 2 for index in range(len(SHORT_SIDES))]
    e_mean = [2.0 * (m[index] - float(MREF[index])) ** 2 for index in range(len(SHORT_SIDES))]
    e_difference = [2.0 * (d[index] - float(DREF[index])) ** 2 for index in range(len(SHORT_SIDES))]
    total = [e_square[index] + e_mean[index] + e_difference[index] for index in range(len(SHORT_SIDES))]
    score = math.sqrt(_trapz(total) / (3.0 * float(AREA_SPAN)))
    return score, {
        "C_SQUARE": square,
        "C_DIR1": dir1,
        "C_DIR2": dir2,
        "M": m,
        "D": d,
        "integrated_square_component": _trapz(e_square),
        "integrated_directional_mean_component_times2": _trapz(e_mean),
        "integrated_directional_difference_component_times2": _trapz(e_difference),
        "integrated_total_E": _trapz(total),
        "area_span": float(AREA_SPAN),
    }


def process(gray):
    if int(gray.shape[0]) != ROI_HEIGHT or int(gray.shape[1]) != ROI_WIDTH:
        raise RuntimeError("frozen port requires 256x256 gray")
    overall_start = now_ms()
    theta_main, theta_gradient, confidence, theta_timing = global_main_orientation(gray)
    geometry_start = now_ms()
    plans = build_plans(theta_main)
    geometry_ms = elapsed_ms(geometry_start)
    native_enabled = gunay_native is not None
    native_row_prefix = native_enabled and hasattr(gunay_native, "region_stats_row_prefix")
    native_fused = native_enabled and hasattr(gunay_native, "region_stats_fused_span_query")
    native_backend = (
        "FUSED_SPAN_QUERY_NATIVE_C" if native_fused else
        ("ROW_PREFIX_NATIVE_C" if native_row_prefix else
        ("DIRECT_SPAN_NATIVE_C" if native_enabled else "CACHED_PREFIX_MICROPYTHON")
        )
    )
    packed_plans = _pack_native_plans(plans) if native_enabled else None
    accumulator = _curve_accumulator(plans)
    per_scale = []
    min_free = mem_free()
    for sigma in SIGMAS:
        start = now_ms()
        jxx, jxy, jyy, timings = _tensor(gray, sigma)
        channel_start = now_ms()
        channel_profile = {
            "backend": native_backend,
            "row_prefix_ms": 0,
            "region_accumulation_ms": 0,
            "cb_statistics_ms": 0,
            "native_kernel_ms": 0,
            "prefix_build_ms": 0,
            "span_query_ms": 0,
            "prefix_scratch_bytes": 0,
            "span_visits": 0,
            "channel_traversals": 0,
        }
        channel_stats = {}
        native_region_stats = None
        if native_enabled:
            # Keep the peak at four 256x256 float arrays: Q is allocated once,
            # while Jyy and Jxy are reused in place as T and U.
            q_channel = jxx - jyy
            jyy += jxx
            del jxx
            t_channel = jyy
            jxy *= 2.0
            u_channel = jxy
            for name, channel in (("T", t_channel), ("Q", q_channel), ("U", u_channel)):
                channel_stats[name + "_mean"] = float(np.mean(channel))
                channel_stats[name + "_std"] = float(np.std(channel))
            native_values = _accumulate_native_channels(
                t_channel, q_channel, u_channel, packed_plans, accumulator, channel_profile
            )
            native_region_stats = []
            for plan_index, values in enumerate(native_values):
                native_region_stats.append({
                    "shape": plans[plan_index]["shape"],
                    "point_index": int(plans[plan_index]["point_index"]),
                    "T_region_mean": float(values[0]),
                    "T_region_variance_sample": float(values[1]),
                    "Q_region_mean": float(values[2]),
                    "Q_region_variance_sample": float(values[3]),
                    "U_region_mean": float(values[4]),
                    "U_region_variance_sample": float(values[5]),
                })
            del native_values
            del t_channel
            del q_channel
            del u_channel
            collect()
        else:
            for name in ("T", "Q", "U"):
                if name == "T":
                    channel = jxx + jyy
                elif name == "Q":
                    channel = jxx - jyy
                else:
                    channel = 2.0 * jxy
                channel_stats[name + "_mean"] = float(np.mean(channel))
                channel_stats[name + "_std"] = float(np.std(channel))
                _accumulate_channel(channel, plans, accumulator, channel_profile)
                del channel
                collect()
        channel_statistics_ms = elapsed_ms(channel_start)
        current_free = mem_free()
        if current_free >= 0 and (min_free < 0 or current_free < min_free):
            min_free = current_free
        scale_row = {"sigma": sigma}
        scale_row.update(timings)
        scale_row["rotated_window_statistics_ms"] = channel_statistics_ms
        scale_row["rotated_window_profile"] = channel_profile
        scale_row["T_Q_U"] = channel_stats
        if native_region_stats is not None:
            scale_row["native_region_stats"] = native_region_stats
        scale_row["min_free_bytes_scale"] = current_free
        per_scale.append(scale_row)
        if not native_enabled:
            del jxx
            del jxy
            del jyy
        collect()
    rows = _curve_rows(plans, accumulator)
    curve_start = now_ms()
    score, curve_representation = _functional_score(rows)
    curve_rms_ms = elapsed_ms(curve_start)
    return {
        "algorithm": "FROZEN_MULTIVARIATE_GUNAY_FULL_NORMALIZED_CURVE_U_CURVE",
        "U_curve": float(score),
        "finite": finite(score) and all(row["CB_MV_finite"] for row in rows),
        "theta_main_deg": float(theta_main),
        "theta_gradient_deg": float(theta_gradient),
        "orientation_confidence": float(confidence),
        "region_statistics_backend": (
            native_backend
        ),
        "curves": rows,
        "curve_representation": curve_representation,
        "theta_main_timing": theta_timing,
        "geometry_cache_build_ms": geometry_ms,
        "per_scale": per_scale,
        "curve_normalization_and_rms_ms": curve_rms_ms,
        "total_ms": elapsed_ms(overall_start),
        "min_free_bytes": min_free,
    }
