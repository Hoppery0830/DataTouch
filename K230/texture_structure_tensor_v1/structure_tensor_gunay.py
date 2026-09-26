"""Frozen undecimated Gaussian + Scharr Structure-Tensor dual metric.

One sigma is completed and discarded before the next.  This implementation
does not contain a multiscale tensor, scalar fusion, adaptive parameter, or
fast-path that changes the frozen mathematics.
"""
import math

from ulab import numpy as np

from config import EPS, GAUSSIAN_1D, ROI_HEIGHT, ROI_WIDTH, SCHARR_X, SCHARR_Y, SIGMAS, WINDOWS
from st_utils import collect, elapsed_ms, mem_free, now_ms, reflect101, summary


GAUSSIAN_KERNELS = dict((float(sigma), np.array(GAUSSIAN_1D[float(sigma)], dtype=np.float)) for sigma in SIGMAS)
SCHARR_SMOOTH_1D = np.array((3.0, 10.0, 3.0), dtype=np.float)
# ``np.convolve`` reverses its kernel.  This is therefore the convolution
# representation of the frozen correlation derivative (-1, 0, +1).
SCHARR_DIFF_CONV_1D = np.array((1.0, 0.0, -1.0), dtype=np.float)
# A fixed lower-triangular prefix matrix implements the same 2-D inclusive
# cumulative sum as the former running scalar loop: L @ A @ L.T.  It is built
# once at import, then reused for all tensor components and scales.
INTEGRAL_PREFIX = np.zeros((ROI_HEIGHT, ROI_WIDTH), dtype=np.float)
for _row in range(ROI_HEIGHT):
    INTEGRAL_PREFIX[_row, :_row + 1] = 1.0


def gaussian_workspace(sigma):
    """Allocate reusable per-scale buffers, never a multiscale tensor."""
    radius = len(GAUSSIAN_KERNELS[float(sigma)]) // 2
    return {
        "radius": radius,
        # ROI is square, so this one pad supports the horizontal pass and the
        # transposed vertical pass.  It is reused for all four Gaussians at a
        # given sigma.
        "pad": np.zeros((ROI_HEIGHT, ROI_WIDTH + 2 * radius), dtype=np.float),
        "temp": np.zeros((ROI_HEIGHT, ROI_WIDTH), dtype=np.float),
    }


def _convolve_rows_reflect101(source, kernel, pad, target):
    """Native ulab 1-D convolution for every row with cv2 REFLECT_101 edges.

    The only Python loop is over full rows; the inner convolution is C-level
    ``ulab.numpy.convolve``.  This replaces the previous pixel-by-pixel,
    tap-by-tap implementation without changing the Gaussian kernel, border
    rule, dtype, or output geometry.
    """
    height, width = source.shape
    radius = len(kernel) // 2
    pad[:, radius:radius + width] = source
    pad[:, :radius] = np.flip(source[:, 1:radius + 1], axis=1)
    pad[:, radius + width:radius + width + radius] = np.flip(source[:, width - radius - 1:width - 1], axis=1)
    start = 2 * radius
    for y in range(height):
        full = np.convolve(pad[y, :], kernel)
        target[y, :] = full[start:start + width]


def gaussian_blur(source, sigma, workspace):
    kernel = GAUSSIAN_KERNELS[float(sigma)]
    temp = workspace["temp"]
    pad = workspace["pad"]
    _convolve_rows_reflect101(source, kernel, pad, temp)
    output = np.zeros((ROI_HEIGHT, ROI_WIDTH), dtype=np.float)
    _convolve_rows_reflect101(temp.T, kernel, pad, output.T)
    return output


def scharr_workspace():
    return {"pad": np.zeros((ROI_HEIGHT, ROI_WIDTH + 2), dtype=np.float),
            "temp": np.zeros((ROI_HEIGHT, ROI_WIDTH), dtype=np.float)}


