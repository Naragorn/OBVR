#pragma once

#include "core/Types.h"

namespace obvr::render {

// Redirects the pass that draws Oblivion's 2D layer into a surface of OBVR's
// own - addr::kRenderInterface, the one function every route to the HUD,
// menus, dialogues and loading screens funnels through.
//
// Two hooks working as one, because of how the pass behaves: it begins the
// default render target group *from inside itself*, so a wrapper that set a
// target first would be overridden immediately. The entry detour therefore
// only raises a flag and asks the callbacks for a surface; the actual
// substitution happens in the device's SetRenderTarget - the one method
// every Gamebryo target wrapper ends at, which is an API fact rather than a
// game one. While the flag is up, any target the pass sets for index 0 is
// replaced with the callback's surface; the last target it asked for is
// remembered and put back when the pass returns, so the device ends the call
// in the state the game believes it is in.
//
// The depth stencil surface is never touched: the pass clears depth and
// stencil on entry exactly as vanilla does, into a depth buffer the finished
// world no longer needs.

struct InterfaceRedirect {
	// Asked at the entry of every 2D pass. Return the surface the layer
	// should be drawn into, or null to leave this pass alone - null is the
	// answer for menu frames, for frames with no compositor, and for the
	// feature being off.
	void* (*beginRedirect)() = nullptr;

	// Called after a redirected pass returned, targets already restored.
	void (*endRedirect)() = nullptr;

	// Whether the diagnostic probe is on this frame. Optional - null means
	// never. While it answers true, the redirected pass starts from a
	// recognisable half-transparent clear issued through the render target
	// binding instead of an invisible one, and the first draw of a traced
	// pass logs the pipeline state it actually ran with.
	bool (*probeActive)() = nullptr;
};

// Verifies the entry bytes and patches the entry. The SetRenderTarget table
// entry is replaced lazily, on the first pass that actually redirects,
// because the device does not exist when this is installed. Install once.
bool InstallInterfaceRenderHook(const InterfaceRedirect& callbacks);

bool IsInterfaceRenderHooked();

// How many times the 2D pass was entered since this was last asked, and how
// many primitives it drew in those passes; both zero afterwards. The scene
// render hook asks once per world render, so the two numbers land in the
// same line as the probe rung that frame ran under - which is what turns
// three runs of the game into one.
//
// Two numbers rather than one because they fail differently. Zero passes
// means Oblivion never entered the pass, and the cause is one of the three
// gates in the wrapper at 00579260. One pass with zero draws means it was
// entered and left without drawing, which is where the dual pass puts it.
void TakeInterfaceStats(UInt32& passes, UInt32& draws);

}  // namespace obvr::render
