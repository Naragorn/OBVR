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
// occasion names where in the frame the reading was taken, because that is
// now the whole comparison. The control run settled that the menu is not what
// empties the render: the identical self-initiated call, made from Present on
// an ordinary world frame while the engine was drawing the scene perfectly
// well, came back with the same 343 setup calls and the same zero draws. So
// what differs is not the menu but the moment - inside the engine's own
// render, where it works, against Present, where it does not - and the fields
// have to be read at both to see which one moved.
void ProbeSceneGraph(UInt32 frameIndex, const char* occasion);

}  // namespace obvr::render
