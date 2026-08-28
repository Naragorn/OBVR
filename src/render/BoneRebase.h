#pragma once

#include "core/Types.h"

namespace obvr::render {

// The decision half of the bone lock, after the headset corrected the probes.
//
// The bone palettes are not world-space: the monitor could not show it, but
// in stereo every locked body sat at the first eye's position - no parallax,
// seen double. The measured proof is in the mismatch pairs: matched rows
// differ between the renders by a constant translation of ~4.6 units, which
// is exactly a 66mm IPD at Oblivion's ~1.43cm per unit. So the palettes are
// camera-relative, most second-render rows arrive correct for their eye, and
// blanket replacement was destroying that correctness to fix the minority.
//
// The engine bug is only the minority: rows whose translation is a body
// length off (85 units measured) because the engine re-evaluated the
// skeleton onto the wrong same-posed instance. Those, and only those, get
// the first render's row back - shifted by the eye baseline, which the
// correct rows of the same frame measure as a side effect of being judged.

// A bone row is a 4x3 matrix as three float4 vectors; floats 3, 7 and 11
// are the translation, the other nine the rotation fingerprint.
inline constexpr UInt32 kBoneRowFloats = 12;

inline float BoneTranslationDistSq(const float* a, const float* b) {
	const float dx = a[3] - b[3];
	const float dy = a[7] - b[7];
	const float dz = a[11] - b[11];
	return dx * dx + dy * dy + dz * dz;
}

// Sixteen units squared: the eye baseline measures ~4.6 units (66mm IPD,
// separation scale 1) and an instance mixup a body length (~85 units), so
// sixteen sits a factor of three from either side of the divide.
inline constexpr float kBoneMixupThresholdSq = 256.0f;

enum class BoneRowVerdict { Correct, Mixup };

inline BoneRowVerdict JudgeBoneRow(float translationDistSq, float thresholdSq) {
	return translationDistSq <= thresholdSq ? BoneRowVerdict::Correct
	                                        : BoneRowVerdict::Mixup;
}

// The frame's running estimate of the second render's translation delta -
// the eye baseline in the palette's frame of reference. Every correct row
// is a measurement of it; the mean of a frame's measurements is what a
// mixed-up row in the same frame gets added onto its first-render values.
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

// The best guess right now: this frame's running mean once it has samples,
// the previous frame's mean before that, zero before the first sample ever
// (which reproduces the old blanket lock for exactly that long).
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
