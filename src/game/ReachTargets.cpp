#include "game/ReachTargets.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/Lead.h"
#include "game/NearbyItems.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool LooksLikeCode(UInt32 address) { return address >= addr::kTextStart && address < addr::kTextEnd; }

// As NearbyItems.cpp walks the cell (xOBSE GameObjects.h, GameForms.h).
constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellObjectListOffset = 0x48;
constexpr UInt32 kFormFlagsOffset = 0x08;
constexpr UInt32 kFormDeletedOrDisabled = 0x20 | 0x800;
constexpr UInt32 kNiHiddenFlag = 0x1;
constexpr UInt32 kMaxRefsPerFrame = 8192;

// xOBSE GameForms.h's FormType.
constexpr UInt8 kFormTypeContainer = 0x17;
constexpr UInt8 kFormTypeNpc = 0x23;
constexpr UInt8 kFormTypeCreature = 0x24;

// The geometry reads NearbyItems.cpp measured (NiGeometry data at +0xB4;
// the vertex count at +0x08, the model bound at +0x0C, the vertices at
// +0x1C), checked the same way: every vertex inside its data's bound.
constexpr UInt32 kGeometryDataOffset = 0xB4;
constexpr UInt32 kGeometryVertexCountOffset = 0x08;
constexpr UInt32 kGeometryBoundOffset = 0x0C;
constexpr UInt32 kGeometryVerticesOffset = 0x1C;
constexpr UInt32 kMaxVerticesPerGeometry = 8192;
constexpr UInt32 kMaxGeometries = 24;
constexpr UInt32 kMaxBoxDepth = 8;

// A skeleton: the nodes named "Bip01 ..." under the actor's root, deep (a
// finger is a dozen levels down) but few.
constexpr UInt32 kMaxBones = 64;
constexpr UInt32 kMaxSkeletonDepth = 24;
constexpr UInt32 kMaxSkeletonNodes = 400;
// The flesh round a bone: a hand on a torso is 10-15 cm from the spine.
constexpr float kBodyPadUnits = 8.0f;

// ExtraLock (xOBSE GameExtraData.h, GameExtraData.cpp): extra data type
// 0x31, its table 0x00A357B8, its Data* at +0x0C; Data { lockLevel 0, key 4,
// flags 8 }, kLock_isLocked = 1. ExtraDataList (GameBSExtraData.h): m_data
// at +0x04, the presence bits at +0x08; BSExtraData: type at +0x04, next at
// +0x08.
constexpr UInt8 kExtraLockType = 0x31;
constexpr UInt32 kVtblExtraLock = 0x00A357B8;
constexpr UInt32 kExtraLockDataOffset = 0x0C;
constexpr UInt32 kLockFlagsOffset = 0x08;
constexpr UInt8 kLockIsLocked = 0x01;

// The engine's close-all-menus (xOBSE Commands_Menu.cpp: 0x00579770):
// push 1; push 0; call 0x00582160 (the interface manager); add esp,8.
constexpr UInt32 kCloseAllMenus = 0x00579770;
constexpr UInt8 kCloseAllMenusBytes[] = {0x6A, 0x01, 0x6A, 0x00, 0xE8, 0xE7, 0x89, 0x00, 0x00, 0x83, 0xC4, 0x08};
int g_closeVerified = -1;

UInt32 VirtualAt(UInt32 object, UInt32 slotOffset) {
	if (!LooksLikeObject(object)) {
		return 0;
	}
	const UInt32 vtable = Read(object);
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 entry = Read(vtable + slotOffset);
	return LooksLikeCode(entry) ? entry : 0;
}

bool IsActorRef(UInt32 ref) {
	const UInt32 vtable = Read(ref);
	return vtable == addr::kVtblCharacter || vtable == addr::kVtblCreature;
}

