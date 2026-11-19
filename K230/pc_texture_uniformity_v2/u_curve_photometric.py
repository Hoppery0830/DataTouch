"""Route 2: express tensor dispersion in relative grayscale contrast units.

Retains the five-scale Gaussian/Scharr tensor, envelope, circular CB regions.
Replaces normalization by the fabric's own tensor norm with grayscale mean.
The fourth root converts variance of squared derivatives into contrast units.
All fixed constants precede inspection of this route's real-image results.
"""
import cv2,numpy as np
import u_curve_appearance as appearance
base=appearance.base

def feature_field(gray):
    source=np.asarray(gray,np.float32);channels=[]
    for sigma in base.SIGMAS:
        s=2*sigma;k=2*int(np.ceil(3*s))+1
        sm=cv2.GaussianBlur(source,(k,k),s,borderType=cv2.BORDER_REFLECT101)
        ix=cv2.Scharr(sm,cv2.CV_32F,1,0,borderType=cv2.BORDER_REFLECT101)*2/32
        iy=cv2.Scharr(sm,cv2.CV_32F,0,1,borderType=cv2.BORDER_REFLECT101)*2/32
        xx=cv2.GaussianBlur(ix*ix,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        xy=cv2.GaussianBlur(ix*iy,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        yy=cv2.GaussianBlur(iy*iy,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        for component in [xx+yy,xx-yy,2*xy]:
            env=cv2.GaussianBlur(component,(193,193),32,borderType=cv2.BORDER_REFLECT101)[256:768,256:768]
            channels.append(cv2.resize(env,(256,256),interpolation=cv2.INTER_AREA))
    return np.stack(channels,axis=-1)

def score_context(gray,field=None):
    if gray.shape!=(1024,1024) or not np.isfinite(gray).all() or gray.min()<0 or gray.max()>1:raise ValueError('Expected finite grayscale context in [0,1]')
    if field is None:field=feature_field(gray)
    if field.shape!=(256,256,15) or not np.isfinite(field).all():raise ValueError('Invalid tensor field')
    light=cv2.GaussianBlur(gray.astype(np.float64),(193,193),32,borderType=cv2.BORDER_REFLECT101)[256:768,256:768]
    light=cv2.resize(light,(256,256),interpolation=cv2.INTER_AREA)
    weights=base.center_weights();valid=weights>0;w=weights[valid];corr=1-float(w@w)
    structure4=[];tone2=[];relative2=[]
    for area in base.AREAS:
        kernel=base.disk_kernel(float(area))
        means=cv2.filter2D(field,cv2.CV_64F,kernel,borderType=cv2.BORDER_CONSTANT)[valid]
        lm=cv2.filter2D(light,cv2.CV_64F,kernel,borderType=cv2.BORDER_CONSTANT)[valid]
        mu=np.sum(means*w[:,None],axis=0);brightness=float(lm@w)
        if brightness<1e-6:raise ValueError('Insufficient grayscale signal')
        variance=float(np.sum((means-mu)**2*w[:,None])/corr)
        structure4.append(variance/(15*brightness**4))
        tone2.append(float(((lm-brightness)**2)@w)/(corr*brightness**2))
        relative2.append(variance/max(float(mu@mu),1e-20))
    a=np.log(base.AREAS)
    average=lambda v:float(np.trapezoid(v,a)/(a[-1]-a[0]))
    structure=average(structure4)**.25;tone=average(tone2)**.5
    return dict(U_curve_photometric=float(np.hypot(structure,tone)/np.sqrt(2)),structure_contrast=structure,tone_CB=tone,relative_structure_CB=average(relative2)**.5,structure_curve=np.power(structure4,.25).tolist(),tone_curve=np.sqrt(tone2).tolist(),version='U_curve_photometric_v1',status='DEVELOPMENT_CANDIDATE')
