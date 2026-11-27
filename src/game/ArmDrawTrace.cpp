#include "game/ArmDrawTrace.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ArmDrawTraceLogic.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/SheathHoldLogic.h"

namespace obvr::game {
namespace {

bool Looks(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

const char* NameOrEmpty(const NiAVObject* node) {
	const char* const name = Looks(node) ? node->name : nullptr;
	if (!Looks(name)) {
		return "";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		if (name[at] == '\0') {
			return name;
		}
		if (name[at] < 0x20 || name[at] > 0x7E) {
			return "";
		}
	}
	return "";
}

struct TraceKeep {
	ArmDrawTrace trace;
	const NiAVObject* root = nullptr;  // the first-person tree the twist was found under
	const NiAVObject* twist = nullptr;
	UInt32 shieldLines = 12;  // windows with a shield worn: their own budget
	UInt32 otherLines = 4;
};

TraceKeep& Keep() {
	static TraceKeep k;
	return k;
}

void Say(const ArmDrawTrace& t, const NiAVObject* hand, const NiAVObject* twist) {
	TraceKeep& k = Keep();
	UInt32& lines = t.shield ? k.shieldLines : k.otherLines;
	if (lines == 0) {
		return;
	}
	--lines;
	OBVR_LOG("Arm draw trace: through the %s (a shield worn %d), %u pins (%u frames without one): the twist bone "
	         "(the shield's) moved against the pinned left hand at most %.1f units a frame (pin %u) and %.1f from "
	         "where it began; a frame turned the hand's local at most %.1f degrees, the twist's %.1f, the "
	         "controller %.1f in the camera's frame - the hand under \"%s\", the twist under \"%s\"",
	         t.sheathe ? "sheathe" : "draw", t.shield ? 1 : 0, t.frames, t.skipped, static_cast<double>(t.mostStep),
	         t.mostStepAt, static_cast<double>(t.mostAway), static_cast<double>(t.mostHand),
	         static_cast<double>(t.mostTwist), static_cast<double>(t.mostController),
	         NameOrEmpty(Looks(hand) ? hand->parent : nullptr), NameOrEmpty(Looks(twist) ? twist->parent : nullptr));
}

}  // namespace

void TraceLeftArmInDraw(const NiAVObject* hand, const NiMatrix33& cameraRot, const NiMatrix33& handRot,
                        const NiPoint3& handPos, UInt32 pinFrame) {
	TraceKeep& k = Keep();
	const SInt32 action = ReadPlayerAction();
	ArmDrawSample s;
	s.inDraw = SheathStillFor(action);
	if (!s.inDraw && !k.trace.running) {
		return;
	}
	const NiAVObject* const root = FirstPersonArmsNode();
	if (root != k.root) {
		k.root = root;
		k.twist = root != nullptr ? FindFirstPersonNode("Bip01 L ForearmTwist") : nullptr;
	}
	s.sheathe = action == kSheathActionUnequip;
	s.shield = s.inDraw && PlayerWearsShield();
	s.pinFrame = pinFrame;
	s.handLocal = Looks(hand) ? hand->localTransform.rot : NiMatrix33::Identity();
	s.controller = InverseMatrix(cameraRot) * handRot;
	s.twistValid = Looks(k.twist);
	if (s.twistValid) {
		s.twistLocal = k.twist->localTransform.rot;
		s.twistInHand = InverseMatrix(handRot) * (k.twist->worldTransform.pos - handPos);
	}
	ArmDrawTrace done;
	if (StepArmDrawTrace(k.trace, s, done)) {
		Say(done, hand, k.twist);
	}
}

}  // namespace obvr::game
