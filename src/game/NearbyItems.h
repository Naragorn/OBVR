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
// the marker's distance, all the near side from kNearSideMetres in, eased
// between. The pick's hit is where the ring sits and what the grip takes.
constexpr float kNearSideMetres = 0.10f;

// 0 at `farUnits` or beyond, 1 at `nearUnits` or closer, smooth between.
inline float NearSideWeight(float distanceUnits, float farUnits, float nearUnits) {
	if (!(farUnits > nearUnits)) {
		return distanceUnits <= nearUnits ? 1.0f : 0.0f;
	}
	float t = (farUnits - distanceUnits) / (farUnits - nearUnits);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - 2.0f * t);
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

// One item's bound, and whether it is nearer a hand than the best so far.
struct NearItem {
	bool valid = false;
	bool left = false;     // the left hand is the nearer one
	UInt32 ref = 0;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float distance = 0.0f;  // from that hand to the item's surface, units
};

// Takes the candidate if it is within reach of a valid hand and nearer than
// what `best` holds. The right hand wins a tie.
inline void ConsiderNearItem(NearItem& best, UInt32 ref, const NiPoint3& centre, float radius,
                             const NiPoint3& right, bool rightValid, const NiPoint3& left,
                             bool leftValid, float reachUnits) {
	for (int side = 0; side < 2; ++side) {
		const bool isLeft = side == 1;
		if (!(isLeft ? leftValid : rightValid)) {
			continue;
		}
		const float d = SurfaceDistance(isLeft ? left : right, centre, radius);
		if (d <= reachUnits && (!best.valid || d < best.distance)) {
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
NearItem FindNearestItem(const NiPoint3& right, bool rightValid, const NiPoint3& left,
                         bool leftValid, float reachUnits, UInt32 except);

}  // namespace obvr::game
