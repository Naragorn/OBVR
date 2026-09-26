#include "game/HandGrip.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

constexpr UInt32 kMaxFingerLinks = 24;

// One hand's fingers while they are closed: the links found under the hand
// bone and the rotations they had when the hold began.
struct Grip {
	const NiAVObject* root = nullptr;  // the first-person root they were found under
	NiAVObject* hand = nullptr;
	NiAVObject* links[kMaxFingerLinks] = {};
	NiMatrix33 base[kMaxFingerLinks];
	UInt32 count = 0;
	bool closed = false;
	bool reported = false;
};

Grip g_grips[2];

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

const char* NameOf(const NiAVObject* node) {
	return LooksLikeObject(node->name) ? node->name : "";
}

void Open(Grip& grip) {
	for (UInt32 i = 0; i < grip.count; ++i) {
		if (LooksLikeObject(grip.links[i])) {
			grip.links[i]->localTransform.rot = grip.base[i];
		}
	}
	if (LooksLikeObject(grip.hand)) {
		UpdateNodeTransforms(grip.hand);
	}
	grip.closed = false;
}

}  // namespace

void ForgetHandGrip() {
	g_grips[0] = Grip{};
	g_grips[1] = Grip{};
}

void StepHandGrip(bool rightHand, const char* handBoneName, bool closed, float curlDegrees) {
	Grip& grip = g_grips[rightHand ? 0 : 1];
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr || grip.root != root) {
		// A new model: the old links went with the old tree, never written.
		const bool wasReported = grip.reported;
		grip = Grip{};
		grip.root = root;
		grip.reported = wasReported;
		if (root == nullptr) {
			return;
		}
	}
	if (!closed) {
		if (grip.closed) {
			Open(grip);
		}
		return;
	}
	if (!grip.closed) {
		grip.hand = FindFirstPersonNode(handBoneName);
		grip.count = grip.hand != nullptr
		                 ? CollectNodesContaining(grip.hand, "Finger", grip.links, kMaxFingerLinks)
		                 : 0;
		for (UInt32 i = 0; i < grip.count; ++i) {
			grip.base[i] = grip.links[i]->localTransform.rot;
		}
		grip.closed = true;
		if (!grip.reported) {
			grip.reported = true;
			char names[256];
			UInt32 at = 0;
			for (UInt32 i = 0; i < grip.count && at + 40 < sizeof(names); ++i) {
				const char* name = NameOf(grip.links[i]);
				if (at > 0) {
					names[at++] = ',';
				}
				for (UInt32 c = 0; name[c] != '\0' && c < 32; ++c) {
					names[at++] = name[c];
				}
			}
			names[at] = '\0';
			OBVR_LOG("Hands: the %s hand closes around what it holds - %u finger links under "
			         "\"%s\" (%s), %.0f degrees each",
			         rightHand ? "right" : "left", grip.count, handBoneName, names,
			         static_cast<double>(curlDegrees));
		}
	}
	if (grip.count == 0) {
		return;
	}
	for (UInt32 i = 0; i < grip.count; ++i) {
		if (LooksLikeObject(grip.links[i])) {
			grip.links[i]->localTransform.rot =
				CurledAboutZ(grip.base[i], FingerCurlDegrees(NameOf(grip.links[i]), curlDegrees));
		}
	}
	UpdateNodeTransforms(grip.hand);
}

}  // namespace obvr::game
