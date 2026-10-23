#pragma once

// The engine's per-vertex stream copy replaced by one that decides the
// element size once (render/StreamCopyLogic.h for what the engine does and
// why it costs a fifth of a scene pass with people in view).
// [Render] FastStreamCopy=1 (default) installs the detour on 0x0076E610;
// [Debug] StreamCopyCheck=1 runs the engine's own routine first on every
// call and compares the bytes, logging any difference - the proof the two
// agree in the game, not only in the test.

namespace obvr::render {

// Installs the detour once; logs its outcome. Nothing happens with the
// setting off, or when the entry bytes are not the ones read from 1.2.0.416.
void InstallStreamCopy();

}  // namespace obvr::render
