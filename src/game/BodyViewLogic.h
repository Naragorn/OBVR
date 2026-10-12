#pragma once

#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// What the eyes see of the player's own body - for the harness, which cannot
// look at a picture: whether a look down sees down the body, its front from
// outside, or into it (the tester, 2026-10-12: "nach unten schauen: ich sehe
// immernoch in den körper rein").
//
// The body is the third-person skeleton's skinned shapes ([Body] Visible,
// PlayerBody.cpp), its head collapsed: the collar is an open ring round the
// neck, and the near plane cuts whatever is closer to the eye than it. Both
// let a ray from the eye meet the body's surface from behind - its inside.
// Otherwise the shapes are closed shells drawn from one side, each triangle
// wound with its outside along (b - a) x (c - a): read with pyffi from
// upperbody.nif, lowerbody.nif, hand.nif and foot.nif (2026-10-12), every
// triangle's winding agrees with the stored vertex normals (UpperBody 846 of
// 846, Arms 714 of 714, LowerBody 704 and 448, Hand 2746, Foot 626), and only
// the loincloth (LowerBody:1) carries a NiStencilProperty, drawn from both
// sides (draw mode 3).
//
// So a grid of rays across each eye's view, each classified by the nearest
// triangle it meets beyond the near plane, says how much of the view is the
// body from outside and how much its inside - the open collar, a cut by the
// near plane, an eye inside the trunk - and, past the view's bottom edge, how
// far below it the inside begins.
//
// Pure: the skinning, the rays and the verdict are tested with made-up shapes
// (tests/BodyViewTest.cpp); BodyView.cpp reads the engine's shapes.

// ------------------------------------------------------------------ skinning

// A point carried by a transform the way Gamebryo carries it: scaled,
// turned, moved.
inline NiPoint3 TransformPoint(const NiTransform& t, const NiPoint3& v) { return t.pos + t.rot * (v * t.scale); }

// One vertex's share of one bone - NiSkinData keeps them bone by bone.
struct SkinWeight {
	UInt16 vertex = 0;
	UInt16 bone = 0;
	float weight = 0.0f;
};

// The vertices as the draw places them: each the sum over its bones of the
// weight times the bone's world transform after the bone's skin transform
// (NiSkinData's "skin to bone"). In the bind pose the two together are the
// shape's own place: read with pyffi against skeleton.nif (2026-10-12), the
// product is the identity for every bone of UpperBody, Arms and LowerBody
// (within 0.01, the neck 1.1 units off) and the foot shape's own offset for
// Foot's. A bone collapsed to scale zero (the head, PlayerBody.cpp) pulls its
// share onto its own origin, as the draw does. weightSum gets each vertex's
// total weight; a weight naming no vertex or no bone is left out.
inline void SkinVertices(const NiPoint3* bind, UInt32 vertexCount, const NiTransform* boneWorld,
                         const NiTransform* skinToBone, UInt32 boneCount, const SkinWeight* weights, UInt32 weightCount,
                         NiPoint3* out, float* weightSum) {
	for (UInt32 i = 0; i < vertexCount; ++i) {
		out[i] = NiPoint3{0.0f, 0.0f, 0.0f};
		weightSum[i] = 0.0f;
	}
	for (UInt32 k = 0; k < weightCount; ++k) {
		const SkinWeight& w = weights[k];
		if (w.vertex >= vertexCount || w.bone >= boneCount) {
			continue;
		}
		const NiPoint3 p = TransformPoint(boneWorld[w.bone], TransformPoint(skinToBone[w.bone], bind[w.vertex]));
		out[w.vertex] = out[w.vertex] + p * w.weight;
		weightSum[w.vertex] += w.weight;
	}
}

// How far a vertex's weights may add up away from one before the skin data
// counts as misread. The vanilla body's add up to one to four decimals
// (pyffi, 2026-10-12: Foot, Hand, LowerBody:0/1, Arms, UpperBody, every
// vertex weighted).
inline constexpr float kSkinWeightSlack = 0.01f;

