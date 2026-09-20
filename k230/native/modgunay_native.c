/* Frozen K230 multivariate Gunay rotated-window statistics accelerator.
 *
 * This module changes only the implementation of the region statistics.
 * Geometry, tensor channels, window definitions, CB, normalisation, reference
 * curves, and U_curve remain in MicroPython and are unchanged.
 */

#include <stdint.h>

#include "py/binary.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/objtuple.h"
#include "py/mphal.h"
#include "py/runtime.h"

#include "ndarray.h"

enum {
    PLAN_META_WIDTH = 3,
    SPAN_WIDTH = 3,
    RESULT_WIDTH = 6,
};

static ndarray_obj_t *require_float_2d_dense(mp_obj_t obj) {
    if (!mp_obj_is_type(obj, &ulab_ndarray_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("T/Q/U inputs must be ulab ndarrays"));
    }
    ndarray_obj_t *array = MP_OBJ_TO_PTR(obj);
    if (array->dtype != NDARRAY_FLOAT || array->ndim != 2 || !ndarray_is_dense(array)) {
        mp_raise_TypeError(MP_ERROR_TEXT("T/Q/U inputs must be dense float 2-D ndarrays"));
    }
    return array;
}

static void require_same_shape(const ndarray_obj_t *lhs, const ndarray_obj_t *rhs) {
    if (lhs->shape[ULAB_MAX_DIMS - 2] != rhs->shape[ULAB_MAX_DIMS - 2] ||
        lhs->shape[ULAB_MAX_DIMS - 1] != rhs->shape[ULAB_MAX_DIMS - 1]) {
        mp_raise_ValueError(MP_ERROR_TEXT("T/Q/U shapes differ"));
    }
}

/* region_stats(T, Q, U, plan_offsets_u32, spans_i16, plan_meta_u16)
 *
 * spans is flattened (dy, xlo, xhi), inclusive in x.
 * plan_meta is flattened (rx, ry, actual_pixel_count).
 * Returns one tuple per plan:
 * (T_mean, T_sample_variance, Q_mean, Q_sample_variance,
 *  U_mean, U_sample_variance).
 */
