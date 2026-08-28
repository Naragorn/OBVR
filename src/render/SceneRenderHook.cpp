#include "render/SceneRenderHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"
#include "render/InterfaceRenderHook.h"

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

// World renders, numbered, and the clock the probe sweep runs on.
//
// This trace exists because the interface hook cannot see its own absence.
// With the dual pass on, the HUD leaves the monitor and that hook reported
// nothing drawn - but a pass Oblivion never enters writes no line at all, so
// "drew nothing" and "was never called" were the same silence. This hook
// runs whatever the frame does, so it can tell them apart, and the first run
// of it did: the 2D pass is entered once per frame, on one thread, with the
// loading thread handle null. It is entered and draws nothing.
//
// The handle and the thread id stay in the line because they are what rules
// out the two explanations that would otherwise still be open - a cell still
// believed to be streaming in, and a render running where D3D9 must not be
// touched - and a run where either changed would be worth seeing. What the
// line is now for is the rung beside the draw count: which part of the dual
// pass costs the HUD its draws.
UInt32 g_sceneCall = 0;

void TraceFrame(const char* how, UInt32 passesLastFrame, UInt32 drawsLastFrame) {
	if (g_sceneCall <= 150 || g_sceneCall > 500 || g_sceneCall % 10 != 0) {
		return;
	}
	const UInt32 handle = *reinterpret_cast<const UInt32*>(addr::kLoadingThreadHandle);
	const UInt32 rung =
		g_callbacks.probeStage != nullptr ? g_callbacks.probeStage() : 0;
	OBVR_LOG("Scene call %u (%s, rung %u) on thread %u: after the previous one the 2D pass "
	         "ran %u time(s) and drew %u, loading thread handle=%08X",
	         g_sceneCall, how, rung, static_cast<UInt32>(GetCurrentThreadId()),
	         passesLastFrame, drawsLastFrame, handle);
}

// The two world renders of a dual frame, measured against each other, on a
// slow heartbeat.
//
// Slow because the question is not what one frame did. Both renders draw the
// same world, so they are meant to draw the same number of primitives, and a
// difference is geometry the second render decided it did not need - which is
// what a right eye missing bodies looks like from here. Equal counts move the
// fault out of the render and into what happens to the picture afterwards.
//
// The middle number is the moment between them, where the eye copy is taken
// and the 2D layer is captured. It is in the line because that moment sets
// device state the second render then inherits, so a fault that only ever
// appears in the second eye has to be read next to it.
void TracePassDraws(UInt32 entry, UInt32 afterFirst, UInt32 afterBetween, UInt32 afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual draws at scene call %u: first pass %u, between the passes %u, second "
	         "pass %u",
	         g_sceneCall, afterFirst - entry, afterBetween - afterFirst,
	         afterSecond - afterBetween);
}

// The setup work beside the draw counts, on the same heartbeat. The draw
// line said the geometry is submitted twice; this one says whether it is
// *set up* twice. A render that uploads far fewer shader constants than its
// twin while drawing the same primitives is drawing skinned geometry
// against whatever the registers still held - which is what bodies
// collapsed onto one point look like from this side of the API.
void TracePassState(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                    const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual state at scene call %u (first pass / second pass): constants %u calls "
	         "%u vectors / %u calls %u vectors, shaders %u/%u, declarations %u/%u, "
	         "fvf %u/%u, transforms %u/%u",
	         g_sceneCall, afterFirst.constantCalls - entry.constantCalls,
	         afterFirst.constantVectors - entry.constantVectors,
	         afterSecond.constantCalls - afterBetween.constantCalls,
	         afterSecond.constantVectors - afterBetween.constantVectors,
	         afterFirst.vertexShaders - entry.vertexShaders,
	         afterSecond.vertexShaders - afterBetween.vertexShaders,
	         afterFirst.declarations - entry.declarations,
	         afterSecond.declarations - afterBetween.declarations,
	         afterFirst.fvfs - entry.fvfs, afterSecond.fvfs - afterBetween.fvfs,
	         afterFirst.transforms - entry.transforms,
	         afterSecond.transforms - afterBetween.transforms);
}

