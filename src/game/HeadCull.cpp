#include "game/HeadCull.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/HeadCullLogic.h"
#include "game/PlayerTeleport.h"
#include "test/HandScriptRuntime.h"

namespace obvr::game {
namespace {

// The player's head is two FaceGen nodes (Debug.BodyTreeProbe, harness
// head-cull-on 2026-11-05): "BSFaceGenNiNodeSkinned" under the body's root
// with the skinned FaceGenFace - the node the player's virtual at 0x134
// returns - and "BSFaceGenNiNodeBiped" under Bip01 Head with the rigid
// parts (FaceGenEars, Mouth, TeethLower/Upper, Tongue, Hair, EyeLeft/Right).
constexpr UInt32 kFaceNodes = 2;

// One node culled this render, and whether the bit is this code's.
struct CulledNode {
	NiAVObject* node = nullptr;
	bool ours = false;
	const NiAVObject* reported = nullptr;
};

CulledNode& Slot(UInt32 i) {
	static CulledNode s_nodes[kFaceNodes];
	return s_nodes[i];
}

bool Readable(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

bool Under(const NiAVObject* node, const NiAVObject* root) {
	const NiAVObject* at = node;
	for (int depth = 0; depth < 32 && Readable(at); ++depth) {
		if (at == root) {
			return true;
		}
		at = at->parent;
	}
	return false;
}

// The FaceGen node hanging straight under the head bone, or null.
NiAVObject* BipedFaceUnder(const NiAVObject* head) {
	UInt32 count = 0;
	NiAVObject* const* children = NodeChildren(head, count);
	for (UInt32 i = 0; i < count; ++i) {
		NiAVObject* const child = children[i];
		if (Readable(child) && std::strcmp(NiClassNameOf(child), "BSFaceGenNiNode@@") == 0) {
			return child;
		}
	}
	return nullptr;
}

// Once per face node: what it is, what hangs under it, and the verdict.
void Report(CulledNode& slot, const NiAVObject* face, bool under, bool enabled) {
	if (slot.reported == face) {
		return;
	}
	slot.reported = face;
	char parts[256] = "";
	UInt32 used = 0;
	UInt32 count = 0;
	NiAVObject* const* children = NodeChildren(face, count);
	for (UInt32 i = 0; i < count && i < 16; ++i) {
		const NiAVObject* const child = children[i];
		if (!Readable(child) || !Readable(child->name)) {
			continue;
		}
		for (const char* c = child->name; *c != '\0' && used + 2 < sizeof(parts); ++c) {
			parts[used++] = *c;
		}
		if (used + 2 < sizeof(parts)) {
			parts[used++] = ' ';
		}
		parts[used] = '\0';
	}
	OBVR_LOG("Head cull: the player's FaceGen node \"%s\" (%s) at %08X, %u part(s): %s- %s",
	         Readable(face->name) ? face->name : "?", NiClassNameOf(face), reinterpret_cast<UInt32>(face), count,
	         parts,
	         !enabled ? "drawn ([Body] CullHiddenHead=0), collapsed only"
	         : under  ? "hidden for each world render while the head is collapsed"
	                  : "not under the body shown, left alone");
}

void Release(CulledNode& slot) {
	if (slot.node != nullptr && Readable(slot.node)) {
		slot.node->flags = ReleaseHead(slot.node->flags, slot.ours);
	}
	slot.node = nullptr;
	slot.ours = false;
}

void CullOne(CulledNode& slot, NiAVObject* face, const NiAVObject* bodyRoot, bool enabled) {
	if (face == nullptr) {
		return;
	}
	const bool under = Under(face, bodyRoot);
	Report(slot, face, under, enabled);
	if (!HeadCullWanted(true, enabled, under)) {
		return;
	}
	if (slot.node != face) {
		Release(slot);
	}
	const HeadCullStep step = CullHead(face->flags, slot.node == face && slot.ours);
	face->flags = step.flags;
	slot.node = face;
	slot.ours = step.ours;
}

}  // namespace

void CullPlayerHeadForThisRender(NiAVObject* bodyRoot, NiAVObject* head, bool enabled) {
	CullOne(Slot(0), PlayerFaceNode(), bodyRoot, enabled);
	CullOne(Slot(1), Readable(head) ? BipedFaceUnder(head) : nullptr, bodyRoot, enabled);
}

void ReleasePlayerHeadCull() {
	const bool culled = Slot(0).node != nullptr || Slot(1).node != nullptr;
	for (UInt32 i = 0; i < kFaceNodes; ++i) {
		Release(Slot(i));
	}
	// The harness's evidence: at a mark, whether this render had the head
	// culled, and the hidden bits as the rest of the frame sees them.
	if (test::HandScriptMarkedThisFrame()) {
		const NiAVObject* const skinned = Slot(0).reported;
		const NiAVObject* const biped = Slot(1).reported;
		OBVR_LOG("Head cull: at the mark - the head %s this render; after it the hidden bits skinned %d, biped %d",
		         culled ? "culled" : "drawn", Readable(skinned) ? (skinned->flags & kHeadHiddenBit) : -1,
		         Readable(biped) ? (biped->flags & kHeadHiddenBit) : -1);
	}
}

}  // namespace obvr::game
