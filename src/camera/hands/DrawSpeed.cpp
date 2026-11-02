#include "camera/hands/DrawSpeed.h"

#include "core/Log.h"
#include "game/WeaponDrawSpeed.h"

namespace obvr::camera::hands {

void StepDrawSpeed(DrawSpeedState& state, const HandStep& step) {
	const Config& config = step.config;
	const bool active = step.active;
	const game::WeaponDrawSpeedReport draw =
		game::StepWeaponDrawSpeed(config.hands.weaponDrawSpeed, active);
	if ((draw.newlyHastened > 0 || draw.released > 0 || draw.blendsShortened > 0) && state.drawLinesLeft > 0) {
		--state.drawLinesLeft;
		if (draw.newlyHastened > 0) {
			OBVR_LOG("Hands: the %s animation's time runs %.0fx faster (%u sequence(s))",
			         draw.group == 17 ? "draw" : "sheathe",
			         static_cast<double>(config.hands.weaponDrawSpeed), draw.hastened);
		} else if (draw.blendsShortened > 0) {
			OBVR_LOG("Hands: %u draw or sheathe blend(s) shortened %.0fx", draw.blendsShortened,
			         static_cast<double>(config.hands.weaponDrawSpeed));
		} else {
			OBVR_LOG("Hands: %u draw or sheathe sequence(s) done", draw.released);
		}
	}
}

}  // namespace obvr::camera::hands
