#include "render/InterfaceRenderHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/PresentHook.h"

namespace obvr::render {
namespace {

// The two whole instructions at the entry of kRenderInterface:
//   push -1
//   push 0x9BEAE6
// The same relocatable shape as the scene render's entry, differing only in
// the SEH handler address the second push carries.
const UInt8 kRenderInterfaceEntry[addr::kRenderInterfaceEntryLength] = {
	0x6A, 0xFF, 0x68, 0xE6, 0xEA, 0x9B, 0x00,
};

// __thiscall with one stack argument, called as __fastcall with a dead edx -
// the same ABI identity the scene render hook rests on.
using RenderInterfaceFn = void(__fastcall*)(void* self, void* unusedEdx,
                                            void* renderedTexture);

RenderInterfaceFn g_original = nullptr;
InterfaceRedirect g_callbacks;

// The SetRenderTarget substitution. Installed lazily, on the first pass that
// redirects, because the device this table belongs to does not exist when the
// entry detour goes in.
d3d9::SetRenderTargetFn g_originalSetTarget = nullptr;
d3d9::SetRenderStateFn g_originalSetState = nullptr;
bool g_targetHookRefused = false;

// While true, any colour target the pass sets is replaced with the substitute.
bool g_redirecting = false;
void* g_substitute = nullptr;

// The last target the pass asked for while redirected, so the device can be
// left in the state the game believes it is in. No reference is held: the
// game holds its own, and the pointer is used within the same call.
void* g_lastRequested = nullptr;

SInt32 __stdcall HookedSetRenderTarget(void* self, UInt32 index, void* surface) {
	if (g_redirecting && index == 0 && surface != nullptr && surface != g_substitute) {
		g_lastRequested = surface;
		surface = g_substitute;
	}
	return g_originalSetTarget(self, index, surface);
}

// While the pass is redirected, whatever colour write mask it sets keeps the
// alpha bit. The game's own back buffer has no alpha channel, so the engine
// is entitled to switch alpha writes off whenever it likes - but everything
// it draws into the redirect texture with the bit off lands at alpha zero,
// and an overlay renders alpha zero as nothing at all.
SInt32 __stdcall HookedSetRenderState(void* self, UInt32 state, UInt32 value) {
	if (g_redirecting && state == d3d9::kRenderStateColorWriteEnable) {
		value |= d3d9::kColorWriteAlpha;
	}
	return g_originalSetState(self, state, value);
}

// Makes one table entry writable, changes it, and puts the protection back -
// the same three steps as the Present hook, on the same table, two entries
// apart.
bool WriteTableEntry(void** vtable, UInt32 index, void* value) {
	void** slot = &vtable[index];

	DWORD previous = 0;
	if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previous)) {
		return false;
	}

	*slot = value;

	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(void*), previous, &ignored);
	return true;
}

bool EnsureTargetHook() {
	if (g_originalSetTarget != nullptr) {
		return true;
	}
	if (g_targetHookRefused) {
		return false;
	}

	void* device = GetGameDevice();
	if (device == nullptr) {
		// Not a refusal: the device can arrive later, and the next pass asks
		// again.
		return false;
	}

	auto** vtable = *reinterpret_cast<void***>(device);
	if (!LooksLikeVtable(vtable, d3d9::kDeviceSetRenderTarget + 1)) {
		g_targetHookRefused = true;
		OBVR_LOG("Hud: the device's method table does not look like one, so the 2D layer "
		         "stays in the frame");
		return false;
	}

	auto originalTarget =
		reinterpret_cast<d3d9::SetRenderTargetFn>(vtable[d3d9::kDeviceSetRenderTarget]);
	auto originalState =
		reinterpret_cast<d3d9::SetRenderStateFn>(vtable[d3d9::kDeviceSetRenderState]);
	if (originalTarget == nullptr || originalState == nullptr) {
		g_targetHookRefused = true;
		OBVR_LOG("Hud: the device's table holds a null method, so the 2D layer stays in "
		         "the frame");
		return false;
	}

	// The originals before the patches: the first call through a patched
	// entry can arrive while this function is still on the second one.
	g_originalSetTarget = originalTarget;
	g_originalSetState = originalState;

	if (!WriteTableEntry(vtable, d3d9::kDeviceSetRenderTarget,
	                     reinterpret_cast<void*>(&HookedSetRenderTarget))) {
		g_targetHookRefused = true;
		g_originalSetTarget = nullptr;
		g_originalSetState = nullptr;
		OBVR_LOG("Hud: SetRenderTarget could not be replaced, so the 2D layer stays in "
		         "the frame");
		return false;
	}
	if (!WriteTableEntry(vtable, d3d9::kDeviceSetRenderState,
	                     reinterpret_cast<void*>(&HookedSetRenderState))) {
		// Nothing half-patched is left behind: the first entry goes back
		// before this reports failure.
		WriteTableEntry(vtable, d3d9::kDeviceSetRenderTarget,
		                reinterpret_cast<void*>(originalTarget));
		g_targetHookRefused = true;
		g_originalSetTarget = nullptr;
		g_originalSetState = nullptr;
		OBVR_LOG("Hud: SetRenderState could not be replaced, so the 2D layer stays in "
		         "the frame");
		return false;
	}

	OBVR_LOG("Hud: SetRenderTarget and SetRenderState hooked at table entries %u and %u - "
	         "the 2D pass can be pointed elsewhere, with its alpha kept",
	         d3d9::kDeviceSetRenderTarget, d3d9::kDeviceSetRenderState);
	return true;
}

