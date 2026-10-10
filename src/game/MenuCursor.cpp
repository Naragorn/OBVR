#include "game/MenuCursor.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"

namespace obvr::game {
namespace {

// The first seven bytes of the tile search in 1.2.0.416, read from this
// machine's Oblivion.exe (push 0FFFFFFFFh; push 9BEBC8h - the SEH frame
// every one of these functions opens with): whole instructions, nothing
// relative, verified before patching.
constexpr UInt8 kEntryBytes[] = {0x6A, 0xFF, 0x68, 0xC8, 0xEB, 0x9B, 0x00};

// thiscall with one stack argument the callee pops (the caller pushes edi,
// sets ecx and takes eax straight after: 00582403-0058240B); __fastcall
// with a dead edx mirrors it.
using FindTileFn = UInt32(__fastcall*)(void* self, void* edx, UInt32 arg);
FindTileFn g_original = nullptr;

bool g_wanted = false;
float g_wantedX = 0.0f;
float g_wantedY = 0.0f;
UInt32 g_placed = 0;
UInt32 g_lines = 6;

// The pixel written last, and the engine's own cursor (EngineMenuCursor).
bool g_wrote = false;
float g_writtenX = 0.0f;
float g_writtenY = 0.0f;
bool g_engineKnown = false;
float g_engineX = 0.0f;
float g_engineY = 0.0f;

// The press's knob search (MenuCursor.h, the knob snap).
bool g_snapArmed = false;
bool g_snapDecided = false;
bool g_snapFound = false;
float g_snapStep = 0.0f;
float g_snapDx = 0.0f;
float g_snapDy = 0.0f;
UInt32 g_snapLines = 12;

// Tried once, at the press's start: when the beam's pixel finds a slider's
// part other than its knob, the rings round it are searched for the knob -
// the engine's own search at each point - and the nearest found is kept.
void DecideKnobSnap(void* self, void* edx, UInt32 arg, float* x, float* y) {
	g_snapDecided = true;
	g_snapFound = false;
	const char* const first = TileNameOf(g_original(self, edx, arg));
	if (!IsScrollPartName(first)) {
		return;
	}
	float dx = 0.0f;
	float dy = 0.0f;
	for (UInt32 i = 0; KnobProbeOffset(i, g_snapStep, dx, dy); ++i) {
		*x = g_wantedX + dx;
		*y = g_wantedY + dy;
		const char* const name = TileNameOf(g_original(self, edx, arg));
		if (IsScrollKnobName(name)) {
			g_snapFound = true;
			g_snapDx = dx;
			g_snapDy = dy;
			if (g_snapLines > 0) {
				--g_snapLines;
				OBVR_LOG("Menu cursor: a press on \"%s\" takes the knob \"%s\" %.0f,%.0f pixels off the beam", first,
				         name, static_cast<double>(dx), static_cast<double>(dy));
			}
			return;
		}
	}
	if (g_snapLines > 0) {
		--g_snapLines;
		OBVR_LOG("Menu cursor: a press on \"%s\" - no knob within %.0f pixels", first,
		         static_cast<double>(g_snapStep * kKnobProbeRings));
	}
}

UInt32 __fastcall HookedFindTile(void* self, void* edx, UInt32 arg) {
	auto* const manager = *reinterpret_cast<UInt8* const*>(addr::kInterfaceManagerPointer);
	if (manager != nullptr && self == manager) {
		auto* const x = reinterpret_cast<float*>(manager + addr::kInterfaceCursorXOffset);
		auto* const y = reinterpret_cast<float*>(manager + addr::kInterfaceCursorYOffset);
		if (CursorReadIsEngines(g_wrote, g_writtenX, g_writtenY, *x, *y)) {
			g_engineKnown = true;
			g_engineX = *x;
			g_engineY = *y;
		}
		if (g_wanted && g_snapArmed && !g_snapDecided) {
			*x = g_wantedX;
			*y = g_wantedY;
			DecideKnobSnap(self, edx, arg, x, y);
			// This frame's search at the knob; later frames the hand mode's
			// pixel carries the offset (MenuCursorKnobOffset).
			const float snapX = g_wantedX + (g_snapFound ? g_snapDx : 0.0f);
			const float snapY = g_wantedY + (g_snapFound ? g_snapDy : 0.0f);
			*x = snapX;
			*y = snapY;
			g_wrote = true;
			g_writtenX = snapX;
			g_writtenY = snapY;
			++g_placed;
			return g_original(self, edx, arg);
		}
		if (g_wanted) {
			if (g_lines > 0) {
				--g_lines;
				OBVR_LOG("Menu cursor: placed at %.0f,%.0f before the tile search (the engine's own at %.0f,%.0f)",
				         static_cast<double>(g_wantedX), static_cast<double>(g_wantedY),
				         static_cast<double>(g_engineX), static_cast<double>(g_engineY));
			}
			*x = g_wantedX;
			*y = g_wantedY;
			g_wrote = true;
			g_writtenX = g_wantedX;
			g_writtenY = g_wantedY;
			++g_placed;
		}
	}
	return g_original(self, edx, arg);
}

}  // namespace

bool InstallMenuCursorHook() {
	if (g_original != nullptr) {
		return true;
	}
	if (!mem::Verify(addr::kFindTileAtCursor, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Menu cursor: bytes at %08X differ - the laser keeps steering the cursor by mouse steps",
		         addr::kFindTileAtCursor);
		mem::ReportForeignCode("Menu cursor", addr::kFindTileAtCursor);
		return false;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Menu cursor: no executable memory for the trampoline");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress,
	                                                        addr::kFindTileAtCursor, kEntryBytes, sizeof(kEntryBytes));
	if (trampolineSize == 0) {
		OBVR_LOG("Menu cursor: the trampoline does not fit");
		return false;
	}
	UInt8 patch[sizeof(kEntryBytes)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), addr::kFindTileAtCursor,
	                                              reinterpret_cast<UInt32>(&HookedFindTile), sizeof(kEntryBytes));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Menu cursor: the patch has unexpected length %u", patchSize);
		return false;
	}
	g_original = reinterpret_cast<FindTileFn>(trampoline);
	if (!mem::SafeWrite(addr::kFindTileAtCursor, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Menu cursor: SafeWrite to %08X failed - the laser keeps steering the cursor", addr::kFindTileAtCursor);
		return false;
	}
	OBVR_LOG("Menu cursor: the tile search at %08X takes the laser's pixel as the cursor (trampoline at %08X)",
	         addr::kFindTileAtCursor, trampolineAddress);
	return true;
}

bool MenuCursorHookInstalled() { return g_original != nullptr; }

void SetMenuCursorWanted(bool wanted, float x, float y) {
	const bool finite = x == x && y == y && x > -1.0e6f && x < 1.0e6f && y > -1.0e6f && y < 1.0e6f;
	g_wanted = wanted && finite && g_original != nullptr;
	g_wantedX = x;
	g_wantedY = y;
}

void SetMenuCursorKnobSnap(bool armed, float stepPixels) {
	if (!armed) {
		g_snapArmed = false;
		g_snapDecided = false;
		g_snapFound = false;
		return;
	}
	if (!g_snapArmed) {
		g_snapStep = stepPixels;
	}
	g_snapArmed = true;
}

bool MenuCursorKnobOffset(float& dx, float& dy) {
	if (!g_snapArmed || !g_snapFound) {
		return false;
	}
	dx = g_snapDx;
	dy = g_snapDy;
	return true;
}

bool EngineMenuCursor(float& x, float& y) {
	if (!g_engineKnown) {
		return false;
	}
	x = g_engineX;
	y = g_engineY;
	return true;
}

}  // namespace obvr::game
