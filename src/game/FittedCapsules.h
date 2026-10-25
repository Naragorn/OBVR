#pragma once

// An actor's body capsules fitted to the meshes it wears
// (docs/mesh-fitted-capsules-spec.md; [Hands] FittedBodyCapsules): its
// shown, skinned shapes read as the engine holds them (game/SkinRead.h) -
// body, face, armour, clothing; blood decals and rigid shapes (hair, eyes,
// helmets, weapons) left out - each vertex given to the bone with its
// largest weight, and the capsules fitted per group of bones in the group's
// frame bone's space (game/CapsuleFitLogic.h). The frame bones are the
// skins' own bone pointers, the actor's skeleton nodes, so the capsules
// follow the drawn pose by their world transforms each frame.

#include "core/Types.h"
#include "game/CapsuleFitLogic.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

struct FittedBody {
	UInt32 count = 0;
	FittedCapsule cap[kFitCapsulesMax];
	const NiAVObject* frame[kFitCapsulesMax] = {};
	// what went in, for the log
	UInt32 shapes = 0;
	UInt32 vertices = 0;
	double milliseconds = 0.0;
};

// Reads the shapes under `root` and fits; false, with the reason in `why`,
// when nothing could be fitted (no skinned shape read, no Bip01 group).
bool FitActorBody(const NiAVObject* root, FittedBody& out, char* why, UInt32 whySize);

// A print of the shapes `root` shows that FitActorBody would read (each
// one's data, vertex count and skin): a refit is due when it changes - an
// equip, an unequip. Never 0.
UInt32 ActorOutfitPrint(const NiAVObject* root);

// One line for the log: the capsules' radii by group.
void DescribeFittedBody(const FittedBody& body, char* buf, UInt32 size);

}  // namespace obvr::game
