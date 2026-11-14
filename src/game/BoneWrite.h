#pragma once

// The body IK's booking of what it writes to a bone (game/BodyIk.cpp): the
// local transform before the write and the write, so the write can be given
// back where the engine left it - at the next step, after the world is drawn
// (GiveBackAfterDraw: the next animation then blends from the game's pose, not
// from the write; Nadi, 2026-11-14, the sheath crossing to the other hip and
// the forearms shaking on a draw), and for good on release.

#include "game/GameTypes.h"
#include "game/TransformCompare.h"

namespace obvr::game {

struct BoneWrite {
	NiAVObject* node = nullptr;
	NiTransform base{};   // the local transform before this frame's write
	NiTransform wrote{};  // what was written
	bool held = false;    // written this frame (or last, until begun again)
};

// Before a bone is read: what it held before last frame's write goes back
// if it still holds that write; then its base is what it has now. `kept`
// counts the bones the engine left, `renewed` the ones it wrote again.
inline bool BeginWrite(BoneWrite& w, NiAVObject* node, UInt32& kept, UInt32& renewed) {
	bool restored = false;
	if (w.held && w.node == node && node != nullptr) {
		if (SameTransform(node->localTransform, w.wrote)) {
			node->localTransform = w.base;
			restored = true;
			++kept;
		} else {
			++renewed;
		}
	}
	w.held = false;
	w.node = node;
	if (node != nullptr) {
		w.base = node->localTransform;
	}
	return restored;
}

inline void WriteLocal(BoneWrite& w, const NiTransform& local) {
	if (w.node == nullptr) {
		return;
	}
	w.node->localTransform = local;
	w.wrote = local;
	w.held = true;
}

// Given back for good: only the node recorded, found again in the tree the
// engine has now, and only if it still holds the write.
inline bool ReleaseWrite(BoneWrite& w, NiAVObject* nodeNow) {
	bool restored = false;
	if (w.held && w.node != nullptr && w.node == nodeNow && SameTransform(nodeNow->localTransform, w.wrote)) {
		nodeNow->localTransform = w.base;
		restored = true;
	}
	w = BoneWrite{};
	return restored;
}

// After the world is drawn: the write given back if the bone still holds it,
// the booking kept, so the next step's BeginWrite reads the game's own pose.
inline void GiveBackAfterDraw(BoneWrite& w) {
	if (w.held && w.node != nullptr && SameTransform(w.node->localTransform, w.wrote)) {
		w.node->localTransform = w.base;
	}
}

}  // namespace obvr::game
