#include "game/CutMarks.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

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
	int skip = 0;
	const bool laid = reinterpret_cast<DecalFn>(kEngineDecal)(
		reinterpret_cast<void*>(cell), mark.from.x, mark.from.y, mark.from.z, mark.direction.x, mark.direction.y,
		mark.direction.z, mark.texture, reinterpret_cast<void*>(node), &skip, nullptr, angle, 0);
	std::snprintf(why, whySize, "%s, turned %.2f rad in its root's frame (%.2f in the world's)",
	              laid ? "the engine laid it" : "the engine laid none (its frame's count full, or no geometry hit)",
	              static_cast<double>(angle), static_cast<double>(mark.angle));
	return laid;
}

}  // namespace obvr::game
