#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Items near the hands, found by distance rather than by a ray.
//
// The world pick is one ray a frame, and a hand brought to an item without
// pointing at it found nothing: the left hand's tooltip and marker came only
// once the grip closed, or once the right hand's laser was on the item
// (2026-09-26). So the loaded items of the player's cell are measured
// against both hands - from the hand to the surface of each item's scene
// bound - and the pick is then aimed from the nearer hand at the nearest
// item within reach; the engine's own pick finds it on that line, and the
// tooltip, the marker and the grab follow as before.
//
// Read from xOBSE (GameObjects.h, GameForms.h): TESObjectREFR's parentCell at
// +0x40, its base form at +0x1C, its scene node at +0x3C; TESObjectCELL's
// objectList at +0x48, a list of { refr, next } whose first entry is inline;
// TESForm's flags at +0x08 (0x20 deleted, 0x800 disabled) and type at +0x04.
// Only the player's own cell: in the open world an item just across a cell
// border is not found.

// The item form types a hand can take (xOBSE GameForms.h): apparatus,
// armour, book, clothing, ingredient, light, misc, weapon, ammo, soul gem,
// key, potion, sigil stone.
inline bool IsHandItemType(UInt8 type) {
	switch (type) {
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x21:
	case 0x22:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x2A:
		return true;
	default:
		return false;
	}
}

// Whether the ring and the tooltip go on what the pick found: an item a hand
// can take, within reach of either hand. Anything else under the pick - an
// NPC coming close in a fight, most of all - gets neither (2026-09-26).
inline bool ReachMarkerWanted(bool haveRef, UInt8 baseFormType, bool nearRight, bool nearLeft) {
	return haveRef && IsHandItemType(baseFormType) && (nearRight || nearLeft);
}

// The side of the item nearest the hand (2026-09-26: the closer the hand
// comes, the closer the ring should move to it, and from a threshold on the
// side nearest the hand wins).
// The pick is aimed from the hand at a point between the item's middle and
// its surface point nearest the hand: all the middle while the hand is at
// the marker's distance, all the near side from [Hands] ReachNearSideMetres
// in, between them eased out - it starts moving as soon as the hand comes
// closer (2026-09-26: a smooth start and 10 cm moved it "much too late").
// The pick's hit is where the ring sits and what the grip takes.

// 0 at `farUnits` or beyond, 1 at `nearUnits` or closer, eased out between:
// fastest at the start.
inline float NearSideWeight(float distanceUnits, float farUnits, float nearUnits) {
	if (!(farUnits > nearUnits)) {
		return distanceUnits <= nearUnits ? 1.0f : 0.0f;
	}
	float t = (farUnits - distanceUnits) / (farUnits - nearUnits);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return 1.0f - (1.0f - t) * (1.0f - t);
}

// Whether a closed grip takes what the pick found at `distanceUnits` from the
// hand: anything within the grab's reach, and an item a hand can take (not a
// body or anything else the grab could move) within the pull's reach, from
// where it floats to the hand. A pull reach of 0 is off.
inline bool GripTakes(float distanceUnits, bool isHandItem, float grabReachUnits,
                      float pullReachUnits) {
	if (grabReachUnits > 0.0f && distanceUnits <= grabReachUnits) {
		return true;
	}
	return isHandItem && pullReachUnits > 0.0f && distanceUnits <= pullReachUnits;
}

// Where the pick aims: from the middle towards the near surface point by
// `weight`. The surface point is taken a little inside (a tenth of the way
// to the middle), so a ray at an edge point still meets the object.
inline NiPoint3 NearSideAimPoint(const NiPoint3& centre, const NiPoint3& nearSurface,
                                 float weight) {
	const NiPoint3 inside = nearSurface + (centre - nearSurface) * 0.1f;
	return centre + (inside - centre) * weight;
}

