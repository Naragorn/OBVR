#pragma once

#include "core/Types.h"

namespace obvr::game {

// A stagger that falls the blow's way ([Hands] StaggerSides; the decisions
// in game/StaggerSideLogic.h). Engine side, read in Oblivion.exe 1.2.0.416,
// 2026-11-05 (OBNG knowledge/hit-reactions.md):
//
// - the mirrors join each NPC's stagger groups as special animations: the
//   base form's TESAnimation (TESActorBase +0x94, xOBSE GameForms.h; its
//   list {char* name, next} at +0x98) gets the four names of
//   kStaggerMirrorFiles, as Blockhead's overrides add theirs. The list is
//   read when a reference's animations are built, 0x004E3490
//   thiscall(ref), plain ret (Blockhead hooks inside it at 0x004E34AB); its
//   entry is detoured and the names added just before it runs. Only NPCs
//   (type 0x23) on a skeleton in Characters\_Male (TESActorBase +0xAC
//   TESModel, path at +0xB0), never the player;
// - the pick: 0x0047628A (StaggerSideLogic.h, BuildStaggerPickStub) asks
//   OnStaggerPick for the index; it answers only inside a stagger started by
//   a hit (PlayerStagger.cpp OnStagger brackets the engine's start with
//   BeginStagger/EndStagger), only for an AnimSequenceMultiple (vtable
//   0x00A3C758) and only when the player's hands set a blow on that actor
//   (SetStaggerBlow). Every other stagger keeps the engine's draw, which
//   with the mirrors in the group falls to either side.
//
// The four files have to be in Data\Meshes\Characters\_Male\SpecialAnims
// (assets/meshes); without them nothing is installed.
void InstallStaggerSides();
void SetStaggerSides(bool enabled);

// The player's blow on `actor`: how far its swing went to the actor's right
// (metres, negative its left; StaggerSideLogic.h BlowLateral). Set while the
// hit is handed to the engine, cleared after.
void SetStaggerBlow(UInt32 actor, float lateral);
void ClearStaggerBlow();

// Around the engine's stagger start for a hit (PlayerStagger.cpp).
void BeginStagger(UInt32 actor);
void EndStagger();

}  // namespace obvr::game
