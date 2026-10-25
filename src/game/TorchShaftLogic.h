#pragma once

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/HandContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The torch in the off hand as a guard (the tester, 2026-10-25: "mach das
// man mit torch auch blocken kann"): its shaft parries a blade it meets
// during the attacker's swing, as the player's own blade does
// (game/ParryLogic.h, WhatMeetsTheirBlade).
//
// The shaft is read from the torch's model the way the shield's disc is
// (HandContactLogic.h, DiscScatter and DiscAxesFrom), in the hand's frame:
// the line of the largest spread. Its reach along that line counts only the
// vertices within kTorchBandUnits of it. The engine hangs the fire on the
// torch's FlameNode1 (Fire\FireTorchLarge.nif, read 2026-10-25: a billboard
// quad 32 units wide and 64 tall over the flame), and its corners would
// stretch the shaft half a metre past the torch's head. Lights\Torch02.NIF,
// the torch carried (four LIGH records of Oblivion.esm name it), runs 48
// units along its y axis, from 14 below the grip to 34 above it, 5 units
// thick. Pure; torch_shaft_test.

inline constexpr float kTorchBandUnits = 5.0f;
// A torch's length: 16 to 120 units end to end. A stick is shorter, a
// staff longer.
inline constexpr float kTorchHalfMinUnits = 8.0f;
inline constexpr float kTorchHalfMaxUnits = 60.0f;
// A line, not a disc: the spread across at most this share of the spread
// along (variances: Torch02's shaft reads under 0.02, a shield near 1).
inline constexpr float kTorchSlenderness = 0.25f;
// Read again this often while the same torch is held.
inline constexpr UInt32 kTorchShaftReadFrames = 90;

// The second pass: how far the vertices near the line reach along it.
struct TorchExtent {
	UInt32 count = 0;
	float low = 0.0f;
	float high = 0.0f;

	void Add(const NiPoint3& p, const NiPoint3& centre, const NiPoint3& axis) {
		const NiPoint3 d = p - centre;
		const float along = DotOf3(d, axis);
		const NiPoint3 off = d - axis * along;
		if (!(off.LengthSquared() <= kTorchBandUnits * kTorchBandUnits) || !(along == along)) {
			return;
		}
		if (count == 0 || along < low) {
			low = along;
		}
		if (count == 0 || along > high) {
			high = along;
		}
		++count;
	}
};

// The shaft's two ends, in the hand's frame.
struct TorchShaft {
	bool valid = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	UInt32 vertices = 0;
};

// The shaft from the axes found (`axis` the first, `spread` all three) and
// a finished extent. False for too few vertices near the line, a length no
// torch has, or a spread that is no line's.
inline bool TorchShaftFrom(const NiPoint3& centre, const NiPoint3& axis, const float spread[3], const TorchExtent& e,
                           TorchShaft& out) {
	out = TorchShaft{};
	if (e.count < kDiscMinVertices) {
		return false;
	}
	if (!(spread[0] > 0.0f) || !(spread[1] <= spread[0] * kTorchSlenderness)) {
		return false;
	}
	const float half = (e.high - e.low) * 0.5f;
	if (!(half >= kTorchHalfMinUnits && half <= kTorchHalfMaxUnits)) {
		return false;
	}
	out.valid = true;
	out.a = centre + axis * e.low;
	out.b = centre + axis * e.high;
	out.vertices = e.count;
	return true;
}

// The shaft in the world, carried by the hand's pose (world from the hand).
inline bool TorchShaftInWorld(const TorchShaft& s, const NiMatrix33& rot, const NiPoint3& pos, NiPoint3& a,
                              NiPoint3& b) {
	if (!s.valid) {
		return false;
	}
	a = pos + rot * s.a;
	b = pos + rot * s.b;
	return true;
}

// Whether the shaft kept is still the torch held: read again for another
// node or another torch, or every so often. The node's bound is no sign -
// the fire's particles move it every frame.
inline bool TorchShaftStale(bool have, UInt32 keptNode, UInt32 node, UInt32 keptForm, UInt32 form, UInt32 age) {
	return !have || keptNode != node || keptForm != form || age >= kTorchShaftReadFrames;
}

}  // namespace obvr::game
