// Engine-facing wiring with fake memory/engine services; no game process.
#include <cstdio>
#include <cstring>

#include "../src/game/DialogZoom.cpp"
#include "../src/game/PlayerLookAt.cpp"

namespace {
int failures = 0;
int failStage = 0;
int verifies = 0, allocations = 0, writes = 0, originals = 0;
alignas(4) UInt8 storage[24]{};
void Check(bool ok, const char* label) {
	std::printf("%s %s\n", ok ? "ok" : "FAIL", label);
	if (!ok) ++failures;
}
void __fastcall Original(UInt8*, void*, void*, float, UInt32) {
	++originals;
	Check(obvr::game::PlayerDialogActive(), "focus is captured before the original camera runs");
}
void Reset() {
	using namespace obvr::game;
	g_patched = false;
	g_refused = false;
	g_original = nullptr;
	g_zoomWanted = false;
	g_flippedForDialog = false;
	g_route = DialogZoomRoute{};
	g_dialog = DialogFocus{};
	g_wanted = false;
	g_valid = false;
	verifies = allocations = writes = originals = 0;
	g_calledWithActor.Set(false);
}
}

namespace obvr {
Config& GetConfig() { static Config config; return config; }
namespace log { void Write(const char*, ...) {} }
namespace mem {
bool Verify(UInt32, const UInt8*, UInt32) { ++verifies; return failStage != 1; }
void ReportForeignCode(const char*, UInt32) {}
void* AllocExecutable(UInt32 size) {
	++allocations;
	Check(size == sizeof(storage), "bounded trampoline allocation");
	return failStage == 2 ? nullptr : storage;
}
UInt32 BuildEntryTrampoline(UInt8*, UInt32, UInt32, UInt32, const UInt8* bytes, UInt32 size) {
	const UInt8 expected[] = {0x83, 0xEC, 0x18, 0x55, 0x8B, 0x6C, 0x24, 0x20};
	Check(size == sizeof(expected) && std::memcmp(bytes, expected, sizeof(expected)) == 0,
	      "trampoline replays the verified eight-byte complete prologue");
	return failStage == 3 ? 0 : 13;
}
UInt32 BuildEntryPatch(UInt8*, UInt32, UInt32, UInt32, UInt32 size) {
	Check(size == 8, "patch covers whole instructions");
	return failStage == 4 ? 0 : size;
}
bool SafeWrite(UInt32, const void*, UInt32) { ++writes; return failStage != 5; }
}
}

int main() {
	using namespace obvr;
	using namespace obvr::game;
	for (int zoom = 0; zoom < 2; ++zoom) {
		for (int stage = 0; stage <= 5; ++stage) {
			Reset(); failStage = stage;
			ApplyDialogZoom(zoom != 0);
			Check(g_patched == (stage == 0), "installation succeeds only after every prerequisite");
			Check(verifies == 1 && allocations == (stage == 1 ? 0 : 1), "foreign entry refused before allocation");
			Check(writes == ((stage == 0 || stage == 5) ? 1 : 0), "no patch on failed prerequisites");
			ApplyDialogZoom(zoom == 0);
			Check(verifies == 1 && allocations <= 1, "no reinstall or allocation leak after success or refusal");
			Check(g_zoomWanted == (zoom == 0), "zoom setting still updates without reinstalling observer");
		}
	}
	Reset(); failStage = 0;
	alignas(4) UInt8 player[0x600]{};
	alignas(4) UInt8 actor[0x100]{};
	const NiPoint3 npc{100, 200, 30};
	std::memcpy(actor + addr::kRefPositionOffset, &npc, sizeof(npc));
	const NiPoint3 eyes{10, 20, 130}, hand{50, 60, 300};
	SetPlayerLookAtEyes(true, true, eyes);
	DialogCameraShim(nullptr, nullptr, actor, 1, 0);
	Check(!PlayerDialogActive() && !DialogCameraCallPending(), "null player has no side effects");
	g_zoomWanted = false;
	DialogCameraShim(player, nullptr, actor, 1, 0);
	NiPoint3 speaker{}, look{};
	Check(ReadDialogSpeaker(speaker) && speaker.x == 100 && speaker.z == 30, "actual actor position copied");
	Check(DialogCameraCallPending() && TakeDialogCameraCall() && !TakeDialogCameraCall(), "approach signal is consumed once");
	// While the NPC is still coming over (no menu yet) the gaze follows the
	// live eyes - a player who walked on is looked at where they are now.
	const NiPoint3 walkedOn{30, 40, 135};
	SetPlayerLookAtEyes(true, true, walkedOn);
	OnLookAt(player, nullptr, &look);
	Check(look.x == 30 && look.z == 135 - 6, "the approach follows the live eyes");
	// The menu up: the eyes the conversation holds are frozen from here.
	StepPlayerDialog(true);
	SetPlayerLookAtEyes(true, true, hand);
	OnLookAt(player, nullptr, &look);
	Check(look.x == 30 && look.z == 135, "raised hand during conversation cannot move NPC gaze");
	player[kPlayerThirdPersonOffset] = 1;
	OnLookAt(player, nullptr, &look);
	Check(look.z == 135, "conversation gaze survives a POV switch");
	player[kPlayerThirdPersonOffset] = 0;
	DialogCameraShim(player, nullptr, actor, 1, 0);
	OnLookAt(player, nullptr, &look);
	Check(look.z == 135, "repeated camera calls do not recapture eyes");
	DialogCameraShim(player, nullptr, nullptr, 1, 0);
	Check(!PlayerDialogActive() && !ReadDialogSpeaker(speaker), "explicit end releases speaker and eyes");
	SetPlayerLookAtEyes(true, true, hand);
	OnLookAt(player, nullptr, &look);
	Check(look.z == 294, "outside dialogue ordinary eye tracking resumes");
	g_original = &Original;
	g_zoomWanted = true;
	DialogCameraShim(player, nullptr, actor, 1, 0);
	Check(originals == 1 && ReadDialogSpeaker(speaker), "vanilla zoom also observes actual speaker");
	SetPlayerLookAtEyes(false, false, eyes);
	Check(!g_dialog.eyesValid && !g_wanted, "headset disconnect invalidates frozen eyes");
	StepPlayerDialog(true); StepPlayerDialog(false);
	Check(!PlayerDialogActive() && !ReadDialogSpeaker(speaker), "missing null call is cleaned up on menu exit");
	SetPlayerLookAtEyes(true, false, eyes);
	Check(!g_valid, "invalid live camera is refused");
	SetPlayerLookAtEyes(false, true, eyes);
	Check(!g_valid, "disabled live camera is refused");
	Reset();
	DialogCameraShim(player, nullptr, reinterpret_cast<void*>(1), 1, 0);
	Check(PlayerDialogActive() && !ReadDialogSpeaker(speaker), "invalid actor address is not dereferenced");
	std::printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
