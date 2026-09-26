#include "game/DeathBody.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// A skeleton with its clothes, armour and weapon is a few hundred nodes.
constexpr UInt32 kMaxShifted = 1024;
constexpr UInt32 kMaxDepth = 64;

struct Shifted {
	NiAVObject* node = nullptr;
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	NiPoint3 boundCentre{0.0f, 0.0f, 0.0f};
};

Shifted g_shifted[kMaxShifted];
UInt32 g_shiftedCount = 0;
bool g_overflowReported = false;
bool g_shiftReported = false;

bool NameIs(const NiAVObject* node, const char* wanted) {
	const char* const name = node->name;
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name))) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		if (name[at] != wanted[at]) {
			return false;
		}
		if (name[at] == 0) {
			return true;
		}
	}
	return false;
}

// The children of a node, or none for anything that is not one.
UInt32 ChildrenOf(const NiAVObject* node, UInt16& count) {
	count = 0;
	if (!NiClassIsNode(NiClassNameOf(node))) {
		return 0;
	}
	const UInt32 address = reinterpret_cast<UInt32>(node);
	const UInt32 children = Read(address + addr::kNiChildrenOffset);
	const UInt16 n = *reinterpret_cast<const UInt16*>(address + addr::kNiChildCountOffset);
	if (!LooksLikeObject(children) || n > 512) {
		return 0;
	}
	count = n;
	return children;
}

// The skeleton's top bone under the player's root, depth first.
NiAVObject* FindNamed(NiAVObject* node, const char* wanted, UInt32 depth) {
	if (depth > 8) {
		return nullptr;
	}
	if (NameIs(node, wanted)) {
		return node;
	}
	UInt16 count = 0;
	const UInt32 children = ChildrenOf(node, count);
	for (UInt16 i = 0; i < count; ++i) {
		const UInt32 child = Read(children + i * 4u);
		if (LooksLikeObject(child)) {
			if (NiAVObject* const found =
			        FindNamed(reinterpret_cast<NiAVObject*>(child), wanted, depth + 1)) {
				return found;
			}
		}
	}
	return nullptr;
}

void Shift(NiAVObject* node, const NiPoint3& offset, UInt32 depth) {
	if (depth > kMaxDepth) {
		return;
	}
	if (g_shiftedCount == kMaxShifted) {
		if (!g_overflowReported) {
			g_overflowReported = true;
			OBVR_LOG("Death: the body has more than %u nodes - the rest are drawn where they "
			         "fall",
			         kMaxShifted);
		}
		return;
	}
	Shifted& s = g_shifted[g_shiftedCount++];
	s.node = node;
	s.pos = node->worldTransform.pos;
	s.boundCentre = node->worldBound.center;
	node->worldTransform.pos = node->worldTransform.pos + offset;
	node->worldBound.center = node->worldBound.center + offset;
	UInt16 count = 0;
	const UInt32 children = ChildrenOf(node, count);
	for (UInt16 i = 0; i < count; ++i) {
		const UInt32 child = Read(children + i * 4u);
		if (LooksLikeObject(child)) {
			Shift(reinterpret_cast<NiAVObject*>(child), offset, depth + 1);
		}
	}
}

}  // namespace

void ShiftDeadPlayerBody(bool wanted, const NiPoint3& offset) {
	RestoreDeadPlayerBody();
	if (!wanted || offset.LengthSquared() == 0.0f) {
		g_shiftReported = false;
		return;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return;
	}
	const UInt32 root = Read(player + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(root)) {
		return;
	}
	// The skeleton, not the root: the camera hangs under the root too
	// (game::PlayerBody learned it), and a shifted camera would carry the
	// view along. Skinned meshes follow their bones; what is attached to a
	// bone (the weapon, a shield) sits under the skeleton.
	NiAVObject* const skeleton = FindNamed(reinterpret_cast<NiAVObject*>(root), "Bip01", 0);
	if (skeleton == nullptr) {
		if (!g_shiftReported) {
			g_shiftReported = true;
			OBVR_LOG("Death: no Bip01 under the player's node - the body is drawn where it falls");
		}
		return;
	}
	Shift(skeleton, offset, 0);
	if (!g_shiftReported) {
		g_shiftReported = true;
		OBVR_LOG("Death: the body is drawn %.0f units ahead of the held view (%u nodes)",
		         static_cast<double>(math::Sqrt(offset.LengthSquared())), g_shiftedCount);
	}
}

void RestoreDeadPlayerBody() {
	for (UInt32 i = 0; i < g_shiftedCount; ++i) {
		Shifted& s = g_shifted[i];
		if (LooksLikeObject(reinterpret_cast<UInt32>(s.node))) {
			s.node->worldTransform.pos = s.pos;
			s.node->worldBound.center = s.boundCentre;
		}
	}
	g_shiftedCount = 0;
}

}  // namespace obvr::game
