#pragma once

#include "core/Types.h"

namespace obvr::platform {

// Sizes a window so that its client area is exactly the given size.
//
// Why OBVR needs this at all. Oblivion asks for its Direct3D device in
// exclusive fullscreen, and there Direct3D sizes the game's window itself - so
// Oblivion never had to, and does not. OBVR clears that fullscreen flag,
// because a square frame is not a display mode and no driver can switch a
// monitor into one. What that revealed is that the window had never been sized
// at all: DXVK built the swapchain at 320x240 behind a 3200x3200 back buffer.
// The mouse was mapped against those 320x240 pixels, nothing could be clicked,
// and the device was eventually lost outright.
//
// The size to ask for is the frame's. Oblivion lays out its interface and
// maps its mouse against the resolution it believes in - and OBVR moves that
// belief (the in-memory iSize settings) to the frame's size when it enlarges
// the frame, so window, buffer and belief are one number, which is the state
// a monitor install is in. The first version matched the window to what the
// game had asked for instead, and the layout probe showed where that ends:
// half the 2D against one size, half against the other, and the mouse lost
// between them.
//
// The client area is what is asked for, not the window: a window with a border
// is larger than the area drawn into, and the difference is measured here
// rather than assumed to be zero.
//
// False when the window cannot be measured or moved.
bool SizeClientArea(void* window, UInt32 width, UInt32 height, UInt32& wasWidth,
                    UInt32& wasHeight);

}  // namespace obvr::platform
