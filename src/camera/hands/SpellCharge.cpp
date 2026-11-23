#include "camera/hands/SpellCharge.h"

#include "core/Log.h"
#include "game/SpellOrb.h"
#include "game/SpellRelease.h"

namespace obvr::camera::hands {
namespace {

struct ChargeOwner {
	vr::SpellChargeState state;
	UInt32 lines = 40;
	int stageSaid = 0;
};

ChargeOwner& Owner() {
	static ChargeOwner owner;
	return owner;
}

void Say(ChargeOwner& o, const char* what, const vr::SpellChargeStep& r) {
	if (o.lines > 0) {
		--o.lines;
		OBVR_LOG("Spell charge: %s - held %.2f s, stage %d", what, static_cast<double>(r.heldSeconds), r.stage);
	}
}

}  // namespace

vr::HandControlsWanted WithSpellCharge(const vr::HandControlsWanted& controls, float dtSeconds, bool menuUp,
                                       const vr::SpellChargeSettings& settings) {
	ChargeOwner& o = Owner();
	vr::SpellChargeInput in;
	in.triggerHeld = controls.cast;
	in.menuUp = menuUp;
	in.dtSeconds = dtSeconds;
	const vr::SpellChargeStep r = vr::StepSpellCharge(o.state, in, settings);
	if (r.fireRelease) {
		game::RequestHeldSpellRelease(r.stage);
		Say(o, "let go - the spell goes", r);
	}
	if (r.dropRelease) {
		game::DropHeldSpellRelease();
		Say(o, "a menu came up - the charged spell dropped", r);
	}
	game::SetSpellReleaseHold(r.holdRelease);
	const vr::SpellOrbView orb = vr::SpellOrbFor(r, settings);
	game::WantSpellOrb(orb.shown, orb.scale, orb.full);
	if (r.pressed) {
		Say(o, "the cast pressed", r);
	}
	if (r.chargeBegan) {
		o.stageSaid = 0;
		Say(o, "held past a tap - charging", r);
	}
	if (r.holdRelease && r.stage != o.stageSaid) {
		o.stageSaid = r.stage;
		Say(o, "charged to a new stage", r);
		game::SpellChargeCue(r.stage);
	}
	vr::HandControlsWanted out = controls;
	out.cast = r.castControl;
	return out;
}

}  // namespace obvr::camera::hands