bool ActorDead(UInt32 actor) {
	const UInt32 isDead = VirtualAt(actor, addr::kActorVtableIsDeadOffset);
	if (isDead == 0) {
		return false;  // unreadable: not taken for a body
	}
	using IsDeadFn = UInt8(__fastcall*)(UInt32 self, void* edx, UInt32 arg);
	return (reinterpret_cast<IsDeadFn>(isDead)(actor, nullptr, 0) & 1) != 0;
}

// The model's box in its root's own frame: each vertex taken into the world
// by its geometry's transform and back into the root's frame.
struct LocalBox {
	NiPoint3 low{0.0f, 0.0f, 0.0f};
	NiPoint3 high{0.0f, 0.0f, 0.0f};
	NiTransform root{};
	UInt32 vertices = 0;
	UInt32 geometries = 0;
	bool refused = false;
};

NiPoint3 IntoRoot(const NiTransform& root, const NiPoint3& world) {
	const NiPoint3 d = world - root.pos;
	const float inv = root.scale > 0.0f ? 1.0f / root.scale : 1.0f;
	// The transpose of the root's rotation takes the world back into it.
	const NiMatrix33& r = root.rot;
	return NiPoint3{(r.data[0][0] * d.x + r.data[1][0] * d.y + r.data[2][0] * d.z) * inv,
	                (r.data[0][1] * d.x + r.data[1][1] * d.y + r.data[2][1] * d.z) * inv,
	                (r.data[0][2] * d.x + r.data[1][2] * d.y + r.data[2][2] * d.z) * inv};
}

bool ContainsTri(const char* name) {
	for (const char* at = name; at[0] != '\0' && at[1] != '\0' && at[2] != '\0'; ++at) {
		if (at[0] == 'T' && at[1] == 'r' && at[2] == 'i') {
			return true;
		}
	}
	return false;
}

void BoxGeometry(const NiAVObject* geometry, LocalBox& box) {
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
			box.refused = true;
			return;
		}
		const NiPoint3 local = IntoRoot(box.root, world.pos + world.rot * (v[i] * scale));
		if (box.vertices == 0) {
			box.low = local;
			box.high = local;
		}
		box.low = NiPoint3{local.x < box.low.x ? local.x : box.low.x, local.y < box.low.y ? local.y : box.low.y,
		                   local.z < box.low.z ? local.z : box.low.z};
		box.high = NiPoint3{local.x > box.high.x ? local.x : box.high.x, local.y > box.high.y ? local.y : box.high.y,
		                    local.z > box.high.z ? local.z : box.high.z};
		++box.vertices;
	}
	++box.geometries;
}

void BoxTree(const NiAVObject* object, UInt32 depth, LocalBox& box) {
	if (depth > kMaxBoxDepth || box.refused || box.geometries >= kMaxGeometries ||
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
				BoxTree(reinterpret_cast<const NiAVObject*>(child), depth + 1, box);
			}
		}
		return;
	}
	if (ContainsTri(name)) {
		BoxGeometry(object, box);
	}
}

struct Skeleton {
	NiPoint3 bones[kMaxBones];
	UInt32 count = 0;
	UInt32 visited = 0;
};

bool IsBoneName(const char* name) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name))) {
		return false;
	}
	return name[0] == 'B' && name[1] == 'i' && name[2] == 'p' && name[3] == '0' && name[4] == '1';
}

