#include "game/ThirdPersonAimVisual.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"

namespace obvr::game {
namespace {

constexpr const char* kSpineName = "Bip01 Spine2";
constexpr const char* kHeadName = "Bip01 Head";

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

bool SameRotation(const NiMatrix33& a, const NiMatrix33& b) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const float difference = a.data[row][col] - b.data[row][col];
			if (difference > 1.0e-6f || difference < -1.0e-6f) {
				return false;
			}
		}
	}
	return true;
}

NiAVObject* ThirdPersonRoot() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return nullptr;
	}
	return *reinterpret_cast<NiAVObject* const*>(player + addr::kReferenceNodeOffset);
}

NiAVObject* ThirdPersonNamedNode(const char* wantedName, bool& missingReported,
	                            NiAVObject** rootOut = nullptr) {
	NiAVObject* const root = ThirdPersonRoot();
	if (!LooksLikeObject(root)) {
		return nullptr;
	}

	// A node has to identify itself before its method table is trusted. The
	// third-person root is named Player by the very xOBSE path whose GetObject
	// call is the source for the slot below.
	if (!NameLooksReal(root->name)) {
		static bool s_rootReported = false;
		if (!s_rootReported) {
			s_rootReported = true;
			OBVR_LOG("Third-person aim visual: the player node at %08X has no readable name - "
			         "the skeleton is left untouched",
			         reinterpret_cast<UInt32>(root));
		}
		return nullptr;
	}

	auto* const table = static_cast<UInt8*>(root->vtable);
	if (!LooksLikeObject(table)) {
		return nullptr;
	}
	const UInt32 functionAddress = *reinterpret_cast<const UInt32*>(
		table + addr::kNiAVObjectGetObjectVtableOffset);
	if (functionAddress < addr::kTextStart || functionAddress >= addr::kTextEnd) {
		static bool s_methodReported = false;
		if (!s_methodReported) {
			s_methodReported = true;
			OBVR_LOG("Third-person aim visual: GetObject slot +%02X holds %08X, outside "
			         "Oblivion's code - the skeleton is left untouched",
			         addr::kNiAVObjectGetObjectVtableOffset, functionAddress);
		}
		return nullptr;
	}

	// __thiscall with one stack argument. As elsewhere in OBVR, __fastcall
	// plus a dead EDX gives the same register/stack shape on 32-bit x86.
	using GetObjectFn = NiAVObject*(__fastcall*)(NiAVObject* self, void* unusedEdx,
	                                           const char* name);
	const auto getObject = reinterpret_cast<GetObjectFn>(functionAddress);
	NiAVObject* const node = getObject(root, nullptr, wantedName);
	if (!LooksLikeObject(node) || !NameIs(node->name, wantedName)) {
		if (!missingReported) {
			missingReported = true;
			OBVR_LOG("Third-person aim visual: %s was not found under node \"%s\" - "
			         "the skeleton is left untouched",
			         wantedName, root->name);
		}
		return nullptr;
	}
	if (rootOut != nullptr) {
		*rootOut = root;
	}
	return node;
}

NiAVObject* ThirdPersonSpineNode(NiAVObject** rootOut = nullptr) {
	static bool s_missingReported = false;
	return ThirdPersonNamedNode(kSpineName, s_missingReported, rootOut);
}

NiAVObject* ThirdPersonHeadNode(NiAVObject** rootOut = nullptr) {
	static bool s_missingReported = false;
	return ThirdPersonNamedNode(kHeadName, s_missingReported, rootOut);
}

struct BoneWriteState {
	NiMatrix33 wrote{};
	NiMatrix33 base{};
	NiAVObject* held = nullptr;
	bool parentReported = false;
	bool appliedReported = false;
};

BoneWriteState g_spineWrite{};
BoneWriteState g_headWrite{};

bool ApplyVisualCorrection(NiAVObject* node, NiAVObject* root,
	                       const NiMatrix33& actorCorrection,
	                       const char* nodeName, BoneWriteState& state) {
	NiAVObject* const parent = node->parent;
	if (!LooksLikeObject(parent)) {
		if (!state.parentReported) {
			state.parentReported = true;
			OBVR_LOG("Third-person aim visual: %s has no readable parent - the skeleton "
			         "is left untouched",
			         nodeName);
		}
		return false;
	}

	// If Oblivion left last frame's correction standing, remove it before
	// taking this frame's animated pose as the base. Otherwise animation has
	// already supplied a fresh base.
	if (state.held == node && SameRotation(node->localTransform.rot, state.wrote)) {
		node->localTransform.rot = state.base;
	}
	state.base = node->localTransform.rot;

	const NiMatrix33 localCorrection =
		RebaseRotation(actorCorrection, root->worldTransform.rot,
		               parent->worldTransform.rot);
	node->localTransform.rot = localCorrection * state.base;
	state.wrote = node->localTransform.rot;
	state.held = node;
	UpdateNodeTransforms(node);
	return true;
}

