#include "touch_telemetry.h"
#include "touch_config.h"
#include <math.h>
#include <string.h>
static float maximum(float a, float b)
{
    return a > b ? a : b;
}
void TouchFeatures_Force(touch_features_t *s, const touch_force_t *f)
{
    float v[6];
    const float zero[6] = {TOUCH_ZERO_FX_N, TOUCH_ZERO_FY_N, TOUCH_ZERO_FZ_N, 0, 0, 0};
    for (unsigned i = 0; i < 6; i++)
    {
        v[i] = f->values[i] - zero[i];
        if (!isfinite(v[i]) || fabsf(v[i]) > 1e6f)
            return;
    }
    if (s->fn >= 1000)
    {
        s->window_bad_force = true;
        return;
    }
    if (s->last_force && f->timestamp_ms - s->last_force > 100)
        s->window_bad_force = true;
    s->last_force = f->timestamp_ms;
    float ft = hypotf(v[0], v[1]), normal = fabsf(v[2]);
    for (unsigned i = 0; i < 6; i++)
        s->sums[i] += v[i];
    s->fn++;
    s->ft_sum += ft;
    s->normal_sum += normal;
    if (normal >= 0.2f)
    {
        s->mu_sum += ft / normal;
        if (s->wn < 1200)
        {
            s->window_ft[s->wn++] = ft;
            s->window_normal_sum += normal;
            s->window_ft_sum += ft;
            s->window_mu_max = maximum(s->window_mu_max, ft / normal);
        }
        else
            s->window_bad_force = true;
    }
}
void TouchFeatures_ADC(touch_features_t *s, const touch_adxl_t *a, uint32_t ms)
{
    if (s->last_adc && ms - s->last_adc > 20)
        s->window_bad_adc = true;
    s->last_adc = ms;
    s->clipped |= a->clipped;
    if (a->clipped)
    {
        s->window_bad_adc = true;
        return;
    }
    if (s->an >= 1000)
    {
        s->window_bad_adc = true;
        return;
    }
    s->an++;
    s->rms_sq += (float)a->rms_mg * a->rms_mg;
    if (s->wa < 220)
        s->envelope[s->wa++] = (float)a->rms_mg;
    else
        s->window_bad_adc = true;
}
static void finish_window(touch_features_t *s)
{
    s->force_features = s->wn > 1 && !s->window_bad_force;
    if (s->force_features)
    {
        float mean = s->window_ft_sum / s->wn, variance = 0, lo = s->window_ft[0], hi = lo;
        for (unsigned i = 0; i < s->wn; i++)
        {
            float f = s->window_ft[i], d = f - mean;
            variance += d * d;
            if (f < lo)
                lo = f;
            if (f > hi)
                hi = f;
        }
        float sd = sqrtf(variance / s->wn), threshold = mean + 2 * sd;
        unsigned crossings = 0;
        for (unsigned i = 1; i < s->wn; i++)
            if (s->window_ft[i] >= threshold && s->window_ft[i - 1] < threshold)
                crossings++;
        s->features[0] = s->window_ft_sum / s->window_normal_sum; // ratio of means
        s->features[1] = sd;
        s->features[2] = hi - lo;
        s->features[6] = s->window_mu_max;
        s->features[7] = s->window_normal_sum / s->wn;
        s->features[8] = mean;
        s->features[9] = (float)crossings;
        s->features[12] = (float)s->wn;
    }
    s->adc_features = s->wa >= 190 && s->wa <= 210 && !s->window_bad_adc;
    if (s->adc_features)
    {
        float mean = 0, peak = 0;
        for (unsigned i = 0; i < s->wa; i++)
        {
            mean += s->envelope[i];
            peak = maximum(peak, s->envelope[i]);
        }
        mean /= s->wa;
        float best = 0, freq = 0, variance = 0;
        for (unsigned i = 0; i < s->wa; i++)
        {
            float x = s->envelope[i] - mean;
            variance += x * x;
        }
        // Goertzel on the 100 Hz block-RMS envelope, NOT the 10 kHz vibration spectrum.
        if (variance / s->wa > 0.01f)
            for (unsigned k = 1; k <= 80; k++)
            {
                float hz = 0.5f * k, c = 2 * cosf(6.283185307f * hz / 100), q1 = 0, q2 = 0;
                for (unsigned i = 0; i < s->wa; i++)
                {
                    float q = s->envelope[i] - mean + c * q1 - q2;
                    q2 = q1;
                    q1 = q;
                }
                float power = q1 * q1 + q2 * q2 - c * q1 * q2;
                if (power > best)
                {
                    best = power;
                    freq = hz;
                }
            }
        s->features[4] = peak;
        s->features[10] = freq;
        s->features[11] = mean;
    }
    s->wn = s->wa = 0;
    s->window_ft_sum = s->window_normal_sum = s->window_mu_max = 0;
    s->window_bad_adc = s->window_bad_force = false;
}
bool TouchFeatures_Tick(touch_features_t *s, uint32_t now, touch_telemetry_t *out)
{
    if (now - s->window_ms >= 2000)
    {
        if (now - s->window_ms > 2100)
        {
            s->window_bad_adc = true;
            s->window_bad_force = true;
        }
        finish_window(s);
        s->window_ms = now;
    }
    if (now - s->output_ms < 100)
        return false;
    memset(out, 0, sizeof(*out));
    out->seq = ++s->seq;
    out->ms = now;
    bool force = s->fn && now - s->last_force < 100,
         adc = s->an && now - s->last_adc < 100 && !s->clipped;
    if (force)
    {
        out->flags |= 1;
        for (unsigned i = 0; i < 6; i++)
            out->v[i] = s->sums[i] / s->fn;
        out->v[6] = s->ft_sum / s->fn;
        out->v[7] = s->normal_sum / s->fn;
        if (out->v[7] >= 0.2f)
        {
            out->flags |= 8;
            out->v[8] = out->v[6] / out->v[7];
        }
    }
    if (adc)
    {
        out->flags |= 2;
        out->v[12] = sqrtf(s->rms_sq / s->an);
    }
    if (s->clipped)
        out->flags |= 32;
    if (force && s->force_features)
        out->flags |= 4;
    if (adc && s->adc_features)
        out->flags |= 64;
    for (unsigned i = 0; i < 13; i++)
        if (i != 3 && i != 5)
            out->v[9 + i] = s->features[i];
    s->output_ms = now;
    s->fn = s->an = 0;
    s->ft_sum = s->normal_sum = s->mu_sum = s->rms_sq = 0;
    s->clipped = false;
    memset(s->sums, 0, sizeof(s->sums));
    return true;
}
static void u32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
void TouchTelemetry_Encode(const touch_telemetry_t *s, uint8_t out[TOUCH_TELEMETRY_SIZE])
{
    out[0] = 1;
    out[1] = s->flags;
    out[2] = s->light;
    out[3] = 0;
    u32(out + 4, s->seq);
    u32(out + 8, s->ms);
    u32(out + 12, s->k_seq);
    u32(out + 16, s->k_ms);
    for (unsigned i = 0; i < 22; i++)
    {
        uint32_t bits;
        memcpy(&bits, &s->v[i], 4);
        u32(out + 20 + 4 * i, bits);
    }
}