def separable_filter(source, column_kernel, row_kernel, workspace):
    """Exact 3x3/1-D separable filter with REFLECT_101 boundaries."""
    temp = workspace["temp"]
    pad = workspace["pad"]
    _convolve_rows_reflect101(source.T, column_kernel, pad, temp.T)
    output = np.zeros((ROI_HEIGHT, ROI_WIDTH), dtype=np.float)
    _convolve_rows_reflect101(temp, row_kernel, pad, output)
    return output


def scharr(source, workspace):
    # [-3,0,3;-10,0,10;-3,0,3] = [3,10,3]^T * [-1,0,1]
    # The y kernel is its transpose.  No derivative scale factor is added.
    ix = separable_filter(source, SCHARR_SMOOTH_1D, SCHARR_DIFF_CONV_1D, workspace)
    iy = separable_filter(source, SCHARR_DIFF_CONV_1D, SCHARR_SMOOTH_1D, workspace)
    return ix, iy


def integral(source):
    # Keep the zero first row/column contract used by rect_sum unchanged.
    result = np.zeros((ROI_HEIGHT + 1, ROI_WIDTH + 1), dtype=np.float)
    result[1:, 1:] = np.dot(INTEGRAL_PREFIX, np.dot(source, INTEGRAL_PREFIX.T))
    return result


def rect_sum(ii, y, x, side):
    y2 = y + side
    x2 = x + side
    return float(ii[y2, x2]) - float(ii[y, x2]) - float(ii[y2, x]) + float(ii[y, x])


def curve_for_window(iixx, iixy, iiyy, window, margin):
    stride = window // 2
    limit = ROI_WIDTH - window - margin + 1
    count = 0
    e_sum = e_sum2 = 0.0
    oc_sum = oc_sum2 = 0.0
    os_sum = os_sum2 = 0.0
    first = None
    y = margin
    while y < limit:
        x = margin
        while x < limit:
            sxx = rect_sum(iixx, y, x, window)
            sxy = rect_sum(iixy, y, x, window)
            syy = rect_sum(iiyy, y, x, window)
            trace = sxx + syy
            energy = trace / float(window * window)
            oc = (sxx - syy) / (trace + EPS)
            os = (2.0 * sxy) / (trace + EPS)
            e_sum += energy; e_sum2 += energy * energy
            oc_sum += oc; oc_sum2 += oc * oc
            os_sum += os; os_sum2 += os * os
            if first is None:
                first = {"y": y, "x": x, "Sxx": sxx, "Sxy": sxy, "Syy": syy}
            count += 1
            x += stride
        y += stride
    if count <= 0:
        raise RuntimeError("no valid windows for A=%d margin=%d" % (window, margin))
    inv = 1.0 / float(count)
    e_mean = e_sum * inv
    oc_mean = oc_sum * inv
    os_mean = os_sum * inv
    e_var = max(e_sum2 * inv - e_mean * e_mean, 0.0)
    oc_var = max(oc_sum2 * inv - oc_mean * oc_mean, 0.0)
    os_var = max(os_sum2 * inv - os_mean * os_mean, 0.0)
    return {
        "C_E": e_var / (e_mean * e_mean + EPS),
        "C_O": oc_var + os_var,
        "regional_energy_mean": e_mean,
        "regional_energy_var": e_var,
        "orientation_oc_mean": oc_mean,
        "orientation_os_mean": os_mean,
        "window_count": count,
        "window_stride_px": stride,
        "boundary_margin_px": margin,
        "first_integral_region": first,
    }


def trapezoid_log_average(xs, ys):
    numerator = 0.0
    for i in range(len(xs) - 1):
        delta = math.log(float(xs[i + 1])) - math.log(float(xs[i]))
        numerator += 0.5 * delta * (float(ys[i]) + float(ys[i + 1]))
    denominator = math.log(float(xs[-1])) - math.log(float(xs[0]))
    if denominator <= 0.0:
        raise RuntimeError("invalid log scale grid")
    return numerator / denominator


