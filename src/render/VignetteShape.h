#pragma once

// How dark a vignette is across its ramp (render/VignetteLayer.cpp): t runs
// from 0 where the darkening starts to 1 where it is full. The snap turn's
// vignette keeps its quadratic ramp; the sneak vignette takes the smoothstep
// (Nadi, 2026-11-14: "zu hell am rande" - the square keeps most of the ramp
// light; comfort vignettes reach full black at the edge over a wide, even ramp,
// Fernandes & Feiner 2016, Unity XRI's tunneling vignette). Pure, vignette_shape_test.

namespace obvr::render {

inline float VignetteRampAlpha(float t, bool smooth) {
	if (!(t > 0.0f)) {
		return 0.0f;  // a NaN too
	}
	if (t >= 1.0f) {
		return 1.0f;
	}
	return smooth ? t * t * (3.0f - 2.0f * t) : t * t;
}

}  // namespace obvr::render
