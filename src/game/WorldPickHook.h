#pragma once

#include "game/NiMath.h"

namespace obvr::game {

// Installs the narrow mid-function hook that replaces Oblivion's activation
// ray in third person. False means the executable bytes did not match or the
// trampoline could not be installed; vanilla picking is then left untouched.
bool InstallWorldPickHook();

// Supplies the most recently rendered HMD direction. The world-pick update
// may run before the current frame's camera hook, so retaining the previous
// complete pose is intentional and avoids an uninitialised first frame. Its
// origin remains Oblivion's player-safe activation origin, not the chase
// camera behind the character.
void SetWorldPickDirection(const NiPoint3& direction, bool valid);

// The hand's laser in the world, origin and direction, for the hand-tracked
// mode: while valid it replaces the pick ray in first and third person alike,
// so what the laser's crosshair sits on is what Activate, the grab and the
// tooltip take. The site builds its ray from the player's own rotation in
// both views (0070FDD0/0070FD30 just before it), which is why first person
// needs the replacement too once the ray is not the head's.
void SetWorldPickHandRay(const NiPoint3& origin, const NiPoint3& direction, bool valid);

// While on, the game's own enemy health bar - HUDReticle's third persistent
// root (GameAddresses.h, kHudAuxRootPointer), drawn at the crosshair - is
// hidden after every HUDReticle update the hook above wraps (its visible
// trait set to 1, false), so it does not ride the crosshair quad while the
// bars over the fighters show (game/EnemyHealthLogic.h). The first time the
// game shows it, its tiles are logged once. Off, the engine's own writes
// stand (it shows the bar again when its fight target next changes).
void SetEnemyHealthTileHidden(bool hidden);

}  // namespace obvr::game
