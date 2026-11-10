#pragma once

// The engine side of game/StrikeSkillLogic.h: while one VR strike is handed
// to the engine's fist blow (game::KickStrike), its damage is reckoned with
// the strike's own skill and, for a shove, the struck one is not told of an
// attack.

#include "core/Types.h"
#include "game/StrikeSkillLogic.h"

namespace obvr::game {

// The three call sites checked and rerouted once (the damage 0x005FF419, the
// power factor 0x005FF44D, the reaction 0x006005B1); each part refused alone
// when its bytes are not as read. Logs what it did.
void InstallStrikeRules();

// Whether a strike that must not tell the struck one (rule.reacts false) can
// be handed to the engine: only with the reaction's reroute in place.
bool StrikeCanStayQuiet();

// Set before the engine's hit on `target` by `player`, cleared after: the
// skill's value read off the player (vtable +0x284, as the hit reads
// hand-to-hand's) swapped into the damage, the reaction held back.
void BeginStrike(void* player, void* target, const StrikeRule& rule);
void EndStrike();

// BeginStrike for as long as it lives, EndStrike when it goes - however the
// strike's call returns.
class StrikeScope {
public:
	StrikeScope(void* player, void* target, const StrikeRule& rule) { BeginStrike(player, target, rule); }
	~StrikeScope() { EndStrike(); }
	StrikeScope(const StrikeScope&) = delete;
	StrikeScope& operator=(const StrikeScope&) = delete;
};

// The detour on the engine's weapon sound (0x006AF880) that lets a slap's hit
// go without the fist's hit sound (StrikeRule::quietHit): installed once at
// the start, never taken out. False, and logged, when its bytes are not as
// read (another plugin's hook) - a slap then keeps the hit's sound.
bool InstallQuietHitSound();

// Whether 0x006AF880 may be called: its bytes as read, or OBVR's own detour on
// it (which leaves the bytes changed and passes every call on).
bool WeaponSoundCallable();

}  // namespace obvr::game
