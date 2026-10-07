#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr {
struct NiAVObject;
}

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
// in, and evenly in between: the nearer the hand, the nearer the edge
// (2026-09-26: a smooth start moved it too late, an eased-out one too soon).
// The pick's hit is where the ring sits and what the grip takes.

// 0 at `farUnits` or beyond, 1 at `nearUnits` or closer, linear between.
inline float NearSideWeight(float distanceUnits, float farUnits, float nearUnits) {
	if (!(farUnits > nearUnits)) {
		return distanceUnits <= nearUnits ? 1.0f : 0.0f;
	}
	float t = (farUnits - distanceUnits) / (farUnits - nearUnits);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t;
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

// How far the model under `root` reaches along the line through `origin`
// along the unit vector `dir`: the lowest and highest vertex, in game units
// along it. For the drawn weapon, where its pommel ends against the hand that
// holds it. False when no vertex can be read or a read fails VertexInBound.
//
// With `band` (from, to along the line), also the middle of the vertices in
// that band, in the world, and how many there were: where the model's shaft
// lies across the line there - a staff's shaft is not on the Weapon node's
// axis (2026-09-30).
bool AxialExtentOf(const NiAVObject* root, const NiPoint3& origin, const NiPoint3& dir, float& low, float& high,
                   const float* band = nullptr, NiPoint3* bandCentre = nullptr, UInt32* bandVertices = nullptr);

// A reference's 3D bound in the world. False when its node cannot be read.
bool RefWorldBound(UInt32 ref, NiPoint3& centre, float& radius);

// The type of a reference's base form, 0 when it cannot be read.
UInt8 RefBaseFormType(UInt32 ref);

// For a hand script's marks: each small object with a Havok body near
// `around`, its mesh's box against its Havok shape's box, one line each
// ("Measure: object ..."; the spec's open bug of objects touched early).
void MeasureNearbyShapes(const NiPoint3& around, float radiusUnits, UInt32 maxObjects);

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
	// The way its palm faces; zero: none. An item the palm is turned to
	// counts as reached for too - bringing the open hand to a thing is the
	// usual reach, not pointing at it (2026-09-26).
	NiPoint3 palm{0.0f, 0.0f, 0.0f};
	bool valid = false;
};

// The palm's cone is wider: an open hand is held less exactly than a
// pointer. 50 degrees each side.
constexpr float kPalmConeCos = 0.643f;

// Reached for by either the laser or the palm. With no palm direction only
// the laser counts, as before; with neither, everything does (ReachingFor).
inline bool ReachingForWithHand(const SearchHand& hand, const NiPoint3& centre,
                                float surfaceDistance, float alwaysUnits) {
	const bool palmKnown = hand.palm.LengthSquared() > 1.0e-12f;
	return ReachingFor(hand.position, hand.direction, centre, surfaceDistance, alwaysUnits,
	                   kReachingConeCos) ||
	       (palmKnown && ReachingFor(hand.position, hand.palm, centre, surfaceDistance,
	                                 alwaysUnits, kPalmConeCos));
}

// One item's bound, and how well a hand reaches for it.
struct NearItem {
	bool valid = false;
	bool left = false;     // the left hand is the reaching one
	UInt32 ref = 0;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float distance = 0.0f;  // from that hand to the item's surface, units
	// How it was reached for (PickRank) and the key within that: lower wins.
	UInt8 rankClass = 0xFF;
	float rankKey = 0.0f;
};

// How the laser misses an item: the angle, in radians, from the laser to the
// nearest edge of its bound sphere seen from the hand - 0 when the laser runs
// through the sphere. -1 with no laser or the hand inside the sphere.
inline float LaserMissRadians(const NiPoint3& hand, const NiPoint3& direction, const NiPoint3& centre,
                              float radius) {
	const float dirLength = math::Sqrt(direction.LengthSquared());
	const NiPoint3 to = centre - hand;
	const float toLength = math::Sqrt(to.LengthSquared());
	if (!(dirLength > 1.0e-6f) || !(toLength > 1.0e-6f) || toLength <= radius) {
		return -1.0f;
	}
	float cosine = (to.x * direction.x + to.y * direction.y + to.z * direction.z) / (toLength * dirLength);
	cosine = cosine > 1.0f ? 1.0f : (cosine < -1.0f ? -1.0f : cosine);
	const float angle = math::Atan2(math::Sqrt(1.0f - cosine * cosine), cosine);
	const float half = math::Asin(radius > 0.0f ? radius / toLength : 0.0f);
	return angle > half ? angle - half : 0.0f;
}

// Which item a hand takes when several are in reach (the tester's tester,
// 2026-10-03: before three objects, pointing at one, the nearest was taken).
// By class, then within it by its key, lower first (the distance being to
// the item's mesh within the grab's reach, FindNearestItem, else to its
// bound sphere):
//   kPickTouched   touched (within kPickTouchUnits of its surface) - by distance;
//   kPickInReach   within the grab's reach - by distance;
//   kPickPalmNear  the palm turned to it within kPickPalmNearUnits - by distance;
//   kPickLaserOn   the laser on it (within kPickAimedRadians of its bound) - by the miss;
//   kPickLaserCone in the laser's cone (ReachingFor) - by the miss;
//   kPickPalm      the palm turned to it farther off, or a hand with no laser - by distance.
// So a hand brought to things takes the nearest of them (the tester,
// 2026-10-07: on a laden table the laser from that hand fell on the thing
// behind the nearest), and an open palm held near a thing takes it before
// the laser takes something far off (the tester, the same day: "die
// handfläche nah an einem objekt muss prio haben vor laserpointer zu einem
// entfernten objekt"); beyond that the laser decides, and the item nearest
// the laser wins, not the one nearest the hand.
enum PickClass : UInt8 {
	kPickTouched = 0,
	kPickInReach = 1,
	kPickPalmNear = 2,
	kPickLaserOn = 3,
	kPickLaserCone = 4,
	kPickPalm = 5,
};
constexpr float kPickTouchUnits = 3.5f;       // 5 cm
constexpr float kPickAimedRadians = 0.105f;   // 6 degrees
constexpr float kPickPalmNearUnits = 42.0f;   // 60 cm

