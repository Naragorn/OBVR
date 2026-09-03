#include "core/Watchdog.h"

#include "core/Log.h"
#include "core/StallDetector.h"
#include "platform/Win32Min.h"

namespace obvr::watchdog {
namespace {

// Written by the render thread, read by the watchdog. Both are single
// aligned 32-bit stores and loads, which x86 performs atomically; the
// counter being a frame or two stale on the watchdog's side changes nothing
// about a stall that lasts seconds.
volatile UInt32 g_frames = 0;
const char* volatile g_lastStep = "no step recorded yet";
bool g_started = false;

// A tick a second, and ten quiet ticks before a stall is declared. Loading
// screens keep presenting, so they do not trip it; a minimised window that
// stops presenting does, which costs one line saying so.
constexpr DWORD kTickMilliseconds = 1000;
constexpr UInt32 kStallTicks = 10;

DWORD __stdcall Watch(void*) {
	StallDetector detector;
	for (;;) {
		Sleep(kTickMilliseconds);
		const UInt32 frames = g_frames;
		if (Tick(detector, frames, kStallTicks) == StallVerdict::Stalled) {
			OBVR_LOG("Watchdog: no frame has ended for %u seconds - the render thread's last "
			         "recorded step was \"%s\", after frame %u. If the game is frozen, this "
			         "is where it stopped",
			         kStallTicks, g_lastStep, frames);
		}
	}
}

}  // namespace

void NoteStep(const char* step) {
	if (step != nullptr) {
		g_lastStep = step;
	}
}

void NoteFrame() { ++g_frames; }

void Start() {
	if (g_started) {
		return;
	}
	g_started = true;
	HANDLE thread = CreateThread(nullptr, 0, &Watch, nullptr, 0, nullptr);
	if (thread == nullptr) {
		OBVR_LOG("Watchdog: the thread could not be started, so a hang will leave no "
		         "line naming its last step");
		return;
	}
	// A reference to the thread, not the thread itself; nothing waits on it.
	CloseHandle(thread);
}

}  // namespace obvr::watchdog
