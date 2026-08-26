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

// While true, the back buffer - and only the back buffer - is replaced as a
// colour target with the substitute.
bool g_redirecting = false;
void* g_substitute = nullptr;

// The back buffer's own surface, held with the reference GetBackBuffer added,
// so the substitution can match it exactly. Matching exactly matters: the
// pass may render interface elements into intermediate textures of its own
// before compositing them, and hijacking those would steal the pieces the
// composite step then reads back - a HUD that vanishes everywhere at once.
// Only "aim at the back buffer" means "aim at ours instead"; every other
// target is the pass's own business.
void* g_backBuffer = nullptr;

// The last target the pass asked for while redirected, so the device can be
// left in the state the game believes it is in. No reference is held: the
// game holds its own, and the pointer is used within the same call.
void* g_lastRequested = nullptr;

// What the redirected pass actually does, counted for the first few passes
// and reported once each. The texture came back holding nothing at all, so
// the question is no longer how the pixels look but whether the pass issued
// a single draw while redirected - and if it did, at which targets it was
// aiming.
UInt32 g_statsDraws = 0;
UInt32 g_statsFailedDraws = 0;
UInt32 g_statsMatched = 0;
UInt32 g_statsOtherTargets = 0;
UInt32 g_passTraceLeft = 3;

// Which of the four draw entries the pass used, whether the next redirected
// draw should log the pipeline it runs on, and the probe clear's own trace
// budget.
UInt32 g_statsKind[4] = {};
bool g_sampleNextDraw = false;
UInt32 g_probeClearTraceLeft = 3;

d3d9::DrawPrimitiveFn g_originalDrawPrimitive = nullptr;
d3d9::DrawIndexedPrimitiveFn g_originalDrawIndexed = nullptr;
d3d9::DrawPrimitiveUPFn g_originalDrawUP = nullptr;
d3d9::DrawIndexedPrimitiveUPFn g_originalDrawIndexedUP = nullptr;

SInt32 __stdcall HookedSetRenderTarget(void* self, UInt32 index, void* surface) {
	if (g_redirecting && index == 0 && surface != nullptr && surface == g_backBuffer) {
		++g_statsMatched;
		g_lastRequested = surface;
		surface = g_substitute;
	} else if (g_redirecting && index == 0 && surface != nullptr &&
	           surface != g_substitute) {
		++g_statsOtherTargets;
	}
	return g_originalSetTarget(self, index, surface);
}

// The pipeline at the moment of the first redirected draw, as one line of
// evidence. The pass-entry snapshot already showed the rejection states off,
// but the entry is not the draw: twenty-two successful draws still arrived
// nowhere, so what the device was set to when the game actually drew is
// measured rather than assumed - viewport, blending, masks, and whether the
// draw ran on shaders or fixed function.
void SampleFirstDraw(void* device, const char* kind, UInt32 type, UInt32 count) {
	d3d9::Viewport viewport{};
	if (auto getViewport =
	        d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport)) {
		getViewport(device, &viewport);
	}

	UInt32 blend = 0;
	UInt32 src = 0;
	UInt32 dst = 0;
	UInt32 write = 0;
	UInt32 zEnable = 0;
	UInt32 alphaTest = 0;
	UInt32 scissor = 0;
	UInt32 stencil = 0;
	if (auto getState =
	        d3d9::Method<d3d9::GetRenderStateFn>(device, d3d9::kDeviceGetRenderState)) {
		getState(device, 27, &blend);      // D3DRS_ALPHABLENDENABLE
		getState(device, d3d9::kRenderStateSrcBlend, &src);
		getState(device, d3d9::kRenderStateDestBlend, &dst);
		getState(device, d3d9::kRenderStateColorWriteEnable, &write);
		getState(device, 7, &zEnable);     // D3DRS_ZENABLE
		getState(device, 15, &alphaTest);  // D3DRS_ALPHATESTENABLE
		getState(device, 174, &scissor);   // D3DRS_SCISSORTESTENABLE
		getState(device, d3d9::kRenderStateStencilEnable, &stencil);
	}

	UInt32 fvf = 0;
	void* vertexShader = nullptr;
	void* pixelShader = nullptr;
	if (auto getFvf = d3d9::Method<d3d9::GetFVFFn>(device, d3d9::kDeviceGetFVF)) {
		getFvf(device, &fvf);
	}
	if (auto getShader =
	        d3d9::Method<d3d9::GetShaderFn>(device, d3d9::kDeviceGetVertexShader)) {
		getShader(device, &vertexShader);
	}
	if (auto getShader =
	        d3d9::Method<d3d9::GetShaderFn>(device, d3d9::kDeviceGetPixelShader)) {
		getShader(device, &pixelShader);
	}

	OBVR_LOG("Hud first draw: %s type=%u count=%u viewport=%ux%u at %u,%u blend=%u "
	         "src=%u dst=%u write=0x%X z=%u alphaTest=%u scissor=%u stencil=%u "
	         "fvf=%08X vs=%s ps=%s",
	         kind, type, count, viewport.width, viewport.height, viewport.x, viewport.y,
	         blend, src, dst, write, zEnable, alphaTest, scissor, stencil, fvf,
	         vertexShader != nullptr ? "bound" : "null",
	         pixelShader != nullptr ? "bound" : "null");

	// The references the two shader getters added. IUnknown's Release is
	// entry 2 on every COM object.
	using ReleaseFn = UInt32(__stdcall*)(void*);
	if (vertexShader != nullptr) {
		if (auto release = d3d9::Method<ReleaseFn>(vertexShader, 2)) {
			release(vertexShader);
		}
	}
	if (pixelShader != nullptr) {
		if (auto release = d3d9::Method<ReleaseFn>(pixelShader, 2)) {
			release(pixelShader);
		}
	}
}