// The vertices whose weights do not add up to one.
inline UInt32 SkinWeightMisses(const float* weightSum, UInt32 vertexCount) {
	UInt32 misses = 0;
	for (UInt32 i = 0; i < vertexCount; ++i) {
		const float off = weightSum[i] - 1.0f;
		if (!(off <= kSkinWeightSlack && off >= -kSkinWeightSlack)) {
			++misses;
		}
	}
	return misses;
}

inline constexpr UInt16 kNoBone = 0xFFFF;

// The bone carrying most of each vertex, and its weight; kNoBone and zero
// for a vertex no weight names.
inline void DominantBones(const SkinWeight* weights, UInt32 weightCount, UInt32 vertexCount, UInt16* bone,
                          float* weight) {
	for (UInt32 i = 0; i < vertexCount; ++i) {
		bone[i] = kNoBone;
		weight[i] = 0.0f;
	}
	for (UInt32 k = 0; k < weightCount; ++k) {
		const SkinWeight& w = weights[k];
		if (w.vertex < vertexCount && w.weight > weight[w.vertex]) {
			weight[w.vertex] = w.weight;
			bone[w.vertex] = w.bone;
		}
	}
}

// A triangle's bone: the one at least two of its corners share, else the
// corner's that carries the most.
inline UInt16 TriangleBone(UInt16 a, UInt16 b, UInt16 c, float wa, float wb, float wc) {
	if (a == b || a == c) {
		return a;
	}
	if (b == c) {
		return b;
	}
	if (wa >= wb && wa >= wc) {
		return a;
	}
	return wb >= wc ? b : c;
}

// Which way a shape is wound: +1 when (b - a) x (c - a) points along the
// stored vertex normals on more triangles than against them, -1 for the
// other way, 0 when it cannot be told (no normals, or as many each way).
// agree and against get the two counts.
inline int OutwardWinding(const NiPoint3* vertices, const NiPoint3* normals, UInt32 vertexCount,
                          const UInt16* triangles, UInt32 triangleCount, UInt32& agree, UInt32& against) {
	agree = 0;
	against = 0;
	if (normals == nullptr) {
		return 0;
	}
	for (UInt32 t = 0; t < triangleCount; ++t) {
		const UInt16 ia = triangles[t * 3];
		const UInt16 ib = triangles[t * 3 + 1];
		const UInt16 ic = triangles[t * 3 + 2];
		if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) {
			continue;
		}
		const NiPoint3 u = vertices[ib] - vertices[ia];
		const NiPoint3 v = vertices[ic] - vertices[ia];
		const NiPoint3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
		const NiPoint3 s = normals[ia] + normals[ib] + normals[ic];
		const float d = n.x * s.x + n.y * s.y + n.z * s.z;
		if (d > 0.0f) {
			++agree;
		} else if (d < 0.0f) {
			++against;
		}
	}
	if (agree > against) {
		return 1;
	}
	return against > agree ? -1 : 0;
}

// ------------------------------------------------------------------ the rays

// What a ray of the view meets first beyond the near plane.
enum ViewSeen : UInt8 {
	kViewSeenNothing = 0,
	kViewSeenLimbOutside = 1,
	kViewSeenTrunkOutside = 2,
	kViewSeenLimbInside = 3,
	kViewSeenTrunkInside = 4,
	kViewSeenKinds = 5,
};

inline bool ViewSeenInside(UInt8 seen) { return seen == kViewSeenLimbInside || seen == kViewSeenTrunkInside; }

struct ViewTriangle {
	// In the world, wound so that (b - a) x (c - a) points out of the body.
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	NiPoint3 c{0.0f, 0.0f, 0.0f};
	UInt8 shape = 0;        // the shape it belongs to, for the log
	bool trunk = false;     // its bone is the trunk's (BodyView.cpp, kTrunkBones)
	bool twoSided = false;  // drawn from both sides: its back is a surface too
};

