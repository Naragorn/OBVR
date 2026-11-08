#pragma once

// The living as contact meets them (game/BladeContactLogic.h): the
// capsules on their drawn bones, each with its radial field where its fit
// built one (game/RadialField.h), the lists they are collected in
// (game::CollectBladeBodies), and how deep a shape may lie in them and
// still be held at them (the shallow band). Pure.

#include <memory>
#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyPart.h"
#include "game/NiMath.h"
#include "game/RadialField.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

// How deep in a person's capsule a shape may lie and still be held at them
// rather than passed. The shape is held 1.5 units short of a person, and a
// standing person's idle - breathing, swaying - moves their capsules a unit
// or two a frame: a blade resting on them woke up inside them and, until
// 2026-10-15, passed them and went through (the tester: "manchmal auch
// direkt durch den körper"; the harness: a frozen beggar's torso closed over
// the held blade, "walked into the blade where it was"). Deeper than the
// band the shape was taken up in them, or they walked into it: passed as
// before. The band is at most half the radius, so a thin limb crossed
// through its axis still counts as crossed (PersonAcross).
inline constexpr float kBladeShallowUnits = 3.0f;

// A shape held only just inside someone (the band above) is pushed back out
// of them this far a frame, along the normal out of their capsule, until it
// rests on their surface: held where it was, a blade stayed up to 3 units
// inside them (mesh-fitted-capsules-spec 8, armour-touch-blade-mod.txt
// 2026-11-07: 2.5 units into a mod cuirass's arm). A unit a frame is 0.9
// m/s at 90 Hz - out in a few frames, no jump.
inline constexpr float kBladeShallowPushUnits = 1.0f;

inline float ShallowBand(float radius) {
	const float half = radius * 0.5f;
	return half < kBladeShallowUnits ? half : kBladeShallowUnits;
}

// A capsule on the bones of someone living: which actor it belongs to, and
// which part of them it is; its radial field where its fit built one
// (game/RadialField.h) - contact then meets the field's surface, not the
// round one (docs/mesh-fitted-capsules-spec.md 13).
struct BladeBodyCapsule {
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	FieldRef shape;
};

// Room for a dozen people: a fitted body brings 19-36 capsules (its groups
// split and refined where they leave the surface, game/CapsuleFitLogic.h
// RefineFit; Jauffre in a robe 35, 2026-11-07), a guessed one 13.
inline constexpr UInt32 kBladeBodyCapsulesMax = 768;
// The fields' owners a list holds on to: one a fitted person.
inline constexpr UInt32 kBladeFieldSetsMax = 32;

using FieldSet = std::shared_ptr<const std::vector<RadialField>>;

struct BladeBodies {
	UInt32 count = 0;
	BladeBodyCapsule cap[kBladeBodyCapsulesMax];
	// The fields its capsules point into, held for as long as the list is:
	// a refit replaces a person's fields while a list collected before it
	// is still read.
	UInt32 fieldSets = 0;
	FieldSet keep[kBladeFieldSetsMax];

	void Clear() {
		count = 0;
		for (UInt32 i = 0; i < fieldSets; ++i) {
			keep[i].reset();
		}
		fieldSets = 0;
	}

	// Holds `set` for the list; false when there is no room (its capsules
	// are then added without their fields).
	bool Keep(const FieldSet& set) {
		for (UInt32 i = 0; i < fieldSets; ++i) {
			if (keep[i] == set) {
				return true;
			}
		}
		if (set == nullptr || fieldSets >= kBladeFieldSetsMax) {
			return false;
		}
		keep[fieldSets++] = set;
		return true;
	}

	bool Add(const NiPoint3& a, const NiPoint3& b, float radius, UInt32 actor, BodyPart part = BodyPart::Column,
	         const FieldRef& shape = FieldRef{}) {
		if (count >= kBladeBodyCapsulesMax || !(radius > 0.0f)) {
			return false;
		}
		cap[count].a = a;
		cap[count].b = b;
		cap[count].radius = radius;
		cap[count].actor = actor;
		cap[count].part = part;
		cap[count].shape = shape;
		++count;
		return true;
	}
};

// A body capsule's surface as contact meets it - its field's where it has
// one (RadialField.h's shape queries).
inline float BodyGapAt(const BladeBodyCapsule& c, const NiPoint3& p, NiPoint3* normal = nullptr) {
	return ShapeGapAt(c.a, c.b, c.radius, c.shape, p, normal);
}

inline float BodySegmentGap(const BladeBodyCapsule& c, const NiPoint3& from, const NiPoint3& to, float within,
                            NiPoint3* at = nullptr, NiPoint3* normal = nullptr) {
	return ShapeSegmentGap(c.a, c.b, c.radius, c.shape, from, to, at, normal, within);
}

inline bool BodyEnters(const BladeBodyCapsule& c, const NiPoint3& from, const NiPoint3& to, float grow, float& fraction,
                       NiPoint3& normal, bool& inside) {
	return ShapeSegmentEnters(c.a, c.b, c.radius, c.shape, from, to, grow, fraction, normal, inside);
}

// What the capsule's surface is where `p` is (game/BladeMaterial.h): its
// field's material there, the armour points in `rating`; Unknown without a
// classed field (a guessed capsule).
inline BodyMaterial BodyMaterialAt(const BladeBodyCapsule& c, const NiPoint3& p, UInt8* rating = nullptr) {
	if (c.shape.field == nullptr) {
		if (rating != nullptr) {
			*rating = 0;
		}
		return BodyMaterial::Unknown;
	}
	const FieldPlace place{c.a, c.b, c.shape.u, c.shape.scale, c.shape.w};
	return FieldMaterialAt(*c.shape.field, place, p, rating);
}

// The shallow band where `p` is: the round capsule's. Taken from the
// field's radius there (half a sleeve's 4.4 round a thin arm's 3.6) the
// band let shapes be held deeper, and the armour scenarios sank further in:
// blade mod 94 -> 68 %, torch mod 95 -> 82 % of the lines on the mesh, all
// misses "into" (harness 2026-11-07 22:23).
inline float BodyBandAt(const BladeBodyCapsule& c, const NiPoint3& /*p*/) { return ShallowBand(c.radius); }

// The segment lies deeper in the body than the band where it is deepest.
inline bool BodyCrossedDeep(const BladeBodyCapsule& c, const NiPoint3& from, const NiPoint3& to) {
	NiPoint3 at;
	const float gap = BodySegmentGap(c, from, to, 0.0f, &at);
	return gap < -BodyBandAt(c, at);
}

}  // namespace obvr::game