SInt32 __stdcall HookedDrawPrimitive(void* self, UInt32 type, UInt32 startVertex,
                                     UInt32 primitiveCount) {
	if (g_redirecting && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dp", type, primitiveCount);
	}
	const SInt32 result = g_originalDrawPrimitive(self, type, startVertex, primitiveCount);
	if (g_redirecting) {
		++g_statsDraws;
		++g_statsKind[0];
		if (result < 0) {
			++g_statsFailedDraws;
		}
	}
	return result;
}

SInt32 __stdcall HookedDrawIndexedPrimitive(void* self, UInt32 type, SInt32 baseVertexIndex,
                                            UInt32 minVertexIndex, UInt32 numVertices,
                                            UInt32 startIndex, UInt32 primCount) {
	if (g_redirecting && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dip", type, primCount);
	}
	const SInt32 result = g_originalDrawIndexed(self, type, baseVertexIndex, minVertexIndex,
	                                            numVertices, startIndex, primCount);
	if (g_redirecting) {
		++g_statsDraws;
		++g_statsKind[1];
		if (result < 0) {
			++g_statsFailedDraws;
		}
	}
	return result;
}

SInt32 __stdcall HookedDrawPrimitiveUP(void* self, UInt32 type, UInt32 primitiveCount,
                                       const void* vertexData, UInt32 stride) {
	if (g_redirecting && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dpup", type, primitiveCount);
	}
	const SInt32 result = g_originalDrawUP(self, type, primitiveCount, vertexData, stride);
	if (g_redirecting) {
		++g_statsDraws;
		++g_statsKind[2];
		if (result < 0) {
			++g_statsFailedDraws;
		}
	}
	return result;
}

