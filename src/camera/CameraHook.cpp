#include "camera/CameraHook.h"

#include "camera/CameraTrampoline.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

bool ReadIsThirdPerson() {
	auto* player = *reinterpret_cast<UInt8**>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}
	return player[addr::kPlayerIsThirdPersonOffset] != 0;
}

void MaybeReloadConfig() {
	Config& config = GetConfig();
	if (config.reloadEveryFrames == 0) {
		return;
	}
	if ((g_state.frameCount % config.reloadEveryFrames) != 0) {
		return;
	}

	if (config.Reload("OBVR.ini")) {
		g_headTracker.Configure(config.tracker);
	}
}

}  // namespace

// Wird vom Trampolin gerufen, nachdem Oblivion die Kamera fertig berechnet
// hat. eax hielt dort den CameraNode; das Trampolin reicht ihn als einziges
// Argument durch.
//
// Alle Register sind zu diesem Zeitpunkt gesichert, diese Funktion darf also
// normal C++ sein. Sie muss aber schnell und ausnahmefrei bleiben - sie
// laeuft in jedem gerenderten Frame.
extern "C" void __cdecl OBVR_OnCameraUpdated(NiAVObject* cameraNode) {
	if (cameraNode == nullptr) {
		return;
	}

	const bool isThirdPerson = ReadIsThirdPerson();

	if (!g_state.sawCameraNode) {
		g_state.sawCameraNode = true;
		g_state.isThirdPerson = isThirdPerson;
		OBVR_LOG("Kamera: erster Hook-Durchlauf, CameraNode=%08X, %s",
		         reinterpret_cast<UInt32>(cameraNode),
		         isThirdPerson ? "Third Person" : "First Person");
	} else if (isThirdPerson != g_state.isThirdPerson) {
		g_state.isThirdPerson = isThirdPerson;
		OBVR_LOG("Kamera: Wechsel nach %s (Frame %u)",
		         isThirdPerson ? "Third Person" : "First Person",
		         g_state.frameCount);
	}

	++g_state.frameCount;
	MaybeReloadConfig();

	g_headTracker.Update(g_state.frameCount);

	const Config& config = GetConfig();
	if (config.logEveryFrames != 0 && (g_state.frameCount % config.logEveryFrames) == 0) {
		const vr::Quaternion& raw = g_headTracker.GetRawOrientation();
		const NiPoint3& pos = cameraNode->localTransform.pos;
		OBVR_LOG("Kamera: Frame %u, %s, pos=(%.1f, %.1f, %.1f), Kopf=(%.3f, %.3f, %.3f, %.3f)",
		         g_state.frameCount,
		         isThirdPerson ? "3rd" : "1st",
		         static_cast<double>(pos.x),
		         static_cast<double>(pos.y),
		         static_cast<double>(pos.z),
		         static_cast<double>(raw.x),
		         static_cast<double>(raw.y),
		         static_cast<double>(raw.z),
		         static_cast<double>(raw.w));
	}

	// Der Kern: die Vanilla-Rotation bleibt die Basis, die Kopfrotation wirkt
	// im lokalen Kameraraum.
	cameraNode->localTransform.rot =
		cameraNode->localTransform.rot * g_headTracker.GetCameraRotation();
}

vr::HeadTracker& GetHeadTracker() { return g_headTracker; }

const State& GetState() { return g_state; }

bool Install() {
	const Config& config = GetConfig();
	g_headTracker.Configure(config.tracker);

	// Erst pruefen, dann patchen. Steht dort etwas anderes als erwartet, ist
	// es eine andere Spielversion oder ein anderer Mod war zuerst da - in
	// beiden Faellen waere ein Patch ein Schuss ins Blaue.
	if (!mem::Verify(addr::kHookCameraUpdate, kOriginalBytes, addr::kHookCameraUpdatePatchSize)) {
		OBVR_LOG("Kamera: Bytes an %08X weichen ab, Hook wird nicht gesetzt",
		         addr::kHookCameraUpdate);
		return false;
	}

	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Kamera: kein ausfuehrbarer Speicher fuer das Trampolin");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = BuildTrampoline(
		trampoline, kTrampolineSize, trampolineAddress,
		reinterpret_cast<UInt32>(&OBVR_OnCameraUpdated));

	if (trampolineSize == 0) {
		OBVR_LOG("Kamera: Trampolin passt nicht in %u Bytes", kTrampolineSize);
		return false;
	}

	UInt8 patch[addr::kHookCameraUpdatePatchSize];
	const UInt32 patchSize =
		BuildPatch(patch, sizeof(patch), addr::kHookCameraUpdate, trampolineAddress);

	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Kamera: Patch hat unerwartete Laenge %u", patchSize);
		return false;
	}

	if (!mem::SafeWrite(addr::kHookCameraUpdate, patch, patchSize)) {
		OBVR_LOG("Kamera: SafeWrite auf %08X fehlgeschlagen", addr::kHookCameraUpdate);
		return false;
	}

	OBVR_LOG("Kamera: Hook auf %08X gesetzt, Trampolin bei %08X (%u Bytes)",
	         addr::kHookCameraUpdate, trampolineAddress, trampolineSize);
	return true;
}

}  // namespace obvr::camera
