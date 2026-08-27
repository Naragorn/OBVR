#include "render/InterfaceRenderHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/PresentHook.h"
#include "render/SceneRenderHook.h"

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

// The clear counter, and the depth question. The first draw of the
// redirected pass ran with the z test on while the texture stayed exactly
// the colour of the probe clear: every draw succeeded and no pixel arrived,
// which is what z rejection looks like from the API side. Whether the pass
// clears depth for itself while redirected - vanilla's begin-target-group
// clears depth and stencil, but 'back buffer matched=0' says that branch
// set nothing here - is measured by counting, and the redirect gives the
// pass the fresh depth vanilla gives it either way.
d3d9::ClearFn g_originalClear = nullptr;
UInt32 g_statsClears = 0;
UInt32 g_statsClearFlagsSeen = 0;
bool g_depthChecked = false;
UInt32 g_depthClearFlags = 0;
UInt32 g_depthClearTraceLeft = 3;

// The reference measurement. The redirected first draw ran through a
// perspective projection and a world translation - matrices the interface
// cannot possibly want - so one pass that would have been redirected runs
// vanilla instead, watched by the same counters and the same first-draw
// sample. What the matrices hold when the HUD provably reaches the back
// buffer is the reference every redirected number now gets compared to.
// The three hundredth pass rather than the first, so the game is settled
// and drawing real interface content by then.
UInt32 g_observeCountdown = 300;
bool g_observing = false;

// Every call to the pass, numbered - and a window around the three hundredth
// in which every single invocation is logged with what it was and what it
// did. The per-pass traces only ever saw the first three calls, all inside
// the loading fade; the reference pass then did nothing at all, so the
// question became what the pass is called with, and how often, once the
// game is actually playing.
UInt32 g_invocation = 0;

// Calls to the pass since the scene render last asked. The scene hook runs
// once per frame whatever else happens, so it can say "this frame drew a
// world and then called the 2D pass this many times" - and a zero there is
// the one number this file cannot produce on its own, because a pass that
// is never entered writes no line at all.
UInt32 g_passesSinceScene = 0;
UInt32 g_drawsSinceScene = 0;

// Every draw the game makes, counted whatever it is drawing. The two counters
// above only run while the 2D pass is being watched, which is exactly the
// window that cannot answer "did the second world render draw as much as the
// first". This one runs always, so the scene hook can read it either side of a
// pass and subtract.
UInt32 g_drawsTotal = 0;

// The last object the game called the pass on - the interface manager, which
// 0057929E hands over in ecx. Kept so the pass can be run at a moment of
// OBVR's choosing without calling 00582160 to fetch it: the game has already
// fetched it, this frame or the one before, and the pointer is a singleton
// that outlives the frame.
void* g_lastSelf = nullptr;

// Whether the layer has already been captured this frame by the run between
// the world renders. The game's own pass still happens afterwards and is
// left to run, but it must not be redirected a second time: it draws
// nothing and would clear the texture this one just filled.
bool g_hudCapturedThisFrame = false;

// The first few between-render runs, reported once each, so the log says
// whether the pass draws at that moment - which is the whole premise.
UInt32 g_betweenTraceLeft = 5;

// The one outcome of RunInterfacePass that means something was put into
// OBVR's texture. Named so the caller can compare pointers rather than
// first letters: every outcome is a literal from this file, so identity is
// exact where a character test would quietly match a future "restored".
const char* const kModeRedirected = "redirected";

// How many first-draw pipeline samples may still be written. The window
// arms the sample for up to twenty invocations, and six full matrix dumps
// is what the log can carry before it stops being readable.
UInt32 g_sampleBudget = 6;

void ResetPassStats() {
	g_statsDraws = 0;
	g_statsFailedDraws = 0;
	g_statsMatched = 0;
	g_statsOtherTargets = 0;
	g_statsKind[0] = g_statsKind[1] = g_statsKind[2] = g_statsKind[3] = 0;
	g_statsClears = 0;
	g_statsClearFlagsSeen = 0;
}

d3d9::DrawPrimitiveFn g_originalDrawPrimitive = nullptr;
d3d9::DrawIndexedPrimitiveFn g_originalDrawIndexed = nullptr;
d3d9::DrawPrimitiveUPFn g_originalDrawUP = nullptr;
d3d9::DrawIndexedPrimitiveUPFn g_originalDrawIndexedUP = nullptr;

