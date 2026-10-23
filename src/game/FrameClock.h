#pragma once

// The engine's frame clock made precise - see game/FrameClockLogic.h for
// what the engine does and why a headset's frame rate suffers from it.
// [Render] PreciseFrameClock=1 (default) installs the detour on the time
// info's update at 0x0047D170; [Debug] FrameClockTrace=1 logs, every ten
// seconds, the distinct lengths the engine's own clock produced and the
// lengths the frames were given.

namespace obvr::game {

// Installs the detour once; logs its outcome. Nothing happens with the
// setting off, or when the entry bytes are not the ones read from 1.2.0.416.
void InstallFrameClock();

}  // namespace obvr::game
