"""Region-only tensor/gray dispersion over a larger circular observation field."""
from functools import lru_cache
import numpy as np,cv2
import u_curve_photometric as narrow
base=narrow.base
VERSION='U_curve_region_wide_v2'

@lru_cache(maxsize=1)
def masks():
    y,x=np.mgrid[:1024,:1024];mask=np.clip(370.5-np.hypot(x-511.5,y-511.5),0,1)
    den=cv2.GaussianBlur(mask,(193,193),32,borderType=cv2.BORDER_CONSTANT)
    y,x=np.mgrid[:512,:512];weights=np.clip(160.5-np.hypot(x-255.5,y-255.5),0,1);weights/=weights.sum()
    return mask,den,weights

def fields_cpu(gray):
    source=np.asarray(gray,np.float32);mask,den,_=masks();channels=[]
    for sigma in base.SIGMAS:
        s=2*sigma;k=2*int(np.ceil(3*s))+1;sm=cv2.GaussianBlur(source,(k,k),s,borderType=cv2.BORDER_REFLECT101)
        ix=cv2.Scharr(sm,cv2.CV_32F,1,0,borderType=cv2.BORDER_REFLECT101)/16
        iy=cv2.Scharr(sm,cv2.CV_32F,0,1,borderType=cv2.BORDER_REFLECT101)/16
        xx=cv2.GaussianBlur(ix*ix,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        xy=cv2.GaussianBlur(ix*iy,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        yy=cv2.GaussianBlur(iy*iy,(k,k),s,borderType=cv2.BORDER_REFLECT101).astype(np.float64)
        for v in [xx+yy,xx-yy,2*xy]:
            env=cv2.GaussianBlur(v*mask,(193,193),32,borderType=cv2.BORDER_CONSTANT)/np.maximum(den,1e-20)
            channels.append(cv2.resize(env,(512,512),interpolation=cv2.INTER_AREA))
    return np.stack(channels,-1)

def fields_gpu(gray):
    import torch
    import torch.nn.functional as F
    from photometric_cuda import blur
    torch.backends.cuda.matmul.allow_tf32=False;torch.backends.cudnn.allow_tf32=False
    with torch.inference_mode():
        source=torch.as_tensor(np.asarray(gray,np.float32),device='cuda')[None,None]
        mask=torch.as_tensor(masks()[0],dtype=torch.float32,device='cuda')[None,None]
        den=blur(mask,193,32).clamp_min(1e-20)
        sx=torch.tensor([[-3,0,3],[-10,0,10],[-3,0,3]],dtype=torch.float32,device='cuda')[None,None]/16;sy=sx.transpose(2,3);channels=[]
        for sigma in base.SIGMAS:
            s=2*sigma;k=2*int(np.ceil(3*s))+1;p=F.pad(blur(source,k,s),(1,1,1,1),mode='reflect')
            ix=F.conv2d(p,sx);iy=F.conv2d(p,sy)
            moments=blur(torch.cat([ix*ix,ix*iy,iy*iy],1),k,s)
            env=blur(moments*mask,193,32)/den
            xx,xy,yy=F.avg_pool2d(env,2)[0];channels.extend([xx+yy,xx-yy,2*xy])
        return torch.stack(channels,-1).to(torch.float64).cpu().numpy()

def score_context(gray,gpu=False,field=None):
    gray=np.asarray(gray,np.float32)
    if gray.shape!=(1024,1024) or not np.isfinite(gray).all() or gray.min()<0 or gray.max()>1:raise ValueError('Expected grayscale1024 context in [0,1]')
    if field is None:field=fields_gpu(gray) if gpu else fields_cpu(gray)
    mask,den,weights=masks();valid=weights>0;w=weights[valid];corr=1-float(w@w)
    texture_energy=float(np.sum(field[valid][:,::3]*w[:,None])/5)
    light=cv2.GaussianBlur(gray.astype(np.float64)*mask,(193,193),32,borderType=cv2.BORDER_CONSTANT)/np.maximum(den,1e-20)
    light=cv2.resize(light,(512,512),interpolation=cv2.INTER_AREA);s4=[];t2=[]
    for area in base.AREAS:
        kernel=base.disk_kernel(float(area));means=cv2.filter2D(field,cv2.CV_64F,kernel,borderType=cv2.BORDER_CONSTANT)[valid]
        lm=cv2.filter2D(light,cv2.CV_64F,kernel,borderType=cv2.BORDER_CONSTANT)[valid];mu=np.sum(means*w[:,None],axis=0);brightness=float(lm@w)
        if brightness<=1e-6:raise ValueError('Insufficient grayscale signal')
        s4.append(float(np.sum((means-mu)**2*w[:,None]))/(corr*15*brightness**4))
        t2.append(float(((lm-brightness)**2)@w)/(corr*brightness**2))
    x=np.log(base.AREAS);avg=lambda v:float(np.trapezoid(v,x)/(x[-1]-x[0]));s=avg(s4)**.25;t=avg(t2)**.5
    return dict(U_region=float(np.hypot(s,t)/np.sqrt(2)),structure_contrast=s,tone_CB=t,texture_energy=texture_energy,structure_curve=np.power(s4,.25).tolist(),tone_curve=np.sqrt(t2).tolist(),version=VERSION)
