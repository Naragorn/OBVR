#pragma once

// Cut marks where a blade went in (docs/blade-penetration-spec.md step 3):
// the engine's own decals on skinned bodies, as its melee blood decals are
// laid, turned so the mark's long side runs along the cut. Pure;
// cut_mark_test.
//
// Read 2026-11-08 from the EXE (objdump, Ghidra through REA):
// - 0x004CF1A0, thiscall on the reference's cell (TESObjectREFR+0x40, as
//   0x006ECC80 reads it), `ret 30h`, twelve arguments: the point (x, y, z),
//   the direction it is projected along (x, y, z), the texture's path under
//   "Textures\", the node it is laid on (walked down to every geometry, the
//   nodes named Decal*, FaceGen*, Bip01* skipped), a pointer to a count of
//   geometries still to skip (0: the first), a second node (0: none), the
//   turn about the direction in radians, and one more not read on this path;
//   it lays nothing when fDecalLifetime is 0 or more than iMaxDecalsPerFrame
//   were laid this frame, and on skinned geometry only with
//   bDecalsOnSkinnedGeometry=1 (a BSTempEffectGeometryDecal, 0x0056CE90);
// - the decal's size is the EXE's own 15 units (pushed as 0x41700000 at the
//   call of its constructor), the texture's coordinates the point's place
//   over the size plus a half (0x0056E10C);
// - its frame is built from the direction (0x006F9290): the direction the
//   last row, a helper square to it from its smallest part, the middle row
//   the helper across the direction, the first row the middle across the
//   direction; then turned about the direction by the angle
//   (0x0070FDD0 makes the turn: cos, sin in its first row);
// - how long it lasts is the game's fDecalLifetime, for all decals alike
//   (10 s in both Oblivion.ini of this machine): not held per mark.
// Which row is the texture's u is derived, to be checked against a picture
// (the spec).

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeEntryLogic.h"
#include "game/BladeMaterial.h"
#include "game/NiMath.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

inline constexpr UInt32 kEngineDecal = 0x004CF1A0;
inline constexpr float kEngineDecalSizeUnits = 15.0f;

inline NiPoint3 CutUnit(const NiPoint3& v, const NiPoint3& fallback) {
	const float l = math::Sqrt(v.LengthSquared());
	return l > 1e-6f ? v * (1.0f / l) : fallback;
}

// The engine's frame for a projection direction (0x006F9290): rows[2] the
// direction, rows[1] the helper across it, rows[0] rows[1] across it.
inline void EngineDecalFrame(const NiPoint3& direction, NiPoint3 (&rows)[3]) {
	const NiPoint3 d = CutUnit(direction, NiPoint3{0.0f, 0.0f, 1.0f});
	const float ax = d.x < 0.0f ? -d.x : d.x;
	const float ay = d.y < 0.0f ? -d.y : d.y;
	const float az = d.z < 0.0f ? -d.z : d.z;
	NiPoint3 h;
	if (!(ay < ax || az < ax)) {
		h = NiPoint3{0.0f, -d.z, d.y};
	} else if (ax < ay || ay > az) {
		h = NiPoint3{-d.y, d.x, 0.0f};
	} else {
		h = NiPoint3{-d.z, 0.0f, d.x};
	}
	const NiPoint3 v = CutUnit(CrossOf3(h, d), NiPoint3{1.0f, 0.0f, 0.0f});
	rows[0] = CutUnit(CrossOf3(v, d), NiPoint3{0.0f, 1.0f, 0.0f});
	rows[1] = v;
	rows[2] = d;
}

// The mark's long side (the texture's u, the frame's first row turned by
// `angle` about the direction).
inline NiPoint3 DecalLongAxis(const NiPoint3& direction, float angle) {
	NiPoint3 r[3];
	EngineDecalFrame(direction, r);
	return r[0] * math::Cos(angle) + r[1] * math::Sin(angle);
}

// The turn that lays the long side along `cut` (its part across the
// direction); 0 for a cut along the direction or none.
inline float CutMarkAngle(const NiPoint3& cut, const NiPoint3& direction) {
	NiPoint3 r[3];
	EngineDecalFrame(direction, r);
	const NiPoint3 across = cut - r[2] * DotOf3(cut, r[2]);
	if (!(across.LengthSquared() > 1e-8f)) {
		return 0.0f;
	}
	return math::Atan2(DotOf3(across, r[1]), DotOf3(across, r[0]));
}