#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static mp_obj_t gunay_native_region_stats(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    ndarray_obj_t *t_array = require_float_2d_dense(args[0]);
    ndarray_obj_t *q_array = require_float_2d_dense(args[1]);
    ndarray_obj_t *u_array = require_float_2d_dense(args[2]);
    require_same_shape(t_array, q_array);
    require_same_shape(t_array, u_array);

    const size_t height = t_array->shape[ULAB_MAX_DIMS - 2];
    const size_t width = t_array->shape[ULAB_MAX_DIMS - 1];
    if (height == 0 || width == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("empty T/Q/U array"));
    }

    mp_buffer_info_t offsets_buffer;
    mp_buffer_info_t spans_buffer;
    mp_buffer_info_t meta_buffer;
    mp_get_buffer_raise(args[3], &offsets_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[4], &spans_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[5], &meta_buffer, MP_BUFFER_READ);
    if (offsets_buffer.typecode != 'I' || offsets_buffer.len < 2 * sizeof(uint32_t) ||
        offsets_buffer.len % sizeof(uint32_t) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_offsets must be array('I')"));
    }
    if (spans_buffer.typecode != 'h' || spans_buffer.len % (SPAN_WIDTH * sizeof(int16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("spans must be packed array('h') triples"));
    }
    if (meta_buffer.typecode != 'H' || meta_buffer.len % (PLAN_META_WIDTH * sizeof(uint16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_meta must be packed array('H') triples"));
    }

    const uint32_t *offsets = (const uint32_t *)offsets_buffer.buf;
    const int16_t *spans = (const int16_t *)spans_buffer.buf;
    const uint16_t *meta = (const uint16_t *)meta_buffer.buf;
    const size_t plan_count = offsets_buffer.len / sizeof(uint32_t) - 1;
    const size_t span_count = spans_buffer.len / (SPAN_WIDTH * sizeof(int16_t));
    if (meta_buffer.len / (PLAN_META_WIDTH * sizeof(uint16_t)) != plan_count) {
        mp_raise_ValueError(MP_ERROR_TEXT("plan metadata count mismatch"));
    }
    if (offsets[0] != 0 || offsets[plan_count] != span_count) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid plan offsets"));
    }

    const mp_float_t *restrict t_data = (const mp_float_t *)t_array->array;
    const mp_float_t *restrict q_data = (const mp_float_t *)q_array->array;
    const mp_float_t *restrict u_data = (const mp_float_t *)u_array->array;
    mp_obj_t result = mp_obj_new_list(0, NULL);

    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        const size_t span_begin = offsets[plan_index];
        const size_t span_end = offsets[plan_index + 1];
        if (span_begin > span_end || span_end > span_count) {
            mp_raise_ValueError(MP_ERROR_TEXT("non-monotonic plan offsets"));
        }
        const size_t rx = meta[PLAN_META_WIDTH * plan_index + 0];
        const size_t ry = meta[PLAN_META_WIDTH * plan_index + 1];
        const size_t pixel_count = meta[PLAN_META_WIDTH * plan_index + 2];
        if (pixel_count < 2 || 2 * rx >= width || 2 * ry >= height || span_begin == span_end) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid rotated-window plan"));
        }
        for (size_t span_index = span_begin; span_index < span_end; ++span_index) {
            const int16_t *span = spans + SPAN_WIDTH * span_index;
            if (span[0] < -(int32_t)ry || span[0] > (int32_t)ry ||
                span[1] < -(int32_t)rx || span[2] > (int32_t)rx || span[2] < span[1]) {
                mp_raise_ValueError(MP_ERROR_TEXT("span outside frozen plan bounds"));
            }
        }

        mp_float_t running_mean[3] = {0.0f, 0.0f, 0.0f};
        mp_float_t running_m2[3] = {0.0f, 0.0f, 0.0f};
        size_t unit_count = 0;
        const mp_float_t inv_pixels = 1.0f / (mp_float_t)pixel_count;

        for (size_t center_y = ry; center_y < height - ry; ++center_y) {
            for (size_t center_x = rx; center_x < width - rx; ++center_x) {
                mp_float_t region_sum[3] = {0.0f, 0.0f, 0.0f};
                for (size_t span_index = span_begin; span_index < span_end; ++span_index) {
                    const int16_t *span = spans + SPAN_WIDTH * span_index;
                    const int32_t row = (int32_t)center_y + span[0];
                    const int32_t xlo = (int32_t)center_x + span[1];
                    const int32_t xhi = (int32_t)center_x + span[2];
                    size_t flat_index = (size_t)row * width + (size_t)xlo;
                    const size_t contiguous_count = (size_t)(xhi - xlo + 1);
                    const mp_float_t *tp = t_data + flat_index;
                    const mp_float_t *qp = q_data + flat_index;
                    const mp_float_t *up = u_data + flat_index;
                    for (size_t k = 0; k < contiguous_count; ++k) {
                        region_sum[0] += tp[k];
                        region_sum[1] += qp[k];
                        region_sum[2] += up[k];
                    }
                }

                ++unit_count;
                for (size_t channel = 0; channel < 3; ++channel) {
                    const mp_float_t value = region_sum[channel] * inv_pixels;
                    const mp_float_t delta = value - running_mean[channel];
                    running_mean[channel] += delta / (mp_float_t)unit_count;
                    running_m2[channel] += delta * (value - running_mean[channel]);
                }
            }
        }
        if (unit_count < 2) {
            mp_raise_ValueError(MP_ERROR_TEXT("rotated window has fewer than two valid centers"));
        }

        mp_obj_t values[RESULT_WIDTH];
        for (size_t channel = 0; channel < 3; ++channel) {
            values[2 * channel] = mp_obj_new_float(running_mean[channel]);
            values[2 * channel + 1] = mp_obj_new_float(
                running_m2[channel] / (mp_float_t)(unit_count - 1));
        }
        mp_obj_list_append(result, mp_obj_new_tuple(RESULT_WIDTH, values));
    }
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gunay_native_region_stats_obj, 6, 6, gunay_native_region_stats);

