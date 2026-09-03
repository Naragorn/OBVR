#include "render/SceneRenderHook.h"

#include "render/SceneGraphProbe.h"

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

// The renderer instance of the last world render, kept for the menu-world
// probe. kRenderScene is __thiscall, so calling it needs the object Oblivion
// calls it on - and the only way to that object from here is to remember it
// as it passes through. The pointer is the engine's scene renderer, which
// lives for the whole session; the precedent is the interface hook keeping
// the InterfaceManager for RunHudPassBetweenScenes the same way.
void* g_lastRendererSelf = nullptr;

// Whether the passes below are already running. Not reachable through the
// patched entry - both calls go through the trampoline - but a guard costs a
// comparison and turns "the engine surprised us" into a pass-through instead
// of unbounded recursion.
bool g_rendering = false;

// Engine-side scene graph reports still owed. Unconditional rather than
// gated on the probe setting: this is the reference the probe's own readings
// are read against, it costs a few log lines once per session, and a run that
// switched the probe on mid-session would otherwise have no reference at all.
UInt32 g_engineSideReportsLeft = 3;

// The engine render's own draw and setup counts, owed the same few times.
UInt32 g_engineCostReportsLeft = 3;

// The vertex pipeline setup of one moment, as a single number. Only its
// difference across a call is used, so summing the five is enough to answer
// "did the pipeline get set up at all" without pretending the sum means more.
UInt32 VertexSetupTotal(const StateCallCounts& calls) {
	return calls.transforms + calls.declarations + calls.fvfs + calls.vertexShaders +
	       calls.constantCalls;
}

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

// The first order-sensitive probe, after every order-independent sum came
// back equal. A skinned draw issued while c0 holds an all-zero matrix is
// a body collapsing onto one clip-space point - the very picture in the
// headset. A second-render count above a first-render zero convicts the
// interleaving: same uploads, same draws, wrong order between them.
void TraceZeroMatrixDraws(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                          const StateCallCounts& afterBetween,
                          const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual zero-matrix draws at scene call %u: first %u, between %u, second %u",
	         g_sceneCall, afterFirst.skinnedZeroMatrixDraws - entry.skinnedZeroMatrixDraws,
	         afterBetween.skinnedZeroMatrixDraws - afterFirst.skinnedZeroMatrixDraws,
	         afterSecond.skinnedZeroMatrixDraws - afterBetween.skinnedZeroMatrixDraws);
	OBVR_LOG("Dual vertex processing at scene call %u: toggles first %u second %u, "
	         "software-processed skinned draws first %u second %u",
	         g_sceneCall, afterFirst.swvpToggles - entry.swvpToggles,
	         afterSecond.swvpToggles - afterBetween.swvpToggles,
	         afterFirst.swvpOnDraws - entry.swvpOnDraws,
	         afterSecond.swvpOnDraws - afterBetween.swvpOnDraws);
	float eyeDelta[3];
	UInt32 eyeDeltaSamples = 0;
	GetBoneEyeDelta(eyeDelta, eyeDeltaSamples);
	float eyeShift[3];
	int shiftSign = 0;
	GetBoneShiftState(eyeShift, shiftSign);
	OBVR_LOG("Bone lock at scene call %u: replaced %u (reordered %u), passthrough %u "
	         "(refused %u), offscreen %u, camera shift %g %g %g sign %d, measured "
	         "%g %g %g from %u rows",
	         g_sceneCall, afterSecond.boneLockReplaced - entry.boneLockReplaced,
	         afterSecond.boneLockReordered - entry.boneLockReordered,
	         afterSecond.boneLockPassthrough - entry.boneLockPassthrough,
	         afterSecond.boneLockRefused - entry.boneLockRefused,
	         afterSecond.boneOffscreenRows - entry.boneOffscreenRows,
	         eyeShift[0], eyeShift[1], eyeShift[2], shiftSign, eyeDelta[0],
	         eyeDelta[1], eyeDelta[2], eyeDeltaSamples);
	OBVR_LOG("Dual bone range at scene call %u: first %u calls %u vecs sum %08X, "
	         "second %u calls %u vecs sum %08X",
	         g_sceneCall, afterFirst.boneRangeCalls - entry.boneRangeCalls,
	         afterFirst.boneRangeVectors - entry.boneRangeVectors,
	         afterFirst.boneRangeSum - entry.boneRangeSum,
	         afterSecond.boneRangeCalls - afterBetween.boneRangeCalls,
	         afterSecond.boneRangeVectors - afterBetween.boneRangeVectors,
	         afterSecond.boneRangeSum - afterBetween.boneRangeSum);
	OBVR_LOG("Dual draw addressing at scene call %u: base first %08X second %08X, "
	         "min first %08X second %08X, start first %08X second %08X",
	         g_sceneCall, afterFirst.skinnedBaseVertexSum - entry.skinnedBaseVertexSum,
	         afterSecond.skinnedBaseVertexSum - afterBetween.skinnedBaseVertexSum,
	         afterFirst.skinnedMinVertexSum - entry.skinnedMinVertexSum,
	         afterSecond.skinnedMinVertexSum - afterBetween.skinnedMinVertexSum,
	         afterFirst.skinnedStartIndexSum - entry.skinnedStartIndexSum,
	         afterSecond.skinnedStartIndexSum - afterBetween.skinnedStartIndexSum);
}

