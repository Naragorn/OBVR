#pragma once

// The view at the player's own height ([Head] RealHeight; a tester,
// 2026-11-06: "height seems always slightly off so my character is always
// shorter than me"; docs/player-body-spec.md).
//
// The camera is the game's eye plus the head's move from the recenter's zero
// (vr/HeadTracker.h), so the view stood at the character's eye height
// whatever the player's: 117.9 to 119.4 units over the feet in the harness
// (1.69 to 1.71 m) with the scripted head at 1.60 m, measured 2026-11-06.
// At a recenter - the T-pose is one - the eyes' height over the room's floor
// (the standing universe, SteamVR's room setup) is read, and the difference
// to the character's eye, read in the same frame, is the lift: added to the
// camera's height every frame, outside the lean's limit, in first person on
// foot. The shown body is scaled by the real eye over the character's, about
// its feet, so its feet stay on the ground and its neck comes under the real
// head instead of the body being lifted off the floor.
//
// Refused - no lift, the character's height as before - with the setting
// off, with no floor read, with the eyes outside 1.2 to 2.1 m over the floor
// (vr/BodyMeasure.h's plausible range: sitting, or a floor set wrong) and
// with the character's eye unread or not a standing one. Seated players turn
// the setting off: their eyes over the floor are not their standing height.
// Doorways and ceilings stay the character's.
//
// Pure; real_height_test.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr float kRealHeightEyeLeastMetres = 1.2f;
inline constexpr float kRealHeightEyeMostMetres = 2.1f;
// The lift is held within this either way, whatever the two heights read.
inline constexpr float kRealHeightMostMetres = 0.5f;
// A character eye over the feet the lift is taken against, in units: the
// races stand between about 100 and 135 (Bosmer to Altmer); a sneak or a
// half-loaded body reads outside.
inline constexpr float kRealHeightCharacterLeastUnits = 80.0f;
inline constexpr float kRealHeightCharacterMostUnits = 170.0f;
// The shown body's scale is held to this range.
inline constexpr float kRealHeightBodyScaleLeast = 0.7f;
inline constexpr float kRealHeightBodyScaleMost = 1.4f;

inline bool RealHeightNumber(float v) { return v == v && v > -1.0e6f && v < 1.0e6f; }

inline bool CharacterEyePlausible(float units) {
	return RealHeightNumber(units) && units >= kRealHeightCharacterLeastUnits && units <= kRealHeightCharacterMostUnits;
}

// The lift for a real eye height against a character's, both in units over
// the feet, held within half a metre. 0 for no number or an implausible
// character eye.
inline float RealHeightLift(float realEyeUnits, float characterEyeUnits, float unitsPerMetre) {
	if (!RealHeightNumber(realEyeUnits) || !CharacterEyePlausible(characterEyeUnits) || !(unitsPerMetre > 0.0f) ||
	    !RealHeightNumber(unitsPerMetre)) {
		return 0.0f;
	}
	const float most = kRealHeightMostMetres * unitsPerMetre;
	const float lift = realEyeUnits - characterEyeUnits;
	return lift > most ? most : (lift < -most ? -most : lift);
}

// The eyes over the room's floor at a recenter, in units: known only with
// the setting on, a floor read and the eyes inside 1.2 to 2.1 m.
struct RealEye {
	bool known = false;
	float units = 0.0f;
	const char* why = nullptr;  // a refusal, said in the log
};

inline RealEye RealEyeAtRecenter(bool enabled, bool floorKnown, float eyeOverFloorMetres, float unitsPerMetre) {
	RealEye e;
	if (!enabled) {
		e.why = "Head.RealHeight is off";
		return e;
	}
	if (!floorKnown) {
		e.why = "no floor to measure the eyes from";
		return e;
	}
	if (!RealHeightNumber(eyeOverFloorMetres) || eyeOverFloorMetres < kRealHeightEyeLeastMetres ||
	    eyeOverFloorMetres > kRealHeightEyeMostMetres) {
		e.why = "the eyes read outside 1.2 to 2.1 m over the floor (sitting, or the room's floor set wrong)";
		return e;
	}
	if (!(unitsPerMetre > 0.0f) || !RealHeightNumber(unitsPerMetre)) {
		e.why = "no units per metre";
		return e;
	}
	e.known = true;
	e.units = eyeOverFloorMetres * unitsPerMetre;
	return e;
}