/* region_stats_row_prefix(T, Q, U, plan_offsets_u32, spans_i16, plan_meta_u16)
 *
 * The frozen plan and Welford statistics are identical to region_stats().
 * Only each scanline sum changes from a direct pixel loop to a difference of
 * float32 row-prefix values.  One full prefix buffer is reused for T, Q, U.
 * Returns (statistics, prefix_build_ms, span_query_ms, prefix_scratch_bytes).
 */
#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static mp_obj_t gunay_native_region_stats_row_prefix(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    ndarray_obj_t *t_array = require_float_2d_dense(args[0]);
    ndarray_obj_t *q_array = require_float_2d_dense(args[1]);
    ndarray_obj_t *u_array = require_float_2d_dense(args[2]);
    require_same_shape(t_array, q_array);
    require_same_shape(t_array, u_array);

    const size_t height = t_array->shape[ULAB_MAX_DIMS - 2];
    const size_t width = t_array->shape[ULAB_MAX_DIMS - 1];
    if (height == 0 || width == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("empty T/Q/U array"));
    }

    mp_buffer_info_t offsets_buffer;
    mp_buffer_info_t spans_buffer;
    mp_buffer_info_t meta_buffer;
    mp_get_buffer_raise(args[3], &offsets_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[4], &spans_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[5], &meta_buffer, MP_BUFFER_READ);
    if (offsets_buffer.typecode != 'I' || offsets_buffer.len < 2 * sizeof(uint32_t) ||
        offsets_buffer.len % sizeof(uint32_t) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_offsets must be array('I')"));
    }
    if (spans_buffer.typecode != 'h' || spans_buffer.len % (SPAN_WIDTH * sizeof(int16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("spans must be packed array('h') triples"));
    }
    if (meta_buffer.typecode != 'H' || meta_buffer.len % (PLAN_META_WIDTH * sizeof(uint16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_meta must be packed array('H') triples"));
    }

    const uint32_t *offsets = (const uint32_t *)offsets_buffer.buf;
    const int16_t *spans = (const int16_t *)spans_buffer.buf;
    const uint16_t *meta = (const uint16_t *)meta_buffer.buf;
    const size_t plan_count = offsets_buffer.len / sizeof(uint32_t) - 1;
    const size_t span_count = spans_buffer.len / (SPAN_WIDTH * sizeof(int16_t));
    if (meta_buffer.len / (PLAN_META_WIDTH * sizeof(uint16_t)) != plan_count) {
        mp_raise_ValueError(MP_ERROR_TEXT("plan metadata count mismatch"));
    }
    if (offsets[0] != 0 || offsets[plan_count] != span_count) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid plan offsets"));
    }
    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        const size_t span_begin = offsets[plan_index];
        const size_t span_end = offsets[plan_index + 1];
        const size_t rx = meta[PLAN_META_WIDTH * plan_index + 0];
        const size_t ry = meta[PLAN_META_WIDTH * plan_index + 1];
        const size_t pixel_count = meta[PLAN_META_WIDTH * plan_index + 2];
        if (span_begin > span_end || span_end > span_count || pixel_count < 2 ||
            2 * rx >= width || 2 * ry >= height || span_begin == span_end) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid rotated-window plan"));
        }
        for (size_t span_index = span_begin; span_index < span_end; ++span_index) {
            const int16_t *span = spans + SPAN_WIDTH * span_index;
            if (span[0] < -(int32_t)ry || span[0] > (int32_t)ry ||
                span[1] < -(int32_t)rx || span[2] > (int32_t)rx || span[2] < span[1]) {
                mp_raise_ValueError(MP_ERROR_TEXT("span outside frozen plan bounds"));
            }
        }
    }

    const mp_float_t *channels[3] = {
        (const mp_float_t *)t_array->array,
        (const mp_float_t *)q_array->array,
        (const mp_float_t *)u_array->array,
    };
    const size_t prefix_stride = width + 1;
    const size_t prefix_count = height * prefix_stride;
    mp_float_t *prefix = m_new(mp_float_t, prefix_count);
    mp_float_t *statistics = m_new(mp_float_t, plan_count * RESULT_WIDTH);
    mp_uint_t prefix_build_ms = 0;
    mp_uint_t span_query_ms = 0;

    for (size_t channel = 0; channel < 3; ++channel) {
        mp_uint_t tick = mp_hal_ticks_ms();
        const mp_float_t *source = channels[channel];
        for (size_t row = 0; row < height; ++row) {
            mp_float_t running = 0.0f;
            mp_float_t *prefix_row = prefix + row * prefix_stride;
            const mp_float_t *source_row = source + row * width;
            prefix_row[0] = 0.0f;
            for (size_t x = 0; x < width; ++x) {
                running += source_row[x];
                prefix_row[x + 1] = running;
            }
        }
        prefix_build_ms += mp_hal_ticks_ms() - tick;

        tick = mp_hal_ticks_ms();
        for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
            const size_t span_begin = offsets[plan_index];
            const size_t span_end = offsets[plan_index + 1];
            const size_t rx = meta[PLAN_META_WIDTH * plan_index + 0];
            const size_t ry = meta[PLAN_META_WIDTH * plan_index + 1];
            const size_t pixel_count = meta[PLAN_META_WIDTH * plan_index + 2];
            const mp_float_t inv_pixels = 1.0f / (mp_float_t)pixel_count;
            mp_float_t running_mean = 0.0f;
            mp_float_t running_m2 = 0.0f;
            size_t unit_count = 0;

            for (size_t center_y = ry; center_y < height - ry; ++center_y) {
                for (size_t center_x = rx; center_x < width - rx; ++center_x) {
                    mp_float_t region_sum = 0.0f;
                    for (size_t span_index = span_begin; span_index < span_end; ++span_index) {
                        const int16_t *span = spans + SPAN_WIDTH * span_index;
                        const size_t row = (size_t)((int32_t)center_y + span[0]);
                        const size_t xlo = (size_t)((int32_t)center_x + span[1]);
                        const size_t xhi = (size_t)((int32_t)center_x + span[2]);
                        const mp_float_t *prefix_row = prefix + row * prefix_stride;
                        region_sum += prefix_row[xhi + 1] - prefix_row[xlo];
                    }
                    ++unit_count;
                    const mp_float_t value = region_sum * inv_pixels;
                    const mp_float_t delta = value - running_mean;
                    running_mean += delta / (mp_float_t)unit_count;
                    running_m2 += delta * (value - running_mean);
                }
            }
            statistics[RESULT_WIDTH * plan_index + 2 * channel] = running_mean;
            statistics[RESULT_WIDTH * plan_index + 2 * channel + 1] =
                running_m2 / (mp_float_t)(unit_count - 1);
        }
        span_query_ms += mp_hal_ticks_ms() - tick;
    }

    m_del(mp_float_t, prefix, prefix_count);
    mp_obj_t result = mp_obj_new_list(0, NULL);
    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        mp_obj_t values[RESULT_WIDTH];
        for (size_t index = 0; index < RESULT_WIDTH; ++index) {
            values[index] = mp_obj_new_float(statistics[RESULT_WIDTH * plan_index + index]);
        }
        mp_obj_list_append(result, mp_obj_new_tuple(RESULT_WIDTH, values));
    }
    m_del(mp_float_t, statistics, plan_count * RESULT_WIDTH);

    mp_obj_t output[4] = {
        result,
        mp_obj_new_int_from_uint(prefix_build_ms),
        mp_obj_new_int_from_uint(span_query_ms),
        mp_obj_new_int_from_uint(prefix_count * sizeof(mp_float_t)),
    };
    return mp_obj_new_tuple(4, output);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    gunay_native_region_stats_row_prefix_obj, 6, 6, gunay_native_region_stats_row_prefix);

