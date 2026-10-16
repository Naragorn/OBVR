#include "game/TutorialCards.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "ui/TutorialCards.h"

namespace obvr::game {
namespace {

// The first six bytes of the stage entry reader in 1.2.0.416, read from
// this machine's Oblivion.exe (push ebp; mov ebp, esp; sub esp, 0Ch): whole
// instructions, nothing relative, verified before patching.
constexpr UInt8 kEntryBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};

// thiscall on the stage item with one stack argument the callee pops (ret
// 4 at 0052B073), the text in eax; __fastcall with a dead edx mirrors it.
using EntryTextFn = char*(__fastcall*)(void* item, void* edx, void* quest);
EntryTextFn g_original = nullptr;

bool g_wanted = false;
UInt32 g_lines = 12;

char* __fastcall HookedEntryText(void* item, void* edx, void* quest) {
	char* text = g_original(item, edx, quest);
	if (!g_wanted || text == nullptr) {
		return text;
	}
	const char* card = ui::TutorialCard(text);
	if (card == nullptr) {
		return text;
	}
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Tutorial card: the entry \"%.40s...\" swapped for OBVR's words", text);
	}
	return const_cast<char*>(card);
}

}  // namespace

bool InstallTutorialCardsHook() {
	if (g_original != nullptr) {
		return true;
	}
	if (!mem::Verify(addr::kQuestStageEntryText, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Tutorial cards: bytes at %08X differ - the cards keep their own words",
		         addr::kQuestStageEntryText);
		mem::ReportForeignCode("Tutorial cards", addr::kQuestStageEntryText);
		return false;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Tutorial cards: no executable memory for the trampoline");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize =
		mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress, addr::kQuestStageEntryText,
		                          kEntryBytes, sizeof(kEntryBytes));
	if (trampolineSize == 0) {
		OBVR_LOG("Tutorial cards: the trampoline does not fit");
		return false;
	}
	UInt8 patch[sizeof(kEntryBytes)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), addr::kQuestStageEntryText,
	                                              reinterpret_cast<UInt32>(&HookedEntryText), sizeof(kEntryBytes));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Tutorial cards: the patch has unexpected length %u", patchSize);
		return false;
	}
	g_original = reinterpret_cast<EntryTextFn>(trampoline);
	if (!mem::SafeWrite(addr::kQuestStageEntryText, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Tutorial cards: SafeWrite to %08X failed - the cards keep their own words",
		         addr::kQuestStageEntryText);
		return false;
	}
	OBVR_LOG("Tutorial cards: the stage entry reader at %08X answers OBVR's words for %u of the Tutorial "
	         "quest's cards while the mode runs (trampoline at %08X)",
	         addr::kQuestStageEntryText, ui::kTutorialCardCount, trampolineAddress);
	return true;
}

bool TutorialCardsHookInstalled() { return g_original != nullptr; }

void SetTutorialCardsWanted(bool wanted) { g_wanted = wanted && g_original != nullptr; }

}  // namespace obvr::game
