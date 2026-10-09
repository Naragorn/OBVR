#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/ParryLogic.h"

namespace obvr::game {

// The parry's engine side (game/ParryLogic.h has the decisions and the
// addresses read): two calls in the hit handler 0x005FEBF0 rerouted - the
// block check at 0x005FF7DF (`call 005E5670`, thiscall(target), al) and the
// blocked share at 0x005FF8C7 (`call 005474A0`, cdecl, five dwords, st0) -
// and the cone's answer (0x005FF83E) taken from here by game/BlockCone.
inline constexpr UInt32 kCallBlockingCheck = 0x005FF7DF;
inline constexpr UInt32 kBlockingCheck = 0x005E5670;
inline constexpr UInt32 kCallBlockShare = 0x005FF8C7;
inline constexpr UInt32 kBlockShare = 0x005474A0;

// Reroutes both calls when they read as read; logs which.
void InstallParry();

struct ParryFrame {
	bool enabled = false;  // [Hands] WeaponParries, a melee weapon drawn, in the world
	bool stopsAll = true;  // [Hands] ParryStopsAll
	float fatigue = 0.0f;  // [Hands] ParryFatigue, owed per parried blow
	// The player's blade now and the frame before (world); its guard and tip.
	bool bladeValid = false;
	NiPoint3 guard{0.0f, 0.0f, 0.0f};
	NiPoint3 tip{0.0f, 0.0f, 0.0f};
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
	// The player's shield as a ball (ShieldBall), when one is worn.
	bool shieldValid = false;
	NiPoint3 shieldCentre{0.0f, 0.0f, 0.0f};
	float shieldRadius = 0.0f;
	const BladeFoes* foes = nullptr;
	float dtSeconds = 0.0f;
};

struct ParryEvent {
	bool parried = false;  // blades met this frame, someone's blow parried
	bool byShield = false; // their blade met the player's shield, not the blade
	UInt32 actor = 0;
	SInt32 action = 0;
	NiPoint3 point{0.0f, 0.0f, 0.0f};
};

// Once a frame: whose blade met the player's during their attack; they are
// parried for kParryWindowSeconds.
ParryEvent StepParry(const ParryFrame& frame);

// The cone's answer for a parry, for game/BlockCone's reroute.
ParryCone ParryConeFor(UInt32 target, UInt32 attacker);

// The fatigue owed by the blows parried since the last call: spent by the
// caller in its frame, not inside the hit handler.
float TakeParryFatigue();

// Nobody parried (the mode off, a load).
void ForgetParries();

}  // namespace obvr::game
