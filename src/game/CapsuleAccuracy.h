#pragma once

// How well a body's fitted capsules lie on what the body wears: the same
// measure as the game's "Contact: mesh gap" line (game/MeshGapLogic.h),
// taken over the capsules' whole reachable surface instead of where a hand
// or a blade happens to rest. Each capsule is sampled along its length and
// round its axis; a sample counts only where it is the surface something
// coming from outside would meet first - on the capsule nearest it and in
// no other one. Pure; capsule_accuracy_test runs it on real outfits dumped
// in the game ([Debug] FitDump, game/FitDump.h).

#include <algorithm>
#include <vector>

#include "core/MathFns.h"
#include "game/CapsuleFitLogic.h"
#include "game/MeshGapLogic.h"

namespace obvr::game {

struct PlacedCapsule {
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	UInt8 group = kFitNoGroup;
};

struct CapsuleAccuracy {
	UInt32 samples = 0;   // reachable samples measured
	UInt32 within = 0;    // of them within kMeshOnSurfaceUnits of the mesh
	float worstOver = 0.0f;  // the largest gap over the mesh (air)
	float worstInto = 0.0f;  // the largest gap into it (sunk), as a positive number
	float p95 = 0.0f;        // the 95th percentile of |gap|
	float Share() const { return samples > 0 ? static_cast<float>(within) / static_cast<float>(samples) : 0.0f; }
};

// The capsules in the world: a frame bone's world transform is the inverse
// of what the fit was given (the world into the bone's space).
inline std::vector<PlacedCapsule> PlaceCapsules(const FitBone* bones, const FittedCapsule* caps, UInt32 count) {
	std::vector<PlacedCapsule> out;
	for (UInt32 i = 0; i < count; ++i) {
		PlacedCapsule p;
		PlaceFittedCapsule(InverseTransform(bones[caps[i].frameBone].skinToBone), caps[i], p.a, p.b, p.radius);
		p.group = caps[i].group;
		out.push_back(p);
	}
	return out;
}

// `group`: only that group's capsules are judged (kFitNoGroup: all).
// `groupOf`: each vertex's group (FitGroupOfBone of its bone): a capsule
// is measured against its own group's vertices - a hanging arm's capsule
// facing the thigh would otherwise take the thigh's skin for its own
// surface, where in the game the thigh's capsule stands in between.
inline CapsuleAccuracy MeasureCapsuleAccuracy(const std::vector<PlacedCapsule>& caps, const NiPoint3* vertices,
                                              const UInt8* groupOf, UInt32 count, UInt8 group = kFitNoGroup,
                                              UInt32 alongSteps = 9, UInt32 roundSteps = 16) {
	CapsuleAccuracy acc;
	std::vector<float> gaps;
	std::vector<NiPoint3> own;
	for (UInt32 i = 0; i < caps.size(); ++i) {
		const PlacedCapsule& c = caps[i];
		if (group != kFitNoGroup && c.group != group) {
			continue;
		}
		own.clear();
		for (UInt32 k = 0; k < count; ++k) {
			if (groupOf == nullptr || groupOf[k] == c.group) {
				own.push_back(vertices[k]);
			}
		}
		const NiPoint3 axis = c.b - c.a;
		const float len = math::Sqrt(axis.LengthSquared());
		// a direction square to the axis (any, for a ball)
		NiPoint3 dir = len > 1e-4f ? axis * (1.0f / len) : NiPoint3{0.0f, 0.0f, 1.0f};
		const NiPoint3 helper = (dir.z < 0.9f && dir.z > -0.9f) ? NiPoint3{0.0f, 0.0f, 1.0f} : NiPoint3{1.0f, 0.0f, 0.0f};
		const NiPoint3 u = FitUnit(FitCross(dir, helper));
		const NiPoint3 v = FitCross(dir, u);
		// the side along its length, then each end's cap at 30 and 60 degrees
		// past its rim and at its pole (where the game measured the worst gaps,
		// "at 0.00 along it")
		const UInt32 capRings = 3;
		for (UInt32 s = 0; s < alongSteps + 2 * capRings; ++s) {
			float t = 0.0f;
			float tilt = 0.0f;  // toward the axis's own direction past an end
			if (s < alongSteps) {
				t = alongSteps > 1 ? static_cast<float>(s) / static_cast<float>(alongSteps - 1) : 0.5f;
			} else {
				const UInt32 ring = (s - alongSteps) % capRings;
				const bool atB = (s - alongSteps) >= capRings;
				t = atB ? 1.0f : 0.0f;
				tilt = (atB ? 1.0f : -1.0f) * (math::kPi / 6.0f) * static_cast<float>(ring + 1);
			}
			const NiPoint3 foot = c.a + axis * t;
			const bool pole = tilt > 1.5f || tilt < -1.5f;
			for (UInt32 k = 0; k < (pole ? 1u : roundSteps); ++k) {
				const float ang = 2.0f * math::kPi * static_cast<float>(k) / static_cast<float>(roundSteps);
				const NiPoint3 radial = u * math::Cos(ang) + v * math::Sin(ang);
				const NiPoint3 out = radial * math::Cos(tilt) + dir * math::Sin(tilt);
				const NiPoint3 point = foot + out * c.radius;
				// reachable: on no other capsule's inside, and this one the
				// nearest surface
				bool reachable = true;
				for (UInt32 j = 0; j < caps.size() && reachable; ++j) {
					if (j == i) {
						continue;
					}
					const float d = PointSegmentDistance(point, caps[j].a, caps[j].b) - caps[j].radius;
					reachable = d > 0.0f;
				}
				if (!reachable) {
					continue;
				}
				const MeshGap g = MeasureMeshGap(point, c.a, c.b, own.data(), static_cast<UInt32>(own.size()));
				if (!g.measured) {
					continue;
				}
				++acc.samples;
				const float gap = g.gapUnits;
				gaps.push_back(gap < 0.0f ? -gap : gap);
				if (gap <= kMeshOnSurfaceUnits && gap >= -kMeshOnSurfaceUnits) {
					++acc.within;
				}
				acc.worstOver = gap > acc.worstOver ? gap : acc.worstOver;
				acc.worstInto = -gap > acc.worstInto ? -gap : acc.worstInto;
			}
		}
	}
	if (!gaps.empty()) {
		std::sort(gaps.begin(), gaps.end());
		acc.p95 = gaps[static_cast<size_t>(0.95f * static_cast<float>(gaps.size() - 1))];
	}
	return acc;
}

}  // namespace obvr::game
