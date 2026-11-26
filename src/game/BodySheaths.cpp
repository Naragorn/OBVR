#include "game/BodySheaths.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BodyIkLogic.h"
#include "game/BoneWrite.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/NodeName.h"
#include "game/PlayerAim.h"
#include "game/SheathHoldLogic.h"
#include "game/SheathTrace.h"
#include "game/TransformCompare.h"

namespace obvr::game {
namespace {

enum : UInt32 { kSheathSide = 0, kSheathBack, kSheathQuiver, kSheathNodes };
const char* const kSheathNames[kSheathNodes] = {"SideWeapon", "BackWeapon", "Quiver"};
constexpr float kSheathAxisSlackDegrees = 30.0f;

struct SheathState {
	BoneWrite write[kSheathNodes];
	SheathHoldState hold[kSheathNodes];
	bool mirroredReported[kSheathNodes] = {};
	bool refusedReported[kSheathNodes] = {};
};

SheathState& State() {
	static SheathState s;
	return s;
}

bool LooksLikeObject(const void* pointer) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer)); }

// The right and the left bone a sheath's parent is judged against: the
// thighs for the pelvis, the clavicles for Spine2. False for any other
// parent.
bool SheathParentSides(const BodySheathBones& b, const NiAVObject* parent, NiPoint3& right, NiPoint3& left) {
	const NiAVObject* r = nullptr;
	const NiAVObject* l = nullptr;
	if (parent != nullptr && parent == b.pelvis) {
		r = b.thighRight;
		l = b.thighLeft;
	} else if (parent != nullptr && parent == b.spine2) {
		r = b.clavicleRight;
		l = b.clavicleLeft;
	}
	if (r == nullptr || l == nullptr) {
		return false;
	}
	right = r->worldTransform.pos;
	left = l->worldTransform.pos;
	return true;
}

float SheathRightOf(const NiAVObject* parent, const NiPoint3& localPos, const NiPoint3& right, const NiPoint3& left) {
	return RightOfAcross(parent->worldTransform.rot, parent->worldTransform.scale, localPos, right, left);
}

// The node's local mirrored, when its parent reads as the body's middle;
// false (said once) when it does not.
bool WriteMirrored(SheathState& s, const BodySheathBones& b, UInt32 i, NiAVObject* node, const NiTransform& local) {
	NiPoint3 right{};
	NiPoint3 left{};
	NiAVObject* const parent = node->parent;
	const bool sides = LooksLikeObject(parent) && SheathParentSides(b, parent, right, left);
	if (!sides || !LateralAxisAgrees(parent->worldTransform.rot, right, left, kSheathAxisSlackDegrees)) {
		if (!s.refusedReported[i]) {
			s.refusedReported[i] = true;
			OBVR_LOG("Body: the sheath \"%s\" left where the game hangs it - %s", kSheathNames[i],
			         !sides ? "its parent is neither the pelvis nor Spine2, or their side bones are missing"
			                : "its parent's z is not the body's left-right (a skeleton built otherwise)");
		}
		return false;
	}
	const NiTransform mirrored = MirroredAcrossLocalZ(local);
	WriteLocal(s.write[i], mirrored);
	if (!s.mirroredReported[i]) {
		s.mirroredReported[i] = true;
		const float before = SheathRightOf(parent, local.pos, right, left);
		const float after = SheathRightOf(parent, mirrored.pos, right, left);
		OBVR_LOG("Body: the sheath \"%s\" mirrored for the left hand - under \"%s\" it sat %.1f units to "
		         "the body's %s, now %.1f to its %s",
		         kSheathNames[i], node_name::SafeName(parent), static_cast<double>(before < 0.0f ? -before : before),
		         before < 0.0f ? "left" : "right", static_cast<double>(after < 0.0f ? -after : after),
		         after < 0.0f ? "left" : "right");
	}
	return true;
}

}  // namespace

void StepSheaths(const BodySheathBones& bones, bool side, bool back, bool quiver, float dtSeconds) {
	if (!LooksLikeObject(bones.root)) {
		return;
	}
	SheathState& s = State();
	NiAVObject* nodes[kSheathNodes] = {};
	FindNodesNamed(bones.root, kSheathNames, kSheathNodes, nodes);
	const bool want[kSheathNodes] = {side, back, quiver};
	// The draw's and the sheathe's groups turn the sheath nodes; the weapon
	// hangs there from the sheathe's start (game/SheathHoldLogic.h).
	const SInt32 action = ReadPlayerAction();
	const bool still = SheathStillFor(action);
	for (UInt32 i = 0; i < kSheathNodes; ++i) {
		UInt32 kept = 0;
		UInt32 renewed = 0;
		const bool restored = BeginWrite(s.write[i], nodes[i], kept, renewed);
		NiAVObject* const node = nodes[i];
		if (node == nullptr) {
			continue;
		}
		const NiTransform local =
			StepSheathHold(s.hold[i], reinterpret_cast<UInt32>(node), s.write[i].base, still, dtSeconds);
		bool wrote = want[i] && WriteMirrored(s, bones, i, node, local);
		if (!wrote && !SameTransform(local, s.write[i].base)) {
			WriteLocal(s.write[i], local);
			wrote = true;
		}
		if (wrote || restored) {
			UpdateNodeTransforms(node);
		}
	}
	TraceSheaths(bones.root, nodes, want, action);  // game/SheathTrace.h
}

void GiveBackSheathsAfterDraw() {
	for (BoneWrite& w : State().write) {
		GiveBackAfterDraw(w);
	}
}

bool SheathsHeld() {
	for (const BoneWrite& w : State().write) {
		if (w.held) {
			return true;
		}
	}
	return false;
}

void ReleaseSheaths(NiAVObject* rootNow) {
	SheathState& s = State();
	NiAVObject* sheathsNow[kSheathNodes] = {};
	if (LooksLikeObject(rootNow)) {
		FindNodesNamed(rootNow, kSheathNames, kSheathNodes, sheathsNow);
	}
	for (UInt32 i = 0; i < kSheathNodes; ++i) {
		if (s.write[i].held && ReleaseWrite(s.write[i], sheathsNow[i]) && sheathsNow[i] != nullptr) {
			UpdateNodeTransforms(sheathsNow[i]);
		}
		s.write[i] = BoneWrite{};
		s.hold[i] = SheathHoldState{};
	}
}

}  // namespace obvr::game
