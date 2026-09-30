#pragma once

// The flat picture - the main menu, a loading screen, a film, a menu on the
// cinema screen - brought back in front of the wearer every so often, the way
// an in-game menu opens where the head looks (the tester, 2026-09-30: "das
// hauptmenu auch wie ingame alle paar momente neuausrichten abhängig wo man
// gerade steht oder hinsieht").
//
// The picture hangs where the head was when it appeared (the held pose, see
// HeadsetRenderer). Turned further than followDegrees from there, or moved
// further than followMetres, for settleSeconds on end, it is taken along: a
// glance aside does not move it, turning to face elsewhere or walking off
// does. followDegrees 0 keeps it where it appeared, as before. Pure, covered
// by flat_follow_test.

#include "core/Types.h"

namespace obvr::vr {

struct FlatFollowSettings {
	float followDegrees = 30.0f;
	float followMetres = 0.5f;
	float settleSeconds = 1.0f;
};

struct FlatFollowState {
	float awaySeconds = 0.0f;
};

// Whether to take a fresh anchor this frame. headingApartDegrees and
// movedMetres are the head's now against the anchor's.
inline bool StepFlatFollow(FlatFollowState& s, float headingApartDegrees, float movedMetres, float dtSeconds,
                           const FlatFollowSettings& settings) {
	if (!(settings.followDegrees > 0.0f)) {
		s.awaySeconds = 0.0f;
		return false;
	}
	const bool away = headingApartDegrees > settings.followDegrees ||
	                  (settings.followMetres > 0.0f && movedMetres > settings.followMetres);
	if (!away) {
		s.awaySeconds = 0.0f;
		return false;
	}
	if (dtSeconds > 0.0f) {
		s.awaySeconds += dtSeconds;
	}
	if (s.awaySeconds >= settings.settleSeconds) {
		s.awaySeconds = 0.0f;
		return true;
	}
	return false;
}

}  // namespace obvr::vr
