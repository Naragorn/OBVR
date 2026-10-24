#pragma once

// The time a NiBillboardNode keeps for its controllers - the torch's flame
// that stood still in the headset (controls-spec, the tester's video of
// 2026-10-18, reproduced by Nadi 2026-10-24).
//
// Gamebryo's billboard turns itself to the camera when it is culled, and
// then runs its children's downward update again so their world transforms
// follow the turn. For that it keeps the time of its own last downward
// update (NiBillboardNode::UpdateDownwardPass, 0x007219C0: `fst [esi+0xE0]`,
// and the flag bit at +0xDC) and replays it with the controllers ON
// (0x00721B70, at 0x007222D4: `fld [ebx+0xE0]`, the children's +0x60). In the
// engine's own frame that is the world update's time, and the replay changes
// nothing: every controller already stands at that time.
//
// OBVR moves the first-person tree after the engine's update - the arms to
// the controllers (game::PlaceFirstPersonArms), the hand bones, the body IK -
// and makes the world transforms follow with the engine's Update(0.0f, 0),
// the call the engine itself makes after moving its camera. The billboard
// under the torch's flame took that 0 as its time, so in every culled frame
// its particle systems and its flipbook were updated at time 0: the flipbook
// showed the frame of time 0, and the particle controllers' weighted time
// ran backwards by the whole game time each frame, so no particle ever
// moved (measured in the harness 2026-10-24: the emitter's time 13.4 s from
// the engine's update, 0 after the first eye's render, its weighted time
// -890 and falling by 13.4 a frame; the billboard's kept time 13.455 before
// PlaceFirstPersonArms and 0 after it). Mono and alternate-eye rendering do
// not run the dual frame's late hand work, which is why the flame moved
// there.
//
// The fix stands where the time is taken: a downward update that does not
// run the controllers no longer changes the time the billboard hands its
// controllers. Every OBVR transform update keeps working as before; the
// engine's own updates, which run the controllers, are as they were.
//
// Pure apart from the install; the decision is covered by billboard_time_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kBillboardDownwardPass = 0x007219C0;
inline constexpr UInt32 kBillboardSavedTimeOffset = 0xE0;
// mov eax,[esp+8] (the controllers flag); fld dword [esp+4] (the time).
inline constexpr UInt8 kBillboardDownwardEntry[8] = {0x8B, 0x44, 0x24, 0x08, 0xD9, 0x44, 0x24, 0x04};
// The store of the time a few bytes on: fst dword [esi+0xE0].
inline constexpr UInt32 kBillboardSavedTimeStore = 0x007219D3;
inline constexpr UInt8 kBillboardSavedTimeStoreBytes[6] = {0xD9, 0x96, 0xE0, 0x00, 0x00, 0x00};

// What the billboard keeps after a downward update: the update's time when
// it runs the controllers, the time it kept before when it does not.
inline float BillboardKeptTime(bool updateControllers, float kept, float given) {
	return updateControllers ? given : kept;
}

// [Render] KeepBillboardTime: the detour on the billboard's downward pass.
// Logs what it did; refuses when the engine's bytes are not as read.
void InstallBillboardTime(bool enabled);

}  // namespace obvr::game