struct ViewEye {
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	// The eye's axes in the world, unit and square to each other.
	NiPoint3 right{1.0f, 0.0f, 0.0f};
	NiPoint3 forward{0.0f, 1.0f, 0.0f};
	NiPoint3 up{0.0f, 0.0f, 1.0f};
	// The frustum as the tangents of its half-angles, each positive, and the
	// near plane's distance along forward (NiFrustum's l, r, t, b and n).
	float tanLeft = 1.0f;
	float tanRight = 1.0f;
	float tanUp = 1.0f;
	float tanDown = 1.0f;
	float nearUnits = 10.0f;
};

struct ViewGrid {
	UInt32 columns = 160;
	UInt32 rows = 128;
	// Rows below the view's bottom edge, counted apart from the view: how far
	// below it the trunk's inside begins.
	UInt32 marginRows = 24;
	float marginDegrees = 20.0f;
};

inline constexpr UInt32 kViewMaxShapes = 32;

struct ViewTally {
	// The rays in the view, and what each met (by ViewSeen) - the margin's
	// rows are not among them.
	UInt32 rays = 0;
	UInt32 seen[kViewSeenKinds] = {};
	// The trunk's inside in the view, by shape.
	UInt32 trunkInsideOf[kViewMaxShapes] = {};
	// The nearest margin row that meets the trunk's inside, as degrees below
	// the view's bottom edge; negative when none within the margin.
	float trunkInsideBelowDegrees = -1.0f;
};

// The up-positive tangent of a row's middle: the view's rows evenly in
// tangent from the top edge to the bottom, the margin's evenly in angle from
// the bottom edge down.
inline float ViewRowTangent(const ViewEye& eye, const ViewGrid& grid, UInt32 row) {
	if (row < grid.rows) {
		const float step = (eye.tanUp + eye.tanDown) / static_cast<float>(grid.rows);
		return eye.tanUp - (static_cast<float>(row) + 0.5f) * step;
	}
	const float below = (static_cast<float>(row - grid.rows) + 0.5f) * grid.marginDegrees /
	                    static_cast<float>(grid.marginRows > 0 ? grid.marginRows : 1);
	float angle = math::Atan(eye.tanDown) * math::kRadiansToDegrees + below;
	if (angle > 89.0f) {
		angle = 89.0f;
	}
	return -math::Tan(angle * math::kDegreesToRadians);
}

// The right-positive tangent of a column's middle.
inline float ViewColumnTangent(const ViewEye& eye, const ViewGrid& grid, UInt32 column) {
	const float step = (eye.tanLeft + eye.tanRight) / static_cast<float>(grid.columns);
	return -eye.tanLeft + (static_cast<float>(column) + 0.5f) * step;
}

// A ray from the eye along (x, 1, y) - right, forward, up - against a
// triangle in the eye's space (the same axes): how far along forward it
// meets it, or a negative number when it does not (Moller and Trumbore).
inline float ViewRayHit(const NiPoint3& e0, const NiPoint3& e1, const NiPoint3& e2, float x, float y) {
	const NiPoint3 d{x, 1.0f, y};
	const NiPoint3 edge1 = e1 - e0;
	const NiPoint3 edge2 = e2 - e0;
	const NiPoint3 p{d.y * edge2.z - d.z * edge2.y, d.z * edge2.x - d.x * edge2.z, d.x * edge2.y - d.y * edge2.x};
	const float det = edge1.x * p.x + edge1.y * p.y + edge1.z * p.z;
	if (!(det > 1e-9f || det < -1e-9f)) {
		return -1.0f;
	}
	const float inv = 1.0f / det;
	const NiPoint3 t{-e0.x, -e0.y, -e0.z};
	const float u = (t.x * p.x + t.y * p.y + t.z * p.z) * inv;
	if (u < 0.0f || u > 1.0f) {
		return -1.0f;
	}
	const NiPoint3 q{t.y * edge1.z - t.z * edge1.y, t.z * edge1.x - t.x * edge1.z, t.x * edge1.y - t.y * edge1.x};
	const float v = (d.x * q.x + d.y * q.y + d.z * q.z) * inv;
	if (v < 0.0f || u + v > 1.0f) {
		return -1.0f;
	}
	return (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z) * inv;
}

