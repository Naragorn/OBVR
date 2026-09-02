#pragma once

#include "core/Types.h"

namespace obvr::mem {

// Byte generation for a stub that wraps ONE call: something of OBVR's runs
// before the engine function, the engine function runs untouched, something
// of OBVR's runs after it, and the caller gets exactly the return it expected.
//
// It exists for the aim. The heading a projectile leaves along, and the
// heading a swing is tested against, are read inside a handful of engine
// calls - and only there. Set the heading on the way in and put it back on
// the way out, and no frame ever sees it changed: walking is not pulled, no
// turn is animated, nothing has to be compensated. That is the whole reason
// the "after" half exists; a plain entry detour has no way back.
//
// HOW IT KEEPS THE STACK HONEST. The stub is reached by a call, so the
// caller's return address is on top of the stack and the arguments sit under
// it. The stub pops that return address into a slot of its own, which leaves
// the arguments exactly where the engine function expects them, calls the
// function - whose own `ret N` then removes the arguments as it always did -
// and finally pushes the saved return address back and returns through it.
// The caller can tell no difference. `this` reaches the before-callback the
// way the cast trampoline hands it over: pushed from ecx, after the register
// save that does not touch it.
//
//   pop  [returnSlot]        ; the caller's return address, kept aside
//   mov  [argumentsSlot], esp ; where the arguments now start
//   pushad / pushfd
//   push ecx                  ; this
//   call before               ; cdecl, one argument
//   add  esp, 4
//   popfd / popad
//   call target               ; the engine function, untouched
//   pushad / pushfd
//   call after                ; cdecl, no arguments
//   popfd / popad
//   push [returnSlot]
//   ret
//
// The register save around `after` is what protects the engine's return
// value: eax comes back out of popad as the function left it.
//
// ONE SLOT, ONE CALL AT A TIME. The return address lives in a static slot, so
// the stub is not reentrant. Every function it is put around is one the
// engine calls from a single thread and never from inside itself, and the
// before-callback is where that assumption is defended, not here.
//
// Kept free of Windows so tests/ can check every byte.
struct AroundCallSlots {
	UInt32 returnAddress = 0;   // where the stub keeps the caller's return
	UInt32 arguments = 0;       // where it records the arguments' address
};

// Writes the stub into buffer. stubAddress is where it will run; target is
// the engine function; before and after are OBVR's cdecl callbacks.
// Returns the number of bytes written, or 0 when the capacity is too small.
UInt32 BuildAroundCallStub(UInt8* buffer, UInt32 capacity, UInt32 stubAddress, UInt32 target,
                           UInt32 before, UInt32 after, const AroundCallSlots& slots);

// The five bytes of a direct call at callSite, re-pointed at the stub. Used
// where the engine reaches the function through `call rel32`; a virtual call
// is re-pointed by writing the stub into the vtable slot instead, which needs
// no bytes built.
// Returns 5, or 0 when the capacity is too small.
UInt32 BuildCallSitePatch(UInt8* buffer, UInt32 capacity, UInt32 callSite, UInt32 stubAddress);

// The rel32 a direct call at callSite must carry to reach target - what the
// five bytes at an untouched site are expected to hold before they are
// patched, so the site can be checked against the function it is said to
// call.
UInt32 CallRelativeDisplacement(UInt32 callSite, UInt32 target);

}  // namespace obvr::mem
