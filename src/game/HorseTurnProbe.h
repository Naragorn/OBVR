#pragma once

namespace obvr::game {

// Debug.HorseTurnProbe: a detour at the entry of the actors' turn-by routine
// (0x00659B90: heading from the virtual at +0x1E0 plus the delta on the
// stack, written through the virtual at +0x1E8; the horse's steering at
// 0x0067247B/0x006724FF, the animation's root rotation at 0x006571BC and
// 0x00678C2F and the AI's turns all come through it). While the player is
// mounted, every turn of the horse is logged with its delta and the
// caller's return address - for the horse that turned by 18-22 degrees a few
// seconds after the headset's head turned, with no movement flag set on it
// (horse-ride.txt, 2026-10-19 11:40). Bounded.
void InstallHorseTurnProbe();

}  // namespace obvr::game