// The buffer side of the same question. Gamebryo's software skinning path
// repacks skinned geometry into vertex buffers instead of uploading bone
// palettes - work that shows up as locks the constant counters cannot see.
// Zero uploads ride along: a bone palette that was never computed is a
// palette of zeroes, and the pass that uploads them would be the pass that
// draws bodies onto one point.
void TraceBufferLocks(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                      const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual buffers at scene call %u: locks first %u (discard %u), between %u (%u), "
	         "second %u (%u); zero uploads first %u, second %u",
	         g_sceneCall, afterFirst.vbLocks - entry.vbLocks,
	         afterFirst.vbDiscardLocks - entry.vbDiscardLocks,
	         afterBetween.vbLocks - afterFirst.vbLocks,
	         afterBetween.vbDiscardLocks - afterFirst.vbDiscardLocks,
	         afterSecond.vbLocks - afterBetween.vbLocks,
	         afterSecond.vbDiscardLocks - afterBetween.vbDiscardLocks,
	         afterFirst.zeroUploads - entry.zeroUploads,
	         afterSecond.zeroUploads - afterBetween.zeroUploads);
}

// The contents, at last, after every count came back symmetric. Palette
// candidates are uploads of twelve vectors or more; bone-to-model matrices
// carry no camera, so the two renders must upload identical bytes and the
// two sums must match. A pass whose sum differs while its counts agree is
// uploading different numbers into the same registers - and that pass is
// the collapse. The largest upload rides along to say whether big palettes
// exist at all: if it stays small, the bones travel some other way.
void TracePaletteSums(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                      const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual palettes at scene call %u: first %u uploads %u vectors sum %08X, "
	         "second %u uploads %u vectors sum %08X, largest upload seen %u",
	         g_sceneCall, afterFirst.paletteCalls - entry.paletteCalls,
	         afterFirst.paletteVectors - entry.paletteVectors,
	         afterFirst.paletteSum - entry.paletteSum,
	         afterSecond.paletteCalls - afterBetween.paletteCalls,
	         afterSecond.paletteVectors - afterBetween.paletteVectors,
	         afterSecond.paletteSum - afterBetween.paletteSum,
	         afterSecond.largestUpload);
}

// The same sums, split by start register. Camera blocks and bone palettes
// live in different constant registers; the per-register split is what
// separates "the view moved, as it must between two eyes" from "the bones
// changed, which they must not". A register whose sums differ while its
// counts agree is where the second render goes wrong - and a run where
// only the camera registers differ acquits the constants entirely and
// sends the search into the vertex buffer contents.
void TracePaletteRegisters(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                           const StateCallCounts& afterBetween,
                           const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	char deltas[832];
	const UInt32 active = FormatPaletteRegisterDeltas(
	    entry.paletteRegisters, afterFirst.paletteRegisters, afterBetween.paletteRegisters,
	    afterSecond.paletteRegisters, kPaletteBucketCount, deltas, sizeof(deltas));
	if (active == 0) {
		return;
	}
	OBVR_LOG("Dual palette registers at scene call %u: %s (overflow %u)", g_sceneCall, deltas,
	         afterSecond.paletteOverflow - entry.paletteOverflow);
}

// Binding identities per render, after the register split acquitted the
// constants: the palette registers upload identical bytes to both eyes,
// and the actors turned out software-skinned - their bones never travel
// as constants at all. What remains is what each draw is wired to: which
// vertex buffer, which declaration, which shader. Pointer sums have no
// camera in them, so the two renders must match sum for sum; the sum that
// differs names the binding type the second render gets wrong - the
// declaration slot would be the DXVK issue 2420 shape, the stream slot a
// repacked buffer the second render does not see.
void TraceBindings(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                   const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual bindings at scene call %u: streams first %u calls sum %08X, second %u "
	         "calls sum %08X; declarations first %08X second %08X; shaders first %08X "
	         "second %08X",
	         g_sceneCall, afterFirst.streamSources - entry.streamSources,
	         afterFirst.streamSourceSum - entry.streamSourceSum,
	         afterSecond.streamSources - afterBetween.streamSources,
	         afterSecond.streamSourceSum - afterBetween.streamSourceSum,
	         afterFirst.declarationSum - entry.declarationSum,
	         afterSecond.declarationSum - afterBetween.declarationSum,
	         afterFirst.vertexShaderSum - entry.vertexShaderSum,
	         afterSecond.vertexShaderSum - afterBetween.vertexShaderSum);
}

