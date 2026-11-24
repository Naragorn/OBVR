#pragma once

// A scene node OBVR makes and keeps a reference to, so the pointer stays good
// whatever happens to the tree it hangs in: the drawn arrow in the hand
// (game/BowVisual.cpp) and the weapons kept at the hips (game/KeptOnBody.cpp).
//
// The engine's own calls, as it makes the drawn arrow at
// 0x005FD02C-0x005FD03F: NiObject::Clone (addr::kNiObjectClone, thiscall, no
// arguments, the new object unreferenced) and NiNode::AddObject (vtable
// +0x84: the child and 1, the first free slot). RemoveObject is vtable +0x88
// (an out smart pointer and the child, 0x0070B120); the reference it hands
// back is released as the engine releases one (0x006691A1: decremented,
// destroyed through vtable +0 with 1 at zero).
//
// Moved out of game/BowVisual.cpp on 2026-11-20 for the hips' weapons.

#include "game/GameTypes.h"

namespace obvr::game {

// A copy of `source` and everything under it; nobody holds it yet.
NiAVObject* CloneNode(const NiAVObject* source);

// A reference of OBVR's own on `node`.
void HoldNode(NiAVObject* node);

// A reference given back; the node destroyed when it was the last.
void ReleaseNode(NiAVObject* node);

// `child` hung under `parent` (a node that keeps children) in its first free slot.
void AddChildNode(NiAVObject* parent, NiAVObject* child);

// `child` taken off `parent`; the reference the engine hands back with it released.
void RemoveChildNode(NiAVObject* parent, NiAVObject* child);

// A model loaded by the engine's model loader (0x00439EB0 on [0x00B33A1C],
// thiscall(path, 0, 0, 1), as the wound caps load theirs - the path under
// Meshes, the loader's copy shared and cached) and a clone of it that OBVR
// holds (HoldNode). nullptr with `why` when the loader or the clone is not as
// read, or the model does not load. A first load of a model reads the disk.
NiAVObject* LoadModelCopy(const char* path, const char*& why);

// A node newly hung under a node in the scene brought up as the engine's head
// builders bring theirs (0x00525A70): its effects (0x00707980), its properties
// (0x00707610) and its transforms (0x00707370 with 0, 0). Without them new
// shapes have no render state and the renderer faults (OBNG
// knowledge/carried-lights.md). False, and nothing called, when the three are
// not as read.
bool SettleNewNode(NiAVObject* node);

// A model's own animation started, as the engine starts the spell model's "SpecialIdle_Cast" in the
// hand (0x005EDE12) and "SpecialIdle_Projectile" on a bolt (0x006950C4): the root's controller
// (+0x0C) cast to its NiControllerManager (0x00560920, cdecl(RTTI 0x00B3CAC0, object)), the
// sequence looked up by name in its map (+0x58, 0x0055E000), every active one let go (0x004715C0,
// ease 0), this one activated (0x006C9BA0: priority 0, not over, weight 1, ease 0, no sync, no
// transition), the manager's flag 8 set and the sequence's start (+0x48) to -FLT_MAX, so it starts
// at the next update. False with `why` when a call is not as read, the model has no manager or no
// such sequence. Read 2026-11-23 in Oblivion.exe 1.2.0.416.
bool PlayNodeSequence(NiAVObject* root, const char* name, const char*& why);

// The sequence `name` of a model's controller manager, found as PlayNodeSequence finds it, without
// playing it; null with `why` when a call is not as read, the model has no manager or no such
// sequence. Its start (+0x48) is its offset once it runs, -FLT_MAX until its first update; its state
// (+0x44) is 0 when it is not active (0x006C9BA0 reads it so).
UInt8* NodeSequence(NiAVObject* root, const char* name, const char*& why);

}  // namespace obvr::game
