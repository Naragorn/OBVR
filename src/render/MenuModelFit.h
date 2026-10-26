#pragma once

#include "core/MathFns.h"
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

// The view's y scale, read off one model draw's world-view-projection (the
// vertex shader's c0 to c3, one row each: clip x, y, z, w).
//
// c1.z alone is the y scale only for a piece whose own z axis stands up in
// the scene - the body. A rigid piece turned in the hand or at the hip gives
// the scale times the cosine of its tilt: in one frame the body read 4.229,
// the sheathed sword's pieces 0.075 and 0.027 (paperdoll-size.txt
// 2026-10-25 20:07), and whichever draw came last stood the figure. At 0.027
// the fit put it 2.6 units from the camera instead of 405 - the camera inside
// the figure, nothing to see (Nadi's "extrem vergrößert", Windows,
// 2026-10-24). The rows' lengths do not turn with the piece: the y row is the
// view's up times the y scale, the w row the view's direction, both in the
// piece's own units, so the y row's length over the w row's is the y scale
// for every piece at any turn and any scale of its own.
//
// Only rows that read as a perspective's are taken: the depth row along the
// view's direction (Direct3D's z = a w + b, a 1.001 here), the x and y rows
// square to it and to each other. Another layout - the Windows log's draws
// with the projection in c1 to c4, whose c1 is the x row - is refused, and so
// is anything not a number. A refusal leaves yScale as it was.
inline constexpr float kMenuViewRowSlack = 0.01f;  // of the rows' lengths, for "square" and "along"

inline bool MenuViewYScaleFromRows(const float* c, float& yScale) {
	if (c == nullptr) {
		return false;
	}
	for (int i = 0; i < 16; ++i) {
		if (!MenuModelNumber(c[i])) {
			return false;
		}
	}
	const float* const x = c;
	const float* const y = c + 4;
	const float* const z = c + 8;
	const float* const w = c + 12;
	const auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
	const auto within = [](float value, float bound) { return value <= bound && value >= -bound; };
	const float ww = dot(w, w);
	if (!(ww > 1.0e-8f)) {
		return false;
	}
	const float lw = math::Sqrt(ww);
	const float lx = math::Sqrt(dot(x, x));
	const float ly = math::Sqrt(dot(y, y));
	const float lz = math::Sqrt(dot(z, z));
	const float along = dot(z, w) / ww;
	if (!(along > 0.5f && along < 2.0f)) {
		return false;
	}
	const float off[3] = {z[0] - along * w[0], z[1] - along * w[1], z[2] - along * w[2]};
	if (!within(math::Sqrt(dot(off, off)), kMenuViewRowSlack * lz)) {
		return false;
	}
	if (!within(dot(y, w), kMenuViewRowSlack * ly * lw) || !within(dot(x, w), kMenuViewRowSlack * lx * lw) ||
	    !within(dot(x, y), kMenuViewRowSlack * lx * ly)) {
		return false;
	}
	const float scale = ly / lw;
	if (!(scale > 0.01f && scale < 100.0f)) {
		return false;
	}
	yScale = scale;
	return true;
}

// The bound's jumps, named in the log. Nadi's Windows log of 2026-10-25 21:17
// has the model's world bound at 97.9 and, for single frames, 158, 281, 291,
// 834 and 878 - the fit standing the figure up to three times smaller then -
// with nothing saying what reached out. The first bound read is named once as
// the baseline, and every frame the radius grows by more than 30 % over the
// frame before (the sword drawn, 91 to 105, stays under it).
inline constexpr float kMenuBoundGrowth = 1.3f;

inline bool MenuModelBoundWorthNaming(float before, float now) {
	if (!MenuModelNumber(now) || !(now > 0.0f) || !MenuModelNumber(before)) {
		return false;
	}
	return !(before > 0.0f) || now > before * kMenuBoundGrowth;
}

// How far a piece's bound reaches from the model's root: its centre's
// distance plus its radius; -1 for a bound that reads as nothing. A radius of
// 0 is nothing too: the empty node "Camera01" under the head keeps a bound of
// radius 0 at the scene's origin, 197.7 from the root, and the model's bound
// of 91.2 does not take it in (paperdoll-size.txt 2026-10-26 07:33).
inline float MenuBoundReach(const NiPoint3& centre, float radius, const NiPoint3& root) {
	if (!MenuModelNumber(radius) || !(radius > 0.0f) || !MenuModelNumber(centre.x) || !MenuModelNumber(centre.y) ||
	    !MenuModelNumber(centre.z)) {
		return -1.0f;
	}
	const float dx = centre.x - root.x;
	const float dy = centre.y - root.y;
	const float dz = centre.z - root.z;
	return math::Sqrt(dx * dx + dy * dy + dz * dz) + radius;
}

}  // namespace obvr::render
