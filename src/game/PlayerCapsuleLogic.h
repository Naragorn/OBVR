#pragma once

// The decisions behind the player's body radius (game/PlayerCapsule.h):
// pure, covered by player_capsule_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr float kBodyRadiusScaleMin = 0.3f;
inline constexpr float kBodyRadiusScaleMax = 1.5f;
inline constexpr float kBodyRadiusScaleDefault = 1.0f;

// The setting as used: within 0.3..1.5; not a number is the default.
inline float ClampBodyRadiusScale(float scale) {
	if (!(scale == scale)) {
		return kBodyRadiusScaleDefault;
	}
	if (scale < kBodyRadiusScaleMin) {
		return kBodyRadiusScaleMin;
	}
	return scale > kBodyRadiusScaleMax ? kBodyRadiusScaleMax : scale;
}

// The radius to ask the controller for (Havok units): the game's own
// radius times the scale. 0 when the game's own is no sane radius (no
// controller built yet).
inline float BodyRadiusTarget(float gameRadius, float scale) {
	if (!(gameRadius > 0.0f && gameRadius < 10.0f)) {
		return 0.0f;
	}
	return gameRadius * ClampBodyRadiusScale(scale);
}

// Whether the scale is the game's own (1): then OBVR leaves the radius alone
// - a SetSize from the console or another mod stays.
inline bool BodyRadiusScaled(float scale) {
	const float s = ClampBodyRadiusScale(scale);
	return s > 1.0001f || s < 0.9999f;
}

// Whether to ask: scaled, or back at 1 after OBVR had asked (once, to give
// the game its own radius back); and the controller's target differs from
// the wanted one (the engine puts it back to its own on entering a world).
inline bool BodyRadiusRequestNeeded(float currentTarget, float wanted, bool scaled, bool askedBefore) {
	if (!(wanted > 0.0f) || (!scaled && !askedBefore)) {
		return false;
	}
	const float d = currentTarget - wanted;
	return !(d < 0.0001f && d > -0.0001f);
}

}  // namespace obvr::game
