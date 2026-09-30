#include "game/NearbyItems.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/HandBodies.h"
#include "game/HandBodyLogic.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellObjectListOffset = 0x48;
constexpr UInt32 kFormFlagsOffset = 0x08;
constexpr UInt32 kFormDeletedOrDisabled = 0x20 | 0x800;
constexpr UInt32 kNiHiddenFlag = 0x1;
// A cell with more references than this is not walked further in a frame.
constexpr UInt32 kMaxRefsPerFrame = 8192;

// NiGeometry's data at +0xB4 (xOBSE NiObjects.h: NiGeometry { propertyState
// 0AC, effectState 0B0, geomData 0B4, skinData 0B8, shader 0BC }). The data's
// own fields are not in xOBSE; Gamebryo's NiGeometryData keeps the vertex
// count (UInt16) at +0x08, its model bound (centre, radius) at +0x0C and the
// vertex array at +0x1C (the normals at +0x20). Measured 2026-09-29 on a
// placed cup: +0x0C..+0x18 read 0.02 0.02 5.28, radius 6.27; the array at
// +0x20 held vectors of length 1. Before, the bound was read at +0x10, the
// normals as the vertices and the array's end checked unaligned, so no
// geometry was ever accepted and NearestVertexOf always fell back to the
// middle. That is checked, not trusted: every vertex read
// has to lie in that bound (VertexInBound), else the geometry is refused and
// the log says so once.
constexpr UInt32 kGeometryDataOffset = 0xB4;
constexpr UInt32 kGeometryVertexCountOffset = 0x08;
constexpr UInt32 kGeometryBoundOffset = 0x0C;
constexpr UInt32 kGeometryVerticesOffset = 0x1C;
constexpr UInt32 kMaxVerticesPerGeometry = 8192;
constexpr UInt32 kMaxGeometries = 16;
constexpr UInt32 kMaxDepth = 6;

bool ContainsText(const char* text, const char* part) {
	for (const char* at = text; *at != '\0'; ++at) {
		const char* a = at;
		const char* b = part;
		while (*a != '\0' && *b != '\0' && *a == *b) {
			++a;
			++b;
		}
		if (*b == '\0') {
			return true;
		}
	}
	return false;
}

struct NearestSearch {
	NiPoint3 hand;
	NiPoint3 best{0.0f, 0.0f, 0.0f};
	float bestSquared = -1.0f;
	UInt32 geometries = 0;
	bool refused = false;
};

UInt32 g_vertexLinesLeft = 4;

void SearchGeometry(const NiAVObject* geometry, NearestSearch& s) {
	const UInt32 data = Read(reinterpret_cast<UInt32>(geometry) + kGeometryDataOffset);
	if (!LooksLikeObject(data)) {
		return;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(data + kGeometryVertexCountOffset);
	const NiPoint3 boundCentre = *reinterpret_cast<const NiPoint3*>(data + kGeometryBoundOffset);
	const float boundRadius = *reinterpret_cast<const float*>(data + kGeometryBoundOffset + 12);
	const UInt32 vertices = Read(data + kGeometryVerticesOffset);
	if (count == 0 || count > kMaxVerticesPerGeometry || !LooksLikeObject(vertices) ||
	    !LooksLikeObject(vertices + (count - 1) * sizeof(NiPoint3))) {
		return;
	}
	const auto* v = reinterpret_cast<const NiPoint3*>(vertices);
	const NiTransform& world = geometry->worldTransform;
	const float scale = world.scale > 0.0f ? world.scale : 1.0f;
	NiPoint3 best{0.0f, 0.0f, 0.0f};
	float bestSquared = -1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		if (!VertexInBound(v[i], boundCentre, boundRadius)) {
			s.refused = true;
			if (g_vertexLinesLeft > 0) {
				--g_vertexLinesLeft;
				OBVR_LOG("Hands: the geometry %s of an item did not read as expected (vertex %u of "
				         "%u outside its bound) - its middle is aimed at instead",
				         NiClassNameOf(geometry), i, count);
			}
			return;
		}
		const NiPoint3 w = world.pos + world.rot * (v[i] * scale);
		const float d = (w - s.hand).LengthSquared();
		if (bestSquared < 0.0f || d < bestSquared) {
			bestSquared = d;
			best = w;
		}
	}
	++s.geometries;
	if (bestSquared >= 0.0f && (s.bestSquared < 0.0f || bestSquared < s.bestSquared)) {
		s.bestSquared = bestSquared;
		s.best = best;
	}
}

