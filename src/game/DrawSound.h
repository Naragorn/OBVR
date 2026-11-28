#pragma once

// The draw's and the sheathe's sound in the game (game/DrawSoundLogic.h): the
// key handler's two calls of the weapon's Equip / Unequip sound (0x006B07F0
// at 0x0051B390 and 0x0051B39D) go through OBVR, which counts them for the
// player and keeps a second one of the same draw or sheathe quiet; a weapon
// that came to the hand without its draw animation gets that sound from OBVR
// once (0x006B07F0 called as the key would). One log line per draw and
// sheathe (not per frame):
//
//   Draw sound: <form> drawn|sheathed <way> - the engine's sound call <n>
//   time(s) (<m> kept quiet as a second), OBVR's own played|not owed|...
//
// The calls cost a compare a frame; nothing is allocated.

#include "core/Types.h"

namespace obvr::game {

// The bow's draw sound's call (game/BowDrawSound.h) and the two above: the
// key handler's weapon sounds through OBVR. False when any call site is not
// the call it should be (that one is left alone, the log says which).
bool InstallWeaponKeySounds();

// Once a frame: the drawn weapon's form id (0: none out), the weapon-out
// flag, a menu up, the weapon a staff, the frame's seconds.
void StepDrawSoundWatch(UInt32 onHand, bool out, bool menuUp, bool staff, float dt);

// A grab hung the taken weapon on the hand (camera/hands/Grab.cpp).
void NoteWeaponTakenToHand();

}  // namespace obvr::game
