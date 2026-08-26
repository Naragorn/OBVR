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
// The size to ask for is the one the game itself asked for, not the screen's.
// Oblivion lays out its interface and maps its mouse against the resolution it
// believes in, which is what it put in D3DPRESENT_PARAMETERS before OBVR
// changed it. Giving the window exactly that keeps every one of those
// calculations true, and leaves the enlarged back buffer doing the one thing
// it is there for: more pixels behind the same picture, scaled down on the way
// to the screen.
//
// The client area is what is asked for, not the window: a window with a border
// is larger than the area drawn into, and the difference is measured here
// rather than assumed to be zero.
//
// False when the window cannot be measured or moved.
bool SizeClientArea(void* window, UInt32 width, UInt32 height, UInt32& wasWidth,
                    UInt32& wasHeight);

}  // namespace obvr::platform
