#include "game/ControlWords.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "ui/ControlPhrase.h"

namespace obvr::game {
namespace {

// The first seven bytes of the key-name formatter in 1.2.0.416, read from
// this machine's Oblivion.exe (mov eax, [esp+4]; sub esp, 8): whole
// instructions, nothing relative, verified before patching.
constexpr UInt8 kEntryBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x08};

// cdecl: the control index and the buffer on the stack, a plain ret
// (0057C340, 0057C362), the caller cleaning up.
using KeyNameFn = void(__cdecl*)(UInt32 control, char* out);
KeyNameFn g_original = nullptr;

bool g_wanted = false;
bool g_leftHanded = false;
ui::BowWords g_bow = ui::BowWords::HandTrigger;
UInt32 g_lines = 16;

void __cdecl HookedKeyName(UInt32 control, char* out) {
	if (!g_wanted || out == nullptr) {
		g_original(control, out);
		return;
	}
	char phrase[ui::kControlPhraseCapacity];
	if (!ui::ControlPhrase(control, g_leftHanded, g_bow, phrase, sizeof(phrase))) {
		g_original(control, out);
		return;
	}
	if (g_lines > 0) {
		--g_lines;
		// The engine's own answer beside OBVR's, read into a buffer of the
		// size the layout hands over.
		char engine[1024] = "";
		g_original(control, engine);
		OBVR_LOG("Control words: control %u (%s) answered \"%s\" (the engine's own \"%s\")", control,
		         ui::ControlName(control), phrase, engine);
	}
	UInt32 i = 0;
	for (; phrase[i] != '\0'; ++i) {
		out[i] = phrase[i];
	}
	out[i] = '\0';
}

}  // namespace

bool InstallControlWordsHook() {
	if (g_original != nullptr) {
		return true;
	}
	if (!mem::Verify(addr::kControlKeyName, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Control words: bytes at %08X differ - the texts keep the engine's key names",
		         addr::kControlKeyName);
		mem::ReportForeignCode("Control words", addr::kControlKeyName);
		return false;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Control words: no executable memory for the trampoline");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize =
		mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress, addr::kControlKeyName,
		                          kEntryBytes, sizeof(kEntryBytes));
	if (trampolineSize == 0) {
		OBVR_LOG("Control words: the trampoline does not fit");
		return false;
	}
	UInt8 patch[sizeof(kEntryBytes)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), addr::kControlKeyName,
	                                              reinterpret_cast<UInt32>(&HookedKeyName), sizeof(kEntryBytes));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Control words: the patch has unexpected length %u", patchSize);
		return false;
	}
	g_original = reinterpret_cast<KeyNameFn>(trampoline);
	if (!mem::SafeWrite(addr::kControlKeyName, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Control words: SafeWrite to %08X failed - the texts keep the engine's key names",
		         addr::kControlKeyName);
		return false;
	}
	OBVR_LOG("Control words: the key-name formatter at %08X answers in the mode's words while it runs "
	         "(trampoline at %08X)",
	         addr::kControlKeyName, trampolineAddress);
	return true;
}

bool ControlWordsHookInstalled() { return g_original != nullptr; }

void SetControlWordsWanted(bool wanted, bool leftHanded, ui::BowWords bow) {
	g_wanted = wanted && g_original != nullptr;
	g_leftHanded = leftHanded;
	g_bow = bow;
}

}  // namespace obvr::game
