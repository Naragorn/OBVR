#pragma once

#include "core/Types.h"

namespace obvr::render {

// The geometry half of the bone lock, corrected twice by the headset.
//
// The bone palettes are camera-relative: matched rows differ between the
// renders by the eye baseline (~4.6 units at 66mm IPD, measured), so the
// original blanket replay - first render's rows verbatim - froze every
// skinned body at the first eye's position. No parallax, seen double.
//
// A selective lock that kept "correct" rows was tried and failed on both
// ends: rows that arrive bit-identical between the renders are not correct
// but stale - skeletons the second render never re-evaluated, still at the
// first eye - and rows judged "mixups" by their matched partner were mostly
// ring-matching collisions between same-posed instances, rebased onto
// strangers. The kept/rebased split painted bodies grey and cured nothing.
//
// So the lock is blanket again - every matched row is replaced, the exact
// semantics that held the collapse down for good - but the replacement is
// the first render's row SHIFTED by the eye baseline: the left eye's
// skeletons moved one interpupillary distance over, which is what the
// second eye should have computed. The shift comes from the camera code
// (the very vector the dual pass moves the camera by), its sign against
// the palette convention calibrated at runtime from the rows that DID
// re-evaluate - they measure the true baseline as a side effect.

// A bone row is a 4x3 matrix as three float4 vectors; floats 3, 7 and 11
// are the translation, the other nine the rotation fingerprint.
inline constexpr UInt32 kBoneRowFloats = 12;

inline float BoneTranslationDistSq(const float* a, const float* b) {
	const float dx = a[3] - b[3];
	const float dy = a[7] - b[7];
	const float dz = a[11] - b[11];
	return dx * dx + dy * dy + dz * dz;
}

// The baseline-measurement band. Below: bit-identical rows - stale
// skeletons (or camera-free data), which measure nothing. Above: instance
// mixups a body length off (~85 units measured), which measure the wrong
// thing. Between sits the eye baseline: ~4.6 units at separation scale 1,
// and the sixteen-unit ceiling covers scales up to ~3.4 of the INI's
// maximum 4 - past that the measurement merely loses its samples while
// the calibrated camera shift keeps working.
inline constexpr float kBoneStaleThresholdSq = 0.25f;  // half a unit
inline constexpr float kBoneMixupThresholdSq = 256.0f;  // sixteen units

inline bool IsEyeBaselineSample(float distSq) {
	return distSq > kBoneStaleThresholdSq && distSq <= kBoneMixupThresholdSq;
}

// The frame's running measurement of the second render's translation
// delta, fed by the re-evaluated rows. Used directly while the shift sign
// is uncalibrated, and as the calibration evidence afterwards.
struct EyeDeltaEstimate {
	float sum[3] = {0.0f, 0.0f, 0.0f};
	UInt32 samples = 0;
	float previous[3] = {0.0f, 0.0f, 0.0f};  // last finished frame's mean
	bool hasPrevious = false;
};

// Rolls the finished frame's mean into the fallback and starts a new frame.
inline void BeginEyeDeltaFrame(EyeDeltaEstimate& e) {
	if (e.samples > 0) {
		const float inv = 1.0f / static_cast<float>(e.samples);
		e.previous[0] = e.sum[0] * inv;
		e.previous[1] = e.sum[1] * inv;
		e.previous[2] = e.sum[2] * inv;
		e.hasPrevious = true;
	}
	e.sum[0] = e.sum[1] = e.sum[2] = 0.0f;
	e.samples = 0;
}

inline void AddEyeDeltaSample(EyeDeltaEstimate& e, const float* incoming,
                              const float* logged) {
	e.sum[0] += incoming[3] - logged[3];
	e.sum[1] += incoming[7] - logged[7];
	e.sum[2] += incoming[11] - logged[11];
	++e.samples;
}

// The measured mean: this frame's once it has samples, the previous
// frame's before that, zero before the first sample ever.
inline void CurrentEyeDelta(const EyeDeltaEstimate& e, float out[3]) {
	if (e.samples > 0) {
		const float inv = 1.0f / static_cast<float>(e.samples);
		out[0] = e.sum[0] * inv;
		out[1] = e.sum[1] * inv;
		out[2] = e.sum[2] * inv;
	} else if (e.hasPrevious) {
		out[0] = e.previous[0];
		out[1] = e.previous[1];
		out[2] = e.previous[2];
	} else {
		out[0] = out[1] = out[2] = 0.0f;
	}
}

// Whether the palette convention runs with or against the camera shift.
// The dual pass knows the exact vector it moves the camera by, but not
// whether the palettes store "world minus camera" or its negation - the
// measured baseline settles that once, at runtime, from real rows.
enum class ShiftSign { Unknown, Positive, Negative };

// Judges a finished frame's measurement against the camera shift. Needs
// enough samples to trust the mean, a shift that actually happened, and a
// dot product that commits to a side - the measured mean must project onto
// the shift by more than half its length. Anything less stays Unknown.
inline ShiftSign CalibrateShiftSign(const EyeDeltaEstimate& e, const float shift[3],
                                    UInt32 minSamples) {
	if (e.samples < minSamples) {
		return ShiftSign::Unknown;
	}
	const float shiftNormSq =
		shift[0] * shift[0] + shift[1] * shift[1] + shift[2] * shift[2];
	if (shiftNormSq <= kBoneStaleThresholdSq) {
		return ShiftSign::Unknown;
	}
	float mean[3];
	CurrentEyeDelta(e, mean);
	const float dot = mean[0] * shift[0] + mean[1] * shift[1] + mean[2] * shift[2];
	if (dot > 0.5f * shiftNormSq) {
		return ShiftSign::Positive;
	}
	if (dot < -0.5f * shiftNormSq) {
		return ShiftSign::Negative;
	}
	return ShiftSign::Unknown;
}

// The delta a replaced row's translation is shifted by. Calibrated: the
// camera's own shift vector with the calibrated sign - exact and current
// even in frames where no skeleton re-evaluated. Uncalibrated: the
// measured mean, which is zero before the first sample ever and thereby
// reproduces the old blanket lock for exactly that long.
inline void ChooseRebaseDelta(const EyeDeltaEstimate& e, const float shift[3],
                              ShiftSign sign, float out[3]) {
	if (sign == ShiftSign::Positive) {
		out[0] = shift[0];
		out[1] = shift[1];
		out[2] = shift[2];
		return;
	}
	if (sign == ShiftSign::Negative) {
		out[0] = -shift[0];
		out[1] = -shift[1];
		out[2] = -shift[2];
		return;
	}
	CurrentEyeDelta(e, out);
}

// The first render's row, translation shifted into this render's frame of
// reference. Rotation floats are copied bit-for-bit.
inline void RebaseBoneRow(float* out, const float* logged, const float delta[3]) {
	for (UInt32 i = 0; i < kBoneRowFloats; ++i) {
		out[i] = logged[i];
	}
	out[3] += delta[0];
	out[7] += delta[1];
	out[11] += delta[2];
}

}  // namespace obvr::render