def process(gray, diagnostic=False, reporter=None):
    if int(gray.shape[0]) != ROI_HEIGHT or int(gray.shape[1]) != ROI_WIDTH:
        raise RuntimeError("frozen port requires 256x256 gray")
    per_scale = []
    for sigma in SIGMAS:
        scale_min_free = mem_free()
        workspace = gaussian_workspace(sigma)
        current_free = mem_free()
        if current_free >= 0 and (scale_min_free < 0 or current_free < scale_min_free): scale_min_free = current_free
        t_gaussian = now_ms()
        smooth = gaussian_blur(gray, sigma, workspace)
        gaussian_ms = elapsed_ms(t_gaussian)
        current_free = mem_free()
        if current_free >= 0 and (scale_min_free < 0 or current_free < scale_min_free): scale_min_free = current_free
        smooth_summary = summary(smooth) if diagnostic else None
        t_scharr = now_ms()
        derivative_workspace = scharr_workspace()
        ix, iy = scharr(smooth, derivative_workspace)
        scharr_ms = elapsed_ms(t_scharr)
        current_free = mem_free()
        if current_free >= 0 and (scale_min_free < 0 or current_free < scale_min_free): scale_min_free = current_free
        ix_summary = summary(ix) if diagnostic else None
        iy_summary = summary(iy) if diagnostic else None
        del smooth; del derivative_workspace
        t_tensor = now_ms()
        jxx_raw = ix * ix
        jxy_raw = ix * iy
        jyy_raw = iy * iy
        del ix; del iy
        jxx = gaussian_blur(jxx_raw, sigma, workspace); del jxx_raw
        jxy = gaussian_blur(jxy_raw, sigma, workspace); del jxy_raw
        jyy = gaussian_blur(jyy_raw, sigma, workspace); del jyy_raw
        tensor_ms = elapsed_ms(t_tensor)
        current_free = mem_free()
        if current_free >= 0 and (scale_min_free < 0 or current_free < scale_min_free): scale_min_free = current_free
        t_integral = now_ms()
        iixx = integral(jxx); iixy = integral(jxy); iiyy = integral(jyy)
        margin = int(math.ceil(3.0 * sigma) + 1)
        windows = []
        for window in WINDOWS:
            item = curve_for_window(iixx, iixy, iiyy, window, margin)
            item["window_px"] = window
            windows.append(item)
        integral_statistics_ms = elapsed_ms(t_integral)
        current_free = mem_free()
        if current_free >= 0 and (scale_min_free < 0 or current_free < scale_min_free): scale_min_free = current_free
        de_scale = trapezoid_log_average(WINDOWS, [item["C_E"] for item in windows])
        do_scale = trapezoid_log_average(WINDOWS, [item["C_O"] for item in windows])
        record = {"sigma": sigma, "D_E_scale": de_scale, "D_O_scale": do_scale, "curves": windows,
                  "gaussian_ms": gaussian_ms, "scharr_ms": scharr_ms,
                  "structure_tensor_ms": tensor_ms, "integral_statistics_ms": integral_statistics_ms,
                  "min_free_bytes_scale": scale_min_free}
        if diagnostic:
            record["Gaussian"] = smooth_summary; record["Ix"] = ix_summary; record["Iy"] = iy_summary
            record["Jxx"] = summary(jxx); record["Jxy"] = summary(jxy); record["Jyy"] = summary(jyy)
        per_scale.append(record)
        if reporter is not None:
            reporter(record)
        del jxx; del jxy; del jyy; del iixx; del iixy; del iiyy; del workspace
        collect()
    de = trapezoid_log_average(SIGMAS, [item["D_E_scale"] for item in per_scale])
    do = trapezoid_log_average(SIGMAS, [item["D_O_scale"] for item in per_scale])
    return {"D_E": de, "D_O": do, "per_scale": per_scale}
