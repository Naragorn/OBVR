#include "render/CreateDeviceSlot.h"

#include "render/CreateDeviceGuard.h"

namespace obvr::render {

SlotOutcome KeepOnLookup(CreateDeviceSlot& state, void* hook, const SlotAccess& access) {
	SlotOutcome outcome;
	if (state.slot == nullptr) {
		return outcome;
	}

	void* const found = *state.slot;
	outcome.found = found;

	SlotGuardInput input;
	input.slotKnown = true;
	input.deviceCreated = state.deviceCreated;
	input.slotIsOurs = found == hook;
	input.foreignIsOurModule = !input.slotIsOurs && access.isOurs != nullptr && access.isOurs(found);
	input.repairsLeft = state.repairsLeft;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	if (!decision.repair) {
		if (!input.slotIsOurs && !state.deviceCreated && state.repairsLeft == 0) {
			outcome.step = SlotStep::Refused;
		}
		return outcome;
	}

	if (access.write == nullptr || !access.write(state.slot, hook)) {
		state.repairsLeft = 0;
		outcome.step = SlotStep::NotWritable;
		return outcome;
	}

	--state.repairsLeft;
	if (decision.adoptForeign) {
		state.next = found;
		outcome.step = SlotStep::Retaken;
	} else {
		outcome.step = SlotStep::RetakenOwn;
	}
	return outcome;
}

SlotOutcome KeepOnFactory(CreateDeviceSlot& state, void** slot, void* hook,
                          const SlotAccess& access) {
	if (state.slot != nullptr) {
		return KeepOnLookup(state, hook, access);
	}

	SlotOutcome outcome;
	if (slot == nullptr || *slot == nullptr) {
		outcome.step = SlotStep::NoMethod;
		return outcome;
	}

	void* const found = *slot;
	outcome.found = found;
	if (access.write == nullptr || !access.write(slot, hook)) {
		outcome.step = SlotStep::NotWritable;
		return outcome;
	}

	state.slot = slot;
	state.next = found;
	state.first = found;
	outcome.step = SlotStep::Taken;
	return outcome;
}

void* CreateDeviceNext(const CreateDeviceSlot& state, bool reentered) {
	return reentered ? state.first : state.next;
}

const char* SlotStepName(SlotStep step) {
	switch (step) {
	case SlotStep::Untouched:
		return "untouched";
	case SlotStep::Taken:
		return "taken";
	case SlotStep::Retaken:
		return "retaken";
	case SlotStep::RetakenOwn:
		return "retaken-own";
	case SlotStep::Refused:
		return "refused";
	case SlotStep::NotWritable:
		return "not-writable";
	case SlotStep::NoMethod:
		return "no-method";
	}
	return "?";
}

}  // namespace obvr::render