void CollectBones(const NiAVObject* object, UInt32 depth, Skeleton& s) {
	if (depth > kMaxSkeletonDepth || s.count >= kMaxBones || ++s.visited > kMaxSkeletonNodes) {
		return;
	}
	const char* const className = NiClassNameOf(object);
	if (!NiClassIsNode(className)) {
		return;
	}
	if (IsBoneName(object->name)) {
		s.bones[s.count++] = object->worldTransform.pos;
	}
	const UInt32 address = reinterpret_cast<UInt32>(object);
	const UInt32 children = Read(address + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(address + addr::kNiChildCountOffset);
	if (!LooksLikeObject(children) || count > 512) {
		return;
	}
	for (UInt16 i = 0; i < count && s.count < kMaxBones; ++i) {
		const UInt32 child = Read(children + i * 4u);
		if (LooksLikeObject(child)) {
			CollectBones(reinterpret_cast<const NiAVObject*>(child), depth + 1, s);
		}
	}
}

UInt32 g_boxLinesLeft = 4;
UInt32 g_boneLinesLeft = 4;

// The distance from `hand` to `ref`'s own shape: an actor's bones, else its
// model's box in its own frame; the bound sphere when neither reads. False
// with no node to measure.
bool MeasureRef(UInt32 ref, bool actor, const NiPoint3* hands, const bool* valid, float& best, bool& left) {
	const UInt32 nodeAddress = Read(ref + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(nodeAddress)) {
		return false;
	}
	const auto* node = reinterpret_cast<const NiAVObject*>(nodeAddress);
	best = -1.0f;
	if (actor) {
		Skeleton s;
		CollectBones(node, 0, s);
		if (s.count > 0) {
			for (int side = 0; side < 2; ++side) {
				if (!valid[side]) {
					continue;
				}
				const float d = vr::DistanceToBones(hands[side], s.bones, s.count, kBodyPadUnits);
				if (d >= 0.0f && (best < 0.0f || d < best)) {
					best = d;
					left = side == 1;
				}
			}
			return best >= 0.0f;
		}
		if (g_boneLinesLeft > 0) {
			--g_boneLinesLeft;
			OBVR_LOG("Reach: the actor %08X has no \"Bip01\" bones to measure - its bound is taken", ref);
		}
	} else {
		LocalBox box;
		box.root = node->worldTransform;
		BoxTree(node, 0, box);
		if (!box.refused && box.vertices > 0) {
			const float scale = box.root.scale > 0.0f ? box.root.scale : 1.0f;
			for (int side = 0; side < 2; ++side) {
				if (!valid[side]) {
					continue;
				}
				const float d = vr::DistanceToBox(IntoRoot(box.root, hands[side]), box.low, box.high) * scale;
				if (best < 0.0f || d < best) {
					best = d;
					left = side == 1;
				}
			}
			return best >= 0.0f;
		}
		if (g_boxLinesLeft > 0) {
			--g_boxLinesLeft;
			OBVR_LOG("Reach: the container %08X's model did not read as a box (%s, %u vertices) - its bound is "
			         "taken",
			         ref, box.refused ? "a vertex outside its bound" : "no geometry", box.vertices);
		}
	}
	for (int side = 0; side < 2; ++side) {
		if (!valid[side]) {
			continue;
		}
		const float d = SurfaceDistance(hands[side], node->worldBound.center, node->worldBound.radius * 0.6f);
		if (best < 0.0f || d < best) {
			best = d;
			left = side == 1;
		}
	}
	return best >= 0.0f;
}

}  // namespace

ReachTarget FindReachTarget(const ReachHands& hands, float withinUnits, bool pockets) {
	ReachTarget out;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !(hands.valid[0] || hands.valid[1])) {
		return out;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell)) {
		return out;
	}
	UInt32 entry = cell + kCellObjectListOffset;
	for (UInt32 walked = 0; entry != 0 && walked < kMaxRefsPerFrame; ++walked) {
		const UInt32 ref = Read(entry);
		const UInt32 next = Read(entry + 4);
		entry = LooksLikeObject(next) ? next : 0;
		if (!LooksLikeObject(ref) || ref == player || (Read(ref + kFormFlagsOffset) & kFormDeletedOrDisabled) != 0) {
			continue;
		}
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		if (!LooksLikeObject(base)) {
			continue;
		}
		const UInt8 type = *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset);
		const bool actor = (type == kFormTypeNpc || type == kFormTypeCreature) && IsActorRef(ref);
		if (type != kFormTypeContainer && !actor) {
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
		// Only what a hand is within reach of by the bound: the shape is read
		// for those alone.
		bool inReach = false;
		for (int side = 0; side < 2; ++side) {
			inReach = inReach || (hands.valid[side] &&
			                SurfaceDistance(hands.position[side], node->worldBound.center, node->worldBound.radius) <=
			                    withinUnits);
		}
		if (!inReach) {
			continue;
		}
		vr::ReachKind kind = vr::ReachKind::Container;
		if (actor) {
			if (ActorDead(ref)) {
				kind = vr::ReachKind::Body;
			} else if (!pockets || ActorInCombat(reinterpret_cast<void*>(ref))) {
				continue;
			} else {
				kind = vr::ReachKind::Pocket;
			}
		}
		float d = 0.0f;
		bool left = false;
		if (!MeasureRef(ref, actor, hands.position, hands.valid, d, left) || d > withinUnits ||
		    (out.ref != 0 && d >= out.distanceUnits)) {
			continue;
		}
		out.ref = ref;
		out.distanceUnits = d;
		out.left = left;
		out.kind = kind;
		out.personInCombat = false;
	}
	return out;
}