// Whether a vertex read from a geometry's data lies in that data's own bound
// sphere (with a little slack): the check that the layout read is the real
// one before any of it is used.
inline bool VertexInBound(const NiPoint3& v, const NiPoint3& boundCentre, float boundRadius) {
	const NiPoint3 d = v - boundCentre;
	const float r = boundRadius * 1.05f + 1.0f;
	return boundRadius >= 0.0f && d.LengthSquared() <= r * r;
}

// The vertex of the item's geometry nearest `hand`, in the world, and its
// distance. False when the item has no geometry this can read, or the read
// fails its checks (VertexInBound); the caller then keeps the middle.
bool NearestVertexOf(UInt32 ref, const NiPoint3& hand, NiPoint3& out, float& distanceOut);

// The type of a reference's base form, 0 when it cannot be read.
UInt8 RefBaseFormType(UInt32 ref);

// From a hand to the surface of a bound sphere: 0 inside it.
inline float SurfaceDistance(const NiPoint3& hand, const NiPoint3& centre, float radius) {
	const NiPoint3 d{centre.x - hand.x, centre.y - hand.y, centre.z - hand.z};
	const float toCentre = math::Sqrt(d.LengthSquared());
	const float r = radius > 0.0f ? radius : 0.0f;
	return toCentre > r ? toCentre - r : 0.0f;
}

// Whether a hand is reaching for an item: always within `alwaysUnits` of its
// surface (the grab's own reach), and beyond that only while the hand's
// laser `direction` points at the item's middle within the cone whose
// cosine is `coneCos`. A zero direction points at everything.
constexpr float kReachingConeCos = 0.819f;  // 35 degrees each side

inline bool ReachingFor(const NiPoint3& hand, const NiPoint3& direction, const NiPoint3& centre,
                        float surfaceDistance, float alwaysUnits, float coneCos) {
	if (surfaceDistance <= alwaysUnits) {
		return true;
	}
	const float dirLength = math::Sqrt(direction.LengthSquared());
	const NiPoint3 to = centre - hand;
	const float toLength = math::Sqrt(to.LengthSquared());
	if (!(dirLength > 1.0e-6f) || !(toLength > 1.0e-6f)) {
		return true;
	}
	const float cosine =
		(to.x * direction.x + to.y * direction.y + to.z * direction.z) / (toLength * dirLength);
	return cosine >= coneCos;
}

// One hand as the search sees it.
struct SearchHand {
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	NiPoint3 direction{0.0f, 0.0f, 0.0f};  // its laser; zero: no pointing gate
	bool valid = false;
};

// One item's bound, and whether it is nearer a hand than the best so far.
struct NearItem {
	bool valid = false;
	bool left = false;     // the left hand is the nearer one
	UInt32 ref = 0;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float distance = 0.0f;  // from that hand to the item's surface, units
};

// Takes the candidate if it is within reach of a valid hand that is reaching
// for it (ReachingFor) and nearer than what `best` holds. The right hand wins
// a tie.
inline void ConsiderNearItem(NearItem& best, UInt32 ref, const NiPoint3& centre, float radius,
                             const SearchHand& right, const SearchHand& left, float reachUnits,
                             float alwaysUnits) {
	for (int side = 0; side < 2; ++side) {
		const bool isLeft = side == 1;
		const SearchHand& hand = isLeft ? left : right;
		if (!hand.valid) {
			continue;
		}
		const float d = SurfaceDistance(hand.position, centre, radius);
		if (d <= reachUnits &&
		    ReachingFor(hand.position, hand.direction, centre, d, alwaysUnits, kReachingConeCos) &&
		    (!best.valid || d < best.distance)) {
			best.valid = true;
			best.left = isLeft;
			best.ref = ref;
			best.centre = centre;
			best.distance = d;
		}
	}
}

// The nearest item within reach of either hand in the player's cell, or an
// invalid one. `except` is left out (the one already held).
NearItem FindNearestItem(const SearchHand& right, const SearchHand& left, float reachUnits,
                         float alwaysUnits, UInt32 except);

}  // namespace obvr::game
