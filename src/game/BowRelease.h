#pragma once

#include "core/Types.h"

namespace obvr::game {

// The bow's draw moved on to its Attach and Hold keys, so a loose comes at
// once however early it is let go (vr::StepReleaseSnap decides when).
//
// Read from Oblivion.exe 1.2.0.416 (2026-10-01, static):
// - The attack update 0x005FCAB0 looses a bow shot in its action-5 case
//   (0x005FD106) only when the draw's key counter for slot 3 is 3 (the
//   Release key, 1.433 s in bowattack.kf); it builds the arrow (0x005FD47C)
//   and takes the ammunition (0x005FD4A4) there. Action 4 attaches at 1.
// - The counter (ActorAnimData +0x48 + 4 * slot, slot 3 at +0x54) steps once
//   a frame in the anim update when the next key's time (TESAnimGroup key
//   time, 0x0051AE20 thiscall(group, index), on st0) is reached by the
//   sequence's time (its offset +0x48 plus the anim data's clock +0x94).
//   Reaching 2 (Hold) it snaps the offset to KeyTime(2) - clock
//   (0x004771B5-0x004771D8) - the write done here.
// - Held, the draw pauses at Hold (0x0065EF38-0x0065F045); let go before it,
//   it plays on to Release first: up to 1.4 s late (the tester, 2026-10-01).
// - The shot's power is a timer of its own (player +0x640, grown while the
//   control is held: power = min(1, fArrowBowTimerBase + fArrowBowTimerMult *
//   timer), 0x005FD278-0x005FD2BF) - the pose does not change it.
// The sequence is ActorAnimData's slot 3 (+0xA0 + 3 * 4), on both the
// first-person anim data (player +0x5CC) and the third-person one (the
// process at player +0x58, +0x17C).

// Puts each of the player's bow draws whose counter is on the key before at
// that key: at its Attach (key 1) a draw let go before the arrow was on, at
// its Hold (key 2) one with the arrow on. True when a draw could be read (put
// there now, or nothing to do); false when none could.
bool SnapBowDrawToAttach();
bool SnapBowDrawToHold();

// The shot's power from the draw (vr::BowTimerForDraw): the player's bow timer
// (player +0x640) written for a string drawn `weight` (0 at rest, 1 full),
// with the game's fArrowBowTimerBase and fArrowBowTimerMult (their values at
// 0x00B37080 and 0x00B37088; their initialisers 0x009E9AA0 and 0x009E9AD0,
// defaults 0.25 and 0.4, read 2026-10-01 - no plugin in Data sets them). The
// engine adds the frame's time to it while the control is held (0x0065ED7B),
// so it is written each frame. False when the player cannot be read.
bool WriteBowPowerForDraw(float weight);

// The quiver's arrows shown again from the ammunition's count, on both the
// third- and the first-person body: the draw's Attach hides one of them
// (`or word [node+0x18], 1` at 0x005FD0D6), and a draw taken back spent no
// arrow. The engine's own call, as the player makes it at 0x00660C58 and
// 0x00660C68: 0x005F8300 thiscall(actor, root, 0), ret 8, with the roots at
// player +0x104 and +0x5C8. False when the player cannot be read or the
// function's first bytes are not the ones read.
bool RefreshQuiverArrows();

}  // namespace obvr::game