// What is carried from the recenter on. The lift is taken against the
// character's eye on the first frame it stands readable after the recenter
// and after every load (another save may be another race): `world` is the
// world counter (game/RefFingerprint.h CurrentWorld) it was taken in, 0 not
// yet. A recenter in the main menu - the first start's T-pose - has no
// character to read, so the lift waits for the first frame in the world.
struct RealHeightState {
	bool eyeKnown = false;
	float realEyeUnits = 0.0f;
	float characterEyeUnits = 0.0f;
	float liftUnits = 0.0f;
	UInt32 world = 0;
};

// At a recenter: the eyes as read, the lift to be taken again.
inline void NoteRealEye(RealHeightState& s, const RealEye& e) {
	s = RealHeightState{};
	s.eyeKnown = e.known;
	s.realEyeUnits = e.known ? e.units : 0.0f;
}

// Whether the lift is (re)taken this frame: the eyes known, not taken in
// this world yet, the character standing readable (first person, on foot,
// not sneaking, in the world) and its eye a standing one.
inline bool RealHeightRetakeDue(const RealHeightState& s, UInt32 worldNow, bool standingReadable, float characterEyeUnits) {
	return s.eyeKnown && worldNow != 0 && s.world != worldNow && standingReadable && CharacterEyePlausible(characterEyeUnits);
}

inline void RetakeRealHeight(RealHeightState& s, UInt32 worldNow, float characterEyeUnits, float unitsPerMetre) {
	s.characterEyeUnits = characterEyeUnits;
	s.liftUnits = RealHeightLift(s.realEyeUnits, characterEyeUnits, unitsPerMetre);
	s.world = worldNow;
}

// The lift a frame applies: the setting on, the lift taken, in first person
// on foot, out of the death view.
inline float RealHeightLiftNow(const RealHeightState& s, bool enabled, bool firstPerson, bool riding, bool deathViewHeld) {
	const float liftUnits = s.liftUnits;
	if (!enabled || !s.eyeKnown || s.world == 0 || !firstPerson || riding || deathViewHeld || !RealHeightNumber(liftUnits)) {
		return 0.0f;
	}
	return liftUnits;
}

// The shown body's scale for a lift over a character eye: the real eye over
// the character's, within its bounds. 1 for no number or an implausible eye.
inline float RealHeightBodyScale(float liftUnits, float characterEyeUnits) {
	if (!RealHeightNumber(liftUnits) || !CharacterEyePlausible(characterEyeUnits)) {
		return 1.0f;
	}
	const float s = (characterEyeUnits + liftUnits) / characterEyeUnits;
	return s < kRealHeightBodyScaleLeast ? kRealHeightBodyScaleLeast
	                                     : (s > kRealHeightBodyScaleMost ? kRealHeightBodyScaleMost : s);
}

// The skeleton's own scale under the one OBVR wrote: what it reads, unless
// that is the value written last frame - then the engine left it, and the one
// kept stands. Never compounds a scale onto its own result. A reading that
// is no scale keeps the kept one; with nothing kept or written yet, the
// reading.
inline float BaseScaleNow(float keptBase, float lastWritten, float reading) {
	if (!RealHeightNumber(reading) || !(reading > 0.0f)) {
		return keptBase > 0.0f ? keptBase : 1.0f;
	}
	if (lastWritten > 0.0f && keptBase > 0.0f && reading == lastWritten) {
		return keptBase;
	}
	return reading;
}

}  // namespace obvr::vr
