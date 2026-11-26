#pragma once

// Hiding the hip's sheath node (vr::HideHipSheath, free-weapons-spec 5.12.3):
// the node's local transform with its scale shrunk to almost nothing, so the
// scabbard and the weapon the game hangs there (its children) are too small to
// be seen. Not exactly zero: a zero scale makes the node's matrix singular for
// whatever reads it back. The position and rotation stay, so the node is where
// the game put it when it shows again. Pure; kept_on_body_test.

#include "game/NiMath.h"

namespace obvr::game {

inline constexpr float kHiddenSheathScale = 0.001f;

inline NiTransform HiddenSheathLocal(const NiTransform& local) {
	NiTransform t = local;
	t.scale = local.scale * kHiddenSheathScale;
	return t;
}

}  // namespace obvr::game
