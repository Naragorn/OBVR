#pragma once

// The weapon places shown faintly while the player looks down (Nadi,
// 2026-11-14: "wenn man runter schaut sieht man nun die holster stellen leicht
// eingeblendet als overlay"): the four places' circles of the places' window
// (vr/HolsterPlace.h, ui::PaintStowSpot), at kGlanceOpacity, faded in from
// kGlanceStartDegrees below the horizon to kGlanceFullDegrees, faded out the
// same way. [Hands] HolsterGlance switches it. Pure, covered by
// holster_glance_test.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr float kGlanceStartDegrees = 30.0f;  // below the horizon: nothing above this
inline constexpr float kGlanceFullDegrees = 45.0f;   // and all of kGlanceOpacity from here down
inline constexpr float kGlanceOpacity = 0.35f;
inline constexpr float kGlanceFadeSeconds = 0.25f;   // from nothing to kGlanceOpacity and back
inline constexpr float kGlanceShownFrom = 0.01f;

struct HolsterGlanceSettings {
	bool enabled = true;  // [Hands] HolsterGlance
};

// How visible the places should be: `pitchDegrees` the head's, up positive;
// `allowed` Full VR in the world, no menu, the places' window not up.
inline float GlanceTarget(const HolsterGlanceSettings& s, float pitchDegrees, bool allowed) {
	if (!s.enabled || !allowed || !(pitchDegrees == pitchDegrees)) {
		return 0.0f;
	}
	const float down = -pitchDegrees;
	if (down <= kGlanceStartDegrees) {
		return 0.0f;
	}
	if (down >= kGlanceFullDegrees) {
		return kGlanceOpacity;
	}
	return kGlanceOpacity * (down - kGlanceStartDegrees) / (kGlanceFullDegrees - kGlanceStartDegrees);
}

// One frame of the fade toward `target`.
inline float StepGlanceAlpha(float now, float target, float dtSeconds) {
	const float from = now == now ? now : 0.0f;
	if (!(dtSeconds > 0.0f)) {
		return from;
	}
	const float step = kGlanceOpacity * dtSeconds / kGlanceFadeSeconds;
	if (from < target) {
		return from + step < target ? from + step : target;
	}
	return from - step > target ? from - step : target;
}

inline bool GlanceShown(float alpha) { return alpha >= kGlanceShownFrom; }

}  // namespace obvr::vr