// The draw-level fingerprint of the skinned pool, after every pass-level
// number came back symmetric. Skinned draws are the draws whose stream 0
// is a discard-locked buffer; their vertex and primitive sums have no
// camera in them and must match between the renders. The offset sums are
// the discriminator: equal offset sums mean the second render reads the
// same buffer regions as the first - packed once, drawn twice, and then
// whoever locks those buffers between the renders overwrites what the
// second render is about to read. Distinct sums mean each render packs
// its own regions, and the fault is inside the write itself.
void TraceSkinnedDraws(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                       const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual skinned draws at scene call %u: first %u draws %u verts %u prims "
	         "offsets %08X, between %u/%u/%u/%08X, second %u draws %u verts %u prims "
	         "offsets %08X",
	         g_sceneCall, afterFirst.skinnedDraws - entry.skinnedDraws,
	         afterFirst.skinnedVertexSum - entry.skinnedVertexSum,
	         afterFirst.skinnedPrimSum - entry.skinnedPrimSum,
	         afterFirst.skinnedOffsetSum - entry.skinnedOffsetSum,
	         afterBetween.skinnedDraws - afterFirst.skinnedDraws,
	         afterBetween.skinnedVertexSum - afterFirst.skinnedVertexSum,
	         afterBetween.skinnedPrimSum - afterFirst.skinnedPrimSum,
	         afterBetween.skinnedOffsetSum - afterFirst.skinnedOffsetSum,
	         afterSecond.skinnedDraws - afterBetween.skinnedDraws,
	         afterSecond.skinnedVertexSum - afterBetween.skinnedVertexSum,
	         afterSecond.skinnedPrimSum - afterBetween.skinnedPrimSum,
	         afterSecond.skinnedOffsetSum - afterBetween.skinnedOffsetSum);
	OBVR_LOG("Dual dynamic locks at scene call %u: first %u offsets %08X sizes %08X, "
	         "between %u/%08X/%08X, second %u offsets %08X sizes %08X (set overflow %u)",
	         g_sceneCall, afterFirst.dynamicLocks - entry.dynamicLocks,
	         afterFirst.dynamicLockOffsetSum - entry.dynamicLockOffsetSum,
	         afterFirst.dynamicLockSizeSum - entry.dynamicLockSizeSum,
	         afterBetween.dynamicLocks - afterFirst.dynamicLocks,
	         afterBetween.dynamicLockOffsetSum - afterFirst.dynamicLockOffsetSum,
	         afterBetween.dynamicLockSizeSum - afterFirst.dynamicLockSizeSum,
	         afterSecond.dynamicLocks - afterBetween.dynamicLocks,
	         afterSecond.dynamicLockOffsetSum - afterBetween.dynamicLockOffsetSum,
	         afterSecond.dynamicLockSizeSum - afterBetween.dynamicLockSizeSum,
	         afterSecond.dynamicBufferOverflow);
}

