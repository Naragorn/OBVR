#pragma once

// The engine's head tracking stopped for the player while OBVR shows the body
// in first person (game/PlayerHeadTrackLogic.h has the decision).
//
// Read in Oblivion.exe 1.2.0.416:
// - The actor update (0x00603CA0) runs the head-tracking step 0x00603500 for
//   the player too, on the pass with the third-person flag (player+0x588)
//   set. Every 0.2-0.4 s it picks a nearby actor to look at (a package's
//   target within 500 units first), held 4-10 s - so not only greetings.
// - The step turns the head (yaw +0x184, pitch +0x188 of the head-tracking
//   object) and, past 45 degrees, twists the spine up to 45 degrees through
//   0x0047CAF0 (30/35/35 % over three bones up from the one it starts at) -
//   the upper body seen turning to NPCs.
// - The head-tracking object is the process's +0x15C: HighProcess vtable
//   0x00A71814 slot +0xE8 = 0x0064B350 builds it once (0x004D66B0) and keeps
//   it there; 0x005E12B0 reaches it through that slot.
// - Its byte +0x1DA stops it: every reader (0x0054B010, 0x0054B3E0,
//   0x0054B5A0, 0x0054C6B0 and the face's expression readers 0x00549640..)
//   returns early or answers zero or the identity when it is set; the only
//   writer found is the constructor (0x0054CAF0), with 0. Set, the twist goes
//   out as zero and the head's matrix as the identity. What else it stops is
//   the player's own face (expressions, blinking) - the head is collapsed in
//   first person, so nothing of it shows.

namespace obvr::game {

// Each frame of the camera path: wanted is [Body] PlayerHeadTracking, bodyShown
// whether the body is shown in first person this frame.
// atMark: a hand-script mark this frame - the object's state is logged then.
void StepPlayerHeadTracking(bool wanted, bool bodyShown, bool atMark = false);

}  // namespace obvr::game
