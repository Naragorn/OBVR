#include "game/ArmDrawTrace.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ArmDrawTraceLogic.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/SheathHoldLogic.h"
#include "game/ShieldHold.h"

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
	UInt32 shieldDrawLines = 12;   // the draw and the sheathe with a shield worn: their own budget
	UInt32 shieldOtherLines = 12;  // any other action with a shield worn
	UInt32 otherLines = 4;         // any action without one
};

TraceKeep& Keep() {
	static TraceKeep k;
	return k;
}

void Say(const ArmDrawTrace& t, const NiAVObject* hand, const NiAVObject* twist) {
	TraceKeep& k = Keep();
	UInt32& lines = !t.shield ? k.otherLines : SheathStillFor(t.action) ? k.shieldDrawLines : k.shieldOtherLines;
	if (lines == 0) {
		return;
	}
	--lines;
	OBVR_LOG("Arm draw trace: through the %s (a shield worn %d), %u pins, %u with the shield held to the hand (%u "
	         "frames without one): the twist bone (the shield's) moved against the pinned left hand at most %.2f units "
	         "and %.2f degrees a frame (pin %u) and %.2f from where it began - %s; a frame turned the hand's local (the "
	         "animation's) at most %.1f degrees, the twist's %.1f, the controller %.1f in the camera's frame - the hand "
	         "under \"%s\", the twist under \"%s\"",
	         ArmActionName(t.action), t.shield ? 1 : 0, t.frames, t.heldPins, t.skipped,
	         static_cast<double>(t.mostStep), static_cast<double>(t.mostTurn), t.mostStepAt,
	         static_cast<double>(t.mostAway), ArmShieldStill(t) ? "held still on the hand" : "SWUNG on the hand",
	         static_cast<double>(t.mostHand), static_cast<double>(t.mostTwist), static_cast<double>(t.mostController),
	         NameOrEmpty(Looks(hand) ? hand->parent : nullptr), NameOrEmpty(Looks(twist) ? twist->parent : nullptr));
}

}  // namespace

void TraceLeftArmInDraw(const NiAVObject* hand, const ShieldHoldNote& note, const NiMatrix33& cameraRot,
                        const NiMatrix33& handRot, const NiPoint3& handPos, UInt32 pinFrame) {
	TraceKeep& k = Keep();
	ArmDrawSample s;
	s.inDraw = note.action != addr::kActionNone;
	if (!s.inDraw && !k.trace.running) {
		return;
	}
	s.action = note.action;
	s.shield = s.inDraw && note.shieldWorn;
	s.held = note.step == ShieldHoldStep::Held || note.step == ShieldHoldStep::Taken;
	s.pinFrame = pinFrame;
	s.handLocal = note.handAnimated;
	s.controller = InverseMatrix(cameraRot) * handRot;
	const NiAVObject* const twist = note.twist;
	s.twistValid = Looks(twist);
	if (s.twistValid) {
		const NiMatrix33 back = InverseMatrix(handRot);
		s.twistLocal = note.twistAnimated;
		s.twistInHand = back * (twist->worldTransform.pos - handPos);
		s.twistInHandRot = back * twist->worldTransform.rot;
	}
	ArmDrawTrace done;
	if (StepArmDrawTrace(k.trace, s, done)) {
		Say(done, hand, twist);
	}
}

}  // namespace obvr::game
