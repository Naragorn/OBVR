#pragma once

// Fitting the weapon places to the body (docs/controls-spec.md 4.1, 4.2): a
// guided run started from the settings. First the melee weapon's place, where
// the weapon hand is when its trigger is pulled; then the bow's, where the
// other hand is when its trigger is pulled. Either menu button cancels and
// nothing changes. The places are taken in the body's frame
// (vr::BodyRelative), and left-handed they are stored mirrored, so the
// settings always describe a right-handed body (StepHolster mirrors them
// back).
//
// Pure, covered by holster_fit_test.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

enum class HolsterFitStep : UInt8 { Idle, Melee, Bow };

struct HolsterFitState {
	HolsterFitStep step = HolsterFitStep::Idle;
	bool weaponTriggerWas = true;  // a trigger held when the run starts is not a press
	bool otherTriggerWas = true;
	NiPoint3 melee{0.0f, 0.0f, 0.0f};
};

struct HolsterFitInput {
	bool start = false;          // the settings row fired
	bool weaponValid = false;    // the weapon hand's controller
	bool otherValid = false;
	NiPoint3 weaponRelative{0.0f, 0.0f, 0.0f};  // BodyRelative
	NiPoint3 otherRelative{0.0f, 0.0f, 0.0f};
	bool weaponTrigger = false;
	bool otherTrigger = false;
	bool cancel = false;         // a menu button, this frame's press
	bool leftHanded = false;
};

struct HolsterFitVerdict {
	bool active = false;     // the run is on: the panel shows, the triggers reach nothing else
	HolsterFitStep step = HolsterFitStep::Idle;
	bool stepChanged = false;  // the panel's text changes
	bool finished = false;   // both places taken this frame: save them
	bool cancelled = false;
	NiPoint3 melee{0.0f, 0.0f, 0.0f};  // right-handed body frame, when finished
	NiPoint3 bow{0.0f, 0.0f, 0.0f};
};

inline NiPoint3 RightHanded(const NiPoint3& p, bool leftHanded) {
	return NiPoint3{leftHanded ? -p.x : p.x, p.y, p.z};
}

inline HolsterFitVerdict StepHolsterFit(HolsterFitState& s, const HolsterFitInput& in) {
	HolsterFitVerdict v;
	const bool weaponPress = in.weaponTrigger && !s.weaponTriggerWas;
	const bool otherPress = in.otherTrigger && !s.otherTriggerWas;
	s.weaponTriggerWas = in.weaponTrigger;
	s.otherTriggerWas = in.otherTrigger;
	if (in.start && s.step == HolsterFitStep::Idle) {
		s.step = HolsterFitStep::Melee;
		s.weaponTriggerWas = true;
		s.otherTriggerWas = true;
		v.stepChanged = true;
	} else if (s.step != HolsterFitStep::Idle && in.cancel) {
		s = HolsterFitState{};
		v.cancelled = true;
		v.stepChanged = true;
	} else if (s.step == HolsterFitStep::Melee && weaponPress && in.weaponValid) {
		s.melee = RightHanded(in.weaponRelative, in.leftHanded);
		s.step = HolsterFitStep::Bow;
		v.stepChanged = true;
	} else if (s.step == HolsterFitStep::Bow && otherPress && in.otherValid) {
		v.finished = true;
		v.melee = s.melee;
		v.bow = RightHanded(in.otherRelative, in.leftHanded);
		s = HolsterFitState{};
		v.stepChanged = true;
	}
	v.step = s.step;
	v.active = s.step != HolsterFitStep::Idle;
	return v;
}

}  // namespace obvr::vr
