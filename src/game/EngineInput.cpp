#include "game/EngineInput.h"

#include <atomic>

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/InputRoute.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The tail's first nine bytes in 1.2.0.416 (addr::kInputPollTail): push esi;
// xor eax,eax; lea edx,[ecx+1B58h]. Verified before patching.
constexpr UInt8 kEntryBytes[] = {0x56, 0x33, 0xC0, 0x8D, 0x91, 0x58, 0x1B, 0x00, 0x00};

// thiscall with no arguments and a plain ret: __fastcall with the one
// pointer in ecx mirrors it.
using PollTailFn = void(__fastcall*)(void* input);
PollTailFn g_original = nullptr;

// The poll and the hand controls both run on the game's thread (the frame
// function and the Present hook); the lock costs nothing there and keeps
// the overlay whole should that ever change.
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
EngineInputOverlay g_overlay;
bool g_active = false;
// The first poll of each activation that delivered something, logged.
bool g_deliveryLogged = false;
UInt32 g_deliveryLines = 12;
bool g_unmappedLogged = false;

struct Guard {
	Guard() {
		while (g_lock.test_and_set(std::memory_order_acquire)) {
		}
	}
	~Guard() { g_lock.clear(std::memory_order_release); }
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;
};

EngineInputTarget TargetFor(UInt32 virtualKey) {
	UInt32 layoutScan = 0;
	if (virtualKey > 0x02 && UsScanCode(virtualKey) == 0) {
		layoutScan = MapVirtualKeyA(virtualKey, MAPVK_VK_TO_VSC);
	}
	return EngineTargetFor(virtualKey, layoutScan);
}

void __fastcall HookedPollTail(void* input) {
	bool logDelivery = false;
	UInt32 keys = 0;
	UInt32 buttons = 0;
	SInt32 dx = 0;
	SInt32 dy = 0;
	SInt32 wheel = 0;
	if (input != nullptr) {
		Guard guard;
		if (g_active) {
			if (!g_deliveryLogged && g_deliveryLines > 0) {
				keys = EngineKeysHeld(g_overlay);
				buttons = EngineButtonsHeld(g_overlay);
				dx = g_overlay.dx;
				dy = g_overlay.dy;
				wheel = g_overlay.wheel;
				if (keys != 0 || buttons != 0 || dx != 0 || dy != 0 || wheel != 0) {
					g_deliveryLogged = true;
					--g_deliveryLines;
					logDelivery = true;
				}
			}
			auto* base = static_cast<UInt8*>(input);
			ApplyEngineOverlay(g_overlay, base + addr::kInputCurrentKeysOffset,
			                   reinterpret_cast<SInt32*>(base + addr::kInputMouseStateOffset),
			                   base + addr::kInputMouseButtonsOffset);
		}
	}
	if (logDelivery) {
		OBVR_LOG("Input: the game's poll took the controllers' state - %u key(s) and %u mouse button(s) "
		         "held, mouse %d,%d, wheel %d (thread %u)",
		         keys, buttons, dx, dy, wheel, static_cast<UInt32>(GetCurrentThreadId()));
	}
	g_original(input);
}

}  // namespace

bool InstallEngineInputHook() {
	if (g_original != nullptr) {
		return true;
	}

	if (!mem::Verify(addr::kInputPollTail, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Input: bytes at %08X differ - the controllers cannot reach the game while it is "
		         "behind another window",
		         addr::kInputPollTail);
		mem::ReportForeignCode("Input", addr::kInputPollTail);
		return false;
	}

	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Input: no executable memory for the trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize =
		mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress,
		                          addr::kInputPollTail, kEntryBytes, sizeof(kEntryBytes));
	if (trampolineSize == 0) {
		OBVR_LOG("Input: the trampoline does not fit");
		return false;
	}

	UInt8 patch[sizeof(kEntryBytes)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), addr::kInputPollTail,
	                                              reinterpret_cast<UInt32>(&HookedPollTail),
	                                              sizeof(kEntryBytes));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Input: the patch has unexpected length %u", patchSize);
		return false;
	}

	g_original = reinterpret_cast<PollTailFn>(trampoline);
	if (!mem::SafeWrite(addr::kInputPollTail, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Input: SafeWrite to %08X failed - the controllers cannot reach the game while it is "
		         "behind another window",
		         addr::kInputPollTail);
		return false;
	}

	OBVR_LOG("Input: the poll's tail at %08X takes the controllers' keys while the game is behind "
	         "another window, trampoline at %08X (thread %u)",
	         addr::kInputPollTail, trampolineAddress, static_cast<UInt32>(GetCurrentThreadId()));
	return true;
}

bool EngineInputHookInstalled() { return g_original != nullptr; }

void SetEngineInputActive(bool active) {
	Guard guard;
	if (!active) {
		ClearEngineOverlay(g_overlay);
	} else if (!g_active) {
		g_deliveryLogged = false;
	}
	g_active = active;
}

void EngineHoldKey(UInt32 virtualKey, bool down) {
	const EngineInputTarget target = TargetFor(virtualKey);
	if (target.kind == EngineInputTarget::Kind::None) {
		if (down && !g_unmappedLogged) {
			g_unmappedLogged = true;
			OBVR_LOG("Input: virtual key %02X has no scan code - it cannot be held in the game's input",
			         virtualKey);
		}
		return;
	}
	Guard guard;
	HoldEngineTarget(g_overlay, target, down);
}

bool EngineHoldsKey(UInt32 virtualKey) {
	const EngineInputTarget target = TargetFor(virtualKey);
	Guard guard;
	return g_active && EngineTargetHeld(g_overlay, target);
}

void EngineMoveMouse(SInt32 dx, SInt32 dy) {
	Guard guard;
	// Nobody takes it while inactive: kept, it would arrive in one jump.
	if (g_active) {
		g_overlay.dx += dx;
		g_overlay.dy += dy;
	}
}

void EngineScrollWheel(SInt32 delta) {
	Guard guard;
	if (g_active) {
		g_overlay.wheel += delta;
	}
}

void EngineReleaseAll() {
	Guard guard;
	ClearEngineOverlay(g_overlay);
}

bool GameWindowInFront() {
	void* window = GetForegroundWindow();
	if (window == nullptr) {
		return false;
	}
	DWORD processId = 0;
#if defined(OBVR_NO_WINSDK)
	GetWindowThreadProcessId(window, &processId);
#else
	GetWindowThreadProcessId(reinterpret_cast<HWND>(window), &processId);
#endif
	return processId == GetCurrentProcessId();
}

}  // namespace obvr::game
