"""Appearance-aware revision of the original tensor-CB algorithm.

Two dimensionless dispersion components, with fixed equal squared weights.
No human scores or image IDs are used by this inference module.
"""
from pathlib import Path
import sys, argparse, json
import numpy as np
import cv2
CORE=Path(__file__).resolve().parent
sys.path.insert(0,str(CORE))
import u_curve_fixed_ri as base
import u_curve_subpixel_ri as sampling

VERSION='U_curve_appearance_v1'

def scalar_cb(field):
    weights=base.center_weights();valid=weights>0;w=weights[valid]
    correction=1-float(w@w);out=[]
    for area in base.AREAS:
        means=cv2.filter2D(field,cv2.CV_64F,base.disk_kernel(float(area)),borderType=cv2.BORDER_CONSTANT)[valid]
        mu=float(means@w)
        if mu<=1e-6:raise ValueError('Insufficient grayscale signal')
        variance=float(((means-mu)**2)@w)/correction
        out.append(float(np.sqrt(max(variance,0))/mu))
    loga=np.log(base.AREAS)
    return float(np.sqrt(np.trapezoid(np.square(out),loga)/(loga[-1]-loga[0]))),out

def tone_context(gray):
    env=cv2.GaussianBlur(gray.astype(np.float64),(193,193),32,borderType=cv2.BORDER_REFLECT101)[256:768,256:768]
    return scalar_cb(cv2.resize(env,(256,256),interpolation=cv2.INTER_AREA))

def score_context(gray):
    structural=sampling.score_gray_context(gray)
    tone,curve=tone_context(gray)
    structure=structural['U_curve_subpixel_RI']
    value=float(np.hypot(structure,tone)/np.sqrt(2))
    return dict(U_curve_appearance=value,structure_CB=structure,tone_CB=tone,
                structure_curve=structural['raw_CB_fraction'],tone_curve=curve,
                version=VERSION,status='DEVELOPMENT_CANDIDATE',lower_is_more_uniform=True)

def score_path(path,angle=0):
    source,meta=sampling.prepare_source(path)
    if meta['upsampled']:raise ValueError('Short side must be at least 896 pixels; do not infer lost texture from upsampling')
    if meta['available_real_context_radius_analysis_px']<199:raise ValueError('Insufficient real image context')
    return dict(**score_context(sampling.sample_context(source,meta,angle)),**meta,angle_deg=float(angle))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);p.add_argument('--angle',type=float,default=0)
    args=p.parse_args();cv2.setNumThreads(1)
    print(json.dumps(score_path(args.image,args.angle),ensure_ascii=False,indent=2))
