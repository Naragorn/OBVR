#pragma once

// The turn a dropped item is given: the one it had in the hand (Nadi,
// 2026-10-26: "Die waffe/torch hat wenn sie fällt eine ganz andere
// Ausrichtung als wie sie in der hand gehalten wurde").
//
// A reference keeps its turn as three angles (TESObjectREFR +0x20, +0x24,
// +0x28), and the engine builds the 3D's matrix from them in 0x004D7AF0 for
// anything not an actor, read in Oblivion.exe 1.2.0.416 (objdump,
// 2026-10-26): the identity times about-x (0x0070FD30), times about-y
// (0x0070FD80), times about-z (0x0070FDD0), each a row-major matrix as below,
// multiplied in that order by 0x007100A0. RefMatrixFromAngles is that build;
// RefAnglesFromMatrix finds the three angles back for any turn, the two
// straight up and down included.
//
// The item's turn in the hand is read off the drawn first-person bones,
// which show the hand's pose of the frame before; CarryItemPose keeps the
// item where it hangs in the hand and puts it on the hand's pose now, as the
// hand contacts read their shapes (CameraHook, g_handContact).
//
// Pure; covered by drop_rotation_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::game {

inline NiMatrix33 RefMatrixAboutX(float a) {
	const float c = math::Cos(a);
	const float s = math::Sin(a);
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[1][1] = c;
	m.data[1][2] = s;
	m.data[2][1] = -s;
	m.data[2][2] = c;
	return m;
}

inline NiMatrix33 RefMatrixAboutY(float a) {
	const float c = math::Cos(a);
	const float s = math::Sin(a);
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = c;
	m.data[0][2] = -s;
	m.data[2][0] = s;
	m.data[2][2] = c;
	return m;
}

inline NiMatrix33 RefMatrixAboutZ(float a) {
	const float c = math::Cos(a);
	const float s = math::Sin(a);
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = c;
	m.data[0][1] = s;
	m.data[1][0] = -s;
	m.data[1][1] = c;
	return m;
}

// The engine's build (0x004D7AF0): about x, then y, then z.
inline NiMatrix33 RefMatrixFromAngles(const NiPoint3& angles) {
	return RefMatrixAboutX(angles.x) * RefMatrixAboutY(angles.y) * RefMatrixAboutZ(angles.z);
}

// The three angles whose build is `m`. Row 0 is (cy cz, cy sz, -sy), the
// last column (-sy, sx cy, cx cy); with the y turn a quarter either way (cy
// gone) z is taken as 0 and x read off row 1, (sx sy, cx, 0).
inline NiPoint3 RefAnglesFromMatrix(const NiMatrix33& m) {
	const float cy = math::Sqrt(m.data[0][0] * m.data[0][0] + m.data[0][1] * m.data[0][1]);
	const float y = math::Atan2(-m.data[0][2], cy);
	if (cy > 1.0e-5f) {
		return NiPoint3{math::Atan2(m.data[1][2], m.data[2][2]), y, math::Atan2(m.data[0][1], m.data[0][0])};
	}
	const float sy = -m.data[0][2] > 0.0f ? 1.0f : -1.0f;
	return NiPoint3{math::Atan2(m.data[1][0] * sy, m.data[1][1]), y, 0.0f};
}

// How far apart two turns are, in degrees (the angle of a's transpose times b).
inline float TurnBetweenDegrees(const NiMatrix33& a, const NiMatrix33& b) {
	float trace = 0.0f;
	for (int i = 0; i < 3; ++i) {
		for (int k = 0; k < 3; ++k) {
			trace += a.data[k][i] * b.data[k][i];
		}
	}
	float c = (trace - 1.0f) * 0.5f;
	c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
	return math::Atan2(math::Sqrt(1.0f - c * c), c) * 57.2957795f;
}

struct ItemPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

inline NiMatrix33 TransposedTurn(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

// The item as drawn (`itemRot`, `itemPos`) on the hand as drawn
// (`drawnHand`, `drawnHandPos`), carried onto the hand now.
inline ItemPose CarryItemPose(const NiMatrix33& drawnHand, const NiPoint3& drawnHandPos, const NiMatrix33& itemRot,
                              const NiPoint3& itemPos, const NiMatrix33& nowHand, const NiPoint3& nowHandPos) {
	const NiMatrix33 toHand = TransposedTurn(drawnHand);
	ItemPose out;
	out.rot = nowHand * (toHand * itemRot);
	out.pos = nowHandPos + nowHand * (toHand * (itemPos - drawnHandPos));
	return out;
}

}  // namespace obvr::game
