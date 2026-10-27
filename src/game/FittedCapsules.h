#pragma once

// An actor's body capsules fitted to the meshes it wears
// (docs/mesh-fitted-capsules-spec.md; [Hands] FittedBodyCapsules): its
// shown, skinned shapes read as the engine holds them (game/SkinRead.h) -
// body, face, armour, clothing; blood decals and rigid shapes (hair, eyes,
// helmets, weapons) left out - each vertex given to the bone with its
// largest weight, and the capsules fitted per group of bones in the group's
// frame bone's space (game/CapsuleFitLogic.h). The vertices go in skinned to
// the pose drawn at the fit, each shape by its own skin (the shapes' bind
// spaces differ), and the bones' spaces are their world transforms of that
// moment. The frame bones are the skins' own bone pointers, the actor's
// skeleton nodes, so the capsules follow the drawn pose by their world
// transforms each frame.

#include <vector>

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
	double readMilliseconds = 0.0;  // reading and skinning the shapes, on the game's thread
	double fitMilliseconds = 0.0;   // the capsules computed, on a worker thread
};

// A fit in two halves: the shapes read and skinned at once, on the game's
// thread (they are the engine's to change), and the capsules computed on a
// worker thread of their own - a body of 6000-9000 vertices took 2-3 ms in
// the game (fitted-capsules.txt, 2026-10-25), too long for a 90 Hz frame.
struct FitJob;

// Reads the shapes under `root` and hands the fit to a worker; null, with
// the reason in `why`, when there is nothing to fit (no skinned shape read).
// `why` also names refused shapes when the job starts anyway.
FitJob* StartActorFit(const NiAVObject* root, char* why, UInt32 whySize);

// The fit's result once the worker is done (`job` then null, `out` the
// capsules - count 0 when no group fitted); false while it still runs.
bool PollActorFit(FitJob*& job, FittedBody& out);

// Lets go of a job whose actor went (a load, a new model): the worker
// deletes it when it ends. `job` then null.
void AbandonActorFit(FitJob*& job);

// The shapes `root` shows that the fit reads, every vertex where the drawn
// pose has it now, in the world ([Debug] MeshGapTrace, game/MeshGapLogic.h);
// answers how many shapes went in. On the game's thread: 0.3-0.4 ms for a
// body of 4500 vertices (the fits' reading, 2026-10-26).
UInt32 ReadActorMeshNow(const NiAVObject* root, std::vector<NiPoint3>& out);

// A print of the shapes `root` shows that FitActorBody would read (each
// one's data, vertex count and skin): a refit is due when it changes - an
// equip, an unequip. Never 0.
UInt32 ActorOutfitPrint(const NiAVObject* root);

// One line for the log: the capsules' radii by group.
void DescribeFittedBody(const FittedBody& body, char* buf, UInt32 size);

}  // namespace obvr::game