void SearchTree(const NiAVObject* object, UInt32 depth, NearestSearch& s) {
	if (depth > kMaxDepth || s.refused || s.geometries >= kMaxGeometries ||
	    (object->flags & kNiHiddenFlag) != 0) {
		return;
	}
	const char* const name = NiClassNameOf(object);
	if (NiClassIsNode(name)) {
		const UInt32 address = reinterpret_cast<UInt32>(object);
		const UInt32 children = Read(address + addr::kNiChildrenOffset);
		const UInt16 count = *reinterpret_cast<const UInt16*>(address + addr::kNiChildCountOffset);
		if (!LooksLikeObject(children) || count > 512) {
			return;
		}
		for (UInt16 i = 0; i < count; ++i) {
			const UInt32 child = Read(children + i * 4u);
			if (LooksLikeObject(child)) {
				SearchTree(reinterpret_cast<const NiAVObject*>(child), depth + 1, s);
			}
		}
		return;
	}
	// Triangle geometry only: NiTriShape, NiTriStrips and their kin.
	if (ContainsText(name, "Tri")) {
		SearchGeometry(object, s);
	}
}

// The world box round a model's vertices (MeasureNearbyShapes).
struct MeshBox {
	NiPoint3 low{0.0f, 0.0f, 0.0f};
	NiPoint3 high{0.0f, 0.0f, 0.0f};
	UInt32 vertices = 0;
	bool refused = false;
	UInt32 nodes = 0;
	UInt32 geometries = 0;
	UInt32 unreadable = 0;
	const char* lastLeaf = "";
};

void BoxGeometry(const NiAVObject* geometry, MeshBox& box) {
	++box.geometries;
	const UInt32 data = Read(reinterpret_cast<UInt32>(geometry) + kGeometryDataOffset);
	if (!LooksLikeObject(data)) {
		++box.unreadable;
		return;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(data + kGeometryVertexCountOffset);
	const NiPoint3 boundCentre = *reinterpret_cast<const NiPoint3*>(data + kGeometryBoundOffset);
	const float boundRadius = *reinterpret_cast<const float*>(data + kGeometryBoundOffset + 12);
	const UInt32 vertices = Read(data + kGeometryVerticesOffset);
	if (count == 0 || count > kMaxVerticesPerGeometry || !LooksLikeObject(vertices) ||
	    !LooksLikeObject(vertices + (count - 1) * sizeof(NiPoint3))) {
		++box.unreadable;
		return;
	}
	const auto* v = reinterpret_cast<const NiPoint3*>(vertices);
	const NiTransform& world = geometry->worldTransform;
	const float scale = world.scale > 0.0f ? world.scale : 1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		if (!VertexInBound(v[i], boundCentre, boundRadius)) {
			box.refused = true;
			return;
		}
		const NiPoint3 w = world.pos + world.rot * (v[i] * scale);
		if (box.vertices == 0) {
			box.low = w;
			box.high = w;
		}
		box.low = NiPoint3{w.x < box.low.x ? w.x : box.low.x, w.y < box.low.y ? w.y : box.low.y,
		                   w.z < box.low.z ? w.z : box.low.z};
		box.high = NiPoint3{w.x > box.high.x ? w.x : box.high.x, w.y > box.high.y ? w.y : box.high.y,
		                    w.z > box.high.z ? w.z : box.high.z};
		++box.vertices;
	}
}

void BoxTree(const NiAVObject* object, UInt32 depth, MeshBox& box) {
	if (depth > kMaxDepth || box.refused || (object->flags & kNiHiddenFlag) != 0) {
		return;
	}
	const char* const name = NiClassNameOf(object);
	if (NiClassIsNode(name)) {
		++box.nodes;
		const UInt32 address = reinterpret_cast<UInt32>(object);
		const UInt32 children = Read(address + addr::kNiChildrenOffset);
		const UInt16 count = *reinterpret_cast<const UInt16*>(address + addr::kNiChildCountOffset);
		if (!LooksLikeObject(children) || count > 512) {
			return;
		}
		for (UInt16 i = 0; i < count; ++i) {
			const UInt32 child = Read(children + i * 4u);
			if (LooksLikeObject(child)) {
				BoxTree(reinterpret_cast<const NiAVObject*>(child), depth + 1, box);
			}
		}
		return;
	}
	box.lastLeaf = name;
	if (ContainsText(name, "Tri")) {
		BoxGeometry(object, box);
	}
}

// How far a model's vertices reach along a line (AxialExtentOf).
struct AxialSpan {
	NiPoint3 origin{0.0f, 0.0f, 0.0f};
	NiPoint3 dir{0.0f, 1.0f, 0.0f};
	float low = 0.0f;
	float high = 0.0f;
	UInt32 vertices = 0;
	bool refused = false;
	bool band = false;
	float bandFrom = 0.0f;
	float bandTo = 0.0f;
	NiPoint3 bandSum{0.0f, 0.0f, 0.0f};
	UInt32 bandVertices = 0;
};

