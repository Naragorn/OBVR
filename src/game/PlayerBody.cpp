#include "game/PlayerBody.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BodyPlacement.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

constexpr const char* kHeadName = "Bip01 Head";
constexpr const char* kLeftClavicleName = "Bip01 L Clavicle";
constexpr const char* kRightClavicleName = "Bip01 R Clavicle";
constexpr UInt16 kHiddenBit = 0x0001;

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

bool NameIs(const char* actual, const char* expected) {
	if (!LooksLikeObject(actual) || expected == nullptr) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char a = actual[at];
		const char e = expected[at];
		if (a != e) {
			return false;
		}
		if (a == '\0') {
			return at > 0;
		}
	}
	return false;
}

bool NameLooksReal(const char* name) {
	if (!LooksLikeObject(name)) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const unsigned char value = static_cast<unsigned char>(name[at]);
		if (value == '\0') {
			return at > 0;
		}
		if (value < 0x20 || value > 0x7E) {
			return false;
		}
	}
	return false;
}

// One log line per reason, so a skeleton that cannot be reached says so once
// and a later one that can is not silenced by it.
struct Reported {
	bool root = false;
	bool method = false;
	bool head = false;
	bool leftClavicle = false;
	bool rightClavicle = false;
	bool parent = false;
	bool shown = false;
};
Reported g_reported{};

// The root this code took the hidden bit off, so it can be put back on that
// node and no other. A load or a skeleton swap makes a different root, and
// the old one is never dereferenced again - only compared.
NiAVObject* g_unhiddenRoot = nullptr;

NiAVObject* ThirdPersonRoot() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return nullptr;
	}
	NiAVObject* const root =
		*reinterpret_cast<NiAVObject* const*>(player + addr::kReferenceNodeOffset);
	if (!LooksLikeObject(root)) {
		return nullptr;
	}
	// The node has to identify itself before its method table is trusted;
	// the same check ThirdPersonAimVisual makes of the same node.
	if (!NameLooksReal(root->name)) {
		if (!g_reported.root) {
			g_reported.root = true;
			OBVR_LOG("Body: the player node at %08X has no readable name - the body "
			         "stays as the engine has it",
			         reinterpret_cast<UInt32>(root));
		}
		return nullptr;
	}
	return root;
}

// NiAVObject::GetObject through the root's vtable, as ThirdPersonAimVisual
// does it: the slot has to point into Oblivion's code, and the node that
// comes back has to carry the name that was asked for.
NiAVObject* NamedNode(NiAVObject* root, const char* wantedName, bool& missingReported) {
	auto* const table = static_cast<UInt8*>(root->vtable);
	if (!LooksLikeObject(table)) {
		return nullptr;
	}
	const UInt32 functionAddress =
		*reinterpret_cast<const UInt32*>(table + addr::kNiAVObjectGetObjectVtableOffset);
	if (functionAddress < addr::kTextStart || functionAddress >= addr::kTextEnd) {
		if (!g_reported.method) {
			g_reported.method = true;
			OBVR_LOG("Body: GetObject slot +%02X holds %08X, outside Oblivion's code - the "
			         "body stays as the engine has it",
			         addr::kNiAVObjectGetObjectVtableOffset, functionAddress);
		}
		return nullptr;
	}
	using GetObjectFn = NiAVObject*(__fastcall*)(NiAVObject* self, void* unusedEdx,
	                                           const char* name);
	const auto getObject = reinterpret_cast<GetObjectFn>(functionAddress);
	NiAVObject* const node = getObject(root, nullptr, wantedName);
	if (!LooksLikeObject(node) || !NameIs(node->name, wantedName)) {
		if (!missingReported) {
			missingReported = true;
			OBVR_LOG("Body: %s was not found under node \"%s\" - that part is left as "
			         "the engine has it",
			         wantedName, root->name);
		}
		return nullptr;
	}
	return node;
}

// Collapses a node for this frame's draw: its world transform is recomputed
// at scale zero, so everything skinned to it and everything hanging under it
// lands on one point, and then the LOCAL scale is put back so nothing the
// engine or a menu reads off the local transform later sees a zero. This is
// Enhanced Camera's UpdateSkeletonNodes(0) / ApplyAnimData /
// UpdateSkeletonNodes(1) sequence, with the update pass in the middle.
void CollapseForThisDraw(NiAVObject* node) {
	const float saved = node->localTransform.scale;
	node->localTransform.scale = 0.0f;
	UpdateNodeTransforms(node);
	node->localTransform.scale = saved;
}

}  // namespace

