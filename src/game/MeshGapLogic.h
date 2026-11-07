#pragma once

// How far a point resting on a person's capsule lies from the mesh they
// wear there ([Debug] MeshGapTrace; docs/mesh-fitted-capsules-spec.md 7):
// the capsules are round stand-ins for armour that is not (a pauldron, a
// plate's edge, a robe's hem), and whether a blade or a hand held at them
// shows air or sinks in is what the headset sees and the log never said
// (Nadi, 2026-10-27: "meine hände oder klinge nicht 1 zu 1 auf den npc
// rüstungen auflag").
//
// The measure: from the capsule's axis out through the point, the vertices
// within a narrow lane round that line; the layer of them the point lies at
// (kMeshGapLayerUnits), its outermost vertex the surface there; the gap is the point's distance from the axis less the
// surface's - above 0 the point is in the air over the mesh, below 0 inside
// it. A lane with no vertex is tried once wider; none in that either and
// nothing is measured. Vertices further out than kMeshGapReachUnits past
// the point are left out: an arm beside a torso is not the torso's plate.

#include <algorithm>
#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The lanes' half widths, game units (70 a metre): about 3.5 and 7 cm -
// armour's vertices lie a few units apart.
inline constexpr float kMeshGapLaneUnits = 2.5f;
inline constexpr float kMeshGapWideLaneUnits = 5.0f;
// How far past the point the surface is looked for: further out than this
// (about 17 cm) a vertex in the lane is someone else's limb, not the
// armour under the point.
inline constexpr float kMeshGapReachUnits = 12.0f;
// Vertices along the lane closer than this to each other are one layer of
// the mesh; a wider gap starts another (a limb, the far side of a sleeve).
inline constexpr float kMeshGapLayerUnits = 3.0f;

struct MeshGap {
	bool measured = false;
	float gapUnits = 0.0f;      // the point over the mesh (> 0) or in it (< 0)
	float pointRadius = 0.0f;   // the point's distance from the capsule's axis
	float meshRadius = 0.0f;    // the mesh's surface's, along the same line
	float lane = 0.0f;          // the lane that found it
	UInt32 laneVertices = 0;    // vertices in that lane
	float nearestUnits = 0.0f;  // the vertex nearest the point, any direction
	NiPoint3 outward{0.0f, 0.0f, 0.0f};  // from the capsule's axis out through the point
	float along = 0.0f;  // where along the capsule's axis, 0 its first end, 1 its second
};

// The capsule of `actor` in `bodies` the point lies on: the one whose
// surface is nearest it. -1 when the actor has none.
inline int CapsuleUnderPoint(const BladeBodies& bodies, UInt32 actor, const NiPoint3& point) {
	int best = -1;
	float bestOff = 0.0f;
	for (UInt32 i = 0; i < bodies.count; ++i) {
		const BladeBodyCapsule& c = bodies.cap[i];
		if (c.actor != actor) {
			continue;
		}
		const float d = BodyGapAt(c, point);
		const float off = d < 0.0f ? -d : d;
		if (best < 0 || off < bestOff) {
			best = static_cast<int>(i);
			bestOff = off;
		}
	}
	return best;
}

// The capsule of `actor` in `bodies` a drawn segment (a blade from guard to
// tip, a hand's span) lies nearest, and the segment's point nearest it, in
// `point`: a blade turned about its grip as it slides touches them at
// another point than where its sweep met them. -1 when the actor has none.
inline int CapsuleNearestSegment(const BladeBodies& bodies, UInt32 actor, const NiPoint3& from, const NiPoint3& to,
                                 NiPoint3& point) {
	int best = -1;
	float bestOff = 0.0f;
	for (UInt32 i = 0; i < bodies.count; ++i) {
		const BladeBodyCapsule& c = bodies.cap[i];
		if (c.actor != actor) {
			continue;
		}
		NiPoint3 onSegment;
		const float off = BodySegmentGap(c, from, to, &onSegment);
		if (best < 0 || off < bestOff) {
			best = static_cast<int>(i);
			bestOff = off;
			point = onSegment;
		}
	}
	return best;
}

// The gap at `point` on the capsule a-b over `count` mesh vertices in the
// world, its lanes tried in the order given (`laneCount` half widths). Not
// measured when the point lies on the axis (no way out) or no vertex lies
// in any lane.
inline MeshGap MeasureMeshGapIn(const NiPoint3& point, const NiPoint3& a, const NiPoint3& b, const NiPoint3* vertices,
                                UInt32 count, const float* lanes, UInt32 laneCount);

// As the game's line measures: the lane kMeshGapLaneUnits, then the wide one.
inline MeshGap MeasureMeshGap(const NiPoint3& point, const NiPoint3& a, const NiPoint3& b, const NiPoint3* vertices,
                              UInt32 count) {
	const float lanes[2] = {kMeshGapLaneUnits, kMeshGapWideLaneUnits};
	return MeasureMeshGapIn(point, a, b, vertices, count, lanes, 2);
}

