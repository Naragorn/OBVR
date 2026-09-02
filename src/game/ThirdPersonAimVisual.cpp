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

NiAVObject* ThirdPersonSpineNode(NiAVObject** rootOut = nullptr) {
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
	NiAVObject* const spine = getObject(root, nullptr, kSpineName);
	if (!LooksLikeObject(spine) || !NameIs(spine->name, kSpineName)) {
		static bool s_spineReported = false;
		if (!s_spineReported) {
			s_spineReported = true;
			OBVR_LOG("Third-person aim visual: %s was not found under node \"%s\" - "
			         "the skeleton is left untouched",
			         kSpineName, root->name);
		}
		return nullptr;
	}
	if (rootOut != nullptr) {
		*rootOut = root;
	}
	return spine;
}

NiMatrix33 g_wrote{};
NiMatrix33 g_base{};
NiAVObject* g_held = nullptr;
bool g_reported = false;

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
	NiAVObject* const parent = spine->parent;
	if (!LooksLikeObject(parent)) {
		static bool s_parentReported = false;
		if (!s_parentReported) {
			s_parentReported = true;
			OBVR_LOG("Third-person aim visual: %s has no readable parent - the skeleton "
			         "is left untouched",
			         kSpineName);
		}
		return false;
	}

	// If Oblivion left last frame's correction standing, remove it before
	// taking this frame's animated pose as the base. If it rewrote the bone,
	// that fresh value already is the base. This is the same self-correcting
	// rule used by FirstPersonArms.
	if (g_held == spine && SameRotation(spine->localTransform.rot, g_wrote)) {
		spine->localTransform.rot = g_base;
	}
	g_base = spine->localTransform.rot;

	const NiMatrix33 bodyCorrection =
		EulerToMatrix(pitchRadians * math::kRadiansToDegrees, 0.0f,
		              yawRadians * math::kRadiansToDegrees);
	// The user's report pinned down the old bug exactly: actor pitch around X
	// became a left/right turn, and actor yaw around Z became up/down. Spine2's
	// parent axes are not the actor's axes. Re-express the desired actor-space
	// correction in the actual parent-bone space before touching the local pose.
	const NiMatrix33 localCorrection =
		RebaseRotation(bodyCorrection, root->worldTransform.rot,
		               parent->worldTransform.rot);
	spine->localTransform.rot = localCorrection * g_base;
	g_wrote = spine->localTransform.rot;
	g_held = spine;

	// The animation update has already propagated world transforms by this
	// point. Re-run that propagation from Spine2 so the render sees the local
	// correction and every arm/weapon child inherits it.
	UpdateNodeTransforms(spine);

	if (!g_reported) {
		g_reported = true;
		OBVR_LOG("Third-person aim visual: node %08X named \"%s\" follows the gaze "
		         "after animation (first correction yaw %.1f, pitch %.1f degrees)",
		         reinterpret_cast<UInt32>(spine), spine->name,
		         static_cast<double>(yawRadians * math::kRadiansToDegrees),
		         static_cast<double>(pitchRadians * math::kRadiansToDegrees));
	}
	return true;
}

void ReleaseThirdPersonAimVisual() {
	if (g_held == nullptr) {
		return;
	}

	// Resolve the currently displayed skeleton again rather than following a
	// stored pointer across a load or race/skeleton replacement. A different or
	// absent node means the old one is no longer the object being drawn.
	NiAVObject* const current = ThirdPersonSpineNode();
	if (current == g_held && SameRotation(current->localTransform.rot, g_wrote)) {
		current->localTransform.rot = g_base;
		UpdateNodeTransforms(current);
	}
	g_held = nullptr;
}

}  // namespace obvr::game
