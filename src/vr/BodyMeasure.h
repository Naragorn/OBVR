#pragma once

// Measuring the player in a T-pose (Nadi, 2026-10-22, after Titanfall 2 VR's
// start: "beim spielstart eine pose wo man T Pose machen soll und trigger auf
// beiden controller drückt"): standing straight, both arms out to the sides,
// both triggers pulled. Two numbers come out of it and two settings follow:
//
//   the eye height over the floor  ->  the weapons' places, the quiver's,
//   (OpenVR's standing universe,       the stow spot's and the mouth's (since
//                                      2026-10-25) and the hip's (since
//                                      2026-10-26), scaled from their
//    whose y = 0 is the floor)         defaults - which are a standing adult's
//                                      with the eyes 1.6 m up (vr/Holster.h)
//   the span between the controllers  ->  how far the body's arms may stretch
//                                      to reach the hands ([Body] ArmStretch)
//
// Places fitted by hand before are replaced (the page says so); a side chosen
// for a place stays (the places are stored the right-handed way, the sides
// apart). With no floor to read the places are left as they are and only the
// arms are measured. Out of a plausible range nothing changes and the reason
// is said. Seated play is not measured: "I'm seated" on the page leaves Full
// VR for the gamepad (ui/FirstStart.h, Nadi 2026-10-22). Pure, covered by
// body_measure_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Archery.h"
#include "vr/Holster.h"
#include "vr/OneHandTwoHander.h"
#include "vr/Stow.h"

