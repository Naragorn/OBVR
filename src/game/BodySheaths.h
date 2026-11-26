#pragma once

// The body's sheathed-weapon nodes - "SideWeapon" (the hip), "BackWeapon"
// (the back), "Quiver" - as OBVR writes them each frame: kept still through
// the draw and the sheathe (game/SheathHoldLogic.h), and mirrored across the
// body's middle when asked (vr/SheathMirror.h decides which, MirroredAcrossLocalZ
// in the parent's frame). Booked as every bone the body IK writes
// (game/BoneWrite.h): put back at the next frame's start, given back after the
// world is drawn and for good on release. A node whose parent's z is not the
// body's left-right (LateralAxisAgrees against the thighs or the clavicles,
// 30 degrees) is not mirrored, said once.
//
// Moved out of game/BodyIk.cpp on 2026-11-26 with the hold; game/BodyIk.cpp's
// StepBodySheaths hands it the bones of its frame.

#include "game/GameTypes.h"
#include "vr/SheathMirror.h"

namespace obvr::game {

// The skeleton's root and the bones a sheath's parent is judged against: the
// thighs for the pelvis, the clavicles for Spine2.
struct BodySheathBones {
	NiAVObject* root = nullptr;
	const NiAVObject* pelvis = nullptr;
	const NiAVObject* spine2 = nullptr;
	const NiAVObject* thighRight = nullptr;
	const NiAVObject* thighLeft = nullptr;
	const NiAVObject* clavicleRight = nullptr;
	const NiAVObject* clavicleLeft = nullptr;
};

// This frame's writes: `side`, `back`, `quiver` mirrored when true; the hip's
// node hidden when `hideSide` (a one-hander kept at a shoulder, game/
// SheathHideLogic.h); every node still while the player's action is the draw
// or the sheathe.
void StepSheaths(const BodySheathBones& bones, const vr::SheathMirror& sheaths, float dtSeconds);

// After the world is drawn: the writes given back where the nodes still hold them.
void GiveBackSheathsAfterDraw();

// Whether a write is booked on any of them.
bool SheathsHeld();

// Given back for good, under the body root there is now (none: forgotten).
void ReleaseSheaths(NiAVObject* rootNow);

}  // namespace obvr::game
