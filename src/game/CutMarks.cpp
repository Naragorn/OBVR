#include "game/CutMarks.h"

#include <cstdio>
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

}  // namespace

bool LayCutMark(UInt32 actor, const CutMark& mark, const NiPoint3& cutWorld, char* why, UInt32 whySize) {
	if (!mark.place || mark.texture == nullptr || !mem::LooksLikeObjectAddress(actor)) {
		std::snprintf(why, whySize, "no mark or no actor");
		return false;
	}
	const UInt32 node = Read(actor + addr::kRefNiNodeOffset);
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
	int skip = 0;
	const bool laid = reinterpret_cast<DecalFn>(kEngineDecal)(
		reinterpret_cast<void*>(cell), mark.from.x, mark.from.y, mark.from.z, mark.direction.x, mark.direction.y,
		mark.direction.z, mark.texture, reinterpret_cast<void*>(node), &skip, nullptr, angle, 0);
	// What the call answers is not whether it laid one (its last path
	// returns what a release left in al, 0x004CF9xx): the decals under the
	// node say it, at the next mark (SayGeometries).
	std::snprintf(why, whySize, "asked of the engine (it answered %d), turned %.2f rad in its root's frame (%.2f in "
	              "the world's)", laid ? 1 : 0, static_cast<double>(angle), static_cast<double>(mark.angle));
	return true;
}

}  // namespace obvr::game