void ReleaseVisualCorrection(NiAVObject* current, BoneWriteState& state) {
	if (state.held == nullptr) {
		return;
	}
	if (current == state.held &&
	    SameRotation(current->localTransform.rot, state.wrote)) {
		current->localTransform.rot = state.base;
		UpdateNodeTransforms(current);
	}
	state.held = nullptr;
}

}  // namespace

bool TurnThirdPersonAimVisual(float yawRadians, float pitchRadians) {
	NiAVObject* root = nullptr;
	NiAVObject* const spine = ThirdPersonSpineNode(&root);
	if (spine == nullptr) {
		// Keep the identity only as a comparison token. It is never dereferenced:
		// a later successful lookup can still recognise and undo our last write,
		// while Release resolves the currently displayed skeleton afresh.
		return false;
	}

	const NiMatrix33 bodyCorrection =
		EulerToMatrix(pitchRadians * math::kRadiansToDegrees, 0.0f,
		              yawRadians * math::kRadiansToDegrees);
	// The user's report pinned down the old bug exactly: actor pitch around X
	// became a left/right turn, and actor yaw around Z became up/down. Spine2's
	// parent axes are not the actor's axes. Re-express the desired actor-space
	// correction in the actual parent-bone space before touching the local pose.
	if (!ApplyVisualCorrection(spine, root, bodyCorrection, kSpineName,
	                           g_spineWrite)) {
		return false;
	}

	if (!g_spineWrite.appliedReported) {
		g_spineWrite.appliedReported = true;
		OBVR_LOG("Third-person aim visual: node %08X named \"%s\" follows the gaze "
		         "after animation (first correction yaw %.1f, pitch %.1f degrees)",
		         reinterpret_cast<UInt32>(spine), spine->name,
		         static_cast<double>(yawRadians * math::kRadiansToDegrees),
		         static_cast<double>(pitchRadians * math::kRadiansToDegrees));
	}
	return true;
}

void ReleaseThirdPersonAimVisual() {
	if (g_spineWrite.held == nullptr) {
		return;
	}

	// Resolve the currently displayed skeleton again rather than following a
	// stored pointer across a load or race/skeleton replacement. A different or
	// absent node means the old one is no longer the object being drawn.
	NiAVObject* const current = ThirdPersonSpineNode();
	ReleaseVisualCorrection(current, g_spineWrite);
}

bool TurnThirdPersonHeadVisual(float fullYawRadians, float fullPitchRadians,
	                           float bodyYawRadians, float bodyPitchRadians) {
	NiAVObject* root = nullptr;
	NiAVObject* const head = ThirdPersonHeadNode(&root);
	if (head == nullptr) {
		return false;
	}

	const NiMatrix33 fullCorrection =
		EulerToMatrix(fullPitchRadians * math::kRadiansToDegrees, 0.0f,
		              fullYawRadians * math::kRadiansToDegrees);
	const NiMatrix33 bodyCorrection =
		EulerToMatrix(bodyPitchRadians * math::kRadiansToDegrees, 0.0f,
		              bodyYawRadians * math::kRadiansToDegrees);
	// Spine2 is an ancestor of Head. If it already carries B and the requested
	// complete actor-space gaze is F, the head must add R = F * inverse(B):
	// R * B = F. This stays exact when pitch and yaw are combined.
	const NiMatrix33 remainingCorrection =
		fullCorrection * InverseRotation(bodyCorrection);
	if (!ApplyVisualCorrection(head, root, remainingCorrection, kHeadName,
	                           g_headWrite)) {
		return false;
	}

	if (!g_headWrite.appliedReported) {
		g_headWrite.appliedReported = true;
		OBVR_LOG("Third-person head visual: node %08X named \"%s\" follows the HMD "
		         "after animation",
		         reinterpret_cast<UInt32>(head), head->name);
	}
	return true;
}

void ReleaseThirdPersonHeadVisual() {
	if (g_headWrite.held == nullptr) {
		return;
	}
	NiAVObject* const current = ThirdPersonHeadNode();
	ReleaseVisualCorrection(current, g_headWrite);
}

}  // namespace obvr::game
