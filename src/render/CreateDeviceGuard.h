#pragma once

#include "core/Types.h"

namespace obvr::render {

// Whether OBVR still owns the one method table slot the frame size hangs on.
//
// The slot is shared property. OBVR writes its own CreateDevice into the
// Direct3D factory's method table, and so does anything else in the process
// that wants to see the device being made - a Steam overlay, another wrapper.
// Whoever writes last wins, and the order is not ours to pick.
//
// That is not a theory. In one run the log recorded OBVR's factory hook
// running (it is the line that says the table was patched), and then the
// device came up at the game's own 2560x1440 with none of the lines the
// CreateDevice hook writes unconditionally. The factory was OBVR's; the slot,
// by the time it was called, was not.
//
// So the slot is checked again while the device is still to come, and this is
// the decision that check makes. Nothing here touches memory: the caller reads
// the slot, names the module, and acts on what comes back.
struct SlotGuardInput {
	// The table was patched at all - without that there is no slot to watch.
	bool slotKnown = false;

	// The device is built. Whatever the slot says now, the frame size was
	// decided already, and writing to it would only be noise.
	bool deviceCreated = false;

	// The slot still holds OBVR's hook, which is the ordinary case and the
	// reason this costs one comparison per lookup.
	bool slotIsOurs = false;

	// What sits in the slot belongs to OBVR's own module. Taking that as the
	// function to call after our hook would be a loop, so it is not taken.
	bool foreignIsOurModule = false;

	// A budget, because two hooks that both insist would otherwise trade the
	// slot back and forth for the length of the run.
	UInt32 repairsLeft = 0;
};

struct SlotGuardDecision {
	// Write OBVR's hook back into the slot.
	bool repair = false;

	// Call what was found there afterwards, rather than dropping it: the
	// other hook keeps working, with OBVR in front of it.
	bool adoptForeign = false;

	// Say so in the log. A silent repair would hide exactly the fact this
	// was built to find out.
	bool report = false;
};

SlotGuardDecision GuardCreateDeviceSlot(const SlotGuardInput& input);

}  // namespace obvr::render
