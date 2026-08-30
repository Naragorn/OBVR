#pragma once

#include "core/Types.h"

namespace obvr::render {

// Reads the world scene graph and reports what the renderer would find there,
// without touching any of it.
//
// The question it answers is the one the menu-world probe opened and could
// not close. A self-initiated world render on a menu frame runs to completion
// - 343 vertex setup calls, measured, five times over - and issues not one
// draw. So it does not turn back at a gate; it walks a scene and comes away
// with nothing. Whether that is because the scene graph itself is gone while
// a menu is up, or because it is still there and only the culled list the
// renderer consumes has been emptied, decides what a live background has to
// rebuild - and the two look identical from the draw count alone.
//
// The addresses come from xOBSE's headers and are unverified until this probe
// prints them: g_worldSceneGraph at 0x00B333CC, the camera at +0xDC, the
// culling process at +0xE4 and its culled-geometry list at +0x08. Reading
// them on a live frame, where the render demonstrably works, is the second
// source; a run whose numbers make no sense is the address being wrong rather
// than the engine being strange, and this is the cheapest way to tell.
void ProbeSceneGraph(UInt32 frameIndex, bool menuIsUp);

}  // namespace obvr::render
