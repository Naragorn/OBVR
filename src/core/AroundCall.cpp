#include "core/AroundCall.h"

#include "core/CodeWriter.h"

namespace obvr::mem {

UInt32 BuildAroundCallStub(UInt8* buffer, UInt32 capacity, UInt32 stubAddress, UInt32 target,
                           UInt32 before, UInt32 after, const AroundCallSlots& slots) {
	CodeWriter code(buffer, capacity, stubAddress);

	code.PopToMemory(slots.returnAddress);
	code.StoreStackPointer(slots.arguments);

	code.PushAllRegisters();
	code.PushFlags();
	code.PushEcx();
	code.CallRelative(before);
	code.AddStackPointer(4);
	code.PopFlags();
	code.PopAllRegisters();

	code.CallRelative(target);

	code.PushAllRegisters();
	code.PushFlags();
	code.CallRelative(after);
	code.PopFlags();
	code.PopAllRegisters();

	code.PushFromMemory(slots.returnAddress);
	code.Return();

	return code.Overflowed() ? 0 : code.Size();
}

UInt32 BuildCallSitePatch(UInt8* buffer, UInt32 capacity, UInt32 callSite, UInt32 stubAddress) {
	CodeWriter code(buffer, capacity, callSite);
	code.CallRelative(stubAddress);
	return code.Overflowed() ? 0 : code.Size();
}

UInt32 CallRelativeDisplacement(UInt32 callSite, UInt32 target) {
	// Unsigned on purpose: a target below the site wraps, which is exactly
	// the two's complement the instruction carries.
	return target - (callSite + 5);
}

}  // namespace obvr::mem