// The texture under Textures\ by material: none where no mark is made.
inline const char* CutMarkTexture(BodyMaterial m) {
	switch (m) {
	case BodyMaterial::Skin:
		return "Effects\\OBVR\\CutSkin.dds";
	case BodyMaterial::Cloth:
		return "Effects\\OBVR\\CutCloth.dds";
	case BodyMaterial::Light:
		return "Effects\\OBVR\\CutLeather.dds";
	default:
		return nullptr;
	}
}

struct CutMarkSettings {
	bool enabled = true;
	float spacingUnits = 8.0f;  // a mark each this far along a cut (the decal is 15 across)
	UInt32 mostPerCut = 12;
	float liftUnits = 2.0f;     // laid from this far outside the surface
};

struct CutMarkFrame {
	bool entered = false;  // the blade went in (BladeEntryVerdict)
	BladeEntryKind kind = BladeEntryKind::None;
	BodyMaterial material = BodyMaterial::Unknown;
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};    // outward
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};  // the contact point's
	NiPoint3 edge{1.0f, 0.0f, 0.0f};      // the blade's edge across it
};

struct CutMarkState {
	bool cutting = false;
	UInt32 marks = 0;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
};

struct CutMark {
	bool place = false;
	NiPoint3 from{0.0f, 0.0f, 0.0f};
	NiPoint3 direction{0.0f, 0.0f, -1.0f};
	float angle = 0.0f;
	const char* texture = nullptr;
};

// A mark when a cut begins and each spacing along it, at most so many; a
// stab one. None unless in, nor on what takes no mark.
inline CutMark StepCutMarks(CutMarkState& s, const CutMarkSettings& set, const CutMarkFrame& f) {
	CutMark m;
	const char* texture = CutMarkTexture(f.material);
	if (!set.enabled || !f.entered || texture == nullptr) {
		s = CutMarkState{};
		return m;
	}
	const bool fresh = !s.cutting;
	s.cutting = true;
	const NiPoint3 step = f.point - s.last;
	const bool due = fresh || (f.kind == BladeEntryKind::Slash && s.marks < set.mostPerCut &&
	                           step.LengthSquared() >= set.spacingUnits * set.spacingUnits);
	if (!due) {
		return m;
	}
	++s.marks;
	s.last = f.point;
	const NiPoint3 out = CutUnit(f.normal, NiPoint3{0.0f, 0.0f, 1.0f});
	m.place = true;
	m.texture = texture;
	m.direction = out * -1.0f;
	m.from = f.point + out * set.liftUnits;
	const NiPoint3 cut = f.kind == BladeEntryKind::Slash && f.velocity.LengthSquared() > 1e-6f ? f.velocity : f.edge;
	m.angle = CutMarkAngle(cut, m.direction);
	return m;
}

// A shape under the actor a decal could go to: whether the engine skips it
// by its name (Decal*, FaceGen*, Bip01*), whether it is hidden, its world
// bound.
struct DecalShape {
	bool skippedName = false;
	bool hidden = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
};

// The shapes a decal at `p` is laid on, one engine call each, at most
// `most`, the one the point lies deepest inside first: the bounds `p` is
// within (or `slack` past). From the root, the engine's walk counts every
// shape it visits against iMaxDecalsPerFrame (0x004CF1A0: the count at
// 0x00B35C04 against 0x00B097D0, 10) - the actor's eight FaceGen shapes
// come first - so a body shape further on, the clothes most of all, and
// every call after the first in a frame went unlaid.
inline UInt32 PickDecalShapes(const DecalShape* shapes, UInt32 count, const NiPoint3& p, float slack, UInt32* out,
                              UInt32 most) {
	UInt32 n = 0;
	float depth[16];
	for (UInt32 i = 0; i < count && most > 0; ++i) {
		const DecalShape& s = shapes[i];
		if (s.skippedName || s.hidden) {
			continue;
		}
		const float inside = s.radius - math::Sqrt((p - s.centre).LengthSquared());
		if (!(inside >= -slack)) {
			continue;
		}
		// insert by depth, the deepest first, keeping at most `most` (16)
		UInt32 at = n;
		while (at > 0 && depth[at - 1] < inside) {
			--at;
		}
		const UInt32 keep = most < 16 ? most : 16;
		if (at >= keep) {
			continue;
		}
		const UInt32 last = n < keep ? n : keep - 1;
		for (UInt32 k = last; k > at; --k) {
			out[k] = out[k - 1];
			depth[k] = depth[k - 1];
		}
		out[at] = i;
		depth[at] = inside;
		n = n < keep ? n + 1 : keep;
	}
	return n;
}

}  // namespace obvr::game
