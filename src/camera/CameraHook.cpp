#include "camera/CameraHook.h"

#include "camera/CameraTrampoline.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

KeyEdge g_recenterEdge;

bool ReadIsThirdPerson() {
	auto* player = *reinterpret_cast<UInt8**>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}
	return player[addr::kPlayerIsThirdPersonOffset] != 0;
}

void MaybeReloadConfig() {
	Config& config = GetConfig();
	if (!IsDue(g_state.frameCount, config.reloadEveryFrames)) {
		return;
	}

	if (config.Reload("OBVR.ini")) {
		g_headTracker.Configure(config.tracker);
	}
}

// Polls the recenter key once per frame.
//
// Polled rather than hooked, and the reason is robustness, not speed. The
// obvious alternative would be a WH_KEYBOARD_LL hook, but Microsoft documents
// three properties that rule it out here:
//
//   * "This hook is called in the context of the thread that installed it.
//     The call is made by sending a message to the thread that installed the
//     hook. Therefore, the thread that installed the hook must have a message
//     loop." OBVR is a plugin inside Oblivion and owns no message loop it can
//     service on its own terms.
//   * The hook sits in the system-wide input path. Every keystroke in every
//     application waits for it to sign off before the key state updates.
//   * "If the hook procedure times out ... on Windows 7 and later, the hook is
//     silently removed without being called. There is no way for the
//     application to know whether the hook is removed." A game that stutters
//     is precisely where a timeout happens, and OBVR could not even detect
//     that recentering had stopped working.
//
// Microsoft's own advice is to prefer raw input over low-level hooks. That
// would work, but it needs a window to register against and a message queue to
// drain - considerably more machinery than one call per rendered frame.
//
// Cost is not the argument either way: this runs once per frame rather than in
// a spin loop, so it is a single user32 call every 8 to 16 milliseconds.
//
// The edge itself is detected by KeyEdge in FrameLogic.h, from a stored
// previous state. GetAsyncKeyState does carry a "pressed since the last call"
// bit, but that bit is consumed process wide by whoever reads it first, so it
// cannot be relied on next to the game's own input handling.
//
// The key state is global rather than per window. That is harmless here:
// Oblivion pauses when it loses focus, so this callback does not run at all
// while another application has the keyboard.
void MaybePollRecenter() {
	const UInt32 key = GetConfig().recenterKey;
	if (key == 0) {
		// Explicitly disabled in the configuration. Forgetting the previous
		// state matters here: a key held down while recentering is switched
		// off would otherwise fire an edge the moment it is switched back on.
		g_recenterEdge.Reset();
		return;
	}

	const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;

	if (g_recenterEdge.Update(isDown)) {
		g_headTracker.Recenter();
		OBVR_LOG("Camera: recentered on key 0x%02X (frame %u)", key, g_state.frameCount);
	}
}

}  // namespace

// Called from the trampoline after Oblivion has finished computing the
// camera. eax held the CameraNode there; the trampoline passes it through as
// the single argument.
//
// All registers are saved at this point, so this function may be ordinary
// C++. It does have to stay fast and free of exceptions though - it runs on
// every rendered frame.
extern "C" void __cdecl OBVR_OnCameraUpdated(NiAVObject* cameraNode) {
	if (cameraNode == nullptr) {
		return;
	}

	const bool isThirdPerson = ReadIsThirdPerson();

	switch (g_state.ObservePointOfView(isThirdPerson)) {
	case PovEvent::FirstPass:
		OBVR_LOG("Camera: first hook pass, CameraNode=%08X, %s",
		         reinterpret_cast<UInt32>(cameraNode),
		         isThirdPerson ? "third person" : "first person");
		break;
	case PovEvent::Switched:
		OBVR_LOG("Camera: switched to %s (frame %u)",
		         isThirdPerson ? "third person" : "first person",
		         g_state.frameCount);
		break;
	case PovEvent::Unchanged:
		break;
	}

	++g_state.frameCount;
	MaybeReloadConfig();

	g_headTracker.Update(g_state.frameCount);

	// After Update, so that the recenter reference is this frame's
	// orientation rather than the previous one. The new zero therefore takes
	// effect from the next frame - a single frame of delay that nobody can
	// see, in exchange for the reference being exactly the pose the user was
	// holding when they pressed the key.
	MaybePollRecenter();

	const Config& config = GetConfig();
	if (IsDue(g_state.frameCount, config.logEveryFrames)) {
		const vr::Quaternion& raw = g_headTracker.GetRawOrientation();
		const NiPoint3& pos = cameraNode->localTransform.pos;
		OBVR_LOG("Camera: frame %u, %s, pos=(%.1f, %.1f, %.1f), head=(%.3f, %.3f, %.3f, %.3f)",
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

	// The heart of it: the vanilla rotation stays the base, the head rotation
	// acts in local camera space.
	cameraNode->localTransform.rot =
		cameraNode->localTransform.rot * g_headTracker.GetCameraRotation();
}

vr::HeadTracker& GetHeadTracker() { return g_headTracker; }

const State& GetState() { return g_state; }

bool Install() {
	const Config& config = GetConfig();
	g_headTracker.Configure(config.tracker);

	// Check first, patch second. If something other than the expected bytes
	// sits there, it is a different game version or another mod got there
	// first - in either case patching would be a shot in the dark.
	if (!mem::Verify(addr::kHookCameraUpdate, kOriginalBytes, addr::kHookCameraUpdatePatchSize)) {
		OBVR_LOG("Camera: bytes at %08X differ, hook will not be installed",
		         addr::kHookCameraUpdate);
		return false;
	}

	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Camera: no executable memory for the trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = BuildTrampoline(
		trampoline, kTrampolineSize, trampolineAddress,
		reinterpret_cast<UInt32>(&OBVR_OnCameraUpdated));

	if (trampolineSize == 0) {
		OBVR_LOG("Camera: trampoline does not fit into %u bytes", kTrampolineSize);
		return false;
	}

	UInt8 patch[addr::kHookCameraUpdatePatchSize];
	const UInt32 patchSize =
		BuildPatch(patch, sizeof(patch), addr::kHookCameraUpdate, trampolineAddress);

	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Camera: patch has unexpected length %u", patchSize);
		return false;
	}

	if (!mem::SafeWrite(addr::kHookCameraUpdate, patch, patchSize)) {
		OBVR_LOG("Camera: SafeWrite to %08X failed", addr::kHookCameraUpdate);
		return false;
	}

	OBVR_LOG("Camera: hook installed at %08X, trampoline at %08X (%u bytes)",
	         addr::kHookCameraUpdate, trampolineAddress, trampolineSize);
	return true;
}

}  // namespace obvr::camera
