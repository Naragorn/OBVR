#pragma once

#include "core/Types.h"

namespace obvr::render {

// Keeps OBVR's CreateDevice in the Direct3D factory's method table, slot 16,
// from the first factory the engine makes until the device exists.
//
// Patching the slot once is not enough, and this is why it is looked at again
// after every factory, not only on the GetProcAddress lookups in between.
// Oblivion makes two factories before the device - 0x00761DF0's, which it
// keeps, then 0x00763DE0's inside 0x0076A1C0 just before CreateDevice - and
// with DXVK (as with Microsoft's runtime) they share one table. A hooker that
// registers every factory it sees writes itself into the slot again on the
// second one. Read in Windows' SysWOW64\apphelp.dll (2026-11-21): the
// DetectorDWM8And16Bit shim's Direct3DCreate9 hook (0x100812D0) registers each
// factory (SE_COM_HookObject); its router (0x100842A2) writes its hook back
// when the slot no longer holds it; its CreateDevice (0x10080DF0) calls the
// FIRST pointer it ever recorded there (SE_COM_Lookup, 0x10083FDC) - the
// runtime's. OBVR, written in between, was skipped: the Windows run of
// 2026-11-20 got a 2560x1440 fullscreen device with the game's vsync. The
// second factory comes out through OBVR's Direct3DCreate9 like the first, so
// the look right after it is the moment such a hooker has just acted.
//
// Taking the slot back puts OBVR in front and calls what was found there
// next, so the other hooker keeps working. One that recorded OBVR as its own
// original would then call OBVR again; that second entry goes straight to the
// pointer OBVR found at the very first patch (CreateDeviceNext), so the chain
// cannot loop and the parameters are changed once.
//
// Nothing here touches memory except through SlotAccess::write and the slot
// itself, so every flow runs in tests with a plain array for a table.

struct SlotAccess {
	// Writes a pointer into the slot. False when the page cannot be made
	// writable.
	bool (*write)(void** slot, void* value) = nullptr;

	// Whether a pointer belongs to OBVR's own module.
	bool (*isOurs)(const void* pointer) = nullptr;
};

struct CreateDeviceSlot {
	// The slot written - &table[16] of the first factory - null until then.
	void** slot = nullptr;

	// What OBVR's hook calls after itself.
	void* next = nullptr;

	// What was in the slot when OBVR first wrote it: the way out for a second
	// entry into OBVR's hook.
	void* first = nullptr;

	// Two hooks that both insist would trade the slot for the whole run.
	UInt32 repairsLeft = 4;

	// The device is built, the size decided; the slot is not written again.
	bool deviceCreated = false;
};

enum class SlotStep {
	Untouched,    // nothing to do: no slot yet, the device is made, or it is ours
	Taken,        // the first factory: written, the pointer found kept as next
	Retaken,      // someone wrote over it: OBVR back in front, theirs next
	RetakenOwn,   // written back, but what was found is OBVR's own: next kept
	Refused,      // someone wrote over it and the repairs are spent
	NotWritable,  // the page could not be made writable
	NoMethod,     // no table, or nothing in slot 16
};

struct SlotOutcome {
	SlotStep step = SlotStep::Untouched;
	void* found = nullptr;  // what was in the slot before this step
};

// After each factory comes out of Direct3DCreate9. slot is &table[16] of that
// factory; the first one is taken, every later one only checked, since they
// share the table.
SlotOutcome KeepOnFactory(CreateDeviceSlot& state, void** slot, void* hook,
                          const SlotAccess& access);

// Between factories and the device (the GetProcAddress hook's look): the check
// alone, never the first write.
SlotOutcome KeepOnLookup(CreateDeviceSlot& state, void* hook, const SlotAccess& access);

// What OBVR's hook calls: next, or - when the hook is entered a second time
// within one CreateDevice - the pointer found at the first patch.
void* CreateDeviceNext(const CreateDeviceSlot& state, bool reentered);

const char* SlotStepName(SlotStep step);

}  // namespace obvr::render