// Two hits this close along the view count as one place: where a shell's
// front and back meet at its outline, the front is what shows.
inline constexpr float kViewSameDepthUnits = 0.05f;

// Casts the grid's rays from the eye and tallies what each meets first beyond
// the near plane. cells, when given, gets one ViewSeen a ray: the view's rows
// from the top, then the margin's, each from the left.
inline ViewTally CastBodyView(const ViewEye& eye, const ViewGrid& grid, const ViewTriangle* triangles, UInt32 count,
                              UInt8* cells) {
	ViewTally tally;
	const UInt32 columns = grid.columns;
	const UInt32 allRows = grid.rows + grid.marginRows;
	if (columns == 0 || grid.rows == 0) {
		return tally;
	}
	const UInt32 cellCount = columns * allRows;
	std::vector<float> nearest(cellCount, -1.0f);
	std::vector<UInt8> seen(cellCount, kViewSeenNothing);
	std::vector<UInt8> shapeOf(cellCount, 0);
	std::vector<float> columnTan(columns);
	std::vector<float> rowTan(allRows);
	for (UInt32 c = 0; c < columns; ++c) {
		columnTan[c] = ViewColumnTangent(eye, grid, c);
	}
	for (UInt32 r = 0; r < allRows; ++r) {
		rowTan[r] = ViewRowTangent(eye, grid, r);
	}
	const auto turnToEye = [&eye](const NiPoint3& v) {
		return NiPoint3{v.x * eye.right.x + v.y * eye.right.y + v.z * eye.right.z,
		                v.x * eye.forward.x + v.y * eye.forward.y + v.z * eye.forward.z,
		                v.x * eye.up.x + v.y * eye.up.y + v.z * eye.up.z};
	};
	const float columnStep = (eye.tanLeft + eye.tanRight) / static_cast<float>(columns);
	for (UInt32 i = 0; i < count; ++i) {
		const ViewTriangle& tri = triangles[i];
		const NiPoint3 e0 = turnToEye(tri.a - eye.pos);
		const NiPoint3 e1 = turnToEye(tri.b - eye.pos);
		const NiPoint3 e2 = turnToEye(tri.c - eye.pos);
		// Nothing of it as far out as the near plane: no ray can meet it there.
		if (!(e0.y >= eye.nearUnits || e1.y >= eye.nearUnits || e2.y >= eye.nearUnits)) {
			continue;
		}
		const NiPoint3 u = tri.b - tri.a;
		const NiPoint3 v = tri.c - tri.a;
		const NiPoint3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
		if (!(n.LengthSquared() > 1e-12f)) {
			continue;  // collapsed to a line or a point
		}
		const NiPoint3 nEye = turnToEye(n);
		// The rows and columns it can cover: its corners' tangents when all
		// three stand in front of the eye, every ray when one does not.
		bool bounded = false;
		float xLow = 0.0f;
		float xHigh = 0.0f;
		float yLow = 0.0f;
		float yHigh = 0.0f;
		if (e0.y > 1e-3f && e1.y > 1e-3f && e2.y > 1e-3f) {
			bounded = true;
			const float x0 = e0.x / e0.y;
			const float x1 = e1.x / e1.y;
			const float x2 = e2.x / e2.y;
			const float y0 = e0.z / e0.y;
			const float y1 = e1.z / e1.y;
			const float y2 = e2.z / e2.y;
			xLow = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
			xHigh = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
			yLow = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
			yHigh = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
		}
		UInt32 cFirst = 0;
		UInt32 cLast = columns - 1;
		if (bounded) {
			const float first = (xLow + eye.tanLeft) / columnStep - 0.5f;
			const float last = (xHigh + eye.tanLeft) / columnStep - 0.5f;
			if (last < -1.0f || first > static_cast<float>(columns)) {
				continue;
			}
			cFirst = first <= 0.0f ? 0 : static_cast<UInt32>(first);
			cLast = last >= static_cast<float>(columns - 1) ? columns - 1 : static_cast<UInt32>(last + 1.0f);
			if (cLast > columns - 1) {
				cLast = columns - 1;
			}
		}
		for (UInt32 r = 0; r < allRows; ++r) {
			const float y = rowTan[r];
			if (bounded && (y < yLow || y > yHigh)) {
				continue;
			}
			for (UInt32 c = cFirst; c <= cLast; ++c) {
				const float x = columnTan[c];
				const float t = ViewRayHit(e0, e1, e2, x, y);
				if (!(t >= eye.nearUnits)) {
					continue;
				}
				// The ray meets its outside when it runs against the outward
				// normal; a two-sided triangle's back is a surface too.
				const bool outside = nEye.x * x + nEye.y + nEye.z * y < 0.0f || tri.twoSided;
				const UInt8 kind = outside ? (tri.trunk ? kViewSeenTrunkOutside : kViewSeenLimbOutside)
				                           : (tri.trunk ? kViewSeenTrunkInside : kViewSeenLimbInside);
				const UInt32 cell = r * columns + c;
				const float current = nearest[cell];
				bool take = false;
				if (current < 0.0f || t < current - kViewSameDepthUnits) {
					take = true;
				} else if (t <= current + kViewSameDepthUnits) {
					take = outside && ViewSeenInside(seen[cell]);
				}
				if (take) {
					nearest[cell] = current < 0.0f || t < current ? t : current;
					seen[cell] = kind;
					shapeOf[cell] = tri.shape;
				}
			}
		}
	}
	for (UInt32 r = 0; r < allRows; ++r) {
		for (UInt32 c = 0; c < columns; ++c) {
			const UInt32 cell = r * columns + c;
			const UInt8 kind = seen[cell];
			if (r < grid.rows) {
				++tally.rays;
				++tally.seen[kind];
				if (kind == kViewSeenTrunkInside && shapeOf[cell] < kViewMaxShapes) {
					++tally.trunkInsideOf[shapeOf[cell]];
				}
			} else if (kind == kViewSeenTrunkInside && tally.trunkInsideBelowDegrees < 0.0f) {
				tally.trunkInsideBelowDegrees = (static_cast<float>(r - grid.rows) + 0.5f) * grid.marginDegrees /
				                                static_cast<float>(grid.marginRows);
			}
			if (cells != nullptr) {
				cells[cell] = kind;
			}
		}
	}
	return tally;
}