/* region_stats_fused_span_query(T, Q, U, offsets, spans, meta)
 *
 * A ring of row-prefixes covers the largest frozen vertical window radius.
 * Every source row is prefixed once for T/Q/U together.  Every packed span is
 * then traversed once and updates all three channel sums.  Geometry, center
 * order, float32 prefix arithmetic, and Welford statistics are unchanged.
 * Returns (statistics, prefix_build_ms, span_query_ms, scratch_bytes,
 *          span_visits_per_scale).
 */
#if defined(__GNUC__)
__attribute__((optimize("O3", "no-tree-vectorize")))
#endif
static mp_obj_t gunay_native_region_stats_fused_span_query(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    ndarray_obj_t *t_array = require_float_2d_dense(args[0]);
    ndarray_obj_t *q_array = require_float_2d_dense(args[1]);
    ndarray_obj_t *u_array = require_float_2d_dense(args[2]);
    require_same_shape(t_array, q_array);
    require_same_shape(t_array, u_array);

    const size_t height = t_array->shape[ULAB_MAX_DIMS - 2];
    const size_t width = t_array->shape[ULAB_MAX_DIMS - 1];
    if (height == 0 || width == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("empty T/Q/U array"));
    }

    mp_buffer_info_t offsets_buffer;
    mp_buffer_info_t spans_buffer;
    mp_buffer_info_t meta_buffer;
    mp_get_buffer_raise(args[3], &offsets_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[4], &spans_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(args[5], &meta_buffer, MP_BUFFER_READ);
    if (offsets_buffer.typecode != 'I' || offsets_buffer.len < 2 * sizeof(uint32_t) ||
        offsets_buffer.len % sizeof(uint32_t) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_offsets must be array('I')"));
    }
    if (spans_buffer.typecode != 'h' || spans_buffer.len % (SPAN_WIDTH * sizeof(int16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("spans must be packed array('h') triples"));
    }
    if (meta_buffer.typecode != 'H' || meta_buffer.len % (PLAN_META_WIDTH * sizeof(uint16_t)) != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("plan_meta must be packed array('H') triples"));
    }

    const uint32_t *restrict offsets = (const uint32_t *)offsets_buffer.buf;
    const int16_t *restrict spans = (const int16_t *)spans_buffer.buf;
    const uint16_t *restrict meta = (const uint16_t *)meta_buffer.buf;
    const size_t plan_count = offsets_buffer.len / sizeof(uint32_t) - 1;
    const size_t span_count = spans_buffer.len / (SPAN_WIDTH * sizeof(int16_t));
    if (meta_buffer.len / (PLAN_META_WIDTH * sizeof(uint16_t)) != plan_count ||
        offsets[0] != 0 || offsets[plan_count] != span_count) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid packed plan dimensions"));
    }

    size_t max_ry = 0;
    size_t max_plan_spans = 0;
    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        const size_t span_begin = offsets[plan_index];
        const size_t span_end = offsets[plan_index + 1];
        const size_t rx = meta[PLAN_META_WIDTH * plan_index + 0];
        const size_t ry = meta[PLAN_META_WIDTH * plan_index + 1];
        const size_t pixel_count = meta[PLAN_META_WIDTH * plan_index + 2];
        if (span_begin > span_end || span_end > span_count || pixel_count < 2 ||
            2 * rx >= width || 2 * ry >= height || span_begin == span_end) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid rotated-window plan"));
        }
        if (ry > max_ry) {
            max_ry = ry;
        }
        if (span_end - span_begin > max_plan_spans) {
            max_plan_spans = span_end - span_begin;
        }
        for (size_t span_index = span_begin; span_index < span_end; ++span_index) {
            const int16_t *span = spans + SPAN_WIDTH * span_index;
            if (span[0] < -(int32_t)ry || span[0] > (int32_t)ry ||
                span[1] < -(int32_t)rx || span[2] > (int32_t)rx || span[2] < span[1]) {
                mp_raise_ValueError(MP_ERROR_TEXT("span outside frozen plan bounds"));
            }
        }
    }

    const mp_float_t *restrict t_data = (const mp_float_t *)t_array->array;
    const mp_float_t *restrict q_data = (const mp_float_t *)q_array->array;
    const mp_float_t *restrict u_data = (const mp_float_t *)u_array->array;
    const size_t prefix_stride = width + 1;
    const size_t ring_rows = 2 * max_ry + 1;
    const size_t prefix_count = ring_rows * 3 * prefix_stride;
    const size_t state_count = plan_count * 3;
    mp_float_t *prefix = m_new(mp_float_t, prefix_count);
    int16_t *row_tags = m_new(int16_t, ring_rows);
    mp_float_t *running_mean = m_new(mp_float_t, state_count);
    mp_float_t *running_m2 = m_new(mp_float_t, state_count);
    size_t *unit_count = m_new(size_t, plan_count);
    mp_float_t **span_lo_bases = m_new(mp_float_t *, max_plan_spans);
    uint16_t *span_widths = m_new(uint16_t, max_plan_spans);
    for (size_t slot = 0; slot < ring_rows; ++slot) {
        row_tags[slot] = -1;
    }
    for (size_t index = 0; index < state_count; ++index) {
        running_mean[index] = 0.0f;
        running_m2[index] = 0.0f;
    }
    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        unit_count[plan_index] = 0;
    }

    mp_uint_t prefix_build_ms = 0;
    mp_uint_t span_query_ms = 0;
    uint32_t span_visits = 0;
    for (size_t center_y = 0; center_y < height; ++center_y) {
        const size_t first_row = center_y > max_ry ? center_y - max_ry : 0;
        const size_t last_row = center_y + max_ry < height ? center_y + max_ry : height - 1;
        mp_uint_t tick = mp_hal_ticks_ms();
        for (size_t row = first_row; row <= last_row; ++row) {
            const size_t slot = row % ring_rows;
            if (row_tags[slot] == (int16_t)row) {
                continue;
            }
            mp_float_t *restrict tp = prefix + (slot * 3 + 0) * prefix_stride;
            mp_float_t *restrict qp = prefix + (slot * 3 + 1) * prefix_stride;
            mp_float_t *restrict up = prefix + (slot * 3 + 2) * prefix_stride;
            const mp_float_t *restrict ts = t_data + row * width;
            const mp_float_t *restrict qs = q_data + row * width;
            const mp_float_t *restrict us = u_data + row * width;
            mp_float_t tr = 0.0f;
            mp_float_t qr = 0.0f;
            mp_float_t ur = 0.0f;
            tp[0] = 0.0f;
            qp[0] = 0.0f;
            up[0] = 0.0f;
            for (size_t x = 0; x < width; ++x) {
                tr += ts[x];
                qr += qs[x];
                ur += us[x];
                tp[x + 1] = tr;
                qp[x + 1] = qr;
                up[x + 1] = ur;
            }
            row_tags[slot] = (int16_t)row;
        }
        prefix_build_ms += mp_hal_ticks_ms() - tick;

        tick = mp_hal_ticks_ms();
        for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
            const size_t rx = meta[PLAN_META_WIDTH * plan_index + 0];
            const size_t ry = meta[PLAN_META_WIDTH * plan_index + 1];
            if (center_y < ry || center_y >= height - ry) {
                continue;
            }
            const size_t span_begin = offsets[plan_index];
            const size_t span_end = offsets[plan_index + 1];
            const mp_float_t inv_pixels =
                1.0f / (mp_float_t)meta[PLAN_META_WIDTH * plan_index + 2];
            const size_t plan_span_count = span_end - span_begin;
            const int16_t *prepare_span = spans + SPAN_WIDTH * span_begin;
            for (size_t span_offset = 0; span_offset < plan_span_count;
                 ++span_offset, prepare_span += SPAN_WIDTH) {
                const size_t row = (size_t)((int32_t)center_y + prepare_span[0]);
                const size_t slot = row % ring_rows;
                span_lo_bases[span_offset] =
                    prefix + slot * 3 * prefix_stride +
                    (size_t)((int32_t)rx + prepare_span[1]);
                span_widths[span_offset] =
                    (uint16_t)(prepare_span[2] - prepare_span[1] + 1);
            }
            for (size_t center_x = rx; center_x < width - rx; ++center_x) {
                mp_float_t sums[3] = {0.0f, 0.0f, 0.0f};
                mp_float_t **restrict base_ptr = span_lo_bases;
                const uint16_t *restrict width_ptr = span_widths;
                for (size_t span_offset = 0; span_offset < plan_span_count;
                     ++span_offset, ++base_ptr, ++width_ptr) {
                    const mp_float_t *restrict lo = *base_ptr + (center_x - rx);
                    const size_t span_width = *width_ptr;
                    sums[0] += lo[span_width] - lo[0];
                    lo += prefix_stride;
                    sums[1] += lo[span_width] - lo[0];
                    lo += prefix_stride;
                    sums[2] += lo[span_width] - lo[0];
                    ++span_visits;
                }
                const size_t count = ++unit_count[plan_index];
                const size_t state_base = plan_index * 3;
                for (size_t channel = 0; channel < 3; ++channel) {
                    const mp_float_t value = sums[channel] * inv_pixels;
                    const mp_float_t delta = value - running_mean[state_base + channel];
                    running_mean[state_base + channel] += delta / (mp_float_t)count;
                    running_m2[state_base + channel] +=
                        delta * (value - running_mean[state_base + channel]);
                }
            }
        }
        span_query_ms += mp_hal_ticks_ms() - tick;
    }

    const size_t scratch_bytes =
        prefix_count * sizeof(mp_float_t) + ring_rows * sizeof(int16_t) +
        2 * state_count * sizeof(mp_float_t) + plan_count * sizeof(size_t) +
        max_plan_spans * (sizeof(mp_float_t *) + sizeof(uint16_t));
    m_del(mp_float_t, prefix, prefix_count);
    m_del(int16_t, row_tags, ring_rows);

    mp_obj_t result = mp_obj_new_list(0, NULL);
    for (size_t plan_index = 0; plan_index < plan_count; ++plan_index) {
        mp_obj_t values[RESULT_WIDTH];
        for (size_t channel = 0; channel < 3; ++channel) {
            const size_t state_index = plan_index * 3 + channel;
            values[2 * channel] = mp_obj_new_float(running_mean[state_index]);
            values[2 * channel + 1] = mp_obj_new_float(
                running_m2[state_index] / (mp_float_t)(unit_count[plan_index] - 1));
        }
        mp_obj_list_append(result, mp_obj_new_tuple(RESULT_WIDTH, values));
    }
    m_del(mp_float_t, running_mean, state_count);
    m_del(mp_float_t, running_m2, state_count);
    m_del(size_t, unit_count, plan_count);
    m_del(mp_float_t *, span_lo_bases, max_plan_spans);
    m_del(uint16_t, span_widths, max_plan_spans);

    mp_obj_t output[5] = {
        result,
        mp_obj_new_int_from_uint(prefix_build_ms),
        mp_obj_new_int_from_uint(span_query_ms),
        mp_obj_new_int_from_uint(scratch_bytes),
        mp_obj_new_int_from_uint(span_visits),
    };
    return mp_obj_new_tuple(5, output);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    gunay_native_region_stats_fused_span_query_obj, 6, 6,
    gunay_native_region_stats_fused_span_query);

static const mp_rom_map_elem_t gunay_native_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_gunay_native)},
    {MP_ROM_QSTR(MP_QSTR_region_stats), MP_ROM_PTR(&gunay_native_region_stats_obj)},
    {MP_ROM_QSTR(MP_QSTR_region_stats_row_prefix),
     MP_ROM_PTR(&gunay_native_region_stats_row_prefix_obj)},
    {MP_ROM_QSTR(MP_QSTR_region_stats_fused_span_query),
     MP_ROM_PTR(&gunay_native_region_stats_fused_span_query_obj)},
};
STATIC MP_DEFINE_CONST_DICT(gunay_native_module_globals, gunay_native_module_globals_table);

const mp_obj_module_t gunay_native_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&gunay_native_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_gunay_native, gunay_native_module);
