#pragma once

#include "core/Types.h"

namespace obvr::game {

// The player's block counts towards where the headset looks, not where the
// body faces ([Look] BlockFacesView).
//
// A blow is blocked only when the attacker stands within
// fCombatHitConeAngle (35 degrees) of the target's heading
// (docs/combat-comfort-spec.md, "When a hit counts as blocked"). For the
// player that heading is the body's, and OBVR keeps the body apart from the
// view - so a shield raised towards an attacker the body does not face let
// the blow through (the tester, 2026-09-29: "block mit schild lässt ein paar
// attacken noch durch").
//
// Read in Oblivion.exe 1.2.0.416: the hit handler 0x005FEBF0 asks, only when
// the target is blocking, `call 006131D0` at 0x005FF83E - cdecl(Actor*
// target, TESObjectREFR* attacker or its arrow, float* angleOut) returning
// the verdict in al; it reads the target's position (vtable +0x174) and
// heading (vtable +0x1E0) and compares the folded angle with
// [0x00B36F28]. The call is rerouted to OBVR's own: for the player, with the
// setting on and a headset heading known, the player's rotZ is set to the
// gaze's for the length of the original call and put back - the same swap
// the aim uses (game/AimAtSource.h) - so the engine's own test decides, from
// where the player looks. Everyone else goes straight through.
inline constexpr UInt32 kBlockConeCheck = 0x006131D0;
inline constexpr UInt32 kCallBlockConeCheck = 0x005FF83E;

// Whether this check uses the gaze: on, the target is the player, and the
// headset's heading is known.
inline bool BlockConeUsesGaze(bool enabled, UInt32 target, UInt32 player, bool haveGaze) {
	return enabled && haveGaze && player != 0 && target == player;
}

void InstallBlockCone();

// Every frame: whether the player's block looks along the gaze, and the
// head's turn away from where the body faces (radians, the engine's own
// reckoning - what AimSourcePose::headYaw holds), valid while a headset
// gives it.
void SetBlockCone(bool enabled, bool haveGaze, float headYaw);

}  // namespace obvr::game