SInt32 __stdcall HookedDrawIndexedPrimitiveUP(void* self, UInt32 type, UInt32 minVertexIndex,
                                              UInt32 numVertices, UInt32 primitiveCount,
                                              const void* indexData, UInt32 indexFormat,
                                              const void* vertexData, UInt32 stride) {
	if (g_redirecting && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dipup", type, primitiveCount);
	}
	const SInt32 result =
		g_originalDrawIndexedUP(self, type, minVertexIndex, numVertices, primitiveCount,
	                            indexData, indexFormat, vertexData, stride);
	if (g_redirecting) {
		++g_statsDraws;
		++g_statsKind[3];
		if (result < 0) {
			++g_statsFailedDraws;
		}
	}
	return result;
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

	// The surface the substitution matches against. The reference is kept for
	// the life of the hook: the pointer is compared every SetRenderTarget,
	// and a released surface could be reallocated as something else.
	if (g_backBuffer == nullptr) {
		auto getBackBuffer =
			d3d9::Method<d3d9::GetBackBufferFn>(device, d3d9::kDeviceGetBackBuffer);
		if (getBackBuffer == nullptr ||
		    getBackBuffer(device, 0, 0, d3d9::kBackBufferTypeMono, &g_backBuffer) < 0 ||
		    g_backBuffer == nullptr) {
			g_backBuffer = nullptr;
			return false;
		}
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

	// The draw counters. Diagnostic rather than load-bearing, so a failure
	// here only costs the count: the pass-through hooks count while the
	// redirect flag is up and are inert otherwise.
	g_originalDrawPrimitive = reinterpret_cast<d3d9::DrawPrimitiveFn>(
		vtable[d3d9::kDeviceDrawPrimitive]);
	g_originalDrawIndexed = reinterpret_cast<d3d9::DrawIndexedPrimitiveFn>(
		vtable[d3d9::kDeviceDrawIndexedPrimitive]);
	g_originalDrawUP = reinterpret_cast<d3d9::DrawPrimitiveUPFn>(
		vtable[d3d9::kDeviceDrawPrimitiveUP]);
	g_originalDrawIndexedUP = reinterpret_cast<d3d9::DrawIndexedPrimitiveUPFn>(
		vtable[d3d9::kDeviceDrawIndexedPrimitiveUP]);
	if (g_originalDrawPrimitive != nullptr && g_originalDrawIndexed != nullptr &&
	    g_originalDrawUP != nullptr && g_originalDrawIndexedUP != nullptr) {
		const bool drawsHooked =
			WriteTableEntry(vtable, d3d9::kDeviceDrawPrimitive,
		                    reinterpret_cast<void*>(&HookedDrawPrimitive)) &&
			WriteTableEntry(vtable, d3d9::kDeviceDrawIndexedPrimitive,
		                    reinterpret_cast<void*>(&HookedDrawIndexedPrimitive)) &&
			WriteTableEntry(vtable, d3d9::kDeviceDrawPrimitiveUP,
		                    reinterpret_cast<void*>(&HookedDrawPrimitiveUP)) &&
			WriteTableEntry(vtable, d3d9::kDeviceDrawIndexedPrimitiveUP,
		                    reinterpret_cast<void*>(&HookedDrawIndexedPrimitiveUP));
		if (!drawsHooked) {
			OBVR_LOG("Hud: the draw counters could not all be installed");
		}
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

	const bool probe = g_callbacks.probeActive != nullptr && g_callbacks.probeActive();

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
	if (g_passTraceLeft > 0) {
		g_statsDraws = 0;
		g_statsFailedDraws = 0;
		g_statsMatched = 0;
		g_statsOtherTargets = 0;
		g_statsKind[0] = g_statsKind[1] = g_statsKind[2] = g_statsKind[3] = 0;
	}
	g_sampleNextDraw = g_passTraceLeft > 0;
	g_redirecting = true;

	// Aimed before the pass starts, through the original entry: the pass
	// begins its own target group, and that is intercepted - but a branch
	// that finds a group already current sets nothing, and the layer would
	// otherwise land in the frame after all.
	const SInt32 aimResult = g_originalSetTarget(device, 0, substitute);

	// Identity, not assumption: twenty-two draws land nowhere visible, so
	// where the device was actually aiming - as it reports it, not as this
	// code intended it - is the question. Checked after the aim and after
	// the pass, against both candidates.
	const bool tracing = g_passTraceLeft > 0;
	if (tracing) {
		void* afterAim = nullptr;
		if (getTarget != nullptr) {
			getTarget(device, 0, &afterAim);
		}
		OBVR_LOG("Hud aim: SetRenderTarget=%08X, RT0 %s (ours=%08X, back=%08X, got=%08X)",
		         static_cast<UInt32>(aimResult),
		         afterAim == substitute ? "is ours"
		                                : (afterAim == g_backBuffer ? "is the back buffer"
		                                                            : "is something else"),
		         reinterpret_cast<UInt32>(substitute),
		         reinterpret_cast<UInt32>(g_backBuffer),
		         reinterpret_cast<UInt32>(afterAim));
		if (afterAim != nullptr) {
			using ReleaseFn = UInt32(__stdcall*)(void*);
			if (auto release = d3d9::Method<ReleaseFn>(afterAim, 2)) {
				release(afterAim);
			}
		}
	}

	// The probe clear: the smallest write that goes through the render target
	// binding the draws use. ColorFill writes to the surface by name and its
	// red square arrives in the headset; if this orange does not arrive the
	// same way, the binding the device just confirmed does not reach the
	// texture the compositor shows - and the pass's draws never had a chance.
	// Alpha 0x60, so the world stays visible behind it.
	if (probe) {
		if (auto clear = d3d9::Method<d3d9::ClearFn>(device, d3d9::kDeviceClear)) {
			const SInt32 clearResult =
				clear(device, 0, nullptr, d3d9::kClearTarget, 0x60FF8000u, 1.0f, 0);
			if (g_probeClearTraceLeft > 0) {
				--g_probeClearTraceLeft;
				OBVR_LOG("Hud probe clear through the binding: %08X",
				         static_cast<UInt32>(clearResult));
			}
		}
	}

	g_original(self, unusedEdx, renderedTexture);

	if (tracing) {
		void* afterPass = nullptr;
		if (getTarget != nullptr) {
			getTarget(device, 0, &afterPass);
		}
		OBVR_LOG("Hud aim: after the pass RT0 %s (got=%08X)",
		         afterPass == substitute ? "is still ours"
		                                 : (afterPass == g_backBuffer ? "is the back buffer"
		                                                              : "is something else"),
		         reinterpret_cast<UInt32>(afterPass));
		if (afterPass != nullptr) {
			using ReleaseFn = UInt32(__stdcall*)(void*);
			if (auto release = d3d9::Method<ReleaseFn>(afterPass, 2)) {
				release(afterPass);
			}
		}
	}

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

	// What this pass did, for the first few redirected passes: whether it
	// issued a single draw, and where it aimed. draws=0 means the HUD is not
	// drawn by this pass on world frames at all, and the search moves
	// elsewhere; draws with other targets and none at the back buffer means
	// the interface is composed somewhere else and only arrives here on menu
	// frames.
	if (g_passTraceLeft > 0) {
		--g_passTraceLeft;
		OBVR_LOG("Hud pass trace: draws=%u (failed %u, dp=%u dip=%u dpup=%u dipup=%u), "
		         "back buffer matched=%u, other targets=%u",
		         g_statsDraws, g_statsFailedDraws, g_statsKind[0], g_statsKind[1],
		         g_statsKind[2], g_statsKind[3], g_statsMatched, g_statsOtherTargets);
	}

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
