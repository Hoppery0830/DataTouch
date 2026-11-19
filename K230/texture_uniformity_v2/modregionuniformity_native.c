#include "py/obj.h"
#include "py/objtuple.h"
#include "py/runtime.h"
#include "ndarray.h"

#include "regional_uniformity_v2.h"

static mp_obj_t measure_gray8(mp_obj_t input) {
    if (!mp_obj_is_type(input, &ulab_ndarray_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("Expected a dense 2-D uint8 ulab array"));
    }
    ndarray_obj_t *array = MP_OBJ_TO_PTR(input);
    if (array->ndim != 2 || array->dtype != NDARRAY_UINT8 || !ndarray_is_dense(array)) {
        mp_raise_TypeError(MP_ERROR_TEXT("Expected a dense 2-D uint8 ulab array"));
    }
    const size_t height = array->shape[ULAB_MAX_DIMS - 2];
    const size_t width = array->shape[ULAB_MAX_DIMS - 1];
    if (width >= 32767 || height >= 32767) {
        mp_raise_ValueError(MP_ERROR_TEXT("Unsupported frame dimensions"));
    }

    regional_uniformity_result result;
    const int status = regional_uniformity_v2_measure(array->array, width, height, width, &result);
    switch (status) {
        case REGIONAL_UNIFORMITY_OK:
            break;
        case REGIONAL_UNIFORMITY_INSUFFICIENT_CONTEXT:
            mp_raise_ValueError(MP_ERROR_TEXT("Insufficient original resolution or real image support"));
        case REGIONAL_UNIFORMITY_INSUFFICIENT_LUMINANCE:
            mp_raise_ValueError(MP_ERROR_TEXT("Insufficient mean luminance"));
        case REGIONAL_UNIFORMITY_NO_MEMORY:
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Native workspace allocation failed"));
        default:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("S_uniformity computation failed"));
    }

    mp_obj_t features[6];
    for (size_t i = 0; i < 6; ++i) {
        features[i] = mp_obj_new_float(result.differences[i]);
    }
    mp_obj_t fields[] = {
        mp_obj_new_float(result.score),
        mp_obj_new_tuple(6, features),
        mp_obj_new_bool(result.within_feature_calibration_ranges),
    };
    return mp_obj_new_tuple(3, fields);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(measure_gray8_obj, measure_gray8);

STATIC const mp_rom_map_elem_t regionuniformity_native_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_regionuniformity_native)},
    {MP_ROM_QSTR(MP_QSTR_measure_gray8), MP_ROM_PTR(&measure_gray8_obj)},
};
STATIC MP_DEFINE_CONST_DICT(regionuniformity_native_globals, regionuniformity_native_globals_table);

const mp_obj_module_t regionuniformity_native_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&regionuniformity_native_globals,
};
MP_REGISTER_MODULE(MP_QSTR_regionuniformity_native, regionuniformity_native_module);
