#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::render {

// The figure the engine draws in its menus - the inventory's paperdoll, the
// stats page's - fitted to the rows the 2D layer shows.
//
// The engine keeps a scene of its own in the interface manager for it (its
// constructor 0x57E510, read with Ghidra 2026-10-23): a root, a shadow scene
// node, the "PlayerSceneLight" the fMenuPlayerLight settings colour, and an
// NiNode at manager+0x60 the player's model ("PlayerInventoryPC") is hung on
// at no offset. The scene's camera sits at its origin looking along +y; the
// engine stands the holder 33 units right, 168 ahead and 99 below that axis,
// turned to face the camera (finery-paperdoll.txt 2026-10-23 12:22). The
// model draws go through a projection of 45.7 by 26.6 degrees at the
// believed 16:9 (the vertex shader's c0 to c3, read 12:25) - a camera that
// sees 13.3 degrees down, while the figure's feet stand 30.5 degrees below
// its axis: everything under 40 units below the axis is outside the view,
// and 40 units above the feet is the hips. Which is the picture: the robe
// ending at the hips with nothing beneath, on the harness's woman, the male
// Altmer and the Quest 3 tester's (his "disappearing outfit", 2026-10-17),
// with the body code off and the dual pass off alike. Where the engine's 168
// and 99 come from, and what it assumes of the view, is not read; what is
// read is the view and the figure, and that is enough to stand the figure
// where the view sees it.
//
// So the holder is moved: the figure's middle on the axis, far enough that
// its height with a margin fills the view's rows, the engine's own angle to
// the side kept. The figure's top and bottom come from the model's world
// bound (the sphere round everything on it, the drawn weapon included) over
// its root - the head bone with a head on top read 119 units for a figure
// the view then showed 144 tall from the feet 14 below the root to the
// sword's tip (12:34, the margin gone). Pure, over plain values;
// InterfaceRenderHook reads the scene and writes the holder.

// How much of the view's height the bound leaves free, as a factor over its
// half-height. The bound is a sphere, its radius the farthest point on the
// figure in any direction, so it already stands past the figure's top and
// bottom (the woman with a drawn sword: the sphere from 26 units below her
// feet to 30 over the sword's tip, 12:37, the figure itself three quarters
// of it); 1.05 leaves the sphere a sliver inside the rows and the figure
// well clear of the HUD bar along the bottom.
inline constexpr float kMenuModelMargin = 1.05f;

struct MenuModelFitInput {
	bool holderFound = false;
	bool modelAttached = false;
	float figureBottom = 0.0f;   // units from the model's root, down negative
	float figureTop = 0.0f;      // units from the model's root
	float tanHalfY = 0.0f;       // the view's vertical half-angle, as its tangent
	NiPoint3 engine{0.0f, 0.0f, 0.0f};  // where the engine stands the model's root (right, ahead, up)
};

struct MenuModelFitVerdict {
	bool fit = false;
	NiPoint3 pos{0.0f, 0.0f, 0.0f};  // where the model's root goes
};

inline bool MenuModelNumber(float v) { return v == v && v < 1.0e9f && v > -1.0e9f; }

inline MenuModelFitVerdict FitMenuModel(const MenuModelFitInput& in) {
	MenuModelFitVerdict v;
	if (!in.holderFound || !in.modelAttached) {
		return v;
	}
	if (!MenuModelNumber(in.figureBottom) || !MenuModelNumber(in.figureTop) ||
	    !(in.figureTop - in.figureBottom > 10.0f)) {
		return v;
	}
	if (!MenuModelNumber(in.tanHalfY) || !(in.tanHalfY > 0.01f)) {
		return v;
	}
	if (!MenuModelNumber(in.engine.x) || !MenuModelNumber(in.engine.y) || !(in.engine.y > 1.0f)) {
		return v;
	}
	const float half = (in.figureTop - in.figureBottom) * 0.5f;
	const float middle = (in.figureTop + in.figureBottom) * 0.5f;
	const float distance = half * kMenuModelMargin / in.tanHalfY;
	v.fit = true;
	v.pos = NiPoint3{distance * (in.engine.x / in.engine.y), distance, -middle};
	return v;
}

}  // namespace obvr::render
