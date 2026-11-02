#include "camera/hands/TakeByHand.h"

#include "core/Log.h"
#include "game/CrosshairTarget.h"
#include "game/TakeItem.h"
#include "vr/Stow.h"

namespace obvr::camera::hands {

void StepTakeByHand(TakeByHandState& state, const HandStep& step, const HandShared& shared,
                    const TakeByHandLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	vr::HandModeResult& hand = shared.hand;
const bool pressed = active && hand.controls.activate && !menuIsUp;
if (pressed && !state.activateWas) {
	const game::CrosshairTarget target = game::ReadCrosshairTarget();
	if (target.haveRef) {
		links.activated.ref = target.refAddress;
		links.activated.print = game::TakeRefFingerprint(target.refAddress);
		links.activated.frame = links.frameCount;
	}
	bool isBook = false;
	const bool isItem = target.haveRef && game::RefIsItem(target.refAddress, &isBook);
	state.withheld =
		vr::ActivateWithheld(config.hands.stow, target.haveRef, isItem, isBook);
	if (state.withheld && state.withheldLinesLeft > 0) {
		--state.withheldLinesLeft;
		OBVR_LOG("Hands: activate kept from the game - a loose item is taken by hand here "
		         "([Hands] TakeOnlyByHand)");
	}
} else if (!pressed) {
	state.withheld = false;
}
state.activateWas = pressed;
if (state.withheld) {
	hand.controls.activate = false;
}
}

}  // namespace obvr::camera::hands
