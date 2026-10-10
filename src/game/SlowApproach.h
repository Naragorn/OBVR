#pragma once

#include "core/Types.h"

namespace obvr::game {

// Fighters walk near the player (the tester, 2026-10-09: "gegner zwar wie
// vanilla zum player rennen dürfen, aber sobald sie im umkreis von 10 metern
// sind dürfen sie nur noch langsam gehen, so wie in blade & sorcery ... kämpfe
// in vr müssen entschleunigt werden").
//
// An actor's movement flags (HighProcess +0x1FC, xOBSE GameProcess.h:
// kMovement_Walk 0x100, kMovement_Run 0x200) say whether its legs walk or
// run, and its speed follows them (Actor's walk and run speeds, 0x005E3590
// and 0x005E3750, the run speed being the walk speed times fMoveRunMult and
// Athletics). The AI sets them through HighProcess's own two setters (read
// 2026-10-09 with dumpbin; the vtable is 0x00A71814, which the HighProcess
// constructor writes at 0x00628F14):
//
//   vtable +0x2C4 (0x00A71AD8) -> 0x00631B90  SetMovementFlag(flag, on), ret 8
//   vtable +0x2C8 (0x00A71ADC) -> 0x00631B50  SetMovementFlags(flags), ret 4
//
// Both entries are rerouted: for an actor in combat within SlowApproachMetres
// of the player, a run asked for becomes a walk - its walking legs and its
// walking speed, the game's own. Beyond it, and for everyone else, nothing
// changes; the player is never touched. A flag already set is turned to a
// walk once a frame as well, for one set before the actor came near.

inline constexpr UInt16 kMovementWalk = 0x0100;
inline constexpr UInt16 kMovementRun = 0x0200;

// A run turned to a walk; anything else kept.
inline UInt16 WalkInsteadOfRun(UInt16 flags) {
	return (flags & kMovementRun) != 0 ? static_cast<UInt16>((flags & ~kMovementRun) | kMovementWalk) : flags;
}

// Whether an actor walks: in combat and near - inside the radius to start,
// out past it by a metre (70 units) to stop, so one standing at the edge
// does not switch every frame.
inline constexpr float kSlowApproachHysteresisUnits = 70.0f;

inline bool SlowApproachNear(bool wasSlow, bool inCombat, float distanceUnits, float radiusUnits) {
	if (!inCombat || !(radiusUnits > 0.0f) || !(distanceUnits >= 0.0f)) {
		return false;
	}
	const float limit = wasSlow ? radiusUnits + kSlowApproachHysteresisUnits : radiusUnits;
	return distanceUnits <= limit;
}

// The radius in game units from the setting in metres: 2-50 m; anything
// else (or not a number) the default 5 m (the tester asked for 10 on
// 2026-10-09 and, having played it, for 5 the next day).
inline float SlowApproachUnits(float metres, float unitsPerMetre) {
	const float m = metres >= 2.0f && metres <= 50.0f ? metres : 5.0f;
	return m * (unitsPerMetre > 0.0f ? unitsPerMetre : 70.0f);
}

// Held still: the legs of someone the player's off hand holds
// (GrappleLogic.h) - no step forward, back or to the side, no run; the
// turn kept, so they still face the player.
inline constexpr UInt16 kMovementDirections = 0x000F;  // forward, backward, left, right

inline UInt16 StandStill(UInt16 flags) {
	return static_cast<UInt16>(flags & ~(kMovementDirections | kMovementRun));
}

// How fast someone moves across the ground, smoothed over about a fifth of
// a second, in units a second: the log's proof of a walk or a run.
inline float SmoothedGroundSpeed(float before, float movedUnits, float dtSeconds) {
	if (!(dtSeconds > 0.0f) || !(movedUnits >= 0.0f)) {
		return before;
	}
	const float now = movedUnits / dtSeconds;
	const float k = dtSeconds >= 0.2f ? 1.0f : dtSeconds / 0.2f;
	return before + (now - before) * k;
}

// Rerouting the two setters; logs whether the table read as read.
void InstallSlowApproach();

// Once a frame: which fighters are near (the process manager's high list),
// their run turned to a walk. Off (`enabled` false): nobody slowed. The
// one held still (HoldStill) is kept standing either way.
void StepSlowApproach(bool enabled, float radiusUnits, float dtSeconds, bool logState);

// The fighters waiting at the ring (game/CombatRing.h): their process and
// the flags' rewrite (CombatRingLogic.h RingFlags), applied in the two
// setters and once a frame. Replaced whole each frame; count 0 clears.
struct RingLegs {
	UInt32 process = 0;
	UInt8 order = 0;  // RingOrder
	bool left = false;
};
void SetRingLegs(const RingLegs* legs, UInt32 count);

// The actor whose legs are held (0: nobody); their process's flags are
// stood still through the same two setters, and once a frame.
void HoldStill(UInt32 actor);

}  // namespace obvr::game
