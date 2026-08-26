#include "render/SceneRenderHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::render {
namespace {

// The two whole instructions at the entry of kRenderScene:
//   push -1
//   push 0x9AA163
// Nothing in them is relative, so they can be run from anywhere - which is
// what lets the trampoline below stand in for the original entry.
const UInt8 kRenderSceneEntry[addr::kRenderSceneEntryLength] = {
	0x6A, 0xFF, 0x68, 0x63, 0xA1, 0x9A, 0x00,
};

// The original render function is __thiscall with one stack argument. There
// is no way to spell __thiscall on a free function pointer, so it is called
// as __fastcall with a dead second register: __fastcall passes the first two
// arguments in ecx and edx, pushes the rest, and has the callee clean up -
// which for (self, unused, one argument) is byte for byte the same call.
using RenderSceneFn = void(__fastcall*)(void* self, void* unusedEdx, void* renderedTexture);

RenderSceneFn g_original = nullptr;
ScenePassCallbacks g_callbacks;

// Whether the passes below are already running. Not reachable through the
// patched entry - both calls go through the trampoline - but a guard costs a
// comparison and turns "the engine surprised us" into a pass-through instead
// of unbounded recursion.
bool g_rendering = false;

// Stands where the entry of kRenderScene used to be, with the same calling
// convention. See the type alias above for why __fastcall.
void __fastcall HookedRenderScene(void* self, void* unusedEdx, void* renderedTexture) {
	// Only the ordinary world pass is drawn twice. A non-null argument is a
	// menu wanting the world in a texture or the save game screenshot, and
	// those want exactly what they asked for.
	if (g_rendering || renderedTexture != nullptr || g_callbacks.wantsSecondPass == nullptr ||
	    !g_callbacks.wantsSecondPass()) {
		g_original(self, unusedEdx, renderedTexture);
		return;
	}

	g_rendering = true;

	// First eye. The camera hook already moved the camera there.
	g_original(self, unusedEdx, renderedTexture);
	g_callbacks.betweenPasses();

	// Second eye, from a camera one interpupillary distance over.
	g_original(self, unusedEdx, renderedTexture);
	g_callbacks.afterSecondPass();

	g_rendering = false;
}

}  // namespace

bool InstallSceneRenderHook(const ScenePassCallbacks& callbacks) {
	if (g_original != nullptr) {
		return true;
	}
	if (callbacks.wantsSecondPass == nullptr || callbacks.betweenPasses == nullptr ||
	    callbacks.afterSecondPass == nullptr) {
		return false;
	}

	// Check first, patch second - the same contract as the camera hook. A
	// mismatch means a different game version or another mod's detour already
	// sitting there, and in either case patching would shred a function the
	// game calls every frame.
	if (!mem::Verify(addr::kRenderScene, kRenderSceneEntry, sizeof(kRenderSceneEntry))) {
		OBVR_LOG("Render: bytes at %08X differ, the scene render will not be hooked",
		         addr::kRenderScene);
		return false;
	}

	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Render: no executable memory for the scene render trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, kTrampolineCapacity, trampolineAddress, addr::kRenderScene,
		kRenderSceneEntry, sizeof(kRenderSceneEntry));
	if (trampolineSize == 0) {
		OBVR_LOG("Render: the scene render trampoline does not fit");
		return false;
	}

	UInt8 patch[sizeof(kRenderSceneEntry)];
	const UInt32 patchSize = mem::BuildEntryPatch(
		patch, sizeof(patch), addr::kRenderScene,
		reinterpret_cast<UInt32>(&HookedRenderScene), sizeof(kRenderSceneEntry));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Render: the scene render patch has unexpected length %u", patchSize);
		return false;
	}

	// The callbacks and the way back in have to exist before the patch does:
	// the game may be mid-frame on another thread's timeline, and the first
	// call through the patched entry can arrive before SafeWrite returns.
	g_callbacks = callbacks;
	g_original = reinterpret_cast<RenderSceneFn>(trampoline);

	if (!mem::SafeWrite(addr::kRenderScene, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Render: SafeWrite to %08X failed, the scene render is not hooked",
		         addr::kRenderScene);
		return false;
	}

	OBVR_LOG("Render: the scene render is hooked at %08X, trampoline at %08X - the world "
	         "can now be drawn twice per frame",
	         addr::kRenderScene, trampolineAddress);
	return true;
}

bool IsSceneRenderHooked() { return g_original != nullptr; }

}  // namespace obvr::render
