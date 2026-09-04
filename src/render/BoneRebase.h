#pragma once

#include <cstring>

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

// A bone row as the first render uploaded it, kept for the second render
// to pair its own rows against.
struct BoneRow {
	UInt32 startRegister;
	float floats[kBoneRowFloats];
};

// The fingerprint test: the nine rotation floats bit-identical. The
// rotation of a bone is the same in both renders (only its translation
// changes frame of reference), and it is also the same for two instances
// standing in the same pose facing the same way - which is exactly why
// the fingerprint alone cannot tell instances apart.
inline bool SameBoneRotation(const float* a, const float* b) {
	return std::memcmp(a + 0, b + 0, 12) == 0 && std::memcmp(a + 4, b + 4, 12) == 0 &&
	       std::memcmp(a + 8, b + 8, 12) == 0;
}

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

// Whether a render target the bone rows are heading for is the world
// render's. The lock's shift is only right for the world: shadow and
// reflection sub-passes draw the same skeletons into their own textures
// from light or mirror viewpoints, camera-free, and shifting those gives
// every shadow a body's parallax - it stands in the room instead of lying
// on the ground. The world target is screen-sized and the sub-pass
// targets are smaller, so size is the divide; an unmeasured main width
// (zero) claims every target for the world, which is the pre-shift
// behaviour and the safe degradation.
inline bool BoneTargetIsWorldSized(UInt32 targetWidth, UInt32 mainWidth) {
	return mainWidth == 0 || targetWidth >= mainWidth;
}

// How a second-render row was paired with the first render's log.
//
// InPlace: the row at the running position has the fingerprint. In a steady
// frame the uploads arrive in the first render's order, so this is the
// same draw's row whatever its translation says. Reordered: the running
// position did not match, but a row further along the ring did - the pair
// after a camera turn resorted the uploads, a part of the same body the
// first render did not draw, or the ring slipped because the second render
// skipped a row the first had uploaded. None: nothing in the log carries
// the fingerprint.
//
// The pair is the FIRST fingerprint match along the ring, whatever its
// translation says. A variant that refused off-position matches whose
// translation disagreed by more than a body length - meant to keep a row
// the first render never uploaded from latching onto a same-posed
// stranger - brought the collapse back and was reverted on the evidence of
// a headset log: in frames where the second render uploaded fewer rows
// than the first (1761 against 1793), the ring slipped, every pair after
// the slip sat off position, and every one of them carried the engine's
// mixed-up translation - a body length off, as the mixup always is - so
// 1322 of 1414 rows were refused and passed through as the engine wrote
// them, which is the collapse itself. The second render's translations are
// the thing the lock exists to overwrite; they cannot also be the evidence
// for whether to overwrite them. A stranger's translation is still a wrong
// translation, but it is one same-posed body's position instead of the
// mixup's, and the frames that slip are the ones a stranger would have
// wrecked anyway.
enum class BoneMatchKind { None, InPlace, Reordered };

struct BoneMatch {
	BoneMatchKind kind = BoneMatchKind::None;
	UInt32 index = 0;  // the paired row's position in the log
};

// Pairs an incoming row with the first render's log: the running position
// first, then the first row along the ring with the same register and
// fingerprint. A count of zero pairs nothing.
inline BoneMatch FindBoneRow(const BoneRow* log, UInt32 count, UInt32 running,
                             UInt32 startRegister, const float* incoming) {
	BoneMatch match;
	if (count == 0) {
		return match;
	}
	for (UInt32 step = 0; step < count; ++step) {
		const UInt32 probe = (running + step) % count;
		const BoneRow& candidate = log[probe];
		if (candidate.startRegister != startRegister ||
		    !SameBoneRotation(candidate.floats, incoming)) {
			continue;
		}
		match.kind = step == 0 ? BoneMatchKind::InPlace : BoneMatchKind::Reordered;
		match.index = probe;
		return match;
	}
	return match;
}

// Whether any logged row - any fingerprint, any register - has its
// translation within the mixup band of the given row. Answers, for a row
// that arrived a body length from its pair, whether the wrong translation
// is a place some other body stands in (the engine handed it a neighbour's
// root) or nowhere the first render drew anything.
inline bool LogHoldsTranslation(const BoneRow* log, UInt32 count, const float* floats) {
	for (UInt32 i = 0; i < count; ++i) {
		if (BoneTranslationDistSq(log[i].floats, floats) <= kBoneMixupThresholdSq) {
			return true;
		}
	}
	return false;
}

// The capacity of the first-render log.
inline constexpr UInt32 kBoneLogRows = 4096;  // busy frames carry ~1800 bone uploads

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
