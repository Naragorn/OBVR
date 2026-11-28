#include "game/ShieldHold.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BodyIkLogic.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"

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

struct HoldKeep {
	ShieldHoldState state;
	const NiAVObject* root = nullptr;  // the tree the twist was found under
	NiAVObject* twist = nullptr;
	const NiAVObject* hand = nullptr;
	UInt32 generation = 0;             // the chain's: a new tree or a new hand bone starts afresh
	ShieldHoldStep last = ShieldHoldStep::Off;
	UInt32 restPinsSeen = 0;
	UInt32 lines = 16;
};

HoldKeep& Keep() {
	static HoldKeep k;
	return k;
}

// Whether anything but the skeleton's bones hangs on the twist: the worn
// shield's model ("Shield  (<form>)", OBVR.log 2026-11-28 line 1297, on the
// body's twist; the first-person tree's the same). The shield getter falls
// back to "worn" when it cannot be read (MeleeHits.cpp): this keeps a bare
// arm out of the hold then.
bool CarriesSomething(const NiAVObject* twist) {
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(twist, count);
	if (children == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < count && i < 64; ++i) {
		const NiAVObject* const child = children[i];
		if (!Looks(child)) {
			continue;
		}
		const char* const name = NameOrEmpty(child);
		if (!(name[0] == 'B' && name[1] == 'i' && name[2] == 'p')) {
			return true;
		}
	}
	return false;
}

void Say(HoldKeep& k, ShieldHoldStep step, const NiAVObject* hand, const ShieldHoldInput& in) {
	if (step == k.last) {
		return;
	}
	k.last = step;
	if (k.lines == 0 || (step != ShieldHoldStep::Taken && step != ShieldHoldStep::NotBeside)) {
		return;
	}
	--k.lines;
	if (step == ShieldHoldStep::NotBeside) {
		OBVR_LOG("Shield hold: the shield's bone hangs under \"%s\", the left hand under \"%s\" - not side by side, so "
		         "the shield is left to the animation and may swing on the hand at a draw",
		         NameOrEmpty(k.twist != nullptr ? k.twist->parent : nullptr), NameOrEmpty(hand->parent));
		return;
	}
	const NiPoint3 apart = in.twistLocal.pos - in.handLocal.pos;
	OBVR_LOG("Shield hold: the shield held to the left hand from now on - its place taken after %u pins at rest, the "
	         "twist bone %.1f units from the hand; no animation of the arm moves it on the hand now",
	         kShieldRestPins, static_cast<double>(math::Sqrt(apart.LengthSquared())));
}

}  // namespace

ShieldHoldNote HoldShieldOnPinnedHand(const NiAVObject* root, NiAVObject* hand) {
	HoldKeep& k = Keep();
	ShieldHoldNote note;
	if (root != k.root) {
		k.root = root;
		k.twist = root != nullptr ? FindFirstPersonNode("Bip01 L ForearmTwist") : nullptr;
		++k.generation;
	}
	if (hand != k.hand) {
		k.hand = hand;
		++k.generation;
	}
	NiAVObject* const twist = Looks(k.twist) ? k.twist : nullptr;
	note.twist = twist;
	note.action = ReadPlayerAction();
	note.handAnimated = hand->localTransform.rot;
	ShieldHoldInput in;
	in.handLocal = hand->localTransform;
	if (twist != nullptr) {
		note.twistAnimated = twist->localTransform.rot;
		in.twistLocal = twist->localTransform;
	}
	in.shieldWorn = twist != nullptr && PlayerWearsShield() && CarriesSomething(twist);
	in.beside = twist != nullptr && twist->parent == hand->parent;
	in.bones = k.generation;
	in.atRest = note.action == addr::kActionNone && FeetStand(ReadPlayerMovementFlags());
	const ShieldHoldOut out = StepShieldHold(k.state, in);
	note.shieldWorn = in.shieldWorn;
	note.step = out.step;
	if (out.write) {
		hand->localTransform = out.hand;
		twist->localTransform = out.twist;
	}
	Say(k, out.step, hand, in);
	return note;
}

}  // namespace obvr::game