inline MeshGap MeasureMeshGapIn(const NiPoint3& point, const NiPoint3& a, const NiPoint3& b, const NiPoint3* vertices,
                                UInt32 count, const float* lanes, UInt32 laneCount) {
	MeshGap g;
	g.along = ShareOnSegment(point, a, b);
	const NiPoint3 foot = a + (b - a) * g.along;
	const NiPoint3 out = point - foot;
	const float pointRadius = math::Sqrt(out.LengthSquared());
	if (count == 0 || !(pointRadius > 1e-3f)) {
		return g;
	}
	const NiPoint3 u = out * (1.0f / pointRadius);
	float nearest2 = -1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		const float d2 = (vertices[i] - point).LengthSquared();
		if (nearest2 < 0.0f || d2 < nearest2) {
			nearest2 = d2;
		}
	}
	g.nearestUnits = math::Sqrt(nearest2);
	g.pointRadius = pointRadius;
	g.outward = u;
	std::vector<float> ts;
	for (UInt32 l = 0; l < laneCount; ++l) {
		const float lane = lanes[l];
		ts.clear();
		for (UInt32 i = 0; i < count; ++i) {
			const NiPoint3 w = vertices[i] - foot;
			const float t = DotOf3(w, u);
			if (!(t > 0.0f) || t > pointRadius + kMeshGapReachUnits) {
				continue;
			}
			const NiPoint3 side = w - u * t;
			if (side.LengthSquared() > lane * lane) {
				continue;
			}
			ts.push_back(t);
		}
		const UInt32 in = static_cast<UInt32>(ts.size());
		// the layer the point lies at: the vertices nearest it along the
		// line, and those following on outward without a gap wider than
		// kMeshGapLayerUnits; its outermost is the surface - another limb
		// further out in the lane is not (the other leg, 2026-11-07), a plate
		// standing off the arm under the point is
		std::sort(ts.begin(), ts.end());
		UInt32 seed = 0;
		for (UInt32 k = 1; k < in; ++k) {
			const float dk = ts[k] - pointRadius;
			const float ds = ts[seed] - pointRadius;
			if ((dk < 0.0f ? -dk : dk) < (ds < 0.0f ? -ds : ds)) {
				seed = k;
			}
		}
		UInt32 top = seed;
		while (top + 1 < in && ts[top + 1] - ts[top] <= kMeshGapLayerUnits) {
			++top;
		}
		const float surface = in > 0 ? ts[top] : 0.0f;
		if (in > 0) {
			g.measured = true;
			g.meshRadius = surface;
			g.gapUnits = pointRadius - surface;
			g.lane = lane;
			g.laneVertices = in;
			return g;
		}
	}
	return g;
}

// Whether what rests on a person - a hand, a blade, a torch - sits on what
// they wear (Nadi, 2026-11-07: "kollisionen passieren genau an seiner
// rüstung"): its surface within kMeshOnSurfaceUnits (about 4 cm) over or
// into the mesh. A choice, not the measure's limit: the worn meshes'
// vertices lie 0.4-0.5 units apart (median nearest neighbour, the three
// outfits of tests/data, 2026-11-07), so the measure could judge a
// centimetre; what holds the contact to about this is the round capsules
// over armour that is not round, the hand's own capsule 2.5 round, the
// blade's margin of 1.5 and its shallow band of 3 (BladeContactLogic.h).
// The words are what the harness checks for.
inline constexpr float kMeshOnSurfaceUnits = 3.0f;

enum class MeshSurfaceVerdict { On, Over, Into };

inline MeshSurfaceVerdict JudgeMeshSurface(float steelUnits) {
	if (!(steelUnits == steelUnits)) {
		return MeshSurfaceVerdict::Over;
	}
	if (steelUnits > kMeshOnSurfaceUnits) {
		return MeshSurfaceVerdict::Over;
	}
	if (steelUnits < -kMeshOnSurfaceUnits) {
		return MeshSurfaceVerdict::Into;
	}
	return MeshSurfaceVerdict::On;
}

// A scenario's bar (Nadi, 2026-11-07, option "B"): of what rested on a person,
// at least kMeshTallyShare of the lines on the mesh (JudgeMeshSurface), none
// further off than kMeshFarUnits (about 9 cm), and kMeshTallyLeast lines at
// least - a single line just past the 3 units no longer fails a run; what the
// headset would see as wrong still does.
inline constexpr float kMeshTallyShare = 0.95f;
inline constexpr float kMeshFarUnits = 6.0f;
inline constexpr UInt32 kMeshTallyLeast = 10;

struct MeshGapTally {
	UInt32 lines = 0;
	UInt32 on = 0;
	float worst = 0.0f;  // the largest |steel| seen, units

	void Add(float steelUnits) {
		const float off = steelUnits < 0.0f ? -steelUnits : steelUnits;
		++lines;
		on += off <= kMeshOnSurfaceUnits ? 1u : 0u;
		worst = off > worst || !(off == off) ? off : worst;
	}
	bool Passes() const {
		return lines >= kMeshTallyLeast && static_cast<float>(on) >= kMeshTallyShare * static_cast<float>(lines) &&
		       worst <= kMeshFarUnits;
	}
	const char* Verdict() const { return Passes() ? "the bar met" : "below the bar"; }
};

inline const char* MeshSurfaceVerdictText(MeshSurfaceVerdict v) {
	switch (v) {
	case MeshSurfaceVerdict::On: return "on the mesh, within";
	case MeshSurfaceVerdict::Into: return "into the mesh by more than";
	default: return "over the mesh by more than";
	}
}

}  // namespace obvr::game
