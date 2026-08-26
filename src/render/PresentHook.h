#pragma once

#include "core/Types.h"

namespace obvr::render {

// Learns when Oblivion has finished drawing a frame, by replacing one entry in
// its Direct3D device's method table.
//
// Why this is needed at all: the camera hook fires while the camera is being
// computed, which is *before* the frame is drawn. Everything submitted from
// there is therefore last frame's picture - a whole frame of latency, which in
// a headset is felt directly rather than seen. Present is the other end: by
// the time Direct3D is asked to show a frame, that frame exists.
//
// Why a table entry rather than a trampoline. The camera hook had to patch
// bytes at an address found in Oblivion.exe, because that is a function
// belonging to the game. Present belongs to Direct3D, and every COM object
// reaches its methods through a table of pointers - so swapping one pointer
// is enough, and it needs no address, no instruction decoding, and no
// assumptions about what the first bytes of the function look like. It also
// survives whatever compiled the runtime, which matters here because that
// runtime is DXVK rather than Microsoft's.
//
// What it costs instead: the table belongs to a class, not to an object, so
// this affects every device of that class in the process. Direct3D 9 games
// have one. And the table is in read-only memory, so it has to be made
// writable and put back.
//
// The hook is deliberately thin. It calls the callback and then the original,
// unchanged, with the same arguments - it is there to learn the time, not to
// change what Present does.

// Called just before Oblivion's frame reaches the screen, with the finished
// picture sitting in the back buffer. This is the moment the compositor
// should be given it.
using FrameEndCallback = void (*)();

// Replaces the device's Present with one that calls the callback first.
// False if the device or its table cannot be read, or if a hook is already
// installed - installing twice would chain a hook onto itself and call the
// callback twice per frame.
bool InstallPresentHook(void* gameDevice, FrameEndCallback callback);

// Puts the original pointer back. Safe when nothing is installed.
//
// Not optional at shutdown. A table still pointing into a DLL that has been
// unloaded is a crash on the next frame, and it is a crash with OBVR's name
// nowhere in it.
void RemovePresentHook();

bool IsPresentHooked();

// Whether a pointer that is about to be treated as a COM object's method table
// looks like one at the given depth.
//
// Separated so it can be checked without a device. What this guards against is
// writing through a pointer that was never a vtable: the entry would be
// changed in whatever that memory really is, and the fault would surface
// somewhere with no connection to rendering.
bool LooksLikeVtable(void* const* vtable, UInt32 entries);

}  // namespace obvr::render