SInt32 __stdcall HookedSetRenderTarget(void* self, UInt32 index, void* surface) {
	if ((g_redirecting || g_observing) && index == 0 && surface != nullptr) {
		if (surface == g_backBuffer) {
			++g_statsMatched;
			if (g_redirecting) {
				g_lastRequested = surface;
				surface = g_substitute;
			}
		} else if (surface != g_substitute) {
			++g_statsOtherTargets;
		}
	}
	return g_originalSetTarget(self, index, surface);
}

// One matrix as one log line, %.4g wide - enough to tell an orthographic
// projection (translation in the last row, no perspective terms) from a
// perspective one (m[2][3] carrying the w divide) at a glance.
void LogMatrix(const char* name, const d3d9::Matrix4& m) {
	OBVR_LOG("Hud first draw %s: [%.4g %.4g %.4g %.4g | %.4g %.4g %.4g %.4g | "
	         "%.4g %.4g %.4g %.4g | %.4g %.4g %.4g %.4g]",
	         name, static_cast<double>(m.m[0][0]), static_cast<double>(m.m[0][1]),
	         static_cast<double>(m.m[0][2]), static_cast<double>(m.m[0][3]),
	         static_cast<double>(m.m[1][0]), static_cast<double>(m.m[1][1]),
	         static_cast<double>(m.m[1][2]), static_cast<double>(m.m[1][3]),
	         static_cast<double>(m.m[2][0]), static_cast<double>(m.m[2][1]),
	         static_cast<double>(m.m[2][2]), static_cast<double>(m.m[2][3]),
	         static_cast<double>(m.m[3][0]), static_cast<double>(m.m[3][1]),
	         static_cast<double>(m.m[3][2]), static_cast<double>(m.m[3][3]));
}