// The index side of vertex fetch, mirrored from the vertex side once every
// vertex-side number came back equal. Zeroed indices build every triangle
// out of vertex zero and a zero stride reads one vertex forever - both are
// bodies collapsed onto a point, and neither was watched until now.
void TraceIndexSide(const StateCallCounts& entry, const StateCallCounts& afterFirst,
                    const StateCallCounts& afterBetween, const StateCallCounts& afterSecond) {
	if (g_sceneCall % 120 != 0) {
		return;
	}
	OBVR_LOG("Dual index locks at scene call %u: first %u (discard %u) writes %u sum %08X "
	         "skipped %u, between %u/%u/%u/%08X/%u, second %u (discard %u) writes %u "
	         "sum %08X skipped %u",
	         g_sceneCall, afterFirst.ibLocks - entry.ibLocks,
	         afterFirst.ibDiscardLocks - entry.ibDiscardLocks,
	         afterFirst.ibWrites - entry.ibWrites, afterFirst.ibWriteSum - entry.ibWriteSum,
	         afterFirst.ibWriteSkipped - entry.ibWriteSkipped,
	         afterBetween.ibLocks - afterFirst.ibLocks,
	         afterBetween.ibDiscardLocks - afterFirst.ibDiscardLocks,
	         afterBetween.ibWrites - afterFirst.ibWrites,
	         afterBetween.ibWriteSum - afterFirst.ibWriteSum,
	         afterBetween.ibWriteSkipped - afterFirst.ibWriteSkipped,
	         afterSecond.ibLocks - afterBetween.ibLocks,
	         afterSecond.ibDiscardLocks - afterBetween.ibDiscardLocks,
	         afterSecond.ibWrites - afterBetween.ibWrites,
	         afterSecond.ibWriteSum - afterBetween.ibWriteSum,
	         afterSecond.ibWriteSkipped - afterBetween.ibWriteSkipped);
	OBVR_LOG("Dual fetch state at scene call %u: index binds first %u sum %08X second %u "
	         "sum %08X, stride sums first %08X second %08X, freq calls %u/%u, zero-stride "
	         "skinned draws %u/%u",
	         g_sceneCall, afterFirst.indexBinds - entry.indexBinds,
	         afterFirst.indexBindSum - entry.indexBindSum,
	         afterSecond.indexBinds - afterBetween.indexBinds,
	         afterSecond.indexBindSum - afterBetween.indexBindSum,
	         afterFirst.streamStrideSum - entry.streamStrideSum,
	         afterSecond.streamStrideSum - afterBetween.streamStrideSum,
	         afterFirst.streamFreqCalls - entry.streamFreqCalls,
	         afterSecond.streamFreqCalls - afterBetween.streamFreqCalls,
	         afterFirst.skinnedZeroStrideDraws - entry.skinnedZeroStrideDraws,
	         afterSecond.skinnedZeroStrideDraws - afterBetween.skinnedZeroStrideDraws);
}

