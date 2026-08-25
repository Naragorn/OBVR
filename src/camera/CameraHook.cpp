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

// VK_DELETE - the Del key above the arrow block. Chosen because vanilla
// Oblivion does not bind it, so recentering cannot collide with a game
// action.
constexpr int kRecenterVirtualKey = 0x2E;

bool g_recenterWasDown = false;

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

// Polls the recenter key once per frame.
//
// Polled rather than hooked: OBVR installs no input hook at all, and a single
// GetAsyncKeyState call per frame is far cheaper than setting one up.
//
// The edge is detected here through a stored previous state. GetAsyncKeyState
// does carry a "pressed since the last call" bit, but that bit is consumed
// process wide by whoever reads it first, so it cannot be relied on next to
// the game's own input handling.
//
// The key state is global rather than per window. That is harmless here:
// Oblivion pauses when it loses focus, so this callback does not run at all
// while another application has the keyboard.
void MaybePollRecenter() {
	const bool isDown = (GetAsyncKeyState(kRecenterVirtualKey) & 0x8000) != 0;

	if (isDown && !g_recenterWasDown) {
		g_headTracker.Recenter();
		OBVR_LOG("Camera: recentered on Del (frame %u)", g_state.frameCount);
	}

	g_recenterWasDown = isDown;
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

	if (!g_state.sawCameraNode) {
		g_state.sawCameraNode = true;
		g_state.isThirdPerson = isThirdPerson;
		OBVR_LOG("Camera: first hook pass, CameraNode=%08X, %s",
		         reinterpret_cast<UInt32>(cameraNode),
		         isThirdPerson ? "third person" : "first person");
	} else if (isThirdPerson != g_state.isThirdPerson) {
		g_state.isThirdPerson = isThirdPerson;
		OBVR_LOG("Camera: switched to %s (frame %u)",
		         isThirdPerson ? "third person" : "first person",
		         g_state.frameCount);
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
	if (config.logEveryFrames != 0 && (g_state.frameCount % config.logEveryFrames) == 0) {
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
