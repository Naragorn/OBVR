#pragma once

#include "core/Types.h"

namespace obvr::game {

// Drawing and sheathing without the second of animation ([Hands]
// WeaponDrawSpeed; the tester, 2026-09-27: "anfangs und am ende ein paar
// animationen statt direkt die waffe in der hand ... die müssen weg").
//
// The engine plays the Equip animation group to draw a weapon and Unequip to
// sheathe it; the weapon moves between the hip and the hand at the group's
// text keys, and the action ends with the sequence. So the sequence is played
// faster rather than skipped: every key still fires, the weapon changes hands
// at the same point of the animation, and the action ends as it always does -
// only sooner.
//
// Read from xOBSE (GameProcess.h, NiNodes.h, GameObjects.h):
// - the player's animations: the third-person ActorAnimData at the process's
//   +0x17C (MiddleHighProcess::animData, the process at player+0x58), the
//   first-person one at player+0x5CC (PlayerCharacter::firstPersonAnimData);
// - ActorAnimData::animSequences[5] at +0xA0, BSAnimGroupSequence*;
// - NiControllerSequence::freq at +0x28 (a float, 1 the authored speed);
// - BSAnimGroupSequence::animGroup at +0x68, a TESAnimGroup whose group code
//   byte is at +0x08: Equip 17, Unequip 18.
// A sequence sped up gets its own frequency back once no equip or unequip
// runs any more, in case it is shared with another actor.
//
// Answers how many sequences are running faster this frame, for the log.
struct WeaponDrawSpeedReport {
	UInt32 hastened = 0;     // sequences playing at the wanted speed now
	UInt32 newlyHastened = 0;  // of them, sped up this frame
	UInt32 restored = 0;     // sequences given their own speed back this frame
	float originalFreq = 0.0f;  // the last one's own frequency, for the log
	UInt8 group = 0;         // the last one's group code
};

WeaponDrawSpeedReport StepWeaponDrawSpeed(float speed, bool active);

// The group codes, pure, for the test.
inline bool IsWeaponDrawGroup(UInt8 group) { return group == 17 || group == 18; }

}  // namespace obvr::game
