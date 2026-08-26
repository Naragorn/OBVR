#pragma once

#include "core/Types.h"

namespace obvr::render {

// Sets the size of Oblivion's frame, at the one moment it can be set.
//
// A headset's view is nearly square. Oblivion renders 16:9 because that is the
// shape of a monitor, and spreading that over a square view stretches the
// pixels vertically however right the geometry is. The frustum OBVR writes
// every frame; the frame's SHAPE it cannot, because Direct3D fixes that when
// the device is created and changing it afterwards means destroying every
// resource the game holds.
//
// So this is in the way of the creation. Two hooks, one behind the other:
//
//   Direct3DCreate9  reached through Oblivion's import table, because that is
//                    a single pointer rather than instructions to decode. Its
//                    only job is to catch the factory object on its way past.
//   CreateDevice     entry 16 of that factory's method table, patched once the
//                    factory exists. This is where the size actually lives,
//                    in D3DPRESENT_PARAMETERS, and where it is changed.
//
// The alternative was writing iSize into Oblivion.ini and letting the game
// read it on the next run. That is a detour in the exact sense: it goes around
// the place the decision is made rather than to it, needs two restarts, and
// edits a file that belongs to the user. This needs one restart and touches
// nothing outside the process.
//
// The risk it carries, stated rather than hoped away: a plugin loads at some
// point during startup, and Direct3DCreate9 may already have been called by
// then. If it has, the import entry is replaced too late and nothing catches
// the factory - which is reported in the log rather than left to be inferred
// from a resolution that did not change.

// Asks for the frame to be created at this size. 0 for either leaves that axis
// as the game asked for it.
//
// Called before the hooks go in, because the values are wanted at the moment
// the device is made and that may be very soon afterwards.
void SetWantedResolution(UInt32 width, UInt32 height);

// Puts the first hook in place. False when Oblivion does not import
// Direct3DCreate9 by name, or the table cannot be written.
bool InstallResolutionHook();

// Whether a device has been created through the hook since it went in, and
// what it was given. For the log: a hook that is installed and never reached
// looks exactly like one that is not installed at all.
bool WasDeviceCreated(UInt32& width, UInt32& height);

}  // namespace obvr::render
