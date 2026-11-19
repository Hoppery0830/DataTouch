"""Two-samples-per-analysis-pixel evaluation of the fixed tensor-CB index.

The envelope, support, area range and final formula are unchanged from the
fixed-scale candidate. Tensor derivatives are evaluated before final decimation.
No angles or human ratings are used to choose parameters.
"""
from pathlib import Path
import argparse
import json
import cv2
import numpy as np
import u_curve_fixed_ri as base

VERSION = 'U_curve_subpixel_RI_v1'
SAMPLING = 2


def score_gray_context(gray):
    if gray.shape != (1024, 1024) or not np.isfinite(gray).all():
        raise ValueError('Expected finite 1024x1024 grayscale with real context')
    if gray.min() < 0 or gray.max() > 1:
        raise ValueError('Grayscale values must be in [0,1]')
    source = gray.astype(np.float32)
    channels = []
    integration_sigma = base.ENVELOPE_SIGMA * SAMPLING
    radius = int(np.ceil(3 * integration_sigma))
    for sigma in base.SIGMAS:
        s = sigma * SAMPLING
        k = 2 * int(np.ceil(3 * s)) + 1
        sm = cv2.GaussianBlur(source, (k,k), s, borderType=cv2.BORDER_REFLECT101)
        ix = cv2.Scharr(sm, cv2.CV_32F, 1, 0, borderType=cv2.BORDER_REFLECT101) * SAMPLING
        iy = cv2.Scharr(sm, cv2.CV_32F, 0, 1, borderType=cv2.BORDER_REFLECT101) * SAMPLING
        xx = cv2.GaussianBlur(ix*ix, (k,k), s, borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        xy = cv2.GaussianBlur(ix*iy, (k,k), s, borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        yy = cv2.GaussianBlur(iy*iy, (k,k), s, borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        for component in (xx+yy, xx-yy, 2*xy):
            env = cv2.GaussianBlur(component, (2*radius+1,2*radius+1), integration_sigma,
                                   borderType=cv2.BORDER_REFLECT101)[256:768,256:768]
            channels.append(cv2.resize(env, (256,256), interpolation=cv2.INTER_AREA))
    result = base.score_envelope(np.stack(channels,axis=-1))
    value = result.pop('U_curve_fixed_RI')
    required = base.SUPPORT_RADIUS + .5 + 23 + np.sqrt(2)*(24 + .5 + 48 + .5)
    return dict(U_curve_subpixel_RI=value, **result, version=VERSION, sampling_factor=SAMPLING,
                envelope_sigma_analysis_px=base.ENVELOPE_SIGMA,
                required_real_context_radius_analysis_px=float(required),
                measurement='Spatial variation of fixed-scale tensor envelopes; lower is more uniform',
                scope_note='Coarse regular patterns and illumination can confound this index; not a validated universal fabric-uniformity score')


def prepare_source(path):
    bgr = cv2.imdecode(np.fromfile(str(path),np.uint8),cv2.IMREAD_COLOR)
    if bgr is None:
        raise FileNotFoundError(path)
    gray = cv2.cvtColor(bgr,cv2.COLOR_BGR2GRAY).astype(np.float32)/255
    return prepare_gray_source(gray)


def prepare_gray_source(gray):
    """Same input path for an in-memory image or camera frame, without SSD I/O."""
    gray = np.asarray(gray, np.float32)
    if gray.ndim != 2 or min(gray.shape) < 2 or not np.isfinite(gray).all():
        raise ValueError('Expected a finite grayscale image')
    if gray.min() < 0 or gray.max() > 1:
        raise ValueError('Grayscale values must be in [0,1]')
    h,w = gray.shape
    scale = base.SOURCE_SHORT_SIDE * SAMPLING / min(h,w)
    sigma = .5*np.sqrt(max(1/scale**2-1,0))
    radius = int(np.ceil(3*sigma))
    if radius:
        gray = cv2.GaussianBlur(gray,(2*radius+1,2*radius+1),sigma,borderType=cv2.BORDER_REFLECT101)
    available = ((min(h,w)-1)*scale/2 - np.sqrt(2)*(radius+1)*scale)/SAMPLING
    return gray, dict(source_width=w,source_height=h,scale=float(scale),
                      anti_alias_sigma=float(sigma),anti_alias_radius=int(radius),
                      available_real_context_radius_analysis_px=float(available),
                      source_short_side_analysis_px=base.SOURCE_SHORT_SIDE,
                      measurement_diameter_source_px=float(2*base.SUPPORT_RADIUS*SAMPLING/scale),
                      upsampled=bool(scale>1))


def sample_context(source,meta,angle=0.):
    h,w = source.shape
    center = ((w-1)/2,(h-1)/2)
    matrix = cv2.getRotationMatrix2D(center,float(angle),meta['scale'])
    matrix[:,2] += np.array([511.5-center[0],511.5-center[1]])
    return cv2.warpAffine(source,matrix,(1024,1024),flags=cv2.INTER_LINEAR,
                          borderMode=cv2.BORDER_REFLECT101)


def score_path(path,angle=0.):
    source,meta = prepare_source(path)
    result = score_gray_context(sample_context(source,meta,angle))
    if meta['available_real_context_radius_analysis_px'] < result['required_real_context_radius_analysis_px']:
        raise ValueError('Insufficient real context')
    return dict(**meta,**result,context_valid=True,angle_deg=float(angle),
                status='EXPERIMENTAL_FIXED_SCALE_UNIFORMITY_INDEX')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--angle',type=float,default=0.)
    args=parser.parse_args()
    print(json.dumps(score_path(args.image,args.angle),ensure_ascii=False,indent=2))
