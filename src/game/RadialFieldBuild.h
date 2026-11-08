#pragma once

// How a fitted capsule's radial field (game/RadialField.h) is measured:
// each bin as the game's mesh gap line measures (game/MeshGapLogic.h) -
// from the axis, or the end past it, out through the bin's direction, the
// layer of the mesh nearest the capsule's surface there. A bin with no
// vertex in its lane keeps the capsule's radius. Built on the fit's worker
// (game/FittedCapsules.cpp) in each frame bone's space, as the capsules are.

#include <vector>

#include "game/CapsuleFitLogic.h"
#include "game/MeshGapLogic.h"
#include "game/RadialField.h"

namespace obvr::game {

// The lanes a bin is measured in, narrowest first: the outermost vertex of
// a lane as wide as the game's line (2.5) stands up to lane x tan(slope)
// off a surface the lane meets aslant - 1.2 units on an elliptic cylinder
// (radial_field_test) - so a unit first; the game's two after it where a
// flat plate's vertices lie further apart.
inline constexpr float kFieldLanes[3] = {1.0f, kMeshGapLaneUnits, kMeshGapWideLaneUnits};

// The field of the capsule a-b of `radius` over the mesh vertices it stands
// for (its own group's, in the same space). `frame`: the field's u and w
// (RadialField) when they are given - a field to be compared bin for bin
// with another - else its own. `materials` and `ratings`, one per vertex
// (game/BladeMaterial.h), classify the bins (RadialField::classed); null:
// not classed.
inline RadialField BuildRadialField(const NiPoint3& a, const NiPoint3& b, float radius, const NiPoint3* vertices,
                                    UInt32 count, const FieldPlace* frame = nullptr, const UInt8* materials = nullptr,
                                    const UInt8* ratings = nullptr) {
	RadialField f;
	f.classed = materials != nullptr;
	UInt32 counts[kBodyMaterialCount] = {};
	for (UInt32 k = 0; f.classed && k < count; ++k) {
		++counts[materials[k] < kBodyMaterialCount ? materials[k] : 0];
	}
	const UInt8 most = static_cast<UInt8>(MostCommonMaterial(counts));
	f.w = frame != nullptr ? frame->w : NiPoint3{0.0f, 0.0f, 1.0f};
	f.u = frame != nullptr ? frame->u : FieldSectorZero(a, b, f.w);
	f.outer = radius;
	const FieldPlace home = HomePlace(f, a, b);
	for (UInt32 row = 0; row < kFieldRows; ++row) {
		for (UInt32 sector = 0; sector < kFieldSectors; ++sector) {
			NiPoint3 foot;
			NiPoint3 out;
			FieldBinLane(row, sector, home, foot, out);
			const MeshGap g = MeasureMeshGapIn(foot + out * radius, a, b, vertices, count, kFieldLanes, 3, true);
			float r = radius;
			const UInt32 bin = row * kFieldSectors + sector;
			f.material[bin] = f.classed ? most : 0;
			if (g.measured && g.meshRadius > 0.0f) {
				r = g.meshRadius;
				++f.measured;
				if (f.classed && g.surfaceVertex < count) {
					f.material[bin] = materials[g.surfaceVertex] < kBodyMaterialCount ? materials[g.surfaceVertex] : 0;
					f.rating[bin] = ratings != nullptr ? ratings[g.surfaceVertex] : 0;
				}
			}
			f.radius[bin] = r;
			f.outer = r > f.outer ? r : f.outer;
		}
	}
	f.built = true;
	return f;
}

// The fields of the capsules FitBodyCapsules gave for `bones` and
// `vertices`, each in its frame bone's space over its own group's vertices
// (FitGroupOfBone of each vertex's bone) carried into that space.
inline std::vector<RadialField> BuildFittedFields(const FitBone* bones, UInt32 boneCount, const FitVertex* vertices,
                                                  UInt32 vertexCount, const FittedCapsule* caps, UInt32 capCount) {
	std::vector<UInt8> groupOf(vertexCount, kFitNoGroup);
	for (UInt32 k = 0; k < vertexCount; ++k) {
		if (vertices[k].bone < boneCount) {
			groupOf[k] = FitGroupOfBone(bones[vertices[k].bone].name);
		}
	}
	std::vector<RadialField> out;
	out.reserve(capCount);
	std::vector<NiPoint3> own;
	std::vector<UInt8> ownMaterial;
	std::vector<UInt8> ownRating;
	for (UInt32 i = 0; i < capCount; ++i) {
		const FittedCapsule& c = caps[i];
		const NiTransform& into = bones[c.frameBone].skinToBone;
		own.clear();
		ownMaterial.clear();
		ownRating.clear();
		for (UInt32 k = 0; k < vertexCount; ++k) {
			if (groupOf[k] == c.group) {
				own.push_back(TransformPoint(into, vertices[k].bind));
				ownMaterial.push_back(vertices[k].material);
				ownRating.push_back(vertices[k].rating);
			}
		}
		out.push_back(BuildRadialField(c.a, c.b, c.radius, own.data(), static_cast<UInt32>(own.size()), nullptr,
		                               ownMaterial.data(), ownRating.data()));
	}
	return out;
}

// A field built in its frame bone's space, where the bone is now
// (`frameWorld`): its frame turned with it, its radii scaled as
// PlaceFittedCapsule scales the capsule's.
inline FieldRef PlaceField(const NiTransform& frameWorld, const RadialField& f) {
	FieldRef ref;
	ref.field = &f;
	ref.u = Normalised(frameWorld.rot * f.u, NiPoint3{1.0f, 0.0f, 0.0f});
	ref.w = Normalised(frameWorld.rot * f.w, NiPoint3{0.0f, 0.0f, 1.0f});
	ref.scale = frameWorld.scale > 0.0f && frameWorld.scale < 20.0f ? frameWorld.scale : 1.0f;
	return ref;
}

}  // namespace obvr::game
