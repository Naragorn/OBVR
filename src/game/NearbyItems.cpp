#include "game/NearbyItems.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"

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
// count (UInt16) at +0x08, its model bound (centre, radius) at +0x10 and the
// vertex array at +0x20. That is checked, not trusted: every vertex read
// has to lie in that bound (VertexInBound), else the geometry is refused and
// the log says so once.
constexpr UInt32 kGeometryDataOffset = 0xB4;
constexpr UInt32 kGeometryVertexCountOffset = 0x08;
constexpr UInt32 kGeometryBoundOffset = 0x10;
constexpr UInt32 kGeometryVerticesOffset = 0x20;
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
	    !LooksLikeObject(vertices + count * sizeof(NiPoint3) - 1)) {
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

}  // namespace

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

NearItem FindNearestItem(const NiPoint3& right, bool rightValid, const NiPoint3& left,
                         bool leftValid, float reachUnits, UInt32 except) {
	NearItem best;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !(rightValid || leftValid)) {
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
		ConsiderNearItem(best, ref, node->worldBound.center, node->worldBound.radius, right,
		                 rightValid, left, leftValid, reachUnits);
	}
	return best;
}

}  // namespace obvr::game