// The written bytes of the dynamic pool, fingerprinted at unlock - the
// last unanswered question on the D3D9 side. Each render packs its own
// regions with matching counts, offsets and sizes; what no counter has
// seen yet is whether the second render writes the same bytes. Skinning
// is camera-free, so matching write sums send the fault past the API
// into the upload; differing sums mean the engine's own skinning pass
// computes garbage the second time around, and the fix moves into the
// game code that packs these buffers.
void TracePoolWrites(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                     const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual pool writes at scene call %u: first %u writes sum %08X skipped %u, "
	         "between %u/%08X/%u, second %u writes sum %08X skipped %u",
	         g_sceneCall, afterFirst.dynamicWrites - entry.dynamicWrites,
	         afterFirst.dynamicWriteSum - entry.dynamicWriteSum,
	         afterFirst.dynamicWriteSkipped - entry.dynamicWriteSkipped,
	         afterBetween.dynamicWrites - afterFirst.dynamicWrites,
	         afterBetween.dynamicWriteSum - afterFirst.dynamicWriteSum,
	         afterBetween.dynamicWriteSkipped - afterFirst.dynamicWriteSkipped,
	         afterSecond.dynamicWrites - afterBetween.dynamicWrites,
	         afterSecond.dynamicWriteSum - afterBetween.dynamicWriteSum,
	         afterSecond.dynamicWriteSkipped - afterBetween.dynamicWriteSkipped);
}

// Stands where the entry of kRenderScene used to be, with the same calling
// convention. See the type alias above for why __fastcall.
void __fastcall HookedRenderScene(void* self, void* unusedEdx, void* renderedTexture) {
	++g_sceneCall;

	// Taken before the render, so they count the 2D passes that followed the
	// previous world render - the frame that has finished, rather than the
	// one about to be drawn. Always taken, window or not, so the counts never
	// carry over from frames nobody is looking at.
	UInt32 passesLastFrame = 0;
	UInt32 drawsLastFrame = 0;
	TakeInterfaceStats(passesLastFrame, drawsLastFrame);

	// Only the ordinary world pass is drawn twice. A non-null argument is a
	// menu wanting the world in a texture or the save game screenshot, and
	// those want exactly what they asked for.
	if (g_rendering || renderedTexture != nullptr || g_callbacks.wantsSecondPass == nullptr ||
	    !g_callbacks.wantsSecondPass()) {
		g_original(self, unusedEdx, renderedTexture);
		TraceFrame(renderedTexture != nullptr ? "texture pass" : "single", passesLastFrame,
		           drawsLastFrame);
		return;
	}

	g_rendering = true;

	// What each half of the frame drew. The two renders are the same world from
	// two places, so they are meant to be the same count - and the first run of
	// this said they are, to within the handful of objects that fall out of one
	// eye's frustum and not the other.
	//
	// Which is what rules out the whole class of explanation where one render
	// draws less: the geometry is submitted twice and one of the two comes out
	// wrong. Kept because that has to keep being true while the cause is hunted
	// somewhere else, and because a run where it stops being true would be the
	// most interesting run of all.
	const UInt32 drawsAtEntry = TotalDrawCount();
	const StateCallCounts stateAtEntry = TotalStateCalls();

	// First eye. The camera hook already moved the camera there.
	g_original(self, unusedEdx, renderedTexture);
	const UInt32 drawsAfterFirst = TotalDrawCount();
	const StateCallCounts stateAfterFirst = TotalStateCalls();
	g_callbacks.betweenPasses();
	const UInt32 drawsAfterBetween = TotalDrawCount();
	const StateCallCounts stateAfterBetween = TotalStateCalls();

	// Second eye, from a camera one interpupillary distance over.
	g_original(self, unusedEdx, renderedTexture);
	const UInt32 drawsAfterSecond = TotalDrawCount();
	const StateCallCounts stateAfterSecond = TotalStateCalls();
	g_callbacks.afterSecondPass();

	g_rendering = false;
	TracePassDraws(drawsAtEntry, drawsAfterFirst, drawsAfterBetween, drawsAfterSecond);
	TracePassState(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TraceBufferLocks(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TracePaletteSums(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TracePaletteRegisters(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TraceBindings(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TraceSkinnedDraws(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TracePoolWrites(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TraceFrame("dual", passesLastFrame, drawsLastFrame);
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

UInt32 CurrentSceneCall() { return g_sceneCall; }

}  // namespace obvr::render