bool ReachDistanceTo(UInt32 ref, const ReachHands& hands, float& units, bool& gone) {
	gone = false;
	if (!LooksLikeObject(ref) || (Read(ref + kFormFlagsOffset) & kFormDeletedOrDisabled) != 0) {
		gone = true;
		return false;
	}
	const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
	if (!LooksLikeObject(base)) {
		gone = true;
		return false;
	}
	const UInt8 type = *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset);
	const bool actor = (type == kFormTypeNpc || type == kFormTypeCreature) && IsActorRef(ref);
	bool left = false;
	if (!MeasureRef(ref, actor, hands.position, hands.valid, units, left)) {
		// No node: unloaded with its cell, or never drawn - gone for the reach.
		gone = !LooksLikeObject(Read(ref + addr::kRefNiNodeOffset));
		return false;
	}
	return true;
}

bool RefIsLocked(UInt32 ref) {
	if (!LooksLikeObject(ref)) {
		return false;
	}
	const UInt32 list = ref + addr::kRefExtraListOffset;
	const UInt8 presence = *reinterpret_cast<const UInt8*>(list + 8 + (kExtraLockType >> 3));
	if ((presence & (1u << (kExtraLockType & 7))) == 0) {
		return false;
	}
	UInt32 data = Read(list + 4);
	for (UInt32 walked = 0; LooksLikeObject(data) && walked < 64; ++walked) {
		if (*reinterpret_cast<const UInt8*>(data + 4) == kExtraLockType) {
			if (Read(data) != kVtblExtraLock) {
				return false;
			}
			const UInt32 lock = Read(data + kExtraLockDataOffset);
			return LooksLikeObject(lock) && (*reinterpret_cast<const UInt8*>(lock + kLockFlagsOffset) & kLockIsLocked) != 0;
		}
		data = Read(data + 8);
	}
	return false;
}

bool ActivateByPlayer(UInt32 ref) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !LooksLikeObject(ref)) {
		return false;
	}
	// As the player's activate control calls it (0x0067318A); TakeItem.cpp
	// calls it the same way for the stow.
	using ActivateFn = UInt8(__fastcall*)(UInt32 self, void* edx, UInt32 activator, UInt32 a2, UInt32 a3, UInt32 a4);
	reinterpret_cast<ActivateFn>(addr::kRefActivate)(ref, nullptr, player, 0, 0, 1);
	return true;
}

bool CloseMenus() {
	if (g_closeVerified < 0) {
		g_closeVerified = mem::Verify(kCloseAllMenus, kCloseAllMenusBytes, sizeof(kCloseAllMenusBytes)) ? 1 : 0;
		if (g_closeVerified == 0) {
			OBVR_LOG("Reach: the close-all-menus function at %08X is not the bytes read - a reached menu stays "
			         "open until the player closes it",
			         kCloseAllMenus);
		}
	}
	if (g_closeVerified != 1) {
		return false;
	}
	using CloseFn = void(__cdecl*)();
	reinterpret_cast<CloseFn>(kCloseAllMenus)();
	return true;
}

}  // namespace obvr::game
