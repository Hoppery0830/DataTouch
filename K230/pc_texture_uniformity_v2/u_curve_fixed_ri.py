"""Fixed-scale circular tensor-CB uniformity index (experimental).

Preserves the original five-scale Gaussian/Scharr tensor descriptor and the
multivariate between-region coefficient of variation. A fixed isotropic
integration suppresses regular carrier phase; circular windows and a common
circular support make the continuous measurement invariant to in-plane rotation.
Larger values mean greater spatial variation. No human-score fit is used.
Requires NumPy >= 2 and OpenCV. All lengths below are analysis pixels.
"""
from pathlib import Path
from functools import lru_cache
import argparse
import json
import cv2
import numpy as np

VERSION = 'U_curve_fixed_RI_v1'
SIGMAS = (.5, .75, 1., 2., 4.)
ENVELOPE_SIGMA = 16.
SOURCE_SHORT_SIDE = 448.
SUPPORT_RADIUS = 72.
AREAS = 6 * np.arange(2, 17, dtype=float)**2
EPS = 1e-10


def tensor_field(gray):
    """Same arithmetic/scale order as the existing multivariate_field."""
    source = np.asarray(gray, np.float32)
    channels = []
    for sigma in SIGMAS:
        k = 2 * int(np.ceil(3 * sigma)) + 1
        sm = cv2.GaussianBlur(source, (k, k), sigma,
                              borderType=cv2.BORDER_REFLECT101)
        ix = cv2.Scharr(sm, cv2.CV_32F, 1, 0, borderType=cv2.BORDER_REFLECT101)
        iy = cv2.Scharr(sm, cv2.CV_32F, 0, 1, borderType=cv2.BORDER_REFLECT101)
        xx = cv2.GaussianBlur(ix * ix, (k, k), sigma,
                              borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        xy = cv2.GaussianBlur(ix * iy, (k, k), sigma,
                              borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        yy = cv2.GaussianBlur(iy * iy, (k, k), sigma,
                              borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        channels.extend((xx + yy, xx - yy, 2 * xy))
    return np.stack(channels, axis=-1)


@lru_cache(maxsize=15)
def disk_kernel(area):
    radius = np.sqrt(area / np.pi)
    extent = int(np.ceil(radius + .5))
    coords = np.arange(-extent, extent + 1)
    yy, xx = np.meshgrid(coords, coords, indexing='ij')
    w = np.zeros(xx.shape, float)
    offsets = (np.arange(8) + .5) / 8 - .5
    for oy in offsets:
        for ox in offsets:
            w += (xx + ox)**2 + (yy + oy)**2 <= radius**2
    return w / w.sum()


@lru_cache(maxsize=1)
def center_weights():
    yy, xx = np.mgrid[:256, :256]
    radius = np.hypot(xx - 127.5, yy - 127.5)
    weights = np.clip(SUPPORT_RADIUS + .5 - radius, 0, 1)
    return weights / weights.sum()


def score_envelope(field):
    """Measure dispersion of an already integrated central tensor field."""
    if field.shape != (256, 256, 15) or not np.isfinite(field).all():
        raise ValueError('Expected a finite 256x256x15 field')
    support = center_weights()
    valid = support > 0
    weights = support[valid]
    correction = 1 - float(weights @ weights)
    cb = []
    for area in AREAS:
        means = cv2.filter2D(field, cv2.CV_64F, disk_kernel(float(area)),
                             borderType=cv2.BORDER_CONSTANT)[valid]
        mu = np.sum(means * weights[:, None], axis=0)
        energy = float(mu @ mu)
        if energy <= EPS:
            raise ValueError('Insufficient texture energy; no uniformity value is valid')
        variance = float(np.sum((means - mu)**2 * weights[:, None]) / correction)
        cb.append(np.sqrt(max(variance, 0.) / (energy + EPS)))
    cb = np.asarray(cb)
    log_area = np.log(AREAS)
    value = np.sqrt(np.trapezoid(cb**2, log_area) / (log_area[-1] - log_area[0]))
    return dict(U_curve_fixed_RI=float(value), raw_CB_fraction=cb.tolist(),
                areas=AREAS.tolist())


def score_gray_context(gray):
    """512x512 float grayscale [0,1]; caller must supply valid real context."""
    gray = np.asarray(gray)
    if gray.shape != (512, 512) or not np.isfinite(gray).all():
        raise ValueError('Expected a finite 512x512 grayscale array')
    if gray.min() < 0 or gray.max() > 1:
        raise ValueError('Grayscale values must lie in [0,1]')
    field = tensor_field(gray)
    radius = int(np.ceil(3 * ENVELOPE_SIGMA))
    k = 2 * radius + 1
    envelope = cv2.GaussianBlur(field, (k, k), ENVELOPE_SIGMA,
                                borderType=cv2.BORDER_REFLECT101)[128:384, 128:384]
    result = score_envelope(envelope)
    # Sum of support radii: center support, circular mean, square filter footprints.
    required = SUPPORT_RADIUS + .5 + 23 + np.sqrt(2) * (25 + radius)
    return dict(**result, version=VERSION, envelope_sigma_px=ENVELOPE_SIGMA,
                required_real_context_radius_px=float(required),
                measurement='Spatial variation of a fixed-scale tensor envelope; lower is more uniform',
                validation_scope='Development controls and nine fabric images; no universal calibration')


def prepare_source(path):
    bgr = cv2.imdecode(np.fromfile(str(path), np.uint8), cv2.IMREAD_COLOR)
    if bgr is None:
        raise FileNotFoundError(path)
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255
    h, w = gray.shape
    scale = SOURCE_SHORT_SIDE / min(h, w)
    sigma = .5 * np.sqrt(max(1 / scale**2 - 1, 0))
    radius = int(np.ceil(3 * sigma))
    if radius:
        gray = cv2.GaussianBlur(gray, (2 * radius + 1, 2 * radius + 1), sigma,
                               borderType=cv2.BORDER_REFLECT101)
    available = (min(h, w) - 1) * scale / 2 - np.sqrt(2) * (radius + 1) * scale
    return gray, dict(source_width=w, source_height=h, scale=float(scale),
                      anti_alias_sigma=float(sigma), anti_alias_radius=int(radius),
                      available_real_context_radius_px=float(available),
                      source_short_side_analysis_px=SOURCE_SHORT_SIDE,
                      measurement_diameter_source_px=float(2 * SUPPORT_RADIUS / scale),
                      upsampled=bool(scale > 1))


def sample_context(source, meta, angle=0.):
    """A common physical field of view at every in-plane rotation angle."""
    h, w = source.shape
    center = ((w - 1) / 2, (h - 1) / 2)
    matrix = cv2.getRotationMatrix2D(center, float(angle), meta['scale'])
    matrix[:, 2] += np.array([255.5 - center[0], 255.5 - center[1]])
    return cv2.warpAffine(source, matrix, (512, 512), flags=cv2.INTER_LINEAR,
                          borderMode=cv2.BORDER_REFLECT101)


def score_path(path, angle=0.):
    source, meta = prepare_source(path)
    result = score_gray_context(sample_context(source, meta, angle))
    valid = meta['available_real_context_radius_px'] >= result['required_real_context_radius_px']
    if not valid:
        raise ValueError('Real image context does not cover the complete measurement support')
    return dict(**meta, **result, context_valid=True, angle_deg=float(angle),
                status='EXPERIMENTAL_FIXED_SCALE_UNIFORMITY_INDEX')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--angle', type=float, default=0.)
    args = parser.parse_args()
    print(json.dumps(score_path(args.image, args.angle), ensure_ascii=False, indent=2))
