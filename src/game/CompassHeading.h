#pragma once

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::game {

// The HUD's compass turned with the view, not the body (the tester,
// 2026-10-01: "der kompass wenn man hochschaut dreht sich nicht mit der
// headset sicht").
//
// HUDMainMenu's update (0x005A6DE0, vtable 0x00A6BEE4 slot +0x2C) sets the
// compass strip's user0 (trait 0xFAE, the Tile* at menu+0x40, through the
// float setter 0x0058CEB0) to a heading in degrees; the menu's XML places the
// strip, and the markers against it, from that. The heading is the player's
// virtual at +0x1E0 (0x0065DA60: rotZ plus the float at +0x61C, wrapped to
// [0, 2 pi)), called at 0x005A6F05..0x005A6F14 - `mov ecx,[0x00B333C4]; mov
// edx,[ecx]; mov eax,[edx+1E0h]; call eax` - with the interior cell's north
// rotation added after it (0x005A6F15, fadd [esp+4Ch]). In VR the head turns
// the camera, not the body, so the strip stood still while the eyes turned.
// The markers take their bearing from positions alone and are placed against
// the strip's heading, so they turn with it.
//
// Those sixteen bytes become a call to OBVR, which answers with the view's
// heading in radians on st0 - the camera's, as last drawn - or, with no
// camera yet, the engine's own. The virtual itself stays: it also feeds the
// melee cone and the bow's aim.
bool InstallCompassHeading();

// The view's heading for the compass from now on; false hands it back to the
// engine's.
void SetCompassViewHeading(bool valid, float radians);

// The heading of a camera's rotation in the engine's sense - from +y (north)
// towards +x, in [0, 2 pi) - level even looking straight up or down: the
// forward axis (column 1) laid flat, with the up axis (column 2) laid flat
// added on the side that points the same way (back when looking up, forward
// when looking down), so their sum keeps the heading's length
// (cos + |sin| of the pitch) however steep the look. False for a rotation
// with neither (a broken matrix).
inline bool ViewHeadingOf(const NiMatrix33& rot, float& radians) {
	const float fx = rot.data[0][1];
	const float fy = rot.data[1][1];
	const float fz = rot.data[2][1];
	const float ux = rot.data[0][2];
	const float uy = rot.data[1][2];
	const float side = fz > 0.0f ? -1.0f : 1.0f;
	const float hx = fx + side * ux;
	const float hy = fy + side * uy;
	if (!(hx * hx + hy * hy > 1e-8f)) {
		return false;
	}
	float heading = math::Atan2(hx, hy);
	if (heading < 0.0f) {
		heading += math::kTwoPi;
	}
	if (heading >= math::kTwoPi) {
		heading -= math::kTwoPi;
	}
	radians = heading;
	return true;
}

}  // namespace obvr::game
