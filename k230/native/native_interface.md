# K230 native rotated-window interface

## Frozen boundary

The built-in module is `gunay_native`. One call is made per Gaussian scale:

`region_stats(T, Q, U, plan_offsets, spans, plan_meta)`

- `T/Q/U`: dense 256 x 256 ulab float arrays.
- `plan_offsets`: `array('I')`, length 46, indexing the span triples for all 45 windows.
- `spans`: flattened `array('h')` triples `(dy, xlo, xhi)` with inclusive x bounds.
- `plan_meta`: flattened `array('H')` triples `(rx, ry, actual_pixel_count)`.
- return: 45 tuples `(T_mean, T_sample_variance, Q_mean, Q_sample_variance, U_mean, U_sample_variance)`.

The C loop visits every valid center, every frozen scanline span, and every pixel in that span. It accumulates T/Q/U together and uses the same float Welford sample-variance recurrence as ulab. It allocates no image-sized workspace and constructs no row prefix.

Geometry, Gaussian scales, Scharr/Structure Tensor, T/Q/U definitions, theta_main, Square/Dir1/Dir2 windows, Ak, multivariate CB, curve normalization, reference curves, and U_curve remain outside the native module and are unchanged.

## Pre-registered numerical gates

- `abs(U_native - 0.151281500000) <= 1e-5`.
- normalized CB curve absolute error `<= 1e-5` against the completed K230 cached-prefix baseline.
- raw CB relative tolerance `1e-4` with an absolute floor of `1e-4`.
- PC float64 region-mean relative tolerance `2e-4` with an absolute floor of `2e-4`.
- PC float64 region-variance relative tolerance `5e-4` with an absolute floor of `5e-4`.
- all reported values finite; no OOM or exception.

Any numerical-gate failure is a hard stop before performance acceptance.
