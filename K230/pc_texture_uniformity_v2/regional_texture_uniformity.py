"""Regional texture uniformity with semantically named inputs and quantities.
Frozen numerical model; rename does not refit or change the equation.
"""
from pathlib import Path
import json,argparse
import numpy as np,cv2
import u_curve_region_wide as tensor_backend

def calculate_difference_features(gray_context,use_gpu=False):
    gray_context=np.asarray(gray_context,np.float32)
    if gray_context.shape!=(1024,1024) or not np.isfinite(gray_context).all() or gray_context.min() < -1e-6 or gray_context.max() > 1+1e-6:
        raise ValueError('Expected finite normalized 1024 x 1024 grayscale context')
    gray_context=np.clip(gray_context,0,1)
    tensor_envelope=tensor_backend.fields_gpu(gray_context) if use_gpu else tensor_backend.fields_cpu(gray_context)
    valid_mask,mask_convolution,center_weights=tensor_backend.masks()
    valid_centers=center_weights>0;normalized_weights=center_weights[valid_centers]
    weight_correction=1-normalized_weights@normalized_weights
    luminance_envelope=cv2.GaussianBlur(gray_context.astype(float)*valid_mask,(193,193),32,borderType=cv2.BORDER_CONSTANT)/np.maximum(mask_convolution,1e-20)
    luminance_envelope=cv2.resize(luminance_envelope,(512,512),interpolation=cv2.INTER_AREA)
    difference_curves=[]
    for region_area in tensor_backend.base.AREAS:
        region_kernel=tensor_backend.base.disk_kernel(float(region_area))
        regional_tensor_means=cv2.filter2D(tensor_envelope,cv2.CV_64F,region_kernel,borderType=cv2.BORDER_CONSTANT)[valid_centers]
        regional_luminance_means=cv2.filter2D(luminance_envelope,cv2.CV_64F,region_kernel,borderType=cv2.BORDER_CONSTANT)[valid_centers]
        tensor_population_mean=normalized_weights@regional_tensor_means
        luminance_population_mean=normalized_weights@regional_luminance_means
        if luminance_population_mean<=1e-6:raise ValueError('Insufficient mean luminance for normalized measurement')
        structure_variances=np.sum((regional_tensor_means-tensor_population_mean)**2*normalized_weights[:,None],axis=0).reshape(5,3).sum(1)/(3*weight_correction*luminance_population_mean**4)
        luminance_variance=np.sum((regional_luminance_means-luminance_population_mean)**2*normalized_weights)/(weight_correction*luminance_population_mean**2)
        difference_curves.append([*structure_variances,luminance_variance])
    log_region_areas=np.log(tensor_backend.base.AREAS)
    integrated_variances=np.trapezoid(difference_curves,log_region_areas,axis=0)/(log_region_areas[-1]-log_region_areas[0])
    return np.r_[integrated_variances[:5]**.25,integrated_variances[5]**.5]

def measure_uniformity(image_path,use_gpu=False):
    calibration=json.loads((Path(__file__).parent/'calibration_parameters.json').read_text('utf-8'))
    sampling=tensor_backend.narrow.appearance.sampling
    source_gray,source_metadata=sampling.prepare_source(Path(image_path))
    if source_metadata['upsampled'] or source_metadata['available_real_context_radius_analysis_px']<220:
        raise ValueError('Insufficient original resolution or true image support')
    differences=calculate_difference_features(sampling.sample_context(source_gray,source_metadata,0),use_gpu)
    weights=np.array([item['weight'] for item in calibration['structure_weights']]+[calibration['luminance_weight']])
    score=float(calibration['calibration_intercept']-differences@weights)
    return dict(regional_texture_uniformity_score=score,score_symbol='S_uniformity',higher_is_more_uniform=True,scale_structure_differences=[dict(texture_scale_pixels=item['texture_scale_pixels'],structure_difference=float(value)) for item,value in zip(calibration['structure_weights'],differences[:5])],regional_luminance_difference=float(differences[5]),calibration_scope=calibration['fit_scope'],within_feature_calibration_ranges=bool(np.all(differences>=calibration['calibration_feature_min']) and np.all(differences<=calibration['calibration_feature_max'])))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('image_path');parser.add_argument('--gpu',action='store_true');args=parser.parse_args();cv2.setNumThreads(1)
    print(json.dumps(measure_uniformity(args.image_path,args.gpu),ensure_ascii=False,indent=2))
