#pragma once

// The strike's blade swept between two frames, kept per strike slot, and the
// swing's summary in the log (game/BladeSweepLogic.h,
// docs/weapon-collision-spec.md 26).

#include "core/Types.h"
#include "game/BladeSweepLogic.h"

namespace obvr::game {

// The blades remembered and the swings summed, per strike slot (0 the weapon
// hand, 1 the other hand's fist); owned by the swings' step (camera/hands/
// Swings.h), forgotten with it.
inline constexpr UInt32 kStrikeSweepSlots = 2;
struct StrikeSweeps {
	BladeSweepMemory seen[kStrikeSweepSlots];
	SwingSweepSummary summary[kStrikeSweepSlots];
};

// What this frame's strike sweeps from: `steps` poses from fromBase-fromTip
// to the blade now (1: nothing between).
struct StrikeSweep {
	SweepFrom from = SweepFrom::FirstFrame;
	UInt32 steps = 1;
	NiPoint3 fromBase{0.0f, 0.0f, 0.0f};
	NiPoint3 fromTip{0.0f, 0.0f, 0.0f};
};

// The slot's blade this frame (slot 0 the weapon hand, 1 the other hand's
// fist; no sweeps kept: nothing): where to sweep from, remembered for the
// next frame. Remembered every frame the hand could strike; only a frame of a
// swing (`swinging`) counts in the swing's summary, and a new swing logs the
// slot's last one.
StrikeSweep BeginStrikeSweep(StrikeSweeps* sweeps, UInt32 slot, UInt32 serial, bool swinging, UInt32 frame,
                             float frameSeconds, const NiPoint3& base, const NiPoint3& tip);

// A body touched this frame by the slot's blade - only between two frames
// when `swept`.
void NoteStrikeTouch(StrikeSweeps* sweeps, UInt32 slot, bool swept);

// The frame's walk over the actors, the weapon's type and reach, and how many
// were struck this frame.
void NoteStrikeWalk(StrikeSweeps* sweeps, UInt32 slot, const StrikeWalkTally& frame, SInt32 weaponType, float reach,
                    UInt32 struck);

// Forgets the blades remembered (the swings' summaries stay to be logged),
// for when Full VR stops: the next strike sweeps from nothing.
void ForgetStrikeBlades(StrikeSweeps& sweeps);

}  // namespace obvr::game
