"""One-shot K230 entry point for the regional texture uniformity V2 score."""

from regionuniformity_native import measure_gray8

TEXTURE_SCALES = (0.5, 0.75, 1.0, 2.0, 4.0)


def measure_image(frame):
    """Score an original-resolution CanMV image, keeping Gray8 alive for ulab."""
    gray = frame.to_grayscale()
    score, differences, within_range = measure_gray8(gray.to_numpy_ref())
    return {
        "regional_texture_uniformity_score": score,
        "score_symbol": "S_uniformity",
        "higher_is_more_uniform": True,
        "scale_structure_differences": [
            {"texture_scale_pixels": scale, "structure_difference": value}
            for scale, value in zip(TEXTURE_SCALES, differences[:5])
        ],
        "regional_luminance_difference": differences[5],
        "calibration_scope": "nine supplied images, internal calibration",
        "within_feature_calibration_ranges": within_range,
    }


def measure_camera(camera):
    """Use an initialized DataTouch Camera and the complete sensor snapshot."""
    return measure_image(camera.snapshot())
