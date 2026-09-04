#pragma once

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::game {

// The first node under the first-person root with this name, a few levels
// down, or null. Case does not matter. A fresh walk each call; callers that
// need it every frame keep the answer.
NiAVObject* FindFirstPersonNode(const char* name);

// Hiding parts of the first-person model, for the hand-tracked mode: the
// arms are the animation's and stay where the animation puts them, while the
// hands are the controllers', so the arms go and the hands and what they
// hold stay.
//
// "Hidden" is the engine's own switch: bit 0 of the flags word at +0x18 of
// any NiAVObject, the one the scene render sets on the first-person root to
// hide the whole model between passes and the one NiAVObject::Cull tests on
// every object (see kNiFlagsOffset). A geometry with it set is skipped by the
// cull walk and never drawn; nothing else about it changes, and clearing
// the bit brings it back.
//
// WHICH parts is a list of node names, because the first-person tree is
// built from the race's body meshes and whatever armour is worn. The body
// meshes keep the arms in a shape named "Arms" - the game hides every
// shape named "UpperBody" in first person and shows the rest, so the arms
// need a name of their own (cs.uesp.net, "Fixing the armor incorrectly
// displayed in 1st person"; seen in upperbody.nif, femaleupperbody.nif).
// An armour's arms are named by its modder. The probe below prints the
// tree; the list in [Hands] HideFirstPersonNodes names what to hide.

// Sets the hidden bit on every node under the first-person root whose name
// is in `list` (see NodeNameList.h), and clears it again on any node it hid
// before that is no longer in the list. `enabled` false clears everything
// it hid. Safe to call every frame: a bit already right is left alone.
void HideFirstPersonNodes(bool enabled, const char* list);

// Clears every bit this code set. For when the mode is switched off.
void ReleaseHiddenFirstPersonNodes();

// Logs the first-person tree once - class, name, flags and child count of
// every node a few levels down - and again when the root changes (a new
// model after a race change or a load). For Debug.FirstPersonTreeProbe.
void ProbeFirstPersonTree();

}  // namespace obvr::game
