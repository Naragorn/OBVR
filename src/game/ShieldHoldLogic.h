#pragma once

// The worn shield held rigid to the pinned left hand (Nadi, headset
// 2026-11-26/27/28: "schilde wackeln noch in der hand" - the shield wobbles
// in the hand at a draw or a sheathe; free-weapons-spec, "The shield held to
// the hand").
//
// [R] The chain (the "Arm draw trace" lines of 2026-11-27/28, "the hand under
// Bip01 L Forearm, the twist under Bip01 L Forearm"): the left hand bone and
// the forearm's twist bone, which the shield hangs on, are both children of
// "Bip01 L Forearm". The pin (game/HandBones.cpp PinHandBone) places the
// forearm so the hand, with the local the animation gave it, lands on the
// controller: forearm = wanted * inverse(handLocal). So the twist - and the
// shield - sit at wanted * inverse(handLocal) * twistLocal: wherever the
// animation turns the wrist or the twist, the shield swings round a hand that
// stays on the controller. [M] Through a sheathe the animation turned the
// hand's local up to 73.9 degrees a frame and the twist's 79.3 with the
// controller still (0.2), the shield 5.9 units a frame, 9.2 from where it
// began (OBVR.log 2026-11-28 line 21985).
//
// The rule: with a shield worn, the hand's and the twist's locals are held
// at a rest taken from the animation while the arm stood at rest (the
// player's action none, no step asked, for kShieldRestPins pins in a row) -
// every animation of that arm after that (the draw, the sheathe, an attack,
// a block, a stagger, the idle's sway) leaves the forearm, the twist and the
// shield where the pinned hand puts them, with the offset they had at rest.
// The hand's world is the pin's either way; only the forearm behind it and
// the twist beside it stop swinging. Held in locals, so the left-handed
// mirror (carried by the skeleton's root) changes nothing. Each local keeps
// the animation's scale (the bare-wrist taper's, which never runs with a
// shield on the forearm - game/BonePin.h ForearmScaleFor).
//
// Refused: no shield worn (the animation's, and the rest forgotten: the next
// one worn takes its own); the twist bone not beside the hand under one
// parent (another skeleton: the hold's arithmetic is the siblings'); a new
// tree or new bones (a load, a race change) start afresh. No pin, no step:
// third person, a menu with the hands away, a pin that did not run keep the
// rest for the next pin. Waiting for the rest, the animation's locals as
// before.
//
// Pure; shield_hold_test. Applied by game/ShieldHold.cpp inside the left
// hand's pin, before the forearm is placed.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Pins in a row at rest before the rest is taken: about half a second at 72
// to 90 Hz, as the trunk's rest waits half a second of standing idle
// (game/BodyIk.h) - a draw's or a sheathe's blend out is over by then.
inline constexpr UInt32 kShieldRestPins = 36;

enum class ShieldHoldStep : UInt8 {
	Off,        // no shield worn: the animation's locals
	NotBeside,  // the twist bone is not the hand's sibling: the animation's
	Waiting,    // no rest yet: the animation's locals
	Taken,      // the rest taken at this pin (the animation's, held from now on)
	Held,       // the rest written
};

struct ShieldHoldInput {
	bool shieldWorn = false;
	bool beside = false;      // the twist bone found under the hand's own parent
	UInt32 bones = 0;         // the chain's generation (the tree, the hand, the twist); another starts afresh
	bool atRest = false;      // the player's action none and no step asked
	NiTransform handLocal{};  // the hand bone's local as the animation left it
	NiTransform twistLocal{};
};

struct ShieldHoldState {
	UInt32 bones = 0;
	UInt32 restPins = 0;  // pins in a row at rest while waiting
	bool have = false;
	NiTransform hand{};   // the rest
	NiTransform twist{};
};

struct ShieldHoldOut {
	ShieldHoldStep step = ShieldHoldStep::Off;
	bool write = false;   // the locals below are to be written
	NiTransform hand{};
	NiTransform twist{};
};

// A rest's turn and place with the animation's scale of this frame.
inline NiTransform ShieldHoldLocal(const NiTransform& rest, float scale) {
	NiTransform t = rest;
	t.scale = scale;
	return t;
}

// One left-hand pin: what its hand and twist locals are to be.
inline ShieldHoldOut StepShieldHold(ShieldHoldState& s, const ShieldHoldInput& in) {
	ShieldHoldOut out;
	if (!in.shieldWorn || !in.beside) {
		s = ShieldHoldState{};
		out.step = in.shieldWorn ? ShieldHoldStep::NotBeside : ShieldHoldStep::Off;
		return out;
	}
	if (s.bones != in.bones) {
		s = ShieldHoldState{};
		s.bones = in.bones;
	}
	if (!s.have) {
		s.restPins = in.atRest ? s.restPins + 1 : 0;
		if (s.restPins < kShieldRestPins) {
			out.step = ShieldHoldStep::Waiting;
			return out;
		}
		s.have = true;
		s.hand = in.handLocal;
		s.twist = in.twistLocal;
		out.step = ShieldHoldStep::Taken;
	} else {
		out.step = ShieldHoldStep::Held;
	}
	out.write = true;
	out.hand = ShieldHoldLocal(s.hand, in.handLocal.scale);
	out.twist = ShieldHoldLocal(s.twist, in.twistLocal.scale);
	return out;
}

}  // namespace obvr::game
