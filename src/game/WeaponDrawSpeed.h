#pragma once

#include "core/Types.h"

namespace obvr::game {

// Drawing and sheathing without the second of animation ([Hands]
// WeaponDrawSpeed; the tester, 2026-09-27: "anfangs und am ende ein paar
// animationen statt direkt die waffe in der hand ... die müssen weg"; and
// 2026-09-28, of the fists: "die h2h ready anim müssen wir beschleunigen").
//
// The engine plays the Equip animation group to draw a weapon and Unequip to
// sheathe it. The weapon changes hands at the group's text keys, and the
// player's action (HighProcess +0x1F4) stays Equip or Unequip until the
// group's sequence is inactive again - which is what the ready click, the
// holster and the fists wait for.
//
// Measured on 2026-09-28 (the fists' draw, handtohandequip.kf: 0.2 s long,
// "Blend: 6"): 0.66 s from the key to the action's end, whatever the
// sequence's frequency - 10 and 40 times gave the same. Three parts:
// - 0.25 s blending in (the sequence's state 5, TransDest);
// - 0.2 s playing (state 1), however fast: the engine stops a group when
//   its time reaches the group's end key (0x004774A1, called from the anim
//   update at 0x00476FA6), and that time is the sequence's offset (+0x48)
//   plus the anim data's clock (ActorAnimData +0x94) - unscaled. The
//   frequency (+0x28) only makes the pose move faster; the keys, the weapon's
//   attach and the end come at the authored times;
// - 0.2 s easing out (state 3): the stop's ease-out, the group's blend-out
//   frames at +0x21 over 30 (0x004733A0) - 6 frames here.
// The action ends in the anim update's check at 0x005FD8B3: the action's
// sequence inactive (state +0x44 == 0) clears it.
//
// So time itself is hastened, at the place the engine reads it:
// - playing, the sequence's offset is moved on by (speed - 1) times the
//   clock's step each frame, so the engine's time and Gamebryo's pose
//   ((clock + offset) * freq) both run `speed` times as fast - the keys, the
//   attach, the end and the stop come that much sooner, in their order;
// - blending in or out, the blend's window (start +0x4C, end +0x50, xOBSE
//   NiNodes.h NiControllerSequence) is shrunk around the clock by `speed`,
//   once per blend, so the weight goes on from where it was, faster.
// The frequency is left alone.
//
// The draw's sound is a key too ("Enum: Equip" / "Enum: Unequip", kinds 9
// and 10 of TESAnimGroup's key handler, 0x0051AF70, which plays the weapon
// type's WPN...Equip sound through 0x006B07F0). The anim update fires keys
// over the span from the clock before its step to the clock after, both at
// the sequence's current offset (0x00476F4D, 0x00477A4B) - so a span the
// offset was moved over was skipped, and the sound with it (the tester,
// 2026-09-28: "der sound fehlt nun"). So the first-person sequence's keys
// over that span are fired through the same handler as the offset moves.
// The harness showed one sound per draw and sheathe, as at the game's own
// speed, and none for the fists, which have none at the game's speed
// either. The third-person sequence's keys are not fired: at the game's
// speed only one sound plays.
//
// Read from xOBSE (GameProcess.h, NiNodes.h, GameObjects.h):
// - the player's animations: the third-person ActorAnimData at the process's
//   +0x17C (MiddleHighProcess::animData, the process at player+0x58), the
//   first-person one at player+0x5CC (PlayerCharacter::firstPersonAnimData);
// - ActorAnimData::animSequences[5] at +0xA0, BSAnimGroupSequence*;
// - BSAnimGroupSequence::animGroup at +0x68, a TESAnimGroup whose group code
//   byte is at +0x08: Equip 17, Unequip 18.
// A sequence stopped leaves the anim data's slots while it still eases out
// (0x00471088), so each one is followed until it is inactive.

inline constexpr UInt32 kSequenceInactive = 0;
inline constexpr UInt32 kSequenceAnimating = 1;
inline constexpr UInt32 kSequenceEaseIn = 2;
inline constexpr UInt32 kSequenceEaseOut = 3;
inline constexpr UInt32 kSequenceTransDest = 5;

struct WeaponDrawSpeedReport {
	UInt32 hastened = 0;       // sequences followed this frame
	UInt32 newlyHastened = 0;  // of them, first seen this frame
	UInt32 blendsShortened = 0;  // blends shrunk this frame
	UInt32 released = 0;       // sequences no longer followed (inactive, gone)
	UInt8 group = 0;           // the last new one's group code
};

WeaponDrawSpeedReport StepWeaponDrawSpeed(float speed, bool active);

// The group codes, pure, for the test.
inline bool IsWeaponDrawGroup(UInt8 group) { return group == 17 || group == 18; }

// The clock's step as it counts: a step back (a new clock) or a long one (a
// pause, a load) moves nothing.
inline float ClockStep(float now, float last) {
	const float step = now - last;
	return (step > 0.0f && step < 0.5f) ? step : 0.0f;
}

// A playing sequence's offset, moved on so that its time runs `speed` times
// as fast over this step. Speeds of 1 or less leave it.
inline float HastenedOffset(float offset, float step, float speed) {
	return speed > 1.0f ? offset + (speed - 1.0f) * step : offset;
}

// Which states are a blend whose window can be shrunk.
inline bool IsBlendState(UInt32 state) {
	return state == kSequenceEaseIn || state == kSequenceEaseOut || state == kSequenceTransDest;
}

// A blend's window shrunk around the clock by `speed`: what has passed and
// what is left both divided, so the weight - the passed share - is the same
// now and runs out `speed` times as fast. A window not yet set (the engine
// writes start -1 until its next update), already over, or a speed of 1 or
// less is left, and answers false.
inline bool ShrinkBlend(float now, float& start, float& end, float speed) {
	if (!(speed > 1.0f) || start < 0.0f || !(end > now) || now < start) {
		return false;
	}
	start = now - (now - start) / speed;
	end = now + (end - now) / speed;
	return true;
}

}  // namespace obvr::game