// Whether a class is keyed by the laser's miss (an angle) rather than by a
// distance - what a margin between two keys is measured in (game/PickHold.h).
inline bool PickClassKeyedByAngle(UInt8 rankClass) {
	return rankClass == kPickLaserOn || rankClass == kPickLaserCone;
}

inline bool PickRank(const SearchHand& hand, const NiPoint3& centre, float radius, float surfaceDistance,
                     float alwaysUnits, UInt8& rankClass, float& rankKey) {
	if (!ReachingForWithHand(hand, centre, surfaceDistance, alwaysUnits)) {
		return false;
	}
	const float miss = LaserMissRadians(hand.position, hand.direction, centre, radius);
	const bool palmKnown = hand.palm.LengthSquared() > 1.0e-12f;
	const bool palmNear = palmKnown && surfaceDistance <= kPickPalmNearUnits &&
	                      ReachingFor(hand.position, hand.palm, centre, surfaceDistance, 0.0f, kPalmConeCos);
	if (surfaceDistance <= kPickTouchUnits) {
		rankClass = kPickTouched;
		rankKey = surfaceDistance;
	} else if (surfaceDistance <= alwaysUnits) {
		rankClass = kPickInReach;
		rankKey = surfaceDistance;
	} else if (palmNear) {
		rankClass = kPickPalmNear;
		rankKey = surfaceDistance;
	} else if (miss >= 0.0f && miss <= kPickAimedRadians) {
		rankClass = kPickLaserOn;
		rankKey = miss;
	} else if (miss >= 0.0f && ReachingFor(hand.position, hand.direction, centre, surfaceDistance, 0.0f,
	                                       kReachingConeCos)) {
		rankClass = kPickLaserCone;
		rankKey = miss;
	} else {
		rankClass = kPickPalm;
		rankKey = surfaceDistance;
	}
	return true;
}

// Takes the candidate if it is within reach of a valid hand that is reaching
// for it (ReachingFor) and ranks better than what `best` holds (PickRank).
// The right hand wins a tie. `distances` ([0] the right hand's, [1] the
// left's) are the hands' distances to the item's mesh when known, else
// negative (the bound sphere's then).
inline void ConsiderNearItem(NearItem& best, UInt32 ref, const NiPoint3& centre, float radius,
                             const SearchHand& right, const SearchHand& left, float reachUnits,
                             float alwaysUnits, const float* distances = nullptr) {
	for (int side = 0; side < 2; ++side) {
		const bool isLeft = side == 1;
		const SearchHand& hand = isLeft ? left : right;
		if (!hand.valid) {
			continue;
		}
		const float known = distances != nullptr ? distances[side] : -1.0f;
		const float d = known >= 0.0f ? known : SurfaceDistance(hand.position, centre, radius);
		UInt8 rankClass = 0xFF;
		float rankKey = 0.0f;
		if (d <= reachUnits && PickRank(hand, centre, radius, d, alwaysUnits, rankClass, rankKey) &&
		    (!best.valid || rankClass < best.rankClass ||
		     (rankClass == best.rankClass && rankKey < best.rankKey))) {
			best.valid = true;
			best.left = isLeft;
			best.ref = ref;
			best.centre = centre;
			best.distance = d;
			best.rankClass = rankClass;
			best.rankKey = rankKey;
		}
	}
}

// How one hand ranks one item this frame (PickRank), or invalid when that
// hand is not valid, the item is out of its reach or it is not reaching for
// it. For the item the pick holds (game/PickHold.h), by hand. `distance` is
// the hand's distance to the item when known better than the bound sphere
// gives it (its mesh, HandDistanceToItem), else negative.
inline NearItem RankItemForHand(const SearchHand& hand, bool isLeft, UInt32 ref, const NiPoint3& centre,
                                float radius, float reachUnits, float alwaysUnits, float distance = -1.0f) {
	NearItem out;
	if (!hand.valid) {
		return out;
	}
	const float d = distance >= 0.0f ? distance : SurfaceDistance(hand.position, centre, radius);
	UInt8 rankClass = 0xFF;
	float rankKey = 0.0f;
	if (d > reachUnits || !PickRank(hand, centre, radius, d, alwaysUnits, rankClass, rankKey)) {
		return out;
	}
	out.valid = true;
	out.left = isLeft;
	out.ref = ref;
	out.centre = centre;
	out.distance = d;
	out.rankClass = rankClass;
	out.rankKey = rankKey;
	return out;
}

// The best-ranked item within reach of either hand in the player's cell, or
// an invalid one. `except` is left out (the one already held). With `keep`
// (0 none), that item's own ranks this frame go to `keptByHand` ([0] the
// right hand, [1] the left; invalid where the hand does not reach for it) -
// what the pick's hold needs to know whether to stay on it.
NearItem FindNearestItem(const SearchHand& right, const SearchHand& left, float reachUnits,
                         float alwaysUnits, UInt32 except, UInt32 keep = 0, NearItem* keptByHand = nullptr);

}  // namespace obvr::game
