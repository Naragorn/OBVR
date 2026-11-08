#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The engine's own blood, started by OBVR at a point and a heading (for the
// head cut, game/Dismember.h). Read in Oblivion.exe 1.2.416 (2026-10-31,
// ../OBNG/knowledge/blood-on-hits.md):
//
// - a melee hit's victim (Actor vt+0x398 = 0x005EE760) sprays at
//   0x005EF0A3..0x005EF22E: the path from the actor (0x005E1BF0, thiscall:
//   its blood spray model, `Effects\BloodSpray.nif` for every NPC; null for a
//   creature without blood); the parent node from the cell
//   (cell->0x00441800(quad 0x004C9BE0(actor), 3), thiscall, ret 8); 0x20
//   bytes from the game's heap (0x00401F00, cdecl); BSTempEffectParticle's
//   constructor 0x005713F0 (thiscall: cell, lifetime, parent, path, the
//   direction, the point, scale, shared; ret 0x30); the temp-effect manager
//   0x00B3BD00 takes it (0x00678D30, thiscall, ret 4; it frees it in time).
// - the stain: cell->0x004D3B10(point, direction, the actor's decal texture
//   0x005E1BB0, 0, -1, 0) (thiscall, ret 0x28), which picks along the
//   direction and lays the decal on what it meets - the engine's own gates
//   (fDecalLifetime > 0, iMaxDecalsPerFrame) apply.
//
// Every call is checked against its first bytes once; nothing is called
// when one is not as read.

// Checks the functions; logs the result.
void InstallBlood();

// The actor's blood spray at `at` going `dir` (unit). False when the actor
// does not bleed, the engine's functions are not as read, or no parent node.
bool SprayBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir);

// The actor's blood stain where a ray from `at` along `dir` meets the world
// (the cell's decal ray, 0x004D3B10). Aimed at a body it did not land on it
// in the one stain measured (blood-visible 20261108-144602: "0 on the body,
// 1 elsewhere") - for the body StainBodyBlood. False as above.
bool StainBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir);

// The actor's blood decal texture (Effects\blooddecal.dds unless its race or
// kind says another), nullptr when it does not bleed or the functions are
// not as read.
const char* BloodDecalTexture(UInt32 actor);

// The actor's blood stain on its own body: the engine's decal on the
// actor's 3D (the call the cut marks use, game/CutMarks.h, which the game's
// own blood on a struck body goes through), projected from `at` along
// `dir`. False when it does not bleed or nothing was asked of the engine.
bool StainBodyBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir);

}  // namespace obvr::game