namespace obvr::vr {

// ---------------------------------------------------------------- The pose

// A hand's place as BodyRelative gives it: right, ahead, up from the eyes,
// metres, along the head's heading.
struct TPoseInput {
	bool rightValid = false;
	bool leftValid = false;
	NiPoint3 right{0.0f, 0.0f, 0.0f};
	NiPoint3 left{0.0f, 0.0f, 0.0f};
	bool rightTrigger = false;
	bool leftTrigger = false;
	float dtSeconds = 0.0f;
};

// Each hand at least this far out to its side of the eyes. Half an adult's
// span is 0.65 m and more; 0.45 leaves room for bent elbows and a small
// player, and no hand held before the body comes near it.
inline constexpr float kTPoseHandOut = 0.45f;
// Neither hand further ahead of or behind the eyes than this.
inline constexpr float kTPoseAheadMost = 0.35f;
// Both hands between this far below the eyes and this far above: the
// shoulders sit some 0.15 to 0.25 m under the eyes.
inline constexpr float kTPoseLowest = -0.55f;
inline constexpr float kTPoseHighest = 0.10f;
// The two hands level with each other within this.
inline constexpr float kTPoseLevel = 0.20f;
// How long the pose with both triggers is held before it counts.
inline constexpr float kTPoseHoldSeconds = 0.5f;

inline float TPoseAbs(float v) { return v < 0.0f ? -v : v; }

// Whether the two hands stand as a T: one out to each side, level, about
// shoulder high, neither far ahead. Either controller may be on either side.
inline bool InTPose(const TPoseInput& in) {
	if (!in.rightValid || !in.leftValid) {
		return false;
	}
	const NiPoint3& a = in.right;
	const NiPoint3& b = in.left;
	const bool sides = (a.x >= kTPoseHandOut && b.x <= -kTPoseHandOut) ||
	                   (b.x >= kTPoseHandOut && a.x <= -kTPoseHandOut);
	const bool ahead = TPoseAbs(a.y) <= kTPoseAheadMost && TPoseAbs(b.y) <= kTPoseAheadMost;
	const bool height = a.z >= kTPoseLowest && a.z <= kTPoseHighest && b.z >= kTPoseLowest && b.z <= kTPoseHighest;
	const bool level = TPoseAbs(a.z - b.z) <= kTPoseLevel;
	return sides && ahead && height && level;
}

struct TPoseState {
	float heldSeconds = 0.0f;
	bool fired = false;  // measured; again only after both triggers let go
};

// True once, when both triggers have been held in the pose for
// kTPoseHoldSeconds. Out of the pose or a trigger let go starts the hold
// over; after a measurement both triggers must open before the next.
inline bool StepTPose(TPoseState& s, const TPoseInput& in) {
	const bool both = in.rightTrigger && in.leftTrigger;
	if (!in.rightTrigger && !in.leftTrigger) {
		s.fired = false;
	}
	if (s.fired || !both || !InTPose(in)) {
		s.heldSeconds = 0.0f;
		return false;
	}
	const float dt = in.dtSeconds > 0.0f && in.dtSeconds < 0.25f ? in.dtSeconds : 0.0f;
	s.heldSeconds += dt;
	if (s.heldSeconds < kTPoseHoldSeconds) {
		return false;
	}
	s.fired = true;
	s.heldSeconds = 0.0f;
	return true;
}

// ---------------------------------------------------------- The measuring

// The eye height the places' defaults stand for (vr/Holster.h: "A standing
// adult's eyes are about 1.6 m up and the hip about 0.95 m").
inline constexpr float kPlacesEyeHeightMetres = 1.6f;
// The body's arm, shoulder to wrist, in units: the upper arm 21.85 to the
// forearm, the forearm 14.78 to the hand (game/BodyIkLogic.h, the harness's
// Debug.BodyTreeProbe of 2026-10-11) - at [Head] UnitsPerMetre 69.99125.
inline constexpr float kCharacterArmUnits = 21.85f + 14.78f;
// The arm's length, shoulder to wrist, for the span between the controllers.
// Derived, not measured on people: the span fingertip to fingertip is about
// the stature H; a controller is held in the palm, about 0.6 of a hand
// (0.128 H) back from the fingertips, so the controllers stand 0.846 H apart;
// shoulder to wrist is 0.334 H (Contini, Drillis and Bluestein's ratios,
// O&P Library 1972: upper arm 0.189 H, forearm 0.145 H). 0.334 / 0.846.
inline constexpr float kArmPerControllerSpan = 0.395f;

// Plausible ranges: outside them nothing changes.
inline constexpr float kSpanLeast = 1.0f;
inline constexpr float kSpanMost = 2.2f;
inline constexpr float kEyeLeast = 1.2f;
inline constexpr float kEyeMost = 2.1f;
// The places' scale is held to this range whatever the eyes read.
inline constexpr float kPlaceScaleLeast = 0.8f;
inline constexpr float kPlaceScaleMost = 1.25f;
// The arms' stretch: never under the default the body IK ships with, never
// over the settings row's top; 5 % beyond what the measured arm needs.
inline constexpr float kArmStretchLeast = 1.15f;
inline constexpr float kArmStretchMost = 1.5f;
inline constexpr float kArmStretchMargin = 1.05f;

struct BodyMeasureInput {
	float spanMetres = 0.0f;       // between the two controllers
	bool eyeHeightKnown = false;   // the standing universe read
	float eyeHeightMetres = 0.0f;  // over the floor
	float unitsPerMetre = 69.99125f;
};

struct BodyMeasure {
	bool ok = false;
	const char* refusal = nullptr;  // why nothing changed
	float spanMetres = 0.0f;
	float eyeHeightMetres = 0.0f;
	// The places: set only standing with the floor read.
	bool placesSet = false;
	const char* placesWhy = nullptr;  // why not, when not
	float placeScale = 1.0f;
	NiPoint3 oneHand{0.0f, 0.0f, 0.0f};
	NiPoint3 twoHand{0.0f, 0.0f, 0.0f};
	NiPoint3 bow{0.0f, 0.0f, 0.0f};
	NiPoint3 quiver{0.0f, 0.0f, 0.0f};
	float stowRight = 0.0f;
	float stowForward = 0.0f;
	float stowUp = 0.0f;
	float mouthDown = 0.0f;     // the mouth below and ahead of the eyes (vr/Stow.h)
	float mouthForward = 0.0f;
	float hipBelowEyes = 0.0f;  // the hip under the eyes (vr/OneHandTwoHander.h)
	// The arms.
	float armMetres = 0.0f;
	float armStretch = kArmStretchLeast;
};

inline float MeasureClamp(float v, float least, float most) {
	return v < least ? least : (v > most ? most : v);
}

inline BodyMeasure MeasureBody(const BodyMeasureInput& in) {
	BodyMeasure m;
	m.spanMetres = in.spanMetres;
	m.eyeHeightMetres = in.eyeHeightMetres;
	if (!(in.spanMetres >= kSpanLeast) || !(in.spanMetres <= kSpanMost)) {
		m.refusal = "the hands were not as far apart as arms held out are (1.0 to 2.2 m)";
		return m;
	}
	m.ok = true;

	// The arms: shoulder to wrist from the span, against the body's own.
	m.armMetres = kArmPerControllerSpan * in.spanMetres;
	const float upm = in.unitsPerMetre > 1.0f ? in.unitsPerMetre : 69.99125f;
	const float characterArmMetres = kCharacterArmUnits / upm;
	m.armStretch = MeasureClamp(kArmStretchMargin * m.armMetres / characterArmMetres, kArmStretchLeast,
	                            kArmStretchMost);

	// The places.
	if (!in.eyeHeightKnown) {
		m.placesWhy = "no floor to measure the eyes from - the places stay as they are";
		return m;
	}
	if (!(in.eyeHeightMetres >= kEyeLeast) || !(in.eyeHeightMetres <= kEyeMost)) {
		m.placesWhy = "the eyes read outside 1.2 to 2.1 m over the floor - the places stay as they are";
		return m;
	}
	m.placesSet = true;
	m.placeScale = MeasureClamp(in.eyeHeightMetres / kPlacesEyeHeightMetres, kPlaceScaleLeast, kPlaceScaleMost);
	const HolsterSettings holster{};
	const ArcherySettings archery{};
	const StowSettings stow{};
	m.oneHand = holster.oneHandZone * m.placeScale;
	m.twoHand = holster.twoHandZone * m.placeScale;
	m.bow = holster.bowZone * m.placeScale;
	m.quiver = archery.quiverZone * m.placeScale;
	m.stowRight = stow.centreRight * m.placeScale;
	m.stowForward = stow.centreForward * m.placeScale;
	m.stowUp = stow.centreUp * m.placeScale;
	m.mouthDown = stow.mouthDown * m.placeScale;
	m.mouthForward = stow.mouthForward * m.placeScale;
	m.hipBelowEyes = kHipBelowEyesMetres * m.placeScale;
	return m;
}

}  // namespace obvr::vr
