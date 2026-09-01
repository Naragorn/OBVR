#include "render/CreateDeviceGuard.h"

namespace obvr::render {

SlotGuardDecision GuardCreateDeviceSlot(const SlotGuardInput& input) {
	SlotGuardDecision decision;

	if (!input.slotKnown || input.deviceCreated || input.slotIsOurs) {
		return decision;
	}

	if (input.repairsLeft == 0) {
		return decision;
	}

	decision.repair = true;
	decision.report = true;

	// Chaining in front of the other hook is the polite half of this, and the
	// correct one: whoever wrote there wants to see the device being made too,
	// and there is no reason both cannot. The exception is a pointer that
	// belongs to OBVR itself, which would mean calling ourselves.
	decision.adoptForeign = !input.foreignIsOurModule;
	return decision;
}

}  // namespace obvr::render
