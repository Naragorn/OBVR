#include "game/MessageBoxTrace.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/MessageBoxLogic.h"

namespace obvr::game {
namespace {

// The message box builder every box goes through (read 2026-11-09 with REA,
// Oblivion.exe A8F31384...): both entries of ShowMessageBox (0x00579C10,
// and 0x005BCC00 behind it) call it with the same five arguments, cdecl -
// the text, the callback (kept at the MessageMenu +0x5C), a flag (+0x60),
// the first button's label, and a pointer to the pointer to the caller's
// further labels (each taken as a BSString and advanced, until an empty
// one). It loads Data\Menus\message_menu.xml and answers 1 when built, 0
// when not. Entry: push -1; push 0x009BDF28 (seven bytes, nothing
// relative). Detoured here to log the text and labels.
constexpr UInt32 kMessageBoxBuild = 0x005BC8B0;

// push -1; push 0x009BDF28 - whole instructions, nothing relative, read from
// this machine's Oblivion.exe and verified before patching.
constexpr UInt8 kEntryBytes[] = {0x6A, 0xFF, 0x68, 0x28, 0xDF, 0x9B, 0x00};

using BuildFn = UInt32(__cdecl*)(const char* text, void* callback, UInt32 flag, const char* firstButton,
                                 const char* const** rest);
// The way back into the builder, set once by the install.
BuildFn& Original() {
	static BuildFn original = nullptr;
	return original;
}

UInt32 __cdecl HookedBuild(const char* text, void* callback, UInt32 flag, const char* firstButton,
                           const char* const** rest) {
	static UInt32 s_lines = 60;
	if (s_lines > 0) {
		--s_lines;
		// Read before the call: the builder advances the caller's pointer.
		const char* labels[kMessageBoxButtonsMost] = {};
		const UInt32 count =
			CollectMessageBoxButtons(firstButton, rest != nullptr ? *rest : nullptr, labels, kMessageBoxButtonsMost);
		char line[384];
		ComposeMessageBoxLine(line, sizeof(line), text, labels, count);
		const UInt32 top = TopVisibleMenu();
		OBVR_LOG("Message box: %s - over menu %s (0x%03X)%s", line, MenuIdName(top), top,
		         BarterMenuUp() ? ", in a merchant's barter" : "");
	}
	return Original()(text, callback, flag, firstButton, rest);
}

}  // namespace

bool InstallMessageBoxTrace() {
	if (Original() != nullptr) {
		return true;
	}
	if (!mem::Verify(kMessageBoxBuild, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Message box: bytes at %08X differ - the boxes' texts are not logged", kMessageBoxBuild);
		mem::ReportForeignCode("Message box", kMessageBoxBuild);
		return false;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Message box: no executable memory for the trampoline");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress, kMessageBoxBuild,
	                              kEntryBytes, sizeof(kEntryBytes)) == 0) {
		OBVR_LOG("Message box: the trampoline does not fit");
		return false;
	}
	UInt8 patch[sizeof(kEntryBytes)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kMessageBoxBuild, reinterpret_cast<UInt32>(&HookedBuild),
	                         sizeof(kEntryBytes)) != sizeof(patch)) {
		OBVR_LOG("Message box: the patch has an unexpected length");
		return false;
	}
	Original() = reinterpret_cast<BuildFn>(trampoline);
	if (!mem::SafeWrite(kMessageBoxBuild, patch, sizeof(patch))) {
		Original() = nullptr;
		OBVR_LOG("Message box: SafeWrite to %08X failed - the boxes' texts are not logged", kMessageBoxBuild);
		return false;
	}
	OBVR_LOG("Message box: the builder at %08X logs each box's text and labels (trampoline at %08X)",
	         kMessageBoxBuild, trampolineAddress);
	return true;
}

}  // namespace obvr::game
