#pragma once

// Lever 2: the second eye's pass takes the first pass's vertex streams
// (render/StreamReuseLogic.h for the engine's side and the idea).
// [Render] StreamReuse=1 installs two entry detours - on the stream packer
// 0x00763130 and on the ring manager 0x007829A0 - and [Render]
// StreamRingBytes sets the rings' minimum size; [Debug] StreamReuseCheck=1
// packs in the second pass anyway and compares the bytes with the first
// pass's. Off by default until the headset has seen it.

#include "core/Types.h"

namespace obvr::render {

// Installs the detours once; logs the outcome.
void InstallStreamReuse();

// From the vertex and index buffer hooks: every Lock with its flags (the
// DISCARD generations, and which buffers a pack touches).
void StreamReuseOnLock(void* buffer, UInt32 flags);

// From the fast stream copy (render/StreamCopy.cpp), after every copy a pack
// makes: the elements it wrote into the engine's staging buffer, hashed so
// that a second pass's streams can be told from the first's.
void StreamReuseOnCopy(const void* dest, UInt32 count, UInt32 elementBytes, UInt32 destStride);

// Whether a detoured pack is in flight right now (for the copy counters).
bool StreamReusePacking();

}  // namespace obvr::render
