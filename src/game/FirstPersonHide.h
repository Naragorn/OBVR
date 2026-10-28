#pragma once

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::game {

// The first node under the first-person root with this name, down to the
// bones at the end of the arms, or null. Case does not matter. A fresh walk
// each call; callers that need it every frame keep the answer.
NiAVObject* FindFirstPersonNode(const char* name);

// The nodes under under (itself excluded) whose names contain part,
// case-insensitively, in tree order, at most capacity of them - a hand's
// finger bones, whatever the skeleton calls each one. Answers how many.
UInt32 CollectNodesContaining(NiAVObject* under, const char* part, NiAVObject** out,
                              UInt32 capacity);

// Recomputes the world transforms under a node from its own world transform,
// child by child, without running the node's own update - which for a
// Havok-driven object sets the node back to its rigid body's pose.
void UpdateChildTransforms(NiAVObject* node);

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
// With cuirassKeep, every shape under the worn cuirass's node goes as well
// except those it names (HiddenUnderCuirass, NodeNameList.h); null leaves
// the cuirass to the list alone.
void HideFirstPersonNodes(bool enabled, const char* list, const char* cuirassKeep = nullptr);

// Clears every bit this code set. For when the mode is switched off.
void ReleaseHiddenFirstPersonNodes();

// Logs the first-person tree once - class, name, flags and child count of
// every node a few levels down, with its world position - and again when
// the tree changes (a new model after a race change or a load, armour put on,
// a weapon sheathed), up to 24 reports. For Debug.FirstPersonTreeProbe.
void ProbeFirstPersonTree();

// Logs any tree the same way, once per call: every node's class, name,
// flags, child count and world position, each line headed by `label`, at
// most maxLines of them. For the third-person body (Debug.BodyTreeProbe).
void ProbeNodeTree(const NiAVObject* root, const char* label, UInt32 maxLines);

// Logs every shown shape under root (not a node) whose world bound lies
// within withinUnits of eye ("Menu near shapes:", game/NearShapesLogic.h),
// at most maxLines; answers how many it found.
UInt32 LogShapesNearEye(const NiAVObject* root, const NiPoint3& eye, float withinUnits, const char* label,
                        UInt32 maxLines);

// For the pinned hands: the engine culls a shape by the world bound it
// computed from the animation, and a hand the controller holds in view
// could be thrown away because the animated arm had swung out of it - the
// hands vanishing while running, 2026-09-25 (the cause is a hypothesis; the
// log's "kept in view" line and the next run are its test). Sets the bound
// of every node named in the list, and of the nodes above it, to a sphere of
// this radius around the camera, after the engine's update and before the
// draw. Answers how many named nodes it found.
UInt32 KeepFirstPersonNodesInView(const char* list, const NiPoint3& centre, float radius);

// The compiled class of a scene object, from MSVC's RTTI records ("NiNode@@",
// "NiTriShape@@"), or "?" when it cannot be read; and whether such a name is
// a node that keeps children.
const char* NiClassNameOf(const void* object);
bool NiClassIsNode(const char* className);

// The children of a scene node - a class that keeps children - and their
// count; null and 0 for geometry, or anything that does not read as a node.
// Null entries are normal: the array has holes.
NiAVObject* const* NodeChildren(const NiAVObject* node, UInt32& count);

// One walk of the tree under `under`, itself included: for each of `names`,
// the first node carrying it (NameInList's match - case aside, "Name:N" as
// Name), or null. Answers how many were found. For the third-person
// skeleton's bones, a frame's worth at a time (game/BodyIk.cpp).
UInt32 FindNodesNamed(NiAVObject* under, const char* const* names, UInt32 count, NiAVObject** out);

// KeepFirstPersonNodesInView for any tree: the nodes under `root` named in
// the list, and every node above each up to `root`, get a world bound round
// `centre` at least `radius` wide. For the body's arms, which the IK carries
// out of the bound the engine worked out from the animation.
UInt32 KeepNodesInView(NiAVObject* root, const char* list, const NiPoint3& centre, float radius);

// The same for a whole tree: `root` and every node and shape under it get a
// world bound round `centre` at least `radius` wide. For the body shown
// under the headset: a skinned shape's bound follows its own node, which
// hangs under the player root the engine keeps where the game has the
// player - a step in the room carries the body away from it, and looking
// down the engine culled the cuirass, then the torso (harness
// body-look-down-cull, 2026-10-28: 48 units off after a 72 cm step).
// Answers how many it set.
UInt32 KeepTreeInView(NiAVObject* root, const NiPoint3& centre, float radius);

}  // namespace obvr::game
