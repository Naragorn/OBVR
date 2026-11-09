#include "game/CutMarks.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "core/Log.h"
#include "game/GameTypes.h"
#include "game/SkinRead.h"

namespace obvr::game {
namespace {

using DecalFn = bool(__thiscall*)(void* cell, float px, float py, float pz, float dx, float dy, float dz,
                                  const char* texture, void* node, int* skip, void* second, float angle, UInt32 last);

// The game's decal settings as the call reads them (OBNG
// knowledge/blood-on-hits.md): fDecalLifetime, iMaxDecalsPerFrame and the
// count of this frame, bDecalsOnSkinnedGeometry.
constexpr UInt32 kDecalLifetime = 0x00B097C8;
constexpr UInt32 kDecalsOnSkinned = 0x00B06F5C;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

NiPoint3 IntoFrame(const NiMatrix33& rot, const NiPoint3& v) {
	// rot's transpose times v: the world direction in the node's own frame
	return NiPoint3{rot.data[0][0] * v.x + rot.data[1][0] * v.y + rot.data[2][0] * v.z,
	                rot.data[0][1] * v.x + rot.data[1][1] * v.y + rot.data[2][1] * v.z,
	                rot.data[0][2] * v.x + rot.data[1][2] * v.y + rot.data[2][2] * v.z};
}

// Measured once a run (the spec, step 3): what the engine's decal call asks
// of each geometry before it lays a decal on it (0x004CF1A0): its skin
// (+0xB8), the word after it (+0xBC, asked to be set), its property of type
// 4 (0x00707530) and what that property's virtual +0x54 answers (asked 1 to
// 10); and how many decal shapes ("Decal:") hang under the node.
using GetPropertyFn = UInt32(__thiscall*)(void* object, UInt32 type);
using PropertyKindFn = int(__thiscall*)(void* property);
constexpr UInt32 kGetProperty = 0x00707530;

void SayGeometries(UInt32 node) {
	static UInt32 s_left = 3;
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(reinterpret_cast<const NiAVObject*>(node), shapes);
	UInt32 decals = 0;
	for (const ShapeUnder& u : shapes) {
		const char* name = ShapeName(u.geometry);
		decals += name[0] == 'D' && name[1] == 'e' && name[2] == 'c' && name[3] == 'a' && name[4] == 'l' ? 1 : 0;
	}
	OBVR_LOG("Cut mark: %u shapes under %08X, %u of them decals", static_cast<UInt32>(shapes.size()), node, decals);
	if (s_left == 0) {
		return;
	}
	--s_left;
	for (UInt32 i = 0; i < shapes.size() && i < 8; ++i) {
		const UInt32 g = reinterpret_cast<UInt32>(shapes[i].geometry);
		const UInt32 prop = reinterpret_cast<GetPropertyFn>(kGetProperty)(reinterpret_cast<void*>(g), 4);
		const int kind = mem::LooksLikeObjectAddress(prop) ? reinterpret_cast<PropertyKindFn>(
		                                                         Read(Read(prop) + 0x54))(reinterpret_cast<void*>(prop))
		                                                   : -1;
		OBVR_LOG("Cut mark: geometry \"%s\" skin %08X, +BC %08X, property 4 %08X kind %d%s", ShapeName(shapes[i].geometry),
		         Read(g + 0xB8), Read(g + 0xBC), prop, kind, shapes[i].hidden ? " (hidden)" : "");
	}
}

bool IsDecalName(const char* name) {
	return name[0] == 'D' && name[1] == 'e' && name[2] == 'c' && name[3] == 'a' && name[4] == 'l';
}

// The engine's own skip by name (0x004CF1A0): Decal*, FaceGen*, Bip01*.
bool EngineSkipsName(const char* name) {
	return IsDecalName(name) || std::strncmp(name, "FaceGen", 7) == 0 || std::strncmp(name, "Bip01", 5) == 0;
}

// How far past a shape's world bound the point may lie and still have the
// decal asked of it, and how many shapes one mark goes to (the clothes and
// the body under them).
constexpr float kDecalBoundSlackUnits = 6.0f;
constexpr UInt32 kDecalShapesAMark = 2;

// The shapes under `node` a decal at `p` goes to (CutMarkLogic.h
// PickDecalShapes), at most kDecalShapesAMark.
UInt32 DecalShapesAt(UInt32 node, const NiPoint3& p, const NiAVObject** out) {
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(reinterpret_cast<const NiAVObject*>(node), shapes, 64);
	std::vector<DecalShape> d(shapes.size());
	for (size_t i = 0; i < shapes.size(); ++i) {
		const NiBound& b =
			*reinterpret_cast<const NiBound*>(reinterpret_cast<UInt32>(shapes[i].geometry) + addr::kNiAVObjectWorldBoundOffset);
		d[i].skippedName = EngineSkipsName(ShapeName(shapes[i].geometry));
		d[i].hidden = shapes[i].hidden;
		d[i].centre = b.center;
		d[i].radius = b.radius;
	}
	UInt32 picked[kDecalShapesAMark] = {};
	const UInt32 n = PickDecalShapes(d.data(), static_cast<UInt32>(d.size()), p, kDecalBoundSlackUnits, picked,
	                                 kDecalShapesAMark);
	for (UInt32 i = 0; i < n; ++i) {
		out[i] = shapes[picked[i]].geometry;
	}
	return n;
}

}  // namespace

UInt32 DecalsUnder(UInt32 actor) {
	return DecalsUnderNode(mem::LooksLikeObjectAddress(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0);
}

UInt32 DecalsUnderNode(UInt32 node) {
	if (!mem::LooksLikeObjectAddress(node)) {
		return 0;
	}
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(reinterpret_cast<const NiAVObject*>(node), shapes, 256);
	UInt32 decals = 0;
	for (const ShapeUnder& u : shapes) {
		decals += IsDecalName(ShapeName(u.geometry)) ? 1u : 0u;
	}
	return decals;
}

bool LayCutMark(UInt32 actor, const CutMark& mark, const NiPoint3& cutWorld, char* why, UInt32 whySize) {
	const UInt32 node = mem::LooksLikeObjectAddress(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
	return LayCutMarkOn(actor, node, mark, cutWorld, why, whySize);
}

bool LayCutMarkOn(UInt32 actor, UInt32 node, const CutMark& mark, const NiPoint3& cutWorld, char* why,
                  UInt32 whySize) {
	if (!mark.place || mark.texture == nullptr || !mem::LooksLikeObjectAddress(actor)) {
		std::snprintf(why, whySize, "no mark or no actor");
		return false;
	}
	const UInt32 cell = Read(actor + addr::kRefParentCellOffset);
	if (!mem::LooksLikeObjectAddress(node) || !mem::LooksLikeObjectAddress(cell)) {
		std::snprintf(why, whySize, "its 3D (%08X) or its cell (%08X) not there", node, cell);
		return false;
	}
	if (!(*reinterpret_cast<const float*>(kDecalLifetime) > 0.0f)) {
		std::snprintf(why, whySize, "the game's fDecalLifetime is 0 - decals off");
		return false;
	}
	if (*reinterpret_cast<const UInt8*>(kDecalsOnSkinned) != 1) {
		std::snprintf(why, whySize, "bDecalsOnSkinnedGeometry is off - no decals on bodies");
		return false;
	}
	const NiMatrix33& rot = reinterpret_cast<const NiAVObject*>(node)->worldTransform.rot;
	const float angle = CutMarkAngle(IntoFrame(rot, cutWorld), IntoFrame(rot, mark.direction));
	SayGeometries(node);
	// Each shape round the point asked on its own: from the root, the
	// engine's walk spends iMaxDecalsPerFrame on the FaceGen shapes and
	// stops before the clothes (DecalShapesAt). None round it: the root, as
	// before.
	const NiAVObject* targets[kDecalShapesAMark] = {};
	UInt32 count = DecalShapesAt(node, mark.from, targets);
	const bool toShapes = count > 0;
	if (!toShapes) {
		targets[0] = reinterpret_cast<const NiAVObject*>(node);
		count = 1;
	}
	char names[128] = "";
	const UInt32 before = DecalsUnderNode(node);
	UInt32 was = before;
	for (UInt32 i = 0; i < count; ++i) {
		int skip = 0;
		reinterpret_cast<DecalFn>(kEngineDecal)(reinterpret_cast<void*>(cell), mark.from.x, mark.from.y, mark.from.z,
		                                        mark.direction.x, mark.direction.y, mark.direction.z, mark.texture,
		                                        const_cast<NiAVObject*>(targets[i]), &skip, nullptr, angle, 0);
		if (toShapes) {
			const UInt32 now = DecalsUnderNode(node);
			const size_t used = std::strlen(names);
			std::snprintf(names + used, sizeof(names) - used, "%s\"%s\" +%u", i > 0 ? ", " : "", ShapeName(targets[i]),
			              now - was);
			was = now;
		}
	}
	// What the call answers is not whether it laid one (its last path
	// returns what a release left in al, 0x004CF9xx): the decals under the
	// node say it (DecalsUnder, SayGeometries).
	std::snprintf(why, whySize, "asked of the engine on %s (decals under it %u -> %u at once), turned %.2f rad in its "
	              "root's frame (%.2f in the world's)",
	              toShapes ? names : "the root (no shape round the point)", before, DecalsUnderNode(node),
	              static_cast<double>(angle), static_cast<double>(mark.angle));
	return true;
}

}  // namespace obvr::game
