#pragma once

// Blood where a blade went in (docs/blade-penetration-spec.md section 9;
// Nadi, 2026-11-08): a spurt at a stab's entry and again as it comes out, a
// spurt where a slash goes in, and a stain trail along the cut on the body
// with a drip under it now and then. Only on what the entry gate let in -
// skin and cloth, light armour only after an entry by pressure - and never on
// a blade glancing off armour. Whether the actor bleeds at all (a skeleton, a
// ghost: no blood path) is the engine's, asked by game/Blood.cpp. Pure;
// blade_blood_test.

#include "core/Types.h"
#include "game/BladeEntryLogic.h"
#include "game/BladeMaterial.h"
#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::game {

struct BladeBloodSettings {
	bool enabled = true;
	float trailSpacingUnits = 10.0f;  // a stain each this far along a slash
	UInt32 dripEvery = 2;             // every so many stains a drip to the ground too
	UInt32 mostStainsPerCut = 10;
	float outUnits = 2.0f;            // spurts start this far outside the surface
};

struct BladeBloodFrame {
	bool entered = false;
	bool byPressure = false;
	BladeEntryKind kind = BladeEntryKind::None;
	BodyMaterial material = BodyMaterial::Unknown;
	bool touching = false;  // the contact goes on (false: it ended this frame or none)
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // outward
};

struct BladeBloodState {
	bool in = false;  // a cut that bleeds is going on
	BladeEntryKind kind = BladeEntryKind::None;
	UInt32 stains = 0;
	NiPoint3 lastStain{0.0f, 0.0f, 0.0f};
	NiPoint3 lastPoint{0.0f, 0.0f, 0.0f};
	NiPoint3 lastNormal{0.0f, 0.0f, 1.0f};
};

struct BloodAct {
	bool spray = false;  // SprayBlood(at, dir)
	NiPoint3 sprayAt{0.0f, 0.0f, 0.0f};
	NiPoint3 sprayDir{0.0f, 0.0f, 1.0f};
	bool stain = false;  // StainBlood(at, dir) onto the body
	NiPoint3 stainAt{0.0f, 0.0f, 0.0f};
	NiPoint3 stainDir{0.0f, 0.0f, -1.0f};
	bool drip = false;  // StainBlood straight down from the cut, onto the ground
	NiPoint3 dripAt{0.0f, 0.0f, 0.0f};
	bool pulledOut = false;  // the spurt is a stab's coming out
};

// Whether what was let in bleeds: skin and cloth; light armour only when it
// was pressed through.
inline bool BleedsThrough(BodyMaterial m, bool byPressure) {
	return m == BodyMaterial::Skin || m == BodyMaterial::Cloth || (m == BodyMaterial::Light && byPressure);
}

inline NiPoint3 BloodUnit(const NiPoint3& v) {
	const float l2 = v.LengthSquared();
	if (!(l2 > 1e-12f)) {
		return NiPoint3{0.0f, 0.0f, 1.0f};
	}
	const float inv = 1.0f / math::Sqrt(l2);
	return v * inv;
}

inline BloodAct StepBladeBlood(BladeBloodState& s, const BladeBloodSettings& set, const BladeBloodFrame& f) {
	BloodAct a;
	if (!set.enabled) {
		s = BladeBloodState{};
		return a;
	}
	const bool bleeding = f.touching && f.entered && BleedsThrough(f.material, f.byPressure);
	if (!bleeding) {
		// a stab drawn out of a cut that bled: a spurt where it came out
		if (s.in && s.kind == BladeEntryKind::Stab) {
			a.spray = true;
			a.pulledOut = true;
			a.sprayDir = BloodUnit(s.lastNormal);
			a.sprayAt = s.lastPoint + a.sprayDir * set.outUnits;
		}
		s = BladeBloodState{};
		return a;
	}
	const NiPoint3 out = BloodUnit(f.normal);
	if (!s.in) {
		s.in = true;
		s.kind = f.kind;
		a.spray = true;
		a.sprayDir = out;
		a.sprayAt = f.point + out * set.outUnits;
		a.stain = true;
		a.stainAt = a.sprayAt;
		a.stainDir = out * -1.0f;
		s.stains = 1;
		s.lastStain = f.point;
	} else if (f.kind == BladeEntryKind::Slash && s.stains < set.mostStainsPerCut &&
	           (f.point - s.lastStain).LengthSquared() >= set.trailSpacingUnits * set.trailSpacingUnits) {
		a.stain = true;
		a.stainAt = f.point + out * set.outUnits;
		a.stainDir = out * -1.0f;
		++s.stains;
		s.lastStain = f.point;
		if (set.dripEvery > 0 && s.stains % set.dripEvery == 0) {
			a.drip = true;
			a.dripAt = f.point;
		}
	}
	s.lastPoint = f.point;
	s.lastNormal = out;
	return a;
}

}  // namespace obvr::game
