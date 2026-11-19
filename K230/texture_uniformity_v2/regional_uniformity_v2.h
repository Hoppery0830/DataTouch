#ifndef REGIONAL_UNIFORMITY_V2_H
#define REGIONAL_UNIFORMITY_V2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum regional_uniformity_status {
    REGIONAL_UNIFORMITY_OK = 0,
    REGIONAL_UNIFORMITY_INVALID_INPUT,
    REGIONAL_UNIFORMITY_INSUFFICIENT_CONTEXT,
    REGIONAL_UNIFORMITY_INSUFFICIENT_LUMINANCE,
    REGIONAL_UNIFORMITY_NO_MEMORY,
    REGIONAL_UNIFORMITY_COMPUTE_ERROR,
};

typedef struct {
    double differences[6];
    double score;
    int within_feature_calibration_ranges;
} regional_uniformity_result;

/* Input is an original-resolution, row-major Gray8 frame. Stride is in bytes. */
int regional_uniformity_v2_measure(
    const uint8_t *gray, int width, int height, size_t stride,
    regional_uniformity_result *result
);

#ifdef __cplusplus
}
#endif
#endif
