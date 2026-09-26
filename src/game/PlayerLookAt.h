#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Where NPCs look at the player: the eyes in the headset, not the hand
// (2026-09-26: NPCs near the player sometimes looked at the hand before a
// dialogue, and correctly once it had started).
//
// Read in Oblivion.exe 1.2.0.416: an NPC's head tracking (0x00603500) aims at
// the point its target answers through vtable slot +0x11C. The player's
// (PlayerCharacter vtable 0x00A73A0C, entry 0x00A73B28 = 0x006604C0,
// thiscall(NiPoint3* out), ret 4) answers, in first person (player+0x588
// zero), the world position of the first-person "Camera01" node ([0x00B3BB0C],
// filled at 0x00667CFB) less 6 units of height (the double at 0x00A3F3A0);
// otherwise the Actor version 0x005EE660 (the third-person head).
// Camera01 hangs under the first-person root, and OBVR's hand mode moves and
// turns that root with the right controller (game::PlaceFirstPersonArms), so
// the point followed the hand. In a dialogue the arms are released and the
// root is the engine's again - why it was right then.
//
// The slot is replaced: while the eyes are known (SetPlayerLookAtEyes) and
// the player is in first person, it answers the headset's eyes less the same
// 6 units; otherwise the original.
inline constexpr UInt32 kPlayerLookAtSlot = 0x00A73B28;
inline constexpr UInt32 kPlayerLookAtOriginal = 0x006604C0;
inline constexpr UInt32 kPlayerThirdPersonOffset = 0x588;
inline constexpr float kLookAtBelowEyesUnits = 6.0f;

// Whether the eyes answer: wanted, known this frame, and first person.
inline bool LookAtFromEyes(bool wanted, bool eyesValid, bool thirdPerson) {
	return wanted && eyesValid && !thirdPerson;
}

// The point answered from the eyes, as the engine answers from Camera01.
inline NiPoint3 LookAtPointFromEyes(const NiPoint3& eyes) {
	return NiPoint3{eyes.x, eyes.y, eyes.z - kLookAtBelowEyesUnits};
}

// Replaces the slot after checking it holds the original; logs either way.
void InstallPlayerLookAt();

// Each frame: the headset's eyes in the world, and whether to use them.
void SetPlayerLookAtEyes(bool wanted, bool valid, const NiPoint3& eyes);

}  // namespace obvr::game
