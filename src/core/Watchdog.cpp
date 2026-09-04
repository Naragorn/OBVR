#include "core/Watchdog.h"

#include "core/BranchDecode.h"
#include "core/Log.h"
#include "core/StackScan.h"
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

// The thread the frames end on - the one Start() was first called from,
// which is the Present hook's. The report suspends it by this id.
DWORD g_renderThreadId = 0;

// A tick a second, and ten quiet ticks before a stall is declared. Loading
// screens keep presenting, so they do not trip it; a minimised window that
// stops presenting does, which costs one line saying so.
constexpr DWORD kTickMilliseconds = 1000;
constexpr UInt32 kStallTicks = 10;

// The stack report's reach: how many words below the stack pointer are
// looked at, and how many code addresses among them are named.
constexpr UInt32 kStackWordsWanted = 2048;
constexpr UInt32 kCodeAddressesNamed = 24;

// Whether a word lies inside a loaded module's image. The loader answers
// for any address within a mapped module, data included; a stale data
// pointer that happens to lie in a module costs one spurious entry in the
// list, which the offsets make easy to tell from a return address.
bool IsInModule(UInt32 word) {
	HMODULE module = nullptr;
	const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
	                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	return GetModuleHandleExA(flags, reinterpret_cast<const char*>(word), &module) != 0 &&
	       module != nullptr;
}

// "module+offset" for an address, or the bare address when no module holds
// it. Written into `out`, which is always terminated.
void NameAddress(UInt32 address, char* out, UInt32 size) {
	HMODULE module = nullptr;
	const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
	                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	char path[512] = "";
	if (GetModuleHandleExA(flags, reinterpret_cast<const char*>(address), &module) != 0 &&
	    module != nullptr && GetModuleFileNameA(module, path, sizeof(path)) != 0) {
		const UInt32 base = reinterpret_cast<UInt32>(module);
		_snprintf(out, size, "%s+%X", mem::FileBaseName(path), address - base);
	} else {
		_snprintf(out, size, "%08X", address);
	}
	out[size - 1] = '\0';
}

// Where the render thread stands, and which code addresses lie on its
// stack. The thread is suspended for the reading and resumed straight
// after; the lines are written after the resume, so a thread stopped
// inside the logger cannot be waited on by the logger. A hung thread stays
// hung either way - this only says where.
void ReportRenderThread() {
	if (g_renderThreadId == 0) {
		OBVR_LOG("Watchdog: the render thread's id is unknown, so its position cannot be read");
		return;
	}
	HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
	                               THREAD_QUERY_INFORMATION,
	                           0, g_renderThreadId);
	if (thread == nullptr) {
		OBVR_LOG("Watchdog: the render thread (id %u) could not be opened, so its position "
		         "cannot be read",
		         static_cast<UInt32>(g_renderThreadId));
		return;
	}

	ThreadContext context{};
	bool haveContext = false;
	UInt32 stackWords[kStackWordsWanted];
	UInt32 stackWordCount = 0;
	if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
		haveContext = ReadThreadContext(thread, context);
		if (haveContext) {
			// The stack region the pointer sits in bounds the read: below
			// the pointer's region lies the guard page, and past its end
			// whatever the allocator put there.
			MemoryBasicInfo region{};
			if (QueryMemory(reinterpret_cast<const void*>(context.esp), region) &&
			    region.state == MEM_COMMIT && (region.protect & PAGE_GUARD) == 0 &&
			    region.protect != PAGE_NOACCESS) {
				const UInt32 available = StackWordsAvailable(
					context.esp, reinterpret_cast<UInt32>(region.baseAddress),
					region.regionSize, kStackWordsWanted);
				const UInt32* stack = reinterpret_cast<const UInt32*>(context.esp);
				for (UInt32 i = 0; i < available; ++i) {
					stackWords[i] = stack[i];
				}
				stackWordCount = available;
			}
		}
		ResumeThread(thread);
	}
	CloseHandle(thread);

	if (!haveContext) {
		OBVR_LOG("Watchdog: the render thread (id %u) would not give up its registers",
		         static_cast<UInt32>(g_renderThreadId));
		return;
	}

	char name[96];
	NameAddress(context.eip, name, sizeof(name));
	OBVR_LOG("Watchdog: the render thread (id %u) stands at %s - eip %08X esp %08X ebp %08X, "
	         "%u stack words readable",
	         static_cast<UInt32>(g_renderThreadId), name, context.eip, context.esp,
	         context.ebp, stackWordCount);

	UInt32 code[kCodeAddressesNamed];
	const UInt32 kept = CollectCodeWords(stackWords, stackWordCount, &IsInModule, code,
	                                     kCodeAddressesNamed);
	char line[1000] = "";
	UInt32 used = 0;
	for (UInt32 i = 0; i < kept; ++i) {
		NameAddress(code[i], name, sizeof(name));
		const int wrote = _snprintf(line + used, sizeof(line) - used, "%s%s",
		                            i == 0 ? "" : ", ", name);
		if (wrote < 0 || used + static_cast<UInt32>(wrote) >= sizeof(line) - 1) {
			break;
		}
		used += static_cast<UInt32>(wrote);
	}
	line[sizeof(line) - 1] = '\0';
	OBVR_LOG("Watchdog: code addresses on its stack, nearest first (return addresses among "
	         "stale ones): %s",
	         kept == 0 ? "none" : line);
}

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
			ReportRenderThread();
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
	g_renderThreadId = GetCurrentThreadId();
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