// The pipeline at the moment of the first redirected draw, as one line of
// evidence. The pass-entry snapshot already showed the rejection states off,
// but the entry is not the draw: twenty-two successful draws still arrived
// nowhere, so what the device was set to when the game actually drew is
// measured rather than assumed - viewport, blending, masks, and whether the
// draw ran on shaders or fixed function.
void SampleFirstDraw(void* device, const char* kind, UInt32 type, UInt32 count) {
	if (g_sampleBudget == 0) {
		return;
	}
	--g_sampleBudget;

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

	// The transforms. The vertices are untransformed (D3DFVF_XYZ), so the
	// fixed function clips them against whatever these hold - an interface
	// drawing pixel coordinates through a leftover perspective projection is
	// discarded whole, every draw reporting success. And the pass's own
	// begin branch, the one place its orthographic camera is set, provably
	// set nothing while redirected.
	if (auto getTransform =
	        d3d9::Method<d3d9::GetTransformFn>(device, d3d9::kDeviceGetTransform)) {
		d3d9::Matrix4 world{};
		d3d9::Matrix4 view{};
		d3d9::Matrix4 projection{};
		getTransform(device, d3d9::kTransformWorld, &world);
		getTransform(device, d3d9::kTransformView, &view);
		getTransform(device, d3d9::kTransformProjection, &projection);
		LogMatrix("world", world);
		LogMatrix("view", view);
		LogMatrix("projection", projection);
	}

	// The remaining silent rejectors, and how stage 0 builds its colour and
	// alpha: culled winding produces nothing, lighting against no lights
	// produces black, and an alpha path reading a zero texture factor
	// produces pixels the blend leaves invisible.
	UInt32 cull = 0;
	UInt32 lighting = 0;
	UInt32 factor = 0;
	if (auto getState =
	        d3d9::Method<d3d9::GetRenderStateFn>(device, d3d9::kDeviceGetRenderState)) {
		getState(device, d3d9::kRenderStateCullMode, &cull);
		getState(device, d3d9::kRenderStateLighting, &lighting);
		getState(device, d3d9::kRenderStateTextureFactor, &factor);
	}
	UInt32 colorOp = 0;
	UInt32 colorArg1 = 0;
	UInt32 colorArg2 = 0;
	UInt32 alphaOp = 0;
	UInt32 alphaArg1 = 0;
	UInt32 alphaArg2 = 0;
	if (auto getStage = d3d9::Method<d3d9::GetTextureStageStateFn>(
	        device, d3d9::kDeviceGetTextureStageState)) {
		getStage(device, 0, d3d9::kStageColorOp, &colorOp);
		getStage(device, 0, d3d9::kStageColorArg1, &colorArg1);
		getStage(device, 0, d3d9::kStageColorArg2, &colorArg2);
		getStage(device, 0, d3d9::kStageAlphaOp, &alphaOp);
		getStage(device, 0, d3d9::kStageAlphaArg1, &alphaArg1);
		getStage(device, 0, d3d9::kStageAlphaArg2, &alphaArg2);
	}
	void* texture0 = nullptr;
	if (auto getTexture = d3d9::Method<d3d9::GetTextureFn>(device, d3d9::kDeviceGetTexture)) {
		getTexture(device, 0, &texture0);
	}
	OBVR_LOG("Hud first draw state: cull=%u lighting=%u factor=%08X "
	         "stage0 colour=%u(%u,%u) alpha=%u(%u,%u) texture=%s",
	         cull, lighting, factor, colorOp, colorArg1, colorArg2, alphaOp, alphaArg1,
	         alphaArg2, texture0 != nullptr ? "bound" : "null");
	if (texture0 != nullptr) {
		using ReleaseTexFn = UInt32(__stdcall*)(void*);
		if (auto release = d3d9::Method<ReleaseTexFn>(texture0, 2)) {
			release(texture0);
		}
	}

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
	++g_drawsTotal;
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dp", type, primitiveCount);
	}
	const SInt32 result = g_originalDrawPrimitive(self, type, startVertex, primitiveCount);
	if (g_redirecting || g_observing) {
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
	++g_drawsTotal;
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dip", type, primCount);
	}
	const SInt32 result = g_originalDrawIndexed(self, type, baseVertexIndex, minVertexIndex,
	                                            numVertices, startIndex, primCount);
	if (g_redirecting || g_observing) {
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
	++g_drawsTotal;
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dpup", type, primitiveCount);
	}
	const SInt32 result = g_originalDrawUP(self, type, primitiveCount, vertexData, stride);
	if (g_redirecting || g_observing) {
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
	++g_drawsTotal;
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dipup", type, primitiveCount);
	}
	const SInt32 result =
		g_originalDrawIndexedUP(self, type, minVertexIndex, numVertices, primitiveCount,
	                            indexData, indexFormat, vertexData, stride);
	if (g_redirecting || g_observing) {
		++g_statsDraws;
		++g_statsKind[3];
		if (result < 0) {
			++g_statsFailedDraws;
		}
	}
	return result;
}

