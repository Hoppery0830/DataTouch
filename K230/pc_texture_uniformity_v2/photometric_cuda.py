"""Optional numerical acceleration only; formula is defined in CPU module."""
from functools import lru_cache
import numpy as np,cv2,torch
import torch.nn.functional as F
import u_curve_photometric as cpu

@lru_cache(maxsize=16)
def kernel(k,s):return torch.as_tensor(cv2.getGaussianKernel(k,s,cv2.CV_64F).ravel().copy(),dtype=torch.float32,device='cuda')

def blur(x,k,s):
    g=kernel(k,s);c=x.shape[1];r=k//2
    x=F.conv2d(F.pad(x,(r,r,0,0),mode='reflect'),g.reshape(1,1,1,k).expand(c,1,1,k),groups=c)
    return F.conv2d(F.pad(x,(0,0,r,r),mode='reflect'),g.reshape(1,1,k,1).expand(c,1,k,1),groups=c)

@torch.inference_mode()
def feature_field(gray):
    torch.backends.cuda.matmul.allow_tf32=False;torch.backends.cudnn.allow_tf32=False
    source=torch.as_tensor(np.asarray(gray,np.float32),device='cuda')[None,None]
    sx=torch.tensor([[-3,0,3],[-10,0,10],[-3,0,3]],dtype=torch.float32,device='cuda')[None,None]/16
    sy=sx.transpose(2,3);channels=[]
    for sigma in cpu.base.SIGMAS:
        s=2*sigma;k=2*int(np.ceil(3*s))+1
        sm=blur(source,k,s);padded=F.pad(sm,(1,1,1,1),mode='reflect')
        ix=F.conv2d(padded,sx);iy=F.conv2d(padded,sy)
        moments=blur(torch.cat([ix*ix,ix*iy,iy*iy],1),k,s)
        moments=blur(moments,193,32)[:,:,256:768,256:768]
        moments=F.avg_pool2d(moments,2);xx,xy,yy=moments[0]
        channels.extend([xx+yy,xx-yy,2*xy])
    return torch.stack(channels,-1).to(torch.float64).cpu().numpy()

def score_context(gray):return cpu.score_context(gray,feature_field(gray))