void SpanGeometry(const NiAVObject* geometry, AxialSpan& span) {
	const UInt32 data = Read(reinterpret_cast<UInt32>(geometry) + kGeometryDataOffset);
	if (!LooksLikeObject(data)) {
		return;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(data + kGeometryVertexCountOffset);
	const NiPoint3 boundCentre = *reinterpret_cast<const NiPoint3*>(data + kGeometryBoundOffset);
	const float boundRadius = *reinterpret_cast<const float*>(data + kGeometryBoundOffset + 12);
	const UInt32 vertices = Read(data + kGeometryVerticesOffset);
	if (count == 0 || count > kMaxVerticesPerGeometry || !LooksLikeObject(vertices) ||
	    !LooksLikeObject(vertices + (count - 1) * sizeof(NiPoint3))) {
		return;
	}
	const auto* v = reinterpret_cast<const NiPoint3*>(vertices);
	const NiTransform& world = geometry->worldTransform;
	const float scale = world.scale > 0.0f ? world.scale : 1.0f;
	for (UInt32 i = 0; i < count; ++i) {
		if (!VertexInBound(v[i], boundCentre, boundRadius)) {
			span.refused = true;
			return;
		}
		const NiPoint3 w = world.pos + world.rot * (v[i] * scale) - span.origin;
		const float a = w.x * span.dir.x + w.y * span.dir.y + w.z * span.dir.z;
		if (span.vertices == 0 || a < span.low) {
			span.low = a;
		}
		if (span.vertices == 0 || a > span.high) {
			span.high = a;
		}
		++span.vertices;
		if (span.band && a >= span.bandFrom && a <= span.bandTo) {
			span.bandSum = span.bandSum + w;
			++span.bandVertices;
		}
	}
}

void SpanTree(const NiAVObject* object, UInt32 depth, AxialSpan& span) {
	if (depth > kMaxDepth || span.refused || (object->flags & kNiHiddenFlag) != 0) {
		return;
	}
	const char* const name = NiClassNameOf(object);
	if (NiClassIsNode(name)) {
		const UInt32 address = reinterpret_cast<UInt32>(object);
		const UInt32 children = Read(address + addr::kNiChildrenOffset);
		const UInt16 count = *reinterpret_cast<const UInt16*>(address + addr::kNiChildCountOffset);
		if (!LooksLikeObject(children) || count > 512) {
			return;
		}
		for (UInt16 i = 0; i < count; ++i) {
			const UInt32 child = Read(children + i * 4u);
			if (LooksLikeObject(child)) {
				SpanTree(reinterpret_cast<const NiAVObject*>(child), depth + 1, span);
			}
		}
		return;
	}
	if (ContainsText(name, "Tri")) {
		SpanGeometry(object, span);
	}
}

}  // namespace

bool AxialExtentOf(const NiAVObject* root, const NiPoint3& origin, const NiPoint3& dir, float& low, float& high,
                   const float* band, NiPoint3* bandCentre, UInt32* bandVertices) {
	if (root == nullptr || !LooksLikeObject(reinterpret_cast<UInt32>(root))) {
		return false;
	}
	AxialSpan span;
	span.origin = origin;
	span.dir = dir;
	if (band != nullptr) {
		span.band = true;
		span.bandFrom = band[0];
		span.bandTo = band[1];
	}
	SpanTree(root, 0, span);
	if (span.refused || span.vertices == 0) {
		return false;
	}
	low = span.low;
	high = span.high;
	if (bandVertices != nullptr) {
		*bandVertices = span.bandVertices;
	}
	if (bandCentre != nullptr && span.bandVertices > 0) {
		*bandCentre = origin + span.bandSum * (1.0f / static_cast<float>(span.bandVertices));
	}
	return true;
}

