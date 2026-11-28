#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/ShieldHoldLogic.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// What the left hand's pin knows after the hold, for the trace
// (game/ArmDrawTrace.h): the twist bone, the player's action, and the locals
// the animation gave before the hold wrote its rest.
struct ShieldHoldNote {
	const NiAVObject* twist = nullptr;  // "Bip01 L ForearmTwist" of this tree, the shield's; null when not found
	SInt32 action = -1;                 // the player's action at this pin (game::ReadPlayerAction)
	bool shieldWorn = false;            // worn, and its model hangs on the twist
	ShieldHoldStep step = ShieldHoldStep::Off;
	NiMatrix33 handAnimated = NiMatrix33::Identity();
	NiMatrix33 twistAnimated = NiMatrix33::Identity();
};

// The worn shield held rigid to the pinned left hand (game/ShieldHoldLogic.h):
// called by the left hand's pin (game/HandBones.cpp PinHandBone) before it
// places the forearm from the hand's local - with a shield worn and the rest
// taken, the hand's and the twist's locals are written to the rest, so the
// forearm, the twist and the shield on it keep their place on the pinned
// hand through any animation of the arm. `root` is the first-person tree the
// hand was found under. No allocation; the twist is looked up once per tree;
// a line only when the rest is taken or the chain is refused (bounded).
ShieldHoldNote HoldShieldOnPinnedHand(const NiAVObject* root, NiAVObject* hand);

}  // namespace obvr::game