// Counts what the pass clears while redirected. Only the game's clears land
// here: OBVR's own probe and depth clears go through g_originalClear and
// stay out of their own statistics.
SInt32 __stdcall HookedClear(void* self, UInt32 count, const d3d9::Rect* rects,
                             UInt32 flags, UInt32 color, float z, UInt32 stencil) {
	if (g_redirecting || g_observing) {
		++g_statsClears;
		g_statsClearFlagsSeen |= flags;
	}
	return g_originalClear(self, count, rects, flags, color, z, stencil);
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

	// The clear counter, same diagnostic rank as the draw counters below.
	// The original pointer is kept even if the patch fails: OBVR's own
	// clears go through it either way.
	g_originalClear = reinterpret_cast<d3d9::ClearFn>(vtable[d3d9::kDeviceClear]);
	if (g_originalClear != nullptr &&
	    !WriteTableEntry(vtable, d3d9::kDeviceClear,
	                     reinterpret_cast<void*>(&HookedClear))) {
		OBVR_LOG("Hud: the clear counter could not be installed");
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

// Runs one interface pass, choosing the route, and names the route taken
// for the invocation window's log line. watching: count draws and arm the
// first-draw sample even when nothing redirects, so the window sees vanilla
// passes exactly as it sees redirected ones.
const char* RunInterfacePass(void* self, void* unusedEdx, void* renderedTexture,
                             bool watching) {
	// The order matters: the target hook has to exist before the callbacks
	// change any state, or a pass that could not be redirected would still
	// have its blend states rearranged and its capture claimed.
	if (g_callbacks.beginRedirect == nullptr || !EnsureTargetHook()) {
		g_observing = watching;
		g_original(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "unhooked";
	}

	// A pass aimed at a texture of the game's own - menu-to-texture, not
	// the frame's 2D layer - is the game's business: redirecting it would
	// steal a picture some later draw reads back. Watched, never redirected.
	if (renderedTexture != nullptr) {
		g_observing = watching;
		g_original(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "texture pass";
	}

	// Already captured between the world renders. The game's own pass runs,
	// watched, but nothing of OBVR's is aimed at: redirecting it would hand
	// the texture to a pass that draws nothing and clears on its way in.
	if (g_hudCapturedThisFrame) {
		g_hudCapturedThisFrame = false;
		g_observing = watching;
		g_original(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "already captured";
	}

	void* substitute = g_callbacks.beginRedirect();
	if (substitute == nullptr) {
		g_observing = watching;
		g_original(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "not redirected";
	}

	const bool probe = g_callbacks.probeActive != nullptr && g_callbacks.probeActive();

	// The reference pass: one pass that would have been redirected runs
	// vanilla, watched. The capture the callback just claimed is released
	// again first - its blend states go back, and the untouched texture it
	// cleared submits as one transparent frame, which a probe run accepts.
	// No aim, no substitution, no clears of OBVR's own: the counters and
	// the first-draw sample see the pass exactly as the game runs it, HUD
	// provably landing in the back buffer, and the matrices they record are
	// the reference the redirected numbers get held against.
	if (probe && g_observeCountdown > 0) {
		--g_observeCountdown;
		if (g_observeCountdown == 0) {
			g_callbacks.endRedirect();
			ResetPassStats();
			OBVR_LOG("Hud observe: watching one vanilla pass");
			g_sampleNextDraw = true;
			g_observing = true;
			g_original(self, unusedEdx, renderedTexture);
			g_observing = false;
			g_sampleNextDraw = false;
			OBVR_LOG("Hud observe (vanilla) trace: draws=%u (failed %u, dp=%u dip=%u "
			         "dpup=%u dipup=%u), clears=%u (flags seen 0x%X), back buffer "
			         "matched=%u, other targets=%u",
			         g_statsDraws, g_statsFailedDraws, g_statsKind[0], g_statsKind[1],
			         g_statsKind[2], g_statsKind[3], g_statsClears, g_statsClearFlagsSeen,
			         g_statsMatched, g_statsOtherTargets);
			return "vanilla reference";
		}
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
	if (g_passTraceLeft > 0) {
		ResetPassStats();
		g_sampleNextDraw = true;
	}
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

	// The depth the vanilla begin gives the pass. The orange probe clear
	// arrived through the binding while all twenty-two successful draws did
	// not, and the one anomaly in the first draw's pipeline was z=1 - the
	// z test, the only rejector that discards pixels without an error. If
	// the pass's own begin-target-group branch skipped its depth clear the
	// way it skipped its target set ('back buffer matched=0'), the interface
	// tested orthographic z against the world's perspective depths and lost.
	// So the redirect clears depth - and stencil, when the surface has one -
	// exactly as vanilla's begin does, and the clear counter reports whether
	// the pass also clears for itself.
	if (!g_depthChecked) {
		g_depthChecked = true;
		void* depthStencil = nullptr;
		if (auto getDepthStencil = d3d9::Method<d3d9::GetDepthStencilSurfaceFn>(
		        device, d3d9::kDeviceGetDepthStencilSurface)) {
			getDepthStencil(device, &depthStencil);
		}
		if (depthStencil != nullptr) {
			d3d9::SurfaceDesc desc{};
			auto getDesc =
				d3d9::Method<d3d9::GetDescFn>(depthStencil, d3d9::kSurfaceGetDesc);
			if (getDesc != nullptr && getDesc(depthStencil, &desc) >= 0) {
				OBVR_LOG("Hud depth: %ux%u format=%u multisample=%u bound at the "
				         "redirected pass",
				         desc.width, desc.height, desc.format, desc.multiSampleType);
				g_depthClearFlags = d3d9::kClearZBuffer;
				if (desc.format == d3d9::kFormatD24S8) {
					g_depthClearFlags |= d3d9::kClearStencil;
				}
			}
			using ReleaseFn = UInt32(__stdcall*)(void*);
			if (auto release = d3d9::Method<ReleaseFn>(depthStencil, 2)) {
				release(depthStencil);
			}
		} else {
			OBVR_LOG("Hud depth: no depth stencil bound at the redirected pass");
		}
	}
	if (g_depthClearFlags != 0 && g_originalClear != nullptr) {
		const SInt32 depthResult =
			g_originalClear(device, 0, nullptr, g_depthClearFlags, 0, 1.0f, 0);
		if (g_depthClearTraceLeft > 0) {
			--g_depthClearTraceLeft;
			OBVR_LOG("Hud depth clear: flags=0x%X result=%08X", g_depthClearFlags,
			         static_cast<UInt32>(depthResult));
		}
	}

	// The probe clear: the smallest write that goes through the render target
	// binding the draws use. ColorFill writes to the surface by name and its
	// red square arrives in the headset; if this orange does not arrive the
	// same way, the binding the device just confirmed does not reach the
	// texture the compositor shows - and the pass's draws never had a chance.
	// Alpha 0x60, so the world stays visible behind it. Through the original:
	// OBVR's own clears stay out of the clear counter.
	if (probe && g_originalClear != nullptr) {
		const SInt32 clearResult =
			g_originalClear(device, 0, nullptr, d3d9::kClearTarget, 0x60FF8000u, 1.0f, 0);
		if (g_probeClearTraceLeft > 0) {
			--g_probeClearTraceLeft;
			OBVR_LOG("Hud probe clear through the binding: %08X",
			         static_cast<UInt32>(clearResult));
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
		         "clears=%u (flags seen 0x%X), back buffer matched=%u, other targets=%u",
		         g_statsDraws, g_statsFailedDraws, g_statsKind[0], g_statsKind[1],
		         g_statsKind[2], g_statsKind[3], g_statsClears, g_statsClearFlagsSeen,
		         g_statsMatched, g_statsOtherTargets);
	}

	g_callbacks.endRedirect();
	return kModeRedirected;
}

// The gates between entering the pass and drawing anything, read straight
// out of the object the pass was called on.
//
// That object is the interface manager: 0057929E calls the pass with ecx
// holding what 00582160 just returned. So the gates its own code reads by
// offset can be read here by offset too, with no call into the game and no
// guess about which frame the numbers belong to.
//
// The gates, in the order the game reaches them:
//
//   [+1Ch]   00579280, the wrapper's second gate - null and the pass is
//            never entered at all
//   menus    0057F358, which of the two draw calls the pass makes
//   [+68h]   005903EC, the root 005903E0 hands to the drawing code - null
//            and it draws nothing and returns
//   [+68h]+5 0058FBA6, the first thing the drawing code tests; non-zero
//            and it leaves before drawing or clearing anything
//   [+B8h]   005903FD, the flag that decides whether 005903E0 tail-calls on
//            to 004A25F0 afterwards
//
// The run this exists for showed 22 primitives at invocation 220 and none
// from 240 on, with the first dual pass in between - so one of these went
// from open to shut across that boundary, and stayed shut when the dual
// pass was cut back off again.
void LogInterfaceGates(void* self, UInt32 invocation) {
	if (self == nullptr) {
		OBVR_LOG("Hud gates at invocation %u: the pass was called on nothing", invocation);
		return;
	}

	const auto* manager = static_cast<const char*>(self);
	const UInt32 wrapperGate = *reinterpret_cast<const UInt32*>(manager + 0x1C);
	const UInt32 root = *reinterpret_cast<const UInt32*>(manager + 0x68);
	const UInt32 tailFlag = *reinterpret_cast<const UInt8*>(manager + 0xB8);

	// Only reachable through a root the game itself dereferences one
	// instruction later, so a null here is the finding rather than a crash.
	//
	// [root+34h] is the gate inside the drawing code that the first reading of
	// 0058FBA0 walked past: it is the list the traversal at 0058FC60 walks,
	// and 0058FC4E jumps clean to the end when it is null. An empty list is a
	// pass that runs from top to bottom and draws nothing - which is exactly
	// the shape of what the dual pass leaves behind.
	UInt32 rootFlag = 0xFFu;
	UInt32 rootChildren = 0;
	UInt32 rootPeer = 0;
	if (root != 0) {
		rootFlag = *reinterpret_cast<const UInt8*>(root + 5);
		rootChildren = *reinterpret_cast<const UInt32*>(root + 0x34);
		rootPeer = *reinterpret_cast<const UInt32*>(root + 0x10);
	}

	const UInt32 menuCount = *reinterpret_cast<const UInt16*>(addr::kMenuStackCount);
	const UInt32 menuRoot = *reinterpret_cast<const UInt32*>(addr::kMenuStackRoot);
	const UInt32 menuRootEntry =
		menuRoot != 0 ? *reinterpret_cast<const UInt32*>(menuRoot + 0x18) : 0;

	OBVR_LOG("Hud gates at invocation %u: [+1Ch]=%08X, [+68h]=%08X, +5=%02X, +10h=%08X, "
	         "+34h=%08X, [+B8h]=%u, menus=%u root=%08X entry=%08X",
	         invocation, wrapperGate, root, rootFlag, rootPeer, rootChildren, tailFlag,
	         menuCount, menuRoot, menuRootEntry);
}

// The entry detour target. Numbers the invocations, and counts what each one
// drew - including the calls that redirect nothing, which the per-pass traces
// never saw.
//
// The counting window has to cover the whole probe sweep, and the first
// attempt at it did not: it ended at invocation 460, which is exactly where
// the sweep's second band began.
//
// The two counters are not the same clock. This one starts at the first 2D
// pass of the process - the main menu and the loading screen draw through it
// long before a world is rendered - while the scene call number starts at the
// first world render. That run's log put them roughly two hundred apart, and
// two hundred is not a constant to rely on: it is however many frames the
// person spent in menus before loading a save.
//
// So the window is wide enough to cover any reasonable offset, and every line
// it writes carries the scene call beside the invocation, so the two clocks
// can be lined up in the log rather than assumed to agree.
void __fastcall HookedRenderInterface(void* self, void* unusedEdx, void* renderedTexture) {
	const UInt32 invocation = ++g_invocation;
	++g_passesSinceScene;
	g_lastSelf = self;
	const bool window = invocation > 150 && invocation <= 1400;
	if (window) {
		ResetPassStats();
		g_sampleNextDraw = true;
	}

	const char* mode = RunInterfacePass(self, unusedEdx, renderedTexture, window);

	if (window) {
		g_sampleNextDraw = false;
		g_drawsSinceScene += g_statsDraws;
	}

	if (window && invocation % 10 == 0) {

		// The fade at [this+4]+0x2C, read at 0057F27A. Logged as context,
		// not as a gate: the branch there skips the block at 0057F292 when
		// the fade is zero, and that block is the loading fade overlay -
		// not the HUD. A run with the HUD on the monitor showed fade 0.000
		// on every invocation, which is what settles it. The HUD itself is
		// drawn further down, at 0057F358, where both arms of the branch
		// draw and no arm skips.
		float fade = -1.0f;
		if (self != nullptr) {
			void* inner = *reinterpret_cast<void**>(static_cast<char*>(self) + 4);
			if (inner != nullptr) {
				fade = *reinterpret_cast<float*>(static_cast<char*>(inner) + 0x2C);
			}
		}

		OBVR_LOG("Hud invocation %u at scene call %u (%s): texture=%08X, fade=%.3f, "
		         "draws=%u (dp=%u dip=%u dpup=%u dipup=%u), clears=%u (flags 0x%X), "
		         "set target back=%u other=%u",
		         invocation, CurrentSceneCall(), mode,
		         reinterpret_cast<UInt32>(renderedTexture), static_cast<double>(fade),
		         g_statsDraws, g_statsKind[0], g_statsKind[1], g_statsKind[2],
		         g_statsKind[3], g_statsClears, g_statsClearFlagsSeen, g_statsMatched,
		         g_statsOtherTargets);
		LogInterfaceGates(self, invocation);
	}
}

// The device state the second world render inherits.
//
// RunHudPassBetweenScenes calls Oblivion's own 2D pass at a point in the frame
// the engine never puts it: between the two world renders. That pass sets the
// device up to draw a flat layer - blending, depth, textures, shaders - and
// leaves it that way. Gamebryo keeps its own record of what the device is set
// to and skips the calls it believes are redundant, so the second render never
// puts back what it did not see change. It draws with what the 2D pass left.
//
// Which is invisible until it is not: the second render is one eye, so any
// state that survives the pass and matters to some class of geometry takes
// that geometry out of that eye alone.
//
// Made and released around each pass rather than kept alive. A state block has
// to be released before the device can be reset - the reference is explicit
// about it, "an application should release any explicit render targets, depth
// stencil surfaces, additional swap chains, state blocks, and D3DPOOL_DEFAULT
// resources" - and Oblivion resets its device whenever the video menu changes
// a setting. A block held for the life of the process would turn changing the
// resolution into a reset that fails. One driver allocation per frame is the
// cheaper of the two mistakes.
bool g_betweenStateRefused = false;
bool g_betweenStateReported = false;

// Takes the device state, or returns null. Never fatal: a frame without the
// guard renders exactly as every frame did before this existed.
void* CaptureStateBeforePass(void* device) {
	if (device == nullptr || g_betweenStateRefused) {
		return nullptr;
	}

	auto create =
		d3d9::Method<d3d9::CreateStateBlockFn>(device, d3d9::kDeviceCreateStateBlock);
	void* block = nullptr;
	if (create == nullptr || create(device, d3d9::kStateBlockTypeAll, &block) < 0 ||
	    block == nullptr) {
		g_betweenStateRefused = true;
		OBVR_LOG("Hud: no state block - the pass between the world renders will leave its "
		         "device state to the second render, as it did before");
		return nullptr;
	}

	if (!g_betweenStateReported) {
		g_betweenStateReported = true;
		OBVR_LOG("Hud: the pass between the world renders now hands the second render the "
		         "device state the first one finished with");
	}

	// Creating a block captures the current state as part of creating it, so
	// there is nothing left to ask for here.
	return block;
}

// Puts back what the capture took, and lets the block go again.
void RestoreStateAfterPass(void* block) {
	if (auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(block, d3d9::kStateBlockApply)) {
		apply(block);
	}
	using ReleaseFn = UInt32(__stdcall*)(void*);
	if (auto release = d3d9::Method<ReleaseFn>(block, 2)) {
		release(block);
	}
}

}  // namespace

bool RunHudPassBetweenScenes() {
	if (g_original == nullptr || g_lastSelf == nullptr || g_hudCapturedThisFrame) {
		return false;
	}

	// Everything this pass is about to change, taken first. See the block
	// above for why the second world render cannot be left to notice.
	void* const savedState = CaptureStateBeforePass(GetGameDevice());

	// The same call the game makes at 0057929E: the interface manager in
	// ecx, a null texture argument for the frame's own 2D layer. Run through
	// the same redirect path as a hooked pass, so the layer lands in OBVR's
	// texture rather than in the back buffer the eyes were just copied from.
	ResetPassStats();
	const char* mode = RunInterfacePass(g_lastSelf, nullptr, nullptr, true);
	const UInt32 drew = g_statsDraws;

	if (savedState != nullptr) {
		RestoreStateAfterPass(savedState);
	}

	// Only a pass that was actually redirected has put anything anywhere, so
	// only that one gets to tell the game's later pass to stand down.
	const bool captured = drew > 0 && mode == kModeRedirected;
	g_hudCapturedThisFrame = captured;

	if (g_betweenTraceLeft > 0) {
		--g_betweenTraceLeft;
		OBVR_LOG("Hud between renders (%s): drew %u, cleared %u (flags 0x%X) - %s", mode,
		         drew, g_statsClears, g_statsClearFlagsSeen,
		         captured ? "the layer is OBVR's for this frame"
		                  : "nothing captured, the game's own pass still owns it");
	}
	return captured;
}

void ArmBetweenTrace() { g_betweenTraceLeft = 12; }

UInt32 TotalDrawCount() { return g_drawsTotal; }

void TakeInterfaceStats(UInt32& passes, UInt32& draws) {
	passes = g_passesSinceScene;
	draws = g_drawsSinceScene;
	g_passesSinceScene = 0;
	g_drawsSinceScene = 0;
}

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