void __fastcall HookedRenderInterface(void* self, void* unusedEdx, void* renderedTexture) {
	// The order matters: the target hook has to exist before the callbacks
	// change any state, or a pass that could not be redirected would still
	// have its blend states rearranged and its capture claimed.
	if (g_callbacks.beginRedirect == nullptr || !EnsureTargetHook()) {
		g_original(self, unusedEdx, renderedTexture);
		return;
	}

	void* substitute = g_callbacks.beginRedirect();
	if (substitute == nullptr) {
		g_original(self, unusedEdx, renderedTexture);
		return;
	}

	// What the device is aiming at now, so it can be put back if the pass
	// never sets a target of its own. GetRenderTarget adds a reference.
	void* device = GetGameDevice();
	void* previous = nullptr;
	auto getTarget =
		d3d9::Method<d3d9::GetRenderTargetFn>(device, d3d9::kDeviceGetRenderTarget);
	if (getTarget != nullptr) {
		getTarget(device, 0, &previous);
	}

	g_substitute = substitute;
	g_lastRequested = nullptr;
	g_redirecting = true;

	// Aimed before the pass starts, through the original entry: the pass
	// begins its own target group, and that is intercepted - but a branch
	// that finds a group already current sets nothing, and the layer would
	// otherwise land in the frame after all.
	g_originalSetTarget(device, 0, substitute);

	g_original(self, unusedEdx, renderedTexture);

	g_redirecting = false;

	// The device ends the call aiming where the game last aimed it - at what
	// the pass asked for, or failing that at whatever was current before.
	void* restore = g_lastRequested != nullptr ? g_lastRequested : previous;
	if (restore != nullptr) {
		g_originalSetTarget(device, 0, restore);
	}
	if (previous != nullptr) {
		// The reference GetRenderTarget added. Two vtable steps: IUnknown's
		// Release is entry 2 on every COM object.
		using ReleaseFn = UInt32(__stdcall*)(void*);
		if (auto release = d3d9::Method<ReleaseFn>(previous, 2)) {
			release(previous);
		}
	}

	g_substitute = nullptr;
	g_callbacks.endRedirect();
}

}  // namespace

bool InstallInterfaceRenderHook(const InterfaceRedirect& callbacks) {
	if (g_original != nullptr) {
		return true;
	}
	if (callbacks.beginRedirect == nullptr || callbacks.endRedirect == nullptr) {
		return false;
	}

	if (!mem::Verify(addr::kRenderInterface, kRenderInterfaceEntry,
	                 sizeof(kRenderInterfaceEntry))) {
		OBVR_LOG("Hud: bytes at %08X differ, the 2D pass will not be hooked",
		         addr::kRenderInterface);
		return false;
	}

	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Hud: no executable memory for the 2D pass trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, kTrampolineCapacity, trampolineAddress, addr::kRenderInterface,
		kRenderInterfaceEntry, sizeof(kRenderInterfaceEntry));
	if (trampolineSize == 0) {
		OBVR_LOG("Hud: the 2D pass trampoline does not fit");
		return false;
	}

	UInt8 patch[sizeof(kRenderInterfaceEntry)];
	const UInt32 patchSize = mem::BuildEntryPatch(
		patch, sizeof(patch), addr::kRenderInterface,
		reinterpret_cast<UInt32>(&HookedRenderInterface), sizeof(kRenderInterfaceEntry));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Hud: the 2D pass patch has unexpected length %u", patchSize);
		return false;
	}

	g_callbacks = callbacks;
	g_original = reinterpret_cast<RenderInterfaceFn>(trampoline);

	if (!mem::SafeWrite(addr::kRenderInterface, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Hud: SafeWrite to %08X failed, the 2D pass is not hooked",
		         addr::kRenderInterface);
		return false;
	}

	OBVR_LOG("Hud: the 2D pass is hooked at %08X, trampoline at %08X - the layer can "
	         "leave the frame",
	         addr::kRenderInterface, trampolineAddress);
	return true;
}

bool IsInterfaceRenderHooked() { return g_original != nullptr; }

}  // namespace obvr::render
