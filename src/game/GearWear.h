#pragma once

#include "core/Types.h"
#include "game/BodyPart.h"
#include "game/GearWearLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// Armor and weapon wear on the engine (docs/break-armor-spec.md; the
// decisions in game/GearWearLogic.h, the engine's functions read there):
// - the wear's two settings scaled by [Hands] ArmorWearMult/WeaponWearMult,
//   the engine's own formula for every hit, the player's and everyone's;
// - a VR strike's armor wear on the piece over the struck part, not the
//   engine's random one (the piece picker's call at 0x005FFC1B rerouted);
// - a piece at 0 knocked off an NPC and gone, never a lootable item, and
//   the player's by [Hands] PlayerGearBreaksOff (the actors' item-damage
//   slot, vtable +0x2C4, rerouted; the engine's own drop, 0x004DC000 at
//   0x005F3AF9, watched).
//
// Installed once (StepGearWear's first frame calls it); each part refuses
// on its own and says so when the bytes are not as read.
void InstallGearWear();

// Around a VR strike's engine hit: the struck one, the struck part (`parted`
// false when it is not known - the engine's random piece then), and the
// blow's way in the world for a knocked-off piece's flight.
void BeginStrikeWear(void* target, bool parted, BodyPart part, const NiPoint3& blowWay);
void EndStrikeWear();

// Each frame: the settings written, the knocked-off pieces flown and taken
// away. `inWorld` false (a menu, a load) holds the pieces where they are.
void StepGearWear(const GearWearSettings& settings, float dt, bool inWorld);

}  // namespace obvr::game