// ------------------------------------------------------------------ the verdict

// How many of an eye's rays may meet the trunk's inside before the view
// counts as one into the body, and how much of the view the trunk has to
// fill from outside for it to count as seen at all.
inline constexpr UInt32 kViewInsideRaysAtMost = 0;
inline constexpr float kViewTrunkSeenShare = 0.005f;

enum class ViewVerdict : UInt8 {
	DownTheBody,     // the trunk seen from outside, its inside nowhere
	IntoTheBody,     // an eye meets the trunk's inside
	TrunkOutOfView,  // neither eye sees the trunk
};

inline float ViewShare(const ViewTally& t, UInt8 kind) {
	return t.rays > 0 ? static_cast<float>(t.seen[kind]) / static_cast<float>(t.rays) : 0.0f;
}

inline ViewVerdict BodyViewVerdict(const ViewTally& left, const ViewTally& right) {
	if (left.seen[kViewSeenTrunkInside] > kViewInsideRaysAtMost ||
	    right.seen[kViewSeenTrunkInside] > kViewInsideRaysAtMost) {
		return ViewVerdict::IntoTheBody;
	}
	if (!(ViewShare(left, kViewSeenTrunkOutside) >= kViewTrunkSeenShare) &&
	    !(ViewShare(right, kViewSeenTrunkOutside) >= kViewTrunkSeenShare)) {
		return ViewVerdict::TrunkOutOfView;
	}
	return ViewVerdict::DownTheBody;
}

}  // namespace obvr::game
