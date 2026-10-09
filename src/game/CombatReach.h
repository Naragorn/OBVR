#pragma once

#include "core/Types.h"

namespace obvr::game {

// How close fighters come before they strike (docs/weapon-collision-spec.md,
// 11, option A; the tester, 2026-10-09: "A, mach erst den Harness-Test").
//
// Every melee blow - an NPC's at the player, the player's strike by motion -
// reaches the weapon's reach times the game setting fCombatDistance (0x00547540:
// `fld [00B36F20h]; fmul [esp+4]`; the setting's name read after its value
// confirms it). At the game's 128 a bandit struck the player from 2 m, its
// blade 131-191 units from the eyes, out of any VR blade's reach; at 85 it
// struck from about 1.3 m, its blade 24-55 units from the eyes, and the
// circling blade parried ten of its blows (hand scripts blade-parry and
// blade-parry-near, 2026-10-09).
//
// OBVR sets the game's runtime value while Full VR runs and puts the game's
// own back when it stops. A game setting changed at runtime is not kept in
// the savegame and is gone when Oblivion restarts (cs.uesp.net/wiki/
// Con_SetGameSetting: "Changes made to game settings with this function will
// not be included in the savegame file"); it is no INI setting either, so
// nothing is written back anywhere (the iSize lesson does not apply).

inline constexpr UInt32 kCombatDistanceValue = 0x00B36F20;

// The value to hold the setting at: [Hands] CombatReach while Full VR runs,
// held to 40-200 units; the game's own otherwise, and for 0 or a value that
// is not a number (0 leaves the game's).
inline float CombatReachFor(bool fullVr, float setting, float gameOwn) {
	if (!fullVr || !(setting > 0.0f)) {
		return gameOwn;
	}
	return setting < 40.0f ? 40.0f : (setting > 200.0f ? 200.0f : setting);
}

// Once a frame: holds fCombatDistance at CombatReachFor's value; the first
// call reads the game's own (and checks the setting is the one named
// "fCombatDistance", else leaves it alone for good).
void StepCombatReach(bool fullVr, float setting);

}  // namespace obvr::game
