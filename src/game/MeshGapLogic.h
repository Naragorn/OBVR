#pragma once

// How far a point resting on a person's capsule lies from the mesh they
// wear there ([Debug] MeshGapTrace; docs/mesh-fitted-capsules-spec.md 7):
// the capsules are round stand-ins for armour that is not (a pauldron, a
// plate's edge, a robe's hem), and whether a blade or a hand held at them
// shows air or sinks in is what the headset sees and the log never said
// (Nadi, 2026-10-27: "meine hände oder klinge nicht 1 zu 1 auf den npc
// rüstungen auflag").
//
// The measure: from the capsule's axis out through the point, the mesh's
// outermost vertex within a narrow lane round that line is the surface
// there; the gap is the point's distance from the axis less the
// surface's - above 0 the point is in the air over the mesh, below 0 inside
// it. A lane with no vertex is tried once wider; none in that either and
// nothing is measured. Vertices further out than kMeshGapReachUnits past
// the point are left out: an arm beside a torso is not the torso's plate.

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
		const float d = PointSegmentDistance(point, c.a, c.b) - c.radius;
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
		NiPoint3 onAxis;
		ClosestOnSegments(from, to, c.a, c.b, onSegment, onAxis);
		const float off = math::Sqrt((onSegment - onAxis).LengthSquared()) - c.radius;
		if (best < 0 || off < bestOff) {
			best = static_cast<int>(i);
			bestOff = off;
			point = onSegment;
		}
	}
	return best;
}

// The gap at `point` on the capsule a-b over `count` mesh vertices in the
// world. Not measured when the point lies on the axis (no way out) or no
// vertex lies in either lane.
inline MeshGap MeasureMeshGap(const NiPoint3& point, const NiPoint3& a, const NiPoint3& b, const NiPoint3* vertices,
                              UInt32 count) {
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
	const float lanes[2] = {kMeshGapLaneUnits, kMeshGapWideLaneUnits};
	for (const float lane : lanes) {
		UInt32 in = 0;
		float surface = 0.0f;
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
			surface = in == 0 || t > surface ? t : surface;
			++in;
		}
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

}  // namespace obvr::game