void MeasureNearbyShapes(const NiPoint3& around, float radiusUnits, UInt32 maxObjects) {
	const UInt32 player = Read(addr::kPlayerPointer);
	const UInt32 cell = LooksLikeObject(player) ? Read(player + kRefParentCellOffset) : 0;
	if (!LooksLikeObject(cell)) {
		return;
	}
	UInt32 measured = 0;
	UInt32 small = 0;
	UInt32 noHavok = 0;
	UInt32 entry = cell + kCellObjectListOffset;
	for (UInt32 walked = 0; entry != 0 && walked < kMaxRefsPerFrame && measured < maxObjects; ++walked) {
		const UInt32 ref = Read(entry);
		const UInt32 next = Read(entry + 4);
		entry = LooksLikeObject(next) ? next : 0;
		if (!LooksLikeObject(ref) || ref == player || (Read(ref + kFormFlagsOffset) & kFormDeletedOrDisabled) != 0) {
			continue;
		}
		const UInt32 nodeAddress = Read(ref + addr::kRefNiNodeOffset);
		if (!LooksLikeObject(nodeAddress)) {
			continue;
		}
		const auto* node = reinterpret_cast<const NiAVObject*>(nodeAddress);
		// Small things only (a cup, a plate, a sword), near the player.
		if (!(node->worldBound.radius > 0.5f && node->worldBound.radius < 40.0f) ||
		    (node->worldBound.center - around).LengthSquared() > radiusUnits * radiusUnits) {
			continue;
		}
		++small;
		NiPoint3 havokLow{0.0f, 0.0f, 0.0f};
		NiPoint3 havokHigh{0.0f, 0.0f, 0.0f};
		if (!HavokWorldBoxOf(nodeAddress, havokLow, havokHigh)) {
			++noHavok;
			continue;
		}
		MeshBox mesh;
		BoxTree(node, 0, mesh);
		if (mesh.refused || mesh.vertices == 0) {
			OBVR_LOG("Measure: object %08X - its mesh could not be read (%s, %u vertices; root %s, %u nodes, %u "
			         "geometries, %u unreadable, a leaf %s)",
			         *reinterpret_cast<const UInt32*>(ref + 0x0C), mesh.refused ? "a vertex outside its bound" : "none found",
			         mesh.vertices, NiClassNameOf(node), mesh.nodes, mesh.geometries, mesh.unreadable, mesh.lastLeaf);
			continue;
		}
		++measured;
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		const BoxGaps gaps = HavokBeyondMesh(mesh.low, mesh.high, havokLow, havokHigh);
		const NiPoint3 meshSize = mesh.high - mesh.low;
		const NiPoint3 havokSize = havokHigh - havokLow;
		OBVR_LOG("Measure: object %08X (base %08X, type %02X) - mesh %.1f x %.1f x %.1f, Havok %.1f x %.1f x %.1f; "
		         "Havok beyond the mesh by %.1f at most, %.1f at least (units)",
		         *reinterpret_cast<const UInt32*>(ref + 0x0C), LooksLikeObject(base) ? Read(base + 0x0C) : 0,
		         static_cast<UInt32>(RefBaseFormType(ref)), static_cast<double>(meshSize.x),
		         static_cast<double>(meshSize.y), static_cast<double>(meshSize.z), static_cast<double>(havokSize.x),
		         static_cast<double>(havokSize.y), static_cast<double>(havokSize.z), static_cast<double>(gaps.most),
		         static_cast<double>(gaps.least));
	}
	OBVR_LOG("Measure: %u small objects within %.0f units, %u measured, %u without a Havok body read", small,
	         static_cast<double>(radiusUnits), measured, noHavok);
}

bool NearestVertexOf(UInt32 ref, const NiPoint3& hand, NiPoint3& out, float& distanceOut) {
	if (!LooksLikeObject(ref)) {
		return false;
	}
	const UInt32 node = Read(ref + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(node)) {
		return false;
	}
	NearestSearch s;
	s.hand = hand;
	SearchTree(reinterpret_cast<const NiAVObject*>(node), 0, s);
	if (s.refused || s.bestSquared < 0.0f) {
		return false;
	}
	out = s.best;
	distanceOut = math::Sqrt(s.bestSquared);
	return true;
}

UInt8 RefBaseFormType(UInt32 ref) {
	if (!LooksLikeObject(ref)) {
		return 0;
	}
	const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
	return LooksLikeObject(base) ? *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset)
	                             : 0;
}

NearItem FindNearestItem(const SearchHand& right, const SearchHand& left, float reachUnits,
                         float alwaysUnits, UInt32 except) {
	NearItem best;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !(right.valid || left.valid)) {
		return best;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell)) {
		return best;
	}
	// The first entry is inline in the cell; the rest are linked.
	UInt32 entry = cell + kCellObjectListOffset;
	for (UInt32 walked = 0; entry != 0 && walked < kMaxRefsPerFrame; ++walked) {
		const UInt32 ref = Read(entry);
		const UInt32 next = Read(entry + 4);
		entry = LooksLikeObject(next) ? next : 0;
		if (!LooksLikeObject(ref) || ref == player || ref == except) {
			continue;
		}
		if ((Read(ref + kFormFlagsOffset) & kFormDeletedOrDisabled) != 0) {
			continue;
		}
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		if (!LooksLikeObject(base) ||
		    !IsHandItemType(*reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset))) {
			continue;
		}
		const UInt32 nodeAddress = Read(ref + addr::kRefNiNodeOffset);
		if (!LooksLikeObject(nodeAddress)) {
			continue;
		}
		const auto* node = reinterpret_cast<const NiAVObject*>(nodeAddress);
		if ((node->flags & kNiHiddenFlag) != 0) {
			continue;
		}
		ConsiderNearItem(best, ref, node->worldBound.center, node->worldBound.radius, right, left,
		                 reachUnits, alwaysUnits);
	}
	return best;
}

}  // namespace obvr::game