// Stands where the entry of kRenderScene used to be, with the same calling
// convention. See the type alias above for why __fastcall.
void __fastcall HookedRenderScene(void* self, void* unusedEdx, void* renderedTexture) {
	++g_sceneCall;
	g_lastRendererSelf = self;

	// The last moment anything can still change what gets drawn, and the only
	// one late enough for the player's own bones - the engine's animation step
	// runs after the camera hook and overwrites anything set there.
	//
	// Only on a real world render. renderedTexture non-null is the engine
	// drawing into something of its own, and the player is not in those.
	if (g_callbacks.beforeFirstPass != nullptr && renderedTexture == nullptr) {
		g_callbacks.beforeFirstPass();
	}

	// The reading that makes the probe's readings mean something: the same
	// fields, at the one moment the render provably draws the whole world.
	// Everything the probe records is taken from Present, where the identical
	// call comes back empty even on a world frame - so without this side of
	// the comparison there is nothing to subtract, and a field that looks
	// wrong in Present might have looked exactly the same here.
	if (g_engineSideReportsLeft > 0 && renderedTexture == nullptr) {
		--g_engineSideReportsLeft;
		ProbeSceneGraph(g_sceneCall, "inside the engine's render (it draws here)");
	}

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
		// What the engine's own render costs, in the same two numbers the
		// probe reports. Without them the probe's "343 setup calls, no draws"
		// has nothing to be large or small against - and the difference
		// between "the walk found nothing and 343 is fixed overhead" and "the
		// walk found everything and the draws went missing" is the whole
		// remaining question. Same budget as the scene graph report beside
		// it, and only for the ordinary pass.
		const bool measureThisOne = g_engineCostReportsLeft > 0 && renderedTexture == nullptr;
		const UInt32 drawsBefore = measureThisOne ? TotalDrawCount() : 0;
		const UInt32 setupBefore = measureThisOne ? VertexSetupTotal(TotalStateCalls()) : 0;

		g_original(self, unusedEdx, renderedTexture);

		if (measureThisOne) {
			--g_engineCostReportsLeft;
			OBVR_LOG("Scene render cost: the engine's own render made %u draw call(s) with "
			         "%u vertex setup call(s) (scene call %u)",
			         TotalDrawCount() - drawsBefore,
			         VertexSetupTotal(TotalStateCalls()) - setupBefore, g_sceneCall);
		}

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

	// The timeline records every dual frame afresh until one is worth
	// keeping. Recording the frame after a divergent one missed twice -
	// the divergence does not repeat on a fixed schedule - so the recorder
	// now runs always and the decision to dump is made at the end of the
	// very frame the events belong to.
	if (!PoolTimelineWasDumped()) {
		ArmPoolTimeline();
		MarkPoolTimeline("first pass begins");
	}

	// First eye. The camera hook already moved the camera there. The bone
	// lock records this render's palettes.
	SetBonePassMode(BonePassMode::Capture);
	g_original(self, unusedEdx, renderedTexture);
	SetBonePassMode(BonePassMode::Off);
	const UInt32 drawsAfterFirst = TotalDrawCount();
	const StateCallCounts stateAfterFirst = TotalStateCalls();

	// The same reading the single-pass branch takes, because whichever branch
	// a run happens to take is not the run's subject - the first pass of a
	// dual frame is the identical engine call, and the probe's numbers need
	// something to be compared against either way.
	if (g_engineCostReportsLeft > 0) {
		--g_engineCostReportsLeft;
		OBVR_LOG("Scene render cost: the engine's own render made %u draw call(s) with %u "
		         "vertex setup call(s) (scene call %u, first pass of a dual frame)",
		         drawsAfterFirst - drawsAtEntry,
		         VertexSetupTotal(stateAfterFirst) - VertexSetupTotal(stateAtEntry), g_sceneCall);
	}

	MarkPoolTimeline("between the passes");
	g_callbacks.betweenPasses();
	const UInt32 drawsAfterBetween = TotalDrawCount();
	const StateCallCounts stateAfterBetween = TotalStateCalls();
	MarkPoolTimeline("second pass begins");

	// Second eye, from a camera one interpupillary distance over. The bone
	// lock judges this render's palettes against the first render's: the
	// palettes are camera-relative, correct rows differ by exactly the eye
	// baseline and pass through, and only the instance mixups - the
	// collapse - get the first render's row back, rebased into this eye.
	// (The stereo headset exposed what the monitor could not: the earlier
	// blanket replay froze every skinned body at the first eye's position.)
	//
	// The frame clock is zeroed across this render: every time-driven
	// update inside the walk - animation controllers, the NPC head-aim
	// that hair and helmets hang from - advances by the frame delta, and
	// a second render advancing them again is a frame that plays twice.
	// A delta of zero makes the second walk draw without moving anything.
	// (A microsecond instead of zero was tried, to let time-gated rebuilds
	// see the second eye's camera; it made the head-part offset worse and
	// added a vertical component, so zero stands.) Touched only when the
	// value reads like a frame delta at all; a wrong address would read
	// garbage, and garbage is left alone.
	float* const frameSeconds =
		reinterpret_cast<float*>(addr::kFrameSecondsAddress);
	const float savedFrameSeconds = *frameSeconds;
	const bool clockPlausible = savedFrameSeconds >= 0.0f && savedFrameSeconds < 1.0f;
	static bool s_clockReported = false;
	if (!s_clockReported) {
		s_clockReported = true;
		OBVR_LOG("Dual clock: frame delta reads %.5f s - %s for the second render",
		         savedFrameSeconds,
		         clockPlausible ? "zeroed" : "left alone (implausible)");
	}
	if (clockPlausible) {
		*frameSeconds = 0.0f;
	}
	SetBonePassMode(BonePassMode::Replace);
	g_original(self, unusedEdx, renderedTexture);
	SetBonePassMode(BonePassMode::Off);
	if (clockPlausible) {
		*frameSeconds = savedFrameSeconds;
	}
	const UInt32 drawsAfterSecond = TotalDrawCount();
	const StateCallCounts stateAfterSecond = TotalStateCalls();
	MarkPoolTimeline("frame ends");

	// Kept only when this very frame diverged: same number of skinned
	// draws in both renders, different base-vertex sums - the collapse's
	// signature. Anything else is discarded by the next frame's re-arm.
	{
		const UInt32 drawsFirst = stateAfterFirst.skinnedDraws - stateAtEntry.skinnedDraws;
		const UInt32 drawsSecond =
			stateAfterSecond.skinnedDraws - stateAfterBetween.skinnedDraws;
		const UInt32 baseFirst =
			stateAfterFirst.skinnedBaseVertexSum - stateAtEntry.skinnedBaseVertexSum;
		const UInt32 baseSecond =
			stateAfterSecond.skinnedBaseVertexSum - stateAfterBetween.skinnedBaseVertexSum;
		// A small base difference is the steady-state collapse - one or two
		// draws reading a few dozen vertices off. A huge one is a transition
		// frame (menus reshuffle the pool wholesale) and worth skipping:
		// the first divergent dump caught an Esc transition whose passes
		// used different buffers entirely, which is chaos, not the disease.
		const UInt32 gap = baseFirst > baseSecond ? baseFirst - baseSecond
		                                          : baseSecond - baseFirst;
		if (drawsFirst == drawsSecond && drawsFirst >= 20 && gap != 0 && gap <= 2048) {
			DumpPoolTimeline();
		}
	}
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
	TraceZeroMatrixDraws(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
	TraceIndexSide(stateAtEntry, stateAfterFirst, stateAfterBetween, stateAfterSecond);
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

const char* MenuWorldProbeRefusal() {
	if (g_original == nullptr) {
		return "the scene render is not hooked";
	}
	if (g_lastRendererSelf == nullptr) {
		return "no world render has been seen yet, so there is no renderer to call";
	}
	if (g_rendering) {
		return "a render is already running";
	}
	return "no reason - it ran";
}

bool RunMenuWorldProbe(UInt32& drawsOut, UInt32& vertexSetupOut) {
	drawsOut = 0;
	vertexSetupOut = 0;
	if (g_original == nullptr || g_lastRendererSelf == nullptr || g_rendering) {
		return false;
	}

	// Guarded like the dual pass: if the render somehow reaches the patched
	// entry again, it passes through instead of recursing.
	g_rendering = true;

	// The clock is zeroed exactly as it is for the second pass of a dual
	// frame, and for the same reason: every time-driven update inside the
	// walk advances by the frame delta, and a render the engine did not
	// schedule must not advance the world it is only meant to look at. On a
	// paused menu the delta is likely stale anyway - stale is precisely what
	// must not be replayed.
	float* const frameSeconds = reinterpret_cast<float*>(addr::kFrameSecondsAddress);
	const float savedFrameSeconds = *frameSeconds;
	const bool clockPlausible = savedFrameSeconds >= 0.0f && savedFrameSeconds < 1.0f;
	if (clockPlausible) {
		*frameSeconds = 0.0f;
	}

	// Through the trampoline, not the patched entry: no second pass, no
	// callbacks, no scene-call tick - this render is a measurement, not a
	// frame. A null texture argument is the ordinary world render.
	const UInt32 before = TotalDrawCount();
	const UInt32 setupBefore = VertexSetupTotal(TotalStateCalls());
	g_original(g_lastRendererSelf, nullptr, nullptr);
	drawsOut = TotalDrawCount() - before;
	vertexSetupOut = VertexSetupTotal(TotalStateCalls()) - setupBefore;

	if (clockPlausible) {
		*frameSeconds = savedFrameSeconds;
	}
	g_rendering = false;
	return true;
}

}  // namespace obvr::render
