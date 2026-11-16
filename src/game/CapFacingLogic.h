#pragma once

// Which way a wound cap faces and how it is turned over (game/DismemberCaps.cpp,
// game/Dismember.cpp), out of DismemberLogic.h for its size. Pure, covered by
// dismember_test (TestCapFacing, TestCapFlip).

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Which way a wound cap faces, from its vertex normals: the vertices and
// normals of its shapes summed in the bone's space (each shape's skin-to-bone
// applied). A normal not of unit length is not trusted for the facing.
struct CapFacing {
	NiPoint3 sum{0.0f, 0.0f, 0.0f};     // of the vertices
	NiPoint3 normal{0.0f, 0.0f, 0.0f};  // of the unit normals
	UInt32 count = 0;
	UInt32 unitNormals = 0;
};

inline void AddCapFacing(CapFacing& f, const NiPoint3* verts, const NiPoint3* normals, UInt32 n,
                         const NiTransform& skinToBone) {
	if (verts == nullptr || normals == nullptr) {
		return;
	}
	for (UInt32 i = 0; i < n; ++i) {
		f.sum = f.sum + skinToBone.pos + skinToBone.rot * (verts[i] * skinToBone.scale);
		++f.count;
		const float len2 = normals[i].LengthSquared();
		if (len2 > 0.81f && len2 < 1.21f) {
			f.normal = f.normal + skinToBone.rot * normals[i];
			++f.unitNormals;
		}
	}
}

inline NiPoint3 CapCentre(const CapFacing& f) {
	return f.count == 0 ? NiPoint3{0.0f, 0.0f, 0.0f} : f.sum * (1.0f / static_cast<float>(f.count));
}

// The share of the cap's mean normal along `dir` (unit): 1 facing it, -1
// facing away, 0 when nothing was read.
inline float CapFacingAlong(const CapFacing& f, const NiPoint3& dir) {
	const float len = std::sqrt(f.normal.LengthSquared());
	if (!(len > 1e-6f)) {
		return 0.0f;
	}
	return (f.normal.x * dir.x + f.normal.y * dir.y + f.normal.z * dir.z) / len;
}

// The bone's transform with the cap turned half round about an axis across
// `down` (the bone's space) through `centre` (the bone's space): the cap stays
// in place and faces the other way. The head's cap faced up into the head,
// its underside seen through (2026-11-03).
inline NiTransform FlippedCap(const NiTransform& bone, const NiPoint3& centre, const NiPoint3& down) {
	const NiPoint3 helper = std::fabs(down.x) < 0.9f ? NiPoint3{1.0f, 0.0f, 0.0f} : NiPoint3{0.0f, 1.0f, 0.0f};
	NiPoint3 a{down.y * helper.z - down.z * helper.y, down.z * helper.x - down.x * helper.z,
	           down.x * helper.y - down.y * helper.x};
	const float len = std::sqrt(a.LengthSquared());
	a = len > 1e-6f ? a * (1.0f / len) : NiPoint3{0.0f, 0.0f, 1.0f};
	NiMatrix33 half{};  // 2 a a^T - I: half a turn about a
	const float v[3] = {a.x, a.y, a.z};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			half.data[r][c] = 2.0f * v[r] * v[c] - (r == c ? 1.0f : 0.0f);
		}
	}
	NiTransform t = bone;
	t.rot = bone.rot * half;
	t.pos = bone.pos + bone.rot * ((centre - half * centre) * bone.scale);
	return t;
}

}  // namespace obvr::game