bool ShowPlayerBody(const BodyFrame& frame) {
	NiAVObject* const root = ThirdPersonRoot();
	if (root == nullptr) {
		return false;
	}

	// The engine hides the whole body in first person with the bit it tests
	// at 0x00664FC2; taking it off is what makes the body draw. Remembered so
	// Release can put it back on this very node.
	if ((root->flags & kHiddenBit) != 0) {
		root->flags = static_cast<UInt16>(root->flags & ~kHiddenBit);
		g_unhiddenRoot = root;
	}

	NiAVObject* const head = NamedNode(root, kHeadName, g_reported.head);
	if (head == nullptr) {
		return false;
	}

	// Stand the root where the head lands under the headset. Written under
	// the root's parent, as every node write in OBVR is, and propagated with
	// the engine's own pass so the whole skeleton follows.
	NiAVObject* const parent = root->parent;
	if (!LooksLikeObject(parent)) {
		if (!g_reported.parent) {
			g_reported.parent = true;
			OBVR_LOG("Body: node \"%s\" has no readable parent - the body is shown where "
			         "the engine has it, not under the headset",
			         root->name);
		}
	} else {
		BodyRootInput in;
		in.cameraWorld = frame.cameraWorld;
		in.headWorld = head->worldTransform.pos;
		in.rootWorld = root->worldTransform.pos;
		in.rootWorldRot = root->worldTransform.rot;
		in.rootWorldScale = root->worldTransform.scale;
		in.eyeOffsetUnits = frame.eyeOffsetUnits;
		in.parentRot = parent->worldTransform.rot;
		in.parentPos = parent->worldTransform.pos;
		in.parentScale = parent->worldTransform.scale;
		root->localTransform.pos = BodyRootLocalPos(in);
		UpdateNodeTransforms(root);
	}

	// The head and the animated arms are collapsed AFTER the move, so the
	// zero-scale world transforms are the ones the draw sees.
	if (frame.hideHead) {
		CollapseForThisDraw(head);
	}
	if (frame.hideArms) {
		if (NiAVObject* const left = NamedNode(root, kLeftClavicleName, g_reported.leftClavicle)) {
			CollapseForThisDraw(left);
		}
		if (NiAVObject* const right =
		        NamedNode(root, kRightClavicleName, g_reported.rightClavicle)) {
			CollapseForThisDraw(right);
		}
	}

	if (!g_reported.shown) {
		g_reported.shown = true;
		OBVR_LOG("Body: the third-person skeleton \"%s\" at %08X is shown in first person, "
		         "its root moved under the headset each frame (eyes %.1f forward, %.1f up "
		         "from %s), head %s, clavicles %s",
		         root->name, reinterpret_cast<UInt32>(root),
		         static_cast<double>(frame.eyeOffsetUnits.y),
		         static_cast<double>(frame.eyeOffsetUnits.z), kHeadName,
		         frame.hideHead ? "collapsed" : "kept", frame.hideArms ? "collapsed" : "kept");
	}
	return true;
}

void ReleasePlayerBody(bool isThirdPerson) {
	if (g_unhiddenRoot == nullptr) {
		return;
	}
	NiAVObject* const root = ThirdPersonRoot();
	if (root == g_unhiddenRoot && !isThirdPerson) {
		root->flags = static_cast<UInt16>(root->flags | kHiddenBit);
	}
	g_unhiddenRoot = nullptr;
}

bool InstallPlayerBodyPatches() {
	static const UInt8 kJumpA[addr::kPovSwitchSkipBodyPatchSize] = {0x0F, 0x84, 0x33, 0x02, 0x00, 0x00};
	static const UInt8 kJumpB[addr::kPovSwitchSkipBodyPatchSize] = {0x0F, 0x84, 0xFE, 0x01, 0x00, 0x00};
	static const UInt8 kNops[addr::kPovSwitchSkipBodyPatchSize] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

	const bool aIsJump = mem::Verify(addr::kPovSwitchSkipBodyA, kJumpA, addr::kPovSwitchSkipBodyPatchSize);
	const bool bIsJump = mem::Verify(addr::kPovSwitchSkipBodyB, kJumpB, addr::kPovSwitchSkipBodyPatchSize);
	const bool aIsNops = mem::Verify(addr::kPovSwitchSkipBodyA, kNops, addr::kPovSwitchSkipBodyPatchSize);
	const bool bIsNops = mem::Verify(addr::kPovSwitchSkipBodyB, kNops, addr::kPovSwitchSkipBodyPatchSize);
	if (aIsNops && bIsNops) {
		// Enhanced Camera itself, or an earlier OBVR load, already did this.
		OBVR_LOG("Body: the POV switch already runs the third-person body's setup in first "
		         "person (both jumps at %08X and %08X are NOPs)",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		return true;
	}
	if (!aIsJump || !bIsJump) {
		OBVR_LOG("Body: bytes at %08X or %08X are not the expected jumps, the POV switch is "
		         "left alone - the body may be missing after a load in first person until "
		         "the view is switched once",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		mem::ReportForeignCode("Body", aIsJump ? addr::kPovSwitchSkipBodyB : addr::kPovSwitchSkipBodyA);
		return false;
	}
	if (!mem::SafeWrite(addr::kPovSwitchSkipBodyA, kNops, addr::kPovSwitchSkipBodyPatchSize) ||
	    !mem::SafeWrite(addr::kPovSwitchSkipBodyB, kNops, addr::kPovSwitchSkipBodyPatchSize)) {
		OBVR_LOG("Body: writing the POV switch patches at %08X / %08X failed",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		return false;
	}
	OBVR_LOG("Body: the POV switch now runs the third-person body's setup in first person "
	         "too (jumps at %08X and %08X replaced with NOPs, after Enhanced Camera)",
	         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
	return true;
}

}  // namespace obvr::game
