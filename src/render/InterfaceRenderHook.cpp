#include "render/InterfaceRenderHook.h"

#include <cstring>

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuMode.h"
#include "platform/Win32Min.h"
#include "render/BoneRebase.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/LockLedger.h"
#include "render/PointerSet.h"
#include "render/PresentHook.h"
#include "render/ResolutionHook.h"
#include "render/SceneRenderHook.h"
#include "render/UiScreenSize.h"

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
d3d9::SetViewportFn g_originalSetViewport = nullptr;
bool g_targetHookRefused = false;

// While true, the game is inside its own 2D pass - every run of the original
// kRenderInterface that draws the frame's layer, redirected or not, main menu
// included. Not set for the game's menu-to-texture passes: those draw into
// textures of their own sizes, and their viewports are their own business.
// Read by the SetViewport hook, which is where the split's drawing half is
// closed - see DecideInterfaceViewport in UiScreenSize.h.
bool g_inInterfacePass = false;

// Whether the device's current colour target is the substitute texture -
// bookkeeping fed by the SetRenderTarget hook, so the viewport hook can
// protect the layer texture even OUTSIDE the pass window. The measured need:
// after a redirected pass returns, the engine draws one more cursor quad
// with a viewport it sets to the full frame first - past the pass window,
// into the still-bound layer texture - and that quad is the cursor the
// headset actually shows, three entries below its own hit test.
bool g_targetIsSubstitute = false;

// The one-draw window after a redirected pass. Measured on the user's run:
// once the redirected pass returns and the back buffer is re-aimed, the
// engine sets one more full-frame viewport and draws one more cursor quad -
// into the back buffer the cinema shows, at frameHeight over believedHeight
// times its position. That quad is the pointer the headset shows, three
// entries below the highlight that tracks the real cursor. The window opens
// as the redirected pass returns and closes at the next draw or target
// change, whichever comes first - so the only viewport it can ever shrink
// is that quad's own.
bool g_afterRedirectWindow = false;

// The first few viewports the pass sets, logged with what was done to them:
// the evidence that the engine does set its own viewport inside the pass, and
// what it asked for - the measurement this hook was built on top of.
UInt32 g_viewportTraceLeft = 8;

// Its own budget for the wider window's catches: the eight above are spent
// on the first menu's in-pass viewports long before a redirected pass ever
// draws its after-pass cursor, and the fix's own evidence must not starve.
UInt32 g_afterPassViewportTraceLeft = 6;

// How many dropped late-cursor quads still get a log line of their own.
UInt32 g_cursorDropTraceLeft = 4;

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

// The after-pass window for the cursor-draw probe. The cursor probe showed
// the manager's position, the sprite node and the hit test standing on one
// point, yet the visible sprite sits low by frameHeight/believedHeight and
// runs off the picture before the position reaches its own clamp - so the
// wrong number is in some DRAW's viewport, and the interface pass's draws
// are already corrected. The suspect is a draw after the pass, outside the
// viewport hook's window. While Debug.CursorProbe is on, every 120th pass
// arms a few samples: each draw until the next pass logs its viewport and
// world translation, which is enough to recognise a cursor quad drawn
// full-frame.
UInt32 g_afterPassSamples = 0;
UInt32 g_afterPassCount = 0;

// The place experiment's budget - see HookedRenderInterface. Each one costs a
// whole wasted world render, and three is enough to see whether the answer is
// the same one Present gives.
UInt32 g_placeProbesLeft = 3;
UInt32 g_placeProbesMenuLeft = 3;
bool g_placeProbeMenuWasUp = false;


// And the pass's own tail, kept in a ring on the same armed pass: if the
// cursor is drawn INSIDE the pass it is drawn late, so the last few draws
// with their viewports answer the in-pass half of the question in the same
// run that answers the after-pass half.
struct ProbedDraw {
	const char* kind;
	UInt32 type;
	UInt32 count;
	d3d9::Viewport viewport;
	float worldX;
	float worldY;
	float worldZ;
};
bool g_tailArmed = false;
ProbedDraw g_tailRing[4] = {};
UInt32 g_tailNext = 0;
UInt32 g_tailSeen = 0;

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

d3d9::SetTransformFn g_originalSetTransform = nullptr;
d3d9::SetVertexDeclarationFn g_originalSetVertexDecl = nullptr;
d3d9::SetFVFFn g_originalSetFVF = nullptr;
d3d9::SetVertexShaderFn g_originalSetVertexShader = nullptr;
d3d9::SetVertexShaderConstantFFn g_originalSetVsConstantF = nullptr;

// Counting always, exactly like g_drawsTotal above: the scene hook reads the
// totals either side of each world render and subtracts.
StateCallCounts g_stateCalls;

// The buffers ever locked with DISCARD - the dynamic pool software-skinned
// geometry is packed into - and what stream 0 currently points at. Kept by
// the lock and stream hooks, read by the draw hooks to tell a skinned draw
// from a static one.
PointerSet g_dynamicBuffers;
void* g_stream0Buffer = nullptr;
UInt32 g_stream0Offset = 0;

// The mapped ranges currently open on the dynamic pool, fingerprinted at
// unlock. See LockLedger.h for the bounds.
LockLedger g_openLocks;

// The index-buffer mirror of the vertex pool watch: the buffers ever
// discard-locked, their open mappings, and what SetIndices last bound.
PointerSet g_dynamicIndexBuffers;
LockLedger g_openIbLocks;
UInt32 g_stream0Stride = 0;

// How many leading bytes of each pool write are fingerprinted. Enough to
// catch collapsed positions, cheap enough for write-combined memory.
constexpr UInt32 kWriteFingerprintBytes = 256;

// One frame's pool timeline - see the header note. A fixed event array,
// armed by the scene hook, filled by the hooks below, dumped once.
struct TimelineEvent {
	char kind;          // 'L' lock, 'U' unlock, 'D' skinned draw, 'M' marker
	const char* label;  // markers only; the callers pass string literals
	void* buffer;
	UInt32 a;  // lock: offset  | unlock: fingerprint sum | draw: stream offset
	UInt32 b;  // lock: size    | draw: vertices
	UInt32 c;  // lock: flags   | draw: primitives
};
constexpr UInt32 kTimelineCapacity = 2048;
TimelineEvent g_timeline[kTimelineCapacity];
UInt32 g_timelineCount = 0;
bool g_timelineArmed = false;
bool g_timelineDumped = false;

void RecordTimeline(char kind, const char* label, void* buffer, UInt32 a, UInt32 b, UInt32 c) {
	if (!g_timelineArmed || g_timelineCount >= kTimelineCapacity) {
		return;
	}
	g_timeline[g_timelineCount] = TimelineEvent{kind, label, buffer, a, b, c};
	++g_timelineCount;
}

// The first vertex of the first pool writes of the frame, kept raw. The
// unlock fingerprints showed the second render packing different bytes
// for the same body parts; whether those bytes are correct specular for
// the other camera or a degenerate position is a question sums cannot
// answer and one vertex can.
constexpr UInt32 kVertexPeekSlots = 96;  // both passes' writes, not just the first's
constexpr UInt32 kVertexPeekFloats = 18;  // one 72-byte vertex
struct VertexPeek {
	void* buffer;
	UInt32 offset;
	float floats[kVertexPeekFloats];
};
VertexPeek g_vertexPeeks[kVertexPeekSlots];
UInt32 g_vertexPeekCount = 0;

// Which stretch of the dual frame the recorder is in: 0 first render,
// 1 between, 2 second render. Set by the pass markers, read by the bone
// peeks so both renders contribute their share.
UInt32 g_timelinePhase = 0;

// The whole first render's bone uploads, replayed against the second
// render's as they arrive. The first six compared equal by hand, so the
// divergence the sums keep reporting sits somewhere in the hundreds
// that follow - this finds the first mismatching upload and keeps both
// versions of it.
constexpr UInt32 kBoneLogCapacity = 4096;  // busy frames carry ~1800 bone uploads
BoneRow g_boneLog[kBoneLogCapacity];
UInt32 g_boneLogCount = 0;       // uploads recorded during the first render
UInt32 g_boneCompareIndex = 0;   // second-render uploads compared so far
UInt32 g_boneMismatchCount = 0;  // how many compares disagreed
UInt32 g_boneMismatchAt = 0;     // index of the first disagreement
BoneRow g_boneMismatchFirst;     // the first render's version of it
BoneRow g_boneMismatchSecond;    // the second render's version

BonePassMode g_boneMode = BonePassMode::Off;

// The frame's eye-baseline measurement, the camera shift the dual pass
// reported for this frame, the calibrated sign of the palette convention,
// and the scratch row a rebased upload is served from. One scratch is
// enough: the device consumes the pointer inside the same
// SetVertexShaderConstantF call, and the game's D3D9 use is
// single-threaded.
EyeDeltaEstimate g_eyeDelta;
float g_boneEyeShift[3] = {0.0f, 0.0f, 0.0f};
ShiftSign g_boneShiftSign = ShiftSign::Unknown;
bool g_boneShiftSignReported = false;
float g_boneRebaseRow[kBoneRowFloats];

// Whether bone rows are currently heading for the world render's target.
// Shadow and reflection sub-passes render the same skeletons into their
// own smaller textures, camera-free - shifting those gave every shadow a
// body's parallax. Kept by the SetRenderTarget hook while a bone mode is
// on (see BoneTargetIsWorldSized); rows heading elsewhere pass through
// untouched, in the capture as in the replace, so the ring stays in step.
// The main width is measured once from the back buffer; without the HUD
// hook neither the width nor the target hook exists, and everything
// degrades to the pre-shift behaviour of locking every row.
bool g_boneTargetIsMain = true;
UInt32 g_boneMainWidth = 0;

// Called for every bone-shaped upload (the three register classes, exactly
// three vectors). During the first render it records; during the second it
// pairs each arriving row with the first render's version of the same bone
// and replaces it with that row shifted by the eye baseline - blanket, the
// semantics that held the collapse down, but landing on the second eye
// instead of freezing the first. Rows that did re-evaluate measure the
// baseline on their way through to calibrate the shift sign. See
// BoneRebase.h for the two failed variants this shape came out of, and
// FindBoneRow there for which row counts as the pair.
// Returns null when the upload is to pass through unchanged.
const float* HandleBoneUpload(UInt32 startRegister, const float* data) {
	if (g_boneMode == BonePassMode::Capture) {
		if (g_boneLogCount < kBoneLogCapacity) {
			BoneRow& slot = g_boneLog[g_boneLogCount];
			slot.startRegister = startRegister;
			std::memcpy(slot.floats, data, sizeof(slot.floats));
		}
		++g_boneLogCount;
		return nullptr;
	}
	if (g_boneMode != BonePassMode::Replace) {
		return nullptr;
	}
	// Pairing by content, not position: a bone row's rotation is
	// bit-identical between the renders (only its translation changes
	// frame of reference), which makes the nine rotation floats a
	// fingerprint of the bone. Camera turns reorder the second render's
	// uploads, so the pair is searched for from the current position
	// forward over the whole ring. Off position, only a row that agrees on
	// the instance is taken: a same-posed stranger is refused, because a
	// row the first render never uploaded - a body or a part only this
	// eye's frustum holds - has no pair, and the stranger's translation
	// would snap it a body length over and drag the running position with
	// it (the edge-of-view collapse and the cutscene snap-away).
	const UInt32 limit = g_boneLogCount < kBoneLogCapacity ? g_boneLogCount
	                                                       : kBoneLogCapacity;
	float delta[3];
	ChooseRebaseDelta(g_eyeDelta, g_boneEyeShift, g_boneShiftSign, delta);
	const BoneMatch match =
		FindBoneRow(g_boneLog, limit, g_boneCompareIndex, startRegister, data, delta);
	if (match.kind == BoneMatchKind::None) {
		++g_stateCalls.boneLockPassthrough;
		if (match.strangersRefused != 0) {
			++g_stateCalls.boneLockRefused;
		}
		return nullptr;
	}
	const BoneRow& candidate = g_boneLog[match.index];
	const float* f = candidate.floats;
	// Same bone. Re-evaluated rows sit one eye baseline from their pair
	// and measure it; the evidence recorder keeps the first pair a body
	// length apart - a mixup - when the timeline is armed. Either way
	// the row is replaced with the pair shifted onto the second eye.
	const float distSq = BoneTranslationDistSq(data, f);
	if (IsEyeBaselineSample(distSq)) {
		AddEyeDeltaSample(g_eyeDelta, data, f);
		// The sample just taken counts towards this row's own shift while
		// the sign is still uncalibrated.
		ChooseRebaseDelta(g_eyeDelta, g_boneEyeShift, g_boneShiftSign, delta);
	} else if (g_timelineArmed && distSq > kBoneMixupThresholdSq) {
		if (g_boneMismatchCount == 0) {
			g_boneMismatchAt = match.index;
			g_boneMismatchFirst = candidate;
			g_boneMismatchSecond.startRegister = startRegister;
			std::memcpy(g_boneMismatchSecond.floats, data,
			            sizeof(g_boneMismatchSecond.floats));
		}
		++g_boneMismatchCount;
	}
	RebaseBoneRow(g_boneRebaseRow, f, delta);
	g_boneCompareIndex = match.index + 1;
	++g_stateCalls.boneLockReplaced;
	if (match.kind == BoneMatchKind::Reordered) {
		++g_stateCalls.boneLockReordered;
	}
	return g_boneRebaseRow;
}

void PeekVertices(void* buffer, UInt32 offset, const void* data, UInt32 size) {
	if (!g_timelineArmed || g_vertexPeekCount >= kVertexPeekSlots ||
	    size < kVertexPeekFloats * 4) {
		return;
	}
	VertexPeek& peek = g_vertexPeeks[g_vertexPeekCount];
	peek.buffer = buffer;
	peek.offset = offset;
	std::memcpy(peek.floats, data, sizeof(peek.floats));
	++g_vertexPeekCount;
}

// Whether the last upload that started at register 0 - ModelViewProj in
// every vertex shader of the active package - was an all-zero matrix.
// Order-sensitive where the counters are not: a draw sees the registers
// as they stand, and a zero matrix at draw time collapses every vertex
// onto one clip-space point.
bool g_lastC0Zero = false;

// Whether software vertex processing is currently on - flipped by the hook
// below, read at every skinned draw.
bool g_swvpOn = false;
d3d9::SetSoftwareVertexProcessingFn g_originalSetSwvp = nullptr;

// The skinned-draw half of the fingerprint, shared by both draw hooks.
// The indexed hook passes its addressing triple; the plain hook has none.
void CountSkinnedDrawAddressing(SInt32 baseVertexIndex, UInt32 minVertexIndex,
                                UInt32 startIndex) {
	if (g_stream0Buffer == nullptr || !g_dynamicBuffers.Contains(g_stream0Buffer)) {
		return;
	}
	g_stateCalls.skinnedBaseVertexSum += static_cast<UInt32>(baseVertexIndex);
	g_stateCalls.skinnedMinVertexSum += minVertexIndex;
	g_stateCalls.skinnedStartIndexSum += startIndex;
}

// vertexBase is the draw's BaseVertexIndex (StartVertex for the non-indexed
// hook) - the addressing whose per-pass sums differ while everything else
// matches, and therefore the one number the timeline has to carry per draw.
void CountSkinnedDraw(UInt32 vertexBase, UInt32 numVertices, UInt32 primCount) {
	if (g_stream0Buffer == nullptr || !g_dynamicBuffers.Contains(g_stream0Buffer)) {
		return;
	}
	++g_stateCalls.skinnedDraws;
	g_stateCalls.skinnedVertexSum += numVertices;
	g_stateCalls.skinnedPrimSum += primCount;
	g_stateCalls.skinnedOffsetSum += g_stream0Offset;
	if (g_lastC0Zero) {
		++g_stateCalls.skinnedZeroMatrixDraws;
	}
	if (g_stream0Stride == 0) {
		++g_stateCalls.skinnedZeroStrideDraws;
	}
	if (g_swvpOn) {
		++g_stateCalls.swvpOnDraws;
	}
	RecordTimeline('D', nullptr, g_stream0Buffer, vertexBase, numVertices, primCount);
}

SInt32 __stdcall HookedSetSoftwareVertexProcessing(void* self, SInt32 software) {
	++g_stateCalls.swvpToggles;
	g_swvpOn = software != 0;
	return g_originalSetSwvp(self, software);
}

d3d9::CreateVertexBufferFn g_originalCreateVertexBuffer = nullptr;
d3d9::VertexBufferLockFn g_originalVbLock = nullptr;
d3d9::VertexBufferUnlockFn g_originalVbUnlock = nullptr;

// The one vertex buffer vtable seen so far. Direct3D implements every
// buffer as an instance of one class, so the first buffer's vtable is every
// buffer's vtable - one patch counts them all, existing buffers included.
// A second, different table would mean that assumption broke; it is
// reported rather than patched, because the original Lock kept above
// belongs to the first.
void** g_vbVtable = nullptr;
bool g_vbSecondVtableReported = false;

// The index-buffer class vtable, patched from the first index buffer the
// game creates - the same one-class argument as the vertex side.
d3d9::CreateIndexBufferFn g_originalCreateIndexBuffer = nullptr;
d3d9::VertexBufferLockFn g_originalIbLock = nullptr;
d3d9::VertexBufferUnlockFn g_originalIbUnlock = nullptr;
void** g_ibVtable = nullptr;
bool g_ibSecondVtableReported = false;

SInt32 __stdcall HookedSetRenderTarget(void* self, UInt32 index, void* surface) {
	// Where the next bone rows are heading. Only while a bone mode is on -
	// outside dual frames the answer is never read - and only for target
	// zero, the one the draws land in. An unreadable surface is treated as
	// the world's: locking a row too many is the old, survivable behaviour,
	// while skipping world rows would desynchronise the ring.
	if (index == 0 && surface != nullptr && g_boneMode != BonePassMode::Off) {
		d3d9::SurfaceDesc desc{};
		auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
		if (getDesc != nullptr && getDesc(surface, &desc) >= 0) {
			g_boneTargetIsMain = BoneTargetIsWorldSized(desc.width, g_boneMainWidth);
		} else {
			g_boneTargetIsMain = true;
		}
	}
	bool substituted = false;
	if ((g_redirecting || g_observing) && index == 0 && surface != nullptr) {
		if (surface == g_backBuffer) {
			++g_statsMatched;
			if (g_redirecting) {
				g_lastRequested = surface;
				surface = g_substitute;
				substituted = true;
			}
		} else if (surface != g_substitute) {
			++g_statsOtherTargets;
		}
	}
	const SInt32 result = g_originalSetTarget(self, index, surface);

	if (index == 0 && result >= 0) {
		g_targetIsSubstitute = surface != nullptr && surface == g_substitute;
		g_afterRedirectWindow = false;
	}

	// The API-rule half of the viewport correction. SetRenderTarget resets
	// the viewport to the new target's full size silently - no SetViewport
	// call, so the hook below never sees it - and the substitute texture is
	// frame-sized. The cinema run then measured the engine also setting a
	// full-frame viewport of its own inside the pass, which is what the
	// SetViewport hook shrinks; this write covers the passes where it does
	// not, so both roads end at the believed rectangle. Through the original:
	// what this sets is already believed, and the hook would only wave it
	// through.
	if (substituted && result >= 0 && g_originalSetViewport != nullptr) {
		UInt32 believedWidth = 0;
		UInt32 believedHeight = 0;
		if (GameBelievedSize(believedWidth, believedHeight)) {
			const d3d9::Viewport viewport{0, 0, believedWidth, believedHeight, 0.0f, 1.0f};
			g_originalSetViewport(self, &viewport);
		}
	}
	return result;
}

// The engine's own viewport for the 2D pass, corrected where it is set.
//
// The 2D lays out and maps its mouse against the screen-size copy, but its
// pixels come out of the viewport: untransformed vertices, an orthographic
// projection built from the copy, and then NDC times the viewport. The
// engine sets that viewport per pass to the real target's full size from
// its own bookkeeping - after any target substitution, which is why setting
// a viewport in the SetRenderTarget hook alone did not hold. The cinema run
// measured the result: menus drawn at layout times frameHeight over
// believedHeight, cursor and buttons apart again.
//
// So while the interface pass runs, exactly the full-frame viewport becomes
// the believed rectangle. Partial viewports pass through untouched, and
// with the belief equal to the frame the decision never fires.
SInt32 __stdcall HookedSetViewport(void* self, const d3d9::Viewport* viewport) {
	// Inside the pass, or any time the layer texture is the bound target: the
	// second window is what catches the engine's after-pass cursor quad,
	// whose full-frame viewport is set once the pass window has closed. The
	// 3D never binds the layer texture, so the wider window reaches nothing
	// else.
	if ((g_inInterfacePass || g_targetIsSubstitute || g_afterRedirectWindow) &&
	    viewport != nullptr) {
		UInt32 frameWidth = 0;
		UInt32 frameHeight = 0;
		UInt32 believedWidth = 0;
		UInt32 believedHeight = 0;
		if (WasDeviceCreated(frameWidth, frameHeight) &&
		    GameBelievedSize(believedWidth, believedHeight)) {
			const InterfaceViewportAction action = DecideInterfaceViewport(
				viewport->x, viewport->y, viewport->width, viewport->height, frameWidth,
				frameHeight, believedWidth, believedHeight);
			UInt32& traceLeft =
			    g_inInterfacePass ? g_viewportTraceLeft : g_afterPassViewportTraceLeft;
			if (traceLeft > 0) {
				--traceLeft;
				OBVR_LOG("Hud viewport: %s set %ux%u at %u,%u (frame %ux%u, "
				         "believed %ux%u) - %s",
				         g_inInterfacePass ? "the pass" : "the layer's after-pass",
				         viewport->width, viewport->height, viewport->x, viewport->y,
				         frameWidth, frameHeight, believedWidth, believedHeight,
				         action == InterfaceViewportAction::Shrink ? "shrunk to believed"
				                                                   : "left alone");
			}
			if (action == InterfaceViewportAction::Shrink) {
				const d3d9::Viewport shrunk{0,    0,      believedWidth, believedHeight,
				                            viewport->minZ, viewport->maxZ};
				return g_originalSetViewport(self, &shrunk);
			}
		}
	}
	return g_originalSetViewport(self, viewport);
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

// One line per sampled after-pass draw: enough to recognise the cursor - a
// small primitive count, a world translation matching the cursor's own
// position - and the viewport it runs through, which is the number on trial.
void SampleAfterPassDraw(void* device, const char* kind, UInt32 type, UInt32 count) {
	d3d9::Viewport viewport{};
	if (auto getViewport =
	        d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport)) {
		getViewport(device, &viewport);
	}
	d3d9::Matrix4 world{};
	if (auto getTransform =
	        d3d9::Method<d3d9::GetTransformFn>(device, d3d9::kDeviceGetTransform)) {
		getTransform(device, d3d9::kTransformWorld, &world);
	}
	OBVR_LOG("Cursor draw probe: %s type=%u count=%u viewport=%ux%u at %u,%u "
	         "world=(%.1f, %.1f, %.1f) target=%s",
	         kind, type, count, viewport.width, viewport.height, viewport.x, viewport.y,
	         static_cast<double>(world.m[3][0]), static_cast<double>(world.m[3][1]),
	         static_cast<double>(world.m[3][2]),
	         g_targetIsSubstitute ? "layer texture" : "other");
}

// The shared gate for the four draw hooks: on the armed pass the tail ring
// records in-pass draws, and after it the sample window logs the draws that
// run outside the viewport hook's protection. One branch where both are off.
void MaybeSampleAfterPassDraw(void* device, const char* kind, UInt32 type, UInt32 count) {
	if (g_inInterfacePass) {
		if (!g_tailArmed) {
			return;
		}
		ProbedDraw& slot = g_tailRing[g_tailNext];
		g_tailNext = (g_tailNext + 1) % 4;
		++g_tailSeen;
		slot.kind = kind;
		slot.type = type;
		slot.count = count;
		slot.viewport = d3d9::Viewport{};
		if (auto getViewport =
		        d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport)) {
			getViewport(device, &slot.viewport);
		}
		d3d9::Matrix4 world{};
		if (auto getTransform =
		        d3d9::Method<d3d9::GetTransformFn>(device, d3d9::kDeviceGetTransform)) {
			getTransform(device, d3d9::kTransformWorld, &world);
		}
		slot.worldX = world.m[3][0];
		slot.worldY = world.m[3][1];
		slot.worldZ = world.m[3][2];
		return;
	}
	if (g_afterPassSamples > 0) {
		--g_afterPassSamples;
		SampleAfterPassDraw(device, kind, type, count);
	}
}

// The one-draw window's decision, taken by every draw hook: whether this
// draw is the engine's late cursor quad, which is then dropped whole.
//
// The trail that ends here: the pass draws the cursor correctly (measured -
// its quad's world translation is the cursor position, its viewport the
// believed rectangle, and the headless screenshots show the arrow exactly on
// the button the position points at). Then, after the pass returns, the
// engine draws the cursor AGAIN - the background-mouse drawing path at
// 0x410900, which runs whether or not the background-mouse thread does, and
// which scales through the screen-size copy against a queried real texture
// size. In vanilla those are equal and the second cursor lands exactly on
// the first; with the copy raised it lands at frameHeight over
// believedHeight times the position - the pointer the headset shows, three
// entries below the highlight. Shrinking its viewport did nothing because
// its vertices carry finished pixels. So the redundant quad is dropped: the
// pass's own cursor, already correct everywhere the headset looks, is the
// one that remains. Every draw closes the window either way.
bool TakeAfterPassCursorQuad(void* device, UInt32 type, UInt32 primitiveCount) {
	if (g_inInterfacePass || !g_afterRedirectWindow) {
		return false;
	}
	g_afterRedirectWindow = false;
	if (type != 4 || primitiveCount != 2) {
		return false;
	}

	// A two-triangle strip alone is not identity enough: the same shape
	// draws water reflections into small render targets right after a pass.
	// The cursor quad's viewport is frame-wide - the engine's own, or the
	// believed one the viewport hook already shrank it to.
	UInt32 frameWidth = 0;
	UInt32 frameHeight = 0;
	if (!WasDeviceCreated(frameWidth, frameHeight)) {
		return false;
	}
	d3d9::Viewport viewport{};
	if (auto getViewport =
	        d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport)) {
		getViewport(device, &viewport);
	}
	return viewport.width == frameWidth;
}

SInt32 __stdcall HookedDrawPrimitive(void* self, UInt32 type, UInt32 startVertex,
                                     UInt32 primitiveCount) {
	++g_drawsTotal;
	// No vertex count in this signature; the primitive count still travels.
	CountSkinnedDraw(startVertex, 0, primitiveCount);
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dp", type, primitiveCount);
	}
	MaybeSampleAfterPassDraw(self, "dp", type, primitiveCount);
	if (TakeAfterPassCursorQuad(self, type, primitiveCount)) {
		return 0;
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
	CountSkinnedDraw(static_cast<UInt32>(baseVertexIndex), numVertices, primCount);
	CountSkinnedDrawAddressing(baseVertexIndex, minVertexIndex, startIndex);
	if ((g_redirecting || g_observing) && g_sampleNextDraw) {
		g_sampleNextDraw = false;
		SampleFirstDraw(self, "dip", type, primCount);
	}
	MaybeSampleAfterPassDraw(self, "dip", type, primCount);
	if (TakeAfterPassCursorQuad(self, type, primCount)) {
		if (g_cursorDropTraceLeft > 0) {
			--g_cursorDropTraceLeft;
			OBVR_LOG("Hud cursor: the engine's late cursor quad was dropped - the pass's "
			         "own cursor, drawn in the believed space, is the one that shows");
		}
		return 0;
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
	MaybeSampleAfterPassDraw(self, "dpup", type, primitiveCount);
	if (TakeAfterPassCursorQuad(self, type, primitiveCount)) {
		return 0;
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
	MaybeSampleAfterPassDraw(self, "dipup", type, primitiveCount);
	if (TakeAfterPassCursorQuad(self, type, primitiveCount)) {
		return 0;
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

// The five vertex pipeline counters. Nothing is inspected and nothing is
// gated: one increment, then the game's own method. They stand apart from
// the draw hooks above because they have no share in the redirect - they
// exist so the scene hook can compare the setup work of the two world
// renders of a dual frame.
SInt32 __stdcall HookedSetTransform(void* self, UInt32 state, const d3d9::Matrix4* matrix) {
	++g_stateCalls.transforms;
	return g_originalSetTransform(self, state, matrix);
}

SInt32 __stdcall HookedSetVertexDeclaration(void* self, void* declaration) {
	++g_stateCalls.declarations;
	g_stateCalls.declarationSum += reinterpret_cast<UInt32>(declaration);
	return g_originalSetVertexDecl(self, declaration);
}

SInt32 __stdcall HookedSetFVF(void* self, UInt32 fvf) {
	++g_stateCalls.fvfs;
	return g_originalSetFVF(self, fvf);
}

SInt32 __stdcall HookedSetVertexShader(void* self, void* shader) {
	++g_stateCalls.vertexShaders;
	g_stateCalls.vertexShaderSum += reinterpret_cast<UInt32>(shader);
	return g_originalSetVertexShader(self, shader);
}

d3d9::SetStreamSourceFn g_originalSetStreamSource = nullptr;

SInt32 __stdcall HookedSetStreamSource(void* self, UInt32 streamNumber, void* streamData,
                                       UInt32 offsetInBytes, UInt32 stride) {
	++g_stateCalls.streamSources;
	g_stateCalls.streamSourceSum += reinterpret_cast<UInt32>(streamData);
	if (streamNumber == 0) {
		g_stream0Buffer = streamData;
		g_stream0Offset = offsetInBytes;
		g_stream0Stride = stride;
		g_stateCalls.streamStrideSum += stride;
	}
	return g_originalSetStreamSource(self, streamNumber, streamData, offsetInBytes, stride);
}

d3d9::SetStreamSourceFreqFn g_originalSetStreamSourceFreq = nullptr;

SInt32 __stdcall HookedSetStreamSourceFreq(void* self, UInt32 streamNumber, UInt32 setting) {
	++g_stateCalls.streamFreqCalls;
	return g_originalSetStreamSourceFreq(self, streamNumber, setting);
}

d3d9::SetIndicesFn g_originalSetIndices = nullptr;

SInt32 __stdcall HookedSetIndices(void* self, void* indexData) {
	++g_stateCalls.indexBinds;
	g_stateCalls.indexBindSum += reinterpret_cast<UInt32>(indexData);
	return g_originalSetIndices(self, indexData);
}

SInt32 __stdcall HookedSetVsConstantF(void* self, UInt32 startRegister, const float* data,
                                      UInt32 vector4fCount) {
	++g_stateCalls.constantCalls;
	g_stateCalls.constantVectors += vector4fCount;

	// The bone lock. When it hands back the first render's floats, those
	// are what the device receives and what every counter below sees - so
	// a locked frame's bone range sums must come back equal, which is the
	// lock verifying itself in the same line that convicted the bug.
	//
	// The window is both Bones classes of the active package, each on its
	// own residue: the skin shaders' rows at c42+54 land on registers
	// divisible by three (42, 45 ... 93), the hair shaders' rows at c31+54
	// one above (31, 34 ... 88). The mismatch evidence - identical
	// rotations, translations a body-length apart - is two same-posed NPCs
	// swapped, so the second render confuses palettes between look-alike
	// instances; helmets kept their sideways offset while the hair class
	// went unlocked, which is why it is locked again, now that pairing is
	// by content rather than position.
	// Three Bones classes on three disjoint residues: skin at c42 (rows on
	// registers = 0 mod 3), hair at c31 (= 1 mod 3), and the head-part
	// shaders - eyeballs, teeth, some hair - at c14 (= 2 mod 3). The third
	// class went unlocked at first and wore it as every NPC's eyes, teeth
	// and hair sitting beside the face. The residue ranges are census-clean:
	// the only other three-vector constants in range (LightPosition c16,
	// LightColor c19, SkinToCubeSpace c27) land on the other residues.
	const bool skinBoneRow =
		startRegister >= 42 && startRegister <= 93 && startRegister % 3 == 0;
	const bool hairBoneRow =
		startRegister >= 31 && startRegister <= 88 && startRegister % 3 == 1;
	const bool headPartBoneRow =
		startRegister >= 14 && startRegister <= 65 && startRegister % 3 == 2;
	if (data != nullptr && (skinBoneRow || hairBoneRow || headPartBoneRow) &&
	    vector4fCount == 3) {
		if (g_boneTargetIsMain) {
			const float* replacement = HandleBoneUpload(startRegister, data);
			if (replacement != nullptr) {
				data = replacement;
			}
		} else if (g_boneMode != BonePassMode::Off) {
			// A shadow or reflection sub-pass: the row is camera-free and
			// belongs to this pass as it stands. Counted so the log can say
			// how much of a frame's bone traffic the world never sees.
			++g_stateCalls.boneOffscreenRows;
		}
	}
	if (data != nullptr && vector4fCount > 0) {
		// A palette that was never computed is a palette of zeroes. Sixteen
		// floats are enough to tell one apart, and cheap enough to look at
		// on every upload.
		const UInt32 floats = vector4fCount >= 4 ? 16 : vector4fCount * 4;
		bool allZero = true;
		for (UInt32 i = 0; i < floats; ++i) {
			if (data[i] != 0.0f) {
				allZero = false;
				break;
			}
		}
		if (allZero) {
			++g_stateCalls.zeroUploads;
		}
		// The sixteen floats just examined are exactly c0-c3 when the
		// upload starts at register 0: the ModelViewProj a draw will use.
		if (startRegister == 0 && vector4fCount >= 4) {
			g_lastC0Zero = allZero;
		}
	}
	if (data != nullptr && startRegister >= 40) {
		++g_stateCalls.boneRangeCalls;
		g_stateCalls.boneRangeVectors += vector4fCount;
		const UInt32* bits = reinterpret_cast<const UInt32*>(data);
		for (UInt32 i = 0; i < vector4fCount * 4; ++i) {
			g_stateCalls.boneRangeSum += bits[i];
		}
	}
	if (data != nullptr && vector4fCount >= 12) {
		// Bone palette candidates. A palette is bone-to-model-space and has
		// no camera in it, so the two world renders must upload identical
		// bytes - the sum is order-independent and the deltas subtract
		// cleanly, so if the sums differ, the contents differ.
		++g_stateCalls.paletteCalls;
		g_stateCalls.paletteVectors += vector4fCount;
		const UInt32* bits = reinterpret_cast<const UInt32*>(data);
		UInt32 sum = 0;
		for (UInt32 i = 0; i < vector4fCount * 4; ++i) {
			sum += bits[i];
		}
		g_stateCalls.paletteSum += sum;

		const UInt32 slot =
		    SelectPaletteBucket(g_stateCalls.paletteRegisters, kPaletteBucketCount, startRegister);
		if (slot < kPaletteBucketCount) {
			PaletteRegisterBucket& bucket = g_stateCalls.paletteRegisters[slot];
			bucket.startRegister = startRegister;
			++bucket.calls;
			bucket.vectors += vector4fCount;
			bucket.sum += sum;
		} else {
			++g_stateCalls.paletteOverflow;
		}
	}
	if (vector4fCount > g_stateCalls.largestUpload) {
		g_stateCalls.largestUpload = vector4fCount;
	}
	return g_originalSetVsConstantF(self, startRegister, data, vector4fCount);
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

SInt32 __stdcall HookedVbLock(void* self, UInt32 offset, UInt32 size, void** data,
                              UInt32 flags) {
	++g_stateCalls.vbLocks;
	if ((flags & d3d9::kLockDiscard) != 0) {
		++g_stateCalls.vbDiscardLocks;
		if (!g_dynamicBuffers.Insert(self)) {
			++g_stateCalls.dynamicBufferOverflow;
		}
	}
	const bool dynamic = g_dynamicBuffers.Contains(self);
	if (dynamic) {
		++g_stateCalls.dynamicLocks;
		g_stateCalls.dynamicLockOffsetSum += offset;
		g_stateCalls.dynamicLockSizeSum += size;
		RecordTimeline('L', nullptr, self, offset, size, flags);
	}
	const SInt32 result = g_originalVbLock(self, offset, size, data, flags);
	if (dynamic && result >= 0 && data != nullptr && *data != nullptr) {
		// Size zero means the whole buffer, whose length this hook does not
		// know - skipped rather than guessed at.
		if (size == 0 || !g_openLocks.Begin(self, *data, size)) {
			++g_stateCalls.dynamicWriteSkipped;
		}
	}
	return result;
}

SInt32 __stdcall HookedVbUnlock(void* self) {
	// Read before the unlock goes through: this is the last moment the
	// range is guaranteed to be mapped.
	const void* data = nullptr;
	UInt32 size = 0;
	if (g_openLocks.End(self, &data, &size)) {
		++g_stateCalls.dynamicWrites;
		const UInt32 sum = SumLeadingBytes(data, size, kWriteFingerprintBytes);
		g_stateCalls.dynamicWriteSum += sum;
		RecordTimeline('U', nullptr, self, sum, 0, 0);
		// The peeks land in unlock order, cross-referenced by the same sum
		// the matching 'U' line carries.
		PeekVertices(self, sum, data, size);
	}
	return g_originalVbUnlock(self);
}

// The index-buffer mirrors of the two hooks above, on their own counters
// and their own ledger - indices and vertices fail differently, and one
// shared number would blur exactly the distinction being probed.
SInt32 __stdcall HookedIbLock(void* self, UInt32 offset, UInt32 size, void** data,
                              UInt32 flags) {
	++g_stateCalls.ibLocks;
	if ((flags & d3d9::kLockDiscard) != 0) {
		++g_stateCalls.ibDiscardLocks;
		g_dynamicIndexBuffers.Insert(self);
	}
	const bool dynamic = g_dynamicIndexBuffers.Contains(self);
	const SInt32 result = g_originalIbLock(self, offset, size, data, flags);
	if (dynamic && result >= 0 && data != nullptr && *data != nullptr) {
		if (size == 0 || !g_openIbLocks.Begin(self, *data, size)) {
			++g_stateCalls.ibWriteSkipped;
		}
	}
	return result;
}

SInt32 __stdcall HookedIbUnlock(void* self) {
	const void* data = nullptr;
	UInt32 size = 0;
	if (g_openIbLocks.End(self, &data, &size)) {
		++g_stateCalls.ibWrites;
		g_stateCalls.ibWriteSum += SumLeadingBytes(data, size, kWriteFingerprintBytes);
	}
	return g_originalIbUnlock(self);
}

SInt32 __stdcall HookedCreateIndexBuffer(void* self, UInt32 length, UInt32 usage, UInt32 format,
                                         UInt32 pool, void** indexBuffer, void** sharedHandle) {
	const SInt32 result = g_originalCreateIndexBuffer(self, length, usage, format, pool,
	                                                  indexBuffer, sharedHandle);
	if (result < 0 || indexBuffer == nullptr || *indexBuffer == nullptr) {
		return result;
	}
	if ((usage & d3d9::kUsageDynamic) != 0) {
		OBVR_LOG("Hud: dynamic index buffer %p created - length %u, usage %08X, pool %u",
		         *indexBuffer, length, usage, pool);
	}
	auto** vtable = *reinterpret_cast<void***>(*indexBuffer);
	if (g_ibVtable == nullptr) {
		g_originalIbLock =
			reinterpret_cast<d3d9::VertexBufferLockFn>(vtable[d3d9::kVertexBufferLock]);
		g_originalIbUnlock =
			reinterpret_cast<d3d9::VertexBufferUnlockFn>(vtable[d3d9::kVertexBufferUnlock]);
		if (g_originalIbLock != nullptr && g_originalIbUnlock != nullptr &&
		    WriteTableEntry(vtable, d3d9::kVertexBufferLock,
		                    reinterpret_cast<void*>(&HookedIbLock))) {
			if (WriteTableEntry(vtable, d3d9::kVertexBufferUnlock,
			                    reinterpret_cast<void*>(&HookedIbUnlock))) {
				g_ibVtable = vtable;
				OBVR_LOG("Hud: index buffer Lock and Unlock counted from table "
				         "entries %u and %u",
				         d3d9::kVertexBufferLock, d3d9::kVertexBufferUnlock);
			} else {
				WriteTableEntry(vtable, d3d9::kVertexBufferLock,
				                reinterpret_cast<void*>(g_originalIbLock));
				g_originalIbLock = nullptr;
				g_originalIbUnlock = nullptr;
			}
		} else {
			g_originalIbLock = nullptr;
			g_originalIbUnlock = nullptr;
		}
	} else if (vtable != g_ibVtable && !g_ibSecondVtableReported) {
		g_ibSecondVtableReported = true;
		OBVR_LOG("Hud: a second index buffer vtable appeared - its locks are not counted");
	}
	return result;
}

// Counts Lock on every vertex buffer by patching the class vtable the next
// created buffer reveals. Software-skinned geometry is repacked into vertex
// buffers as it renders - locks the constant counters cannot see - and
// whether that repacking happens once per frame or once per world render is
// exactly the difference between a healthy eye and a collapsed one.
SInt32 __stdcall HookedCreateVertexBuffer(void* self, UInt32 length, UInt32 usage, UInt32 fvf,
                                          UInt32 pool, void** vertexBuffer,
                                          void** sharedHandle) {
	const SInt32 result = g_originalCreateVertexBuffer(self, length, usage, fvf, pool,
	                                                   vertexBuffer, sharedHandle);
	if (result < 0 || vertexBuffer == nullptr || *vertexBuffer == nullptr) {
		return result;
	}
	if ((usage & d3d9::kUsageDynamic) != 0) {
		// The dynamic buffers are the discard-locked pool under the collapse;
		// a handful exist, so each one's creation is worth a line - the
		// parameters decide which upload path the D3D9 layer puts it on.
		OBVR_LOG("Hud: dynamic vertex buffer %p created - length %u, usage %08X, pool %u",
		         *vertexBuffer, length, usage, pool);
	}
	auto** vtable = *reinterpret_cast<void***>(*vertexBuffer);
	if (g_vbVtable == nullptr) {
		g_originalVbLock =
			reinterpret_cast<d3d9::VertexBufferLockFn>(vtable[d3d9::kVertexBufferLock]);
		g_originalVbUnlock =
			reinterpret_cast<d3d9::VertexBufferUnlockFn>(vtable[d3d9::kVertexBufferUnlock]);
		if (g_originalVbLock != nullptr && g_originalVbUnlock != nullptr &&
		    WriteTableEntry(vtable, d3d9::kVertexBufferLock,
		                    reinterpret_cast<void*>(&HookedVbLock))) {
			if (WriteTableEntry(vtable, d3d9::kVertexBufferUnlock,
			                    reinterpret_cast<void*>(&HookedVbUnlock))) {
				g_vbVtable = vtable;
				OBVR_LOG("Hud: vertex buffer Lock and Unlock counted from table "
				         "entries %u and %u",
				         d3d9::kVertexBufferLock, d3d9::kVertexBufferUnlock);
			} else {
				// Half a patch would leave locks opening entries no unlock
				// ever closes - put the lock entry back and stand down.
				WriteTableEntry(vtable, d3d9::kVertexBufferLock,
				                reinterpret_cast<void*>(g_originalVbLock));
				g_originalVbLock = nullptr;
				g_originalVbUnlock = nullptr;
			}
		} else {
			g_originalVbLock = nullptr;
			g_originalVbUnlock = nullptr;
		}
	} else if (vtable != g_vbVtable && !g_vbSecondVtableReported) {
		g_vbSecondVtableReported = true;
		OBVR_LOG("Hud: a second vertex buffer vtable appeared - its locks are not counted");
	}
	return result;
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

	// The viewport correction. Load-bearing for a believed size smaller than
	// the frame - without it the pass draws through the engine's own
	// full-frame viewport and parts from its layout - but a failure to patch
	// only costs that correction, not the redirect, so it reports rather
	// than refuses.
	g_originalSetViewport =
		reinterpret_cast<d3d9::SetViewportFn>(vtable[d3d9::kDeviceSetViewport]);
	if (g_originalSetViewport == nullptr ||
	    !WriteTableEntry(vtable, d3d9::kDeviceSetViewport,
	                     reinterpret_cast<void*>(&HookedSetViewport))) {
		g_originalSetViewport = nullptr;
		OBVR_LOG("Hud: SetViewport could not be replaced - a 2D belief smaller than "
		         "the frame will draw stretched");
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

	// The vertex pipeline counters, on the draw counters' terms: diagnostic,
	// and a failure costs only the count.
	g_originalSetTransform =
		reinterpret_cast<d3d9::SetTransformFn>(vtable[d3d9::kDeviceSetTransform]);
	g_originalSetVertexDecl = reinterpret_cast<d3d9::SetVertexDeclarationFn>(
		vtable[d3d9::kDeviceSetVertexDeclaration]);
	g_originalSetFVF = reinterpret_cast<d3d9::SetFVFFn>(vtable[d3d9::kDeviceSetFVF]);
	g_originalSetVertexShader = reinterpret_cast<d3d9::SetVertexShaderFn>(
		vtable[d3d9::kDeviceSetVertexShader]);
	g_originalSetVsConstantF = reinterpret_cast<d3d9::SetVertexShaderConstantFFn>(
		vtable[d3d9::kDeviceSetVertexShaderConstantF]);
	g_originalSetStreamSource = reinterpret_cast<d3d9::SetStreamSourceFn>(
		vtable[d3d9::kDeviceSetStreamSource]);
	g_originalSetStreamSourceFreq = reinterpret_cast<d3d9::SetStreamSourceFreqFn>(
		vtable[d3d9::kDeviceSetStreamSourceFreq]);
	g_originalSetIndices =
		reinterpret_cast<d3d9::SetIndicesFn>(vtable[d3d9::kDeviceSetIndices]);
	if (g_originalSetTransform != nullptr && g_originalSetVertexDecl != nullptr &&
	    g_originalSetFVF != nullptr && g_originalSetVertexShader != nullptr &&
	    g_originalSetVsConstantF != nullptr && g_originalSetStreamSource != nullptr &&
	    g_originalSetStreamSourceFreq != nullptr && g_originalSetIndices != nullptr) {
		const bool stateHooked =
			WriteTableEntry(vtable, d3d9::kDeviceSetTransform,
		                    reinterpret_cast<void*>(&HookedSetTransform)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetVertexDeclaration,
		                    reinterpret_cast<void*>(&HookedSetVertexDeclaration)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetFVF,
		                    reinterpret_cast<void*>(&HookedSetFVF)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetVertexShader,
		                    reinterpret_cast<void*>(&HookedSetVertexShader)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetVertexShaderConstantF,
		                    reinterpret_cast<void*>(&HookedSetVsConstantF)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetStreamSource,
		                    reinterpret_cast<void*>(&HookedSetStreamSource)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetStreamSourceFreq,
		                    reinterpret_cast<void*>(&HookedSetStreamSourceFreq)) &&
			WriteTableEntry(vtable, d3d9::kDeviceSetIndices,
		                    reinterpret_cast<void*>(&HookedSetIndices));
		if (!stateHooked) {
			OBVR_LOG("Hud: the vertex state counters could not all be installed");
		}
	}

	// The vertex processing axis: how the device was created, and the
	// mid-frame software/hardware switch a MIXED device may throw.
	d3d9::CreationParameters creation{};
	if (d3d9::Method<d3d9::GetCreationParametersFn>(
	        device, d3d9::kDeviceGetCreationParameters)(device, &creation) >= 0) {
		OBVR_LOG("Hud: device behavior flags %08X - %s vertex processing",
		         creation.behaviorFlags,
		         (creation.behaviorFlags & d3d9::kCreateMixedVertexProcessing) != 0
		             ? "MIXED"
		             : (creation.behaviorFlags & d3d9::kCreateSoftwareVertexProcessing) != 0
		                   ? "SOFTWARE"
		                   : "HARDWARE");
	}
	g_originalSetSwvp = reinterpret_cast<d3d9::SetSoftwareVertexProcessingFn>(
		vtable[d3d9::kDeviceSetSoftwareVertexProcessing]);
	if (g_originalSetSwvp == nullptr ||
	    !WriteTableEntry(vtable, d3d9::kDeviceSetSoftwareVertexProcessing,
	                     reinterpret_cast<void*>(&HookedSetSoftwareVertexProcessing))) {
		g_originalSetSwvp = nullptr;
		OBVR_LOG("Hud: the software vertex processing switch could not be counted");
	}

	// The vertex buffer counter reaches its class vtable through the next
	// buffer the game creates. To make that now rather than eventually, a
	// small probe buffer is created and released on the spot - its vtable is
	// every vertex buffer's vtable, the game's existing ones included.
	g_originalCreateVertexBuffer = reinterpret_cast<d3d9::CreateVertexBufferFn>(
		vtable[d3d9::kDeviceCreateVertexBuffer]);
	if (g_originalCreateVertexBuffer != nullptr &&
	    WriteTableEntry(vtable, d3d9::kDeviceCreateVertexBuffer,
	                    reinterpret_cast<void*>(&HookedCreateVertexBuffer))) {
		void* probe = nullptr;
		if (HookedCreateVertexBuffer(device, 64, 0, 0, d3d9::kPoolDefault, &probe,
		                             nullptr) >= 0 &&
		    probe != nullptr) {
			d3d9::Method<d3d9::ReleaseFn>(probe, d3d9::kUnknownRelease)(probe);
		}
	}

	// The index-buffer class, activated the same way: one probe buffer, its
	// vtable patched, every index buffer counted from then on.
	g_originalCreateIndexBuffer = reinterpret_cast<d3d9::CreateIndexBufferFn>(
		vtable[d3d9::kDeviceCreateIndexBuffer]);
	if (g_originalCreateIndexBuffer != nullptr &&
	    WriteTableEntry(vtable, d3d9::kDeviceCreateIndexBuffer,
	                    reinterpret_cast<void*>(&HookedCreateIndexBuffer))) {
		void* probe = nullptr;
		if (HookedCreateIndexBuffer(device, 64, 0, d3d9::kFormatIndex16, d3d9::kPoolDefault,
		                            &probe, nullptr) >= 0 &&
		    probe != nullptr) {
			d3d9::Method<d3d9::ReleaseFn>(probe, d3d9::kUnknownRelease)(probe);
		}
	}


	OBVR_LOG("Hud: SetRenderTarget and SetRenderState hooked at table entries %u and %u - "
	         "the 2D pass can be pointed elsewhere, with its alpha kept",
	         d3d9::kDeviceSetRenderTarget, d3d9::kDeviceSetRenderState);
	return true;
}

// Runs the game's own 2D pass with the interface-pass window flagged for the
// viewport hook - redirected or vanilla, main menu included, because the
// engine sets its full-frame viewport inside the pass either way. Only a
// frame-layer pass counts: a menu-to-texture pass (renderedTexture not null)
// draws into a texture of its own size, and its viewports are its own.
void RunOriginalPassWindowed(void* self, void* unusedEdx, void* renderedTexture) {
	g_inInterfacePass = renderedTexture == nullptr;
	if (g_inInterfacePass) {
		// A fresh frame-layer pass closes any leftover after-pass window, so
		// the samples name only draws between this pass and the next - and
		// every 120th pass arms the tail ring for its own draws.
		g_afterPassSamples = 0;
		if (GetConfig().cursorProbe && ++g_afterPassCount % 120 == 0) {
			g_tailArmed = true;
			g_tailNext = 0;
			g_tailSeen = 0;
		}
	}
	g_original(self, unusedEdx, renderedTexture);
	// Every frame-layer pass leaves one draw behind: the engine paints the
	// cursor quad after this call returns, through a full-frame viewport it
	// sets only then, into whatever colour target stands - the back buffer
	// the cinema shows on the main menu and after a redirect's restore
	// alike. The one-draw window hands that viewport the same correction as
	// everything inside the pass; see g_afterRedirectWindow for its bounds.
	if (g_inInterfacePass) {
		g_afterRedirectWindow = true;
	}
	if (g_inInterfacePass && g_tailArmed) {
		g_tailArmed = false;
		const UInt32 held = g_tailSeen < 4 ? g_tailSeen : 4;
		for (UInt32 i = 0; i < held; ++i) {
			// Oldest first: the ring's next slot is the oldest entry once it
			// has wrapped.
			const ProbedDraw& drawn =
			    g_tailRing[(g_tailNext + 4 - held + i) % 4];
			OBVR_LOG("Cursor draw probe: pass tail %u/%u %s type=%u count=%u "
			         "viewport=%ux%u at %u,%u world=(%.1f, %.1f, %.1f)",
			         i + 1, held, drawn.kind, drawn.type, drawn.count,
			         drawn.viewport.width, drawn.viewport.height, drawn.viewport.x,
			         drawn.viewport.y, static_cast<double>(drawn.worldX),
			         static_cast<double>(drawn.worldY), static_cast<double>(drawn.worldZ));
		}
		g_afterPassSamples = 4;
	}
	g_inInterfacePass = false;
}

// Runs one interface pass, choosing the route, and names the route taken
// for the invocation window's log line. watching: count draws and arm the
// first-draw sample even when nothing redirects, so the window sees vanilla
// passes exactly as it sees redirected ones.
const char* RunInterfacePass(void* self, void* unusedEdx, void* renderedTexture,
                             bool watching) {
	// The hook first, callbacks second - both orders matter. The target hook
	// has to exist before the callbacks change any state, or a pass that
	// could not be redirected would still have its blend states rearranged
	// and its capture claimed. And the hook must not WAIT for the callbacks:
	// the viewport correction and the probes live in these device hooks, and
	// a run that never arms the layer - a cold start straight into the main
	// menu with the headset still asleep, or a headless measuring run -
	// needs them all the same. EnsureTargetHook is idempotent and defers
	// itself while the device is missing.
	if (!EnsureTargetHook() || g_callbacks.beginRedirect == nullptr) {
		g_observing = watching;
		RunOriginalPassWindowed(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "unhooked";
	}

	// A pass aimed at a texture of the game's own - menu-to-texture, not
	// the frame's 2D layer - is the game's business: redirecting it would
	// steal a picture some later draw reads back. Watched, never redirected.
	if (renderedTexture != nullptr) {
		g_observing = watching;
		RunOriginalPassWindowed(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "texture pass";
	}

	// Already captured between the world renders. The game's own pass runs,
	// watched, but nothing of OBVR's is aimed at: redirecting it would hand
	// the texture to a pass that draws nothing and clears on its way in.
	if (g_hudCapturedThisFrame) {
		g_hudCapturedThisFrame = false;
		g_observing = watching;
		RunOriginalPassWindowed(self, unusedEdx, renderedTexture);
		g_observing = false;
		return "already captured";
	}

	void* substitute = g_callbacks.beginRedirect();
	if (substitute == nullptr) {
		g_observing = watching;
		RunOriginalPassWindowed(self, unusedEdx, renderedTexture);
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
			RunOriginalPassWindowed(self, unusedEdx, renderedTexture);
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

	RunOriginalPassWindowed(self, unusedEdx, renderedTexture);

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

	const UInt32 menuCount = *reinterpret_cast<const UInt16*>(addr::kTileMenuArrayCount);
	const UInt32 menuRoot = *reinterpret_cast<const UInt32*>(addr::kTileMenuArrayData);
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

	// The place experiment. Everything measured so far says the world render
	// OBVR calls itself comes back empty because of WHERE it is called from,
	// not because of the menu: the identical call on an ordinary world frame,
	// made from Present, draws nothing either, while the engine's own render
	// of the same scene makes 399 draws with 3216 setup calls against the
	// probe's 343 and none.
	//
	// This is the other place worth trying, and the only other one that is
	// any use: the 2D pass runs inside the engine's own BeginScene/EndScene -
	// which is why no bracket is opened here, unlike in Present - and it goes
	// on running while a pause menu is up, on the very frames a live
	// background would have to be drawn on. If the render draws from here,
	// the feature is a matter of moving the call; if it comes back empty here
	// too, the moment is not what decides it and the search moves on with one
	// more suspect gone.
	// The budget is spent on attempts that actually ran, not on attempts that
	// were refused. The main menu draws its 2D long before any world render
	// has been seen, so there is no renderer instance to call and the probe
	// declines - and a budget decremented there is a budget entirely used up
	// before the game is even loaded, which is exactly how the first run of
	// this measured nothing.
	// Two budgets, because the two answers are different questions. The world
	// one settled that the moment decides it: from here the render draws,
	// where the identical call from Present draws nothing. The menu one is
	// the question the feature actually turns on - a pause menu is the case
	// where the engine has stopped rendering entirely, and whether this place
	// still works THERE is not something the world answer implies.
	if (renderedTexture == nullptr && GetConfig().menuWorldProbe) {
		const bool menuIsUp = game::IsMenuMode();

		// Refilled on every menu that opens, because the first menu of a run
		// is never the one worth measuring. IsMenuMode is true for the main
		// menu and for the load that follows it, so a budget spent once is
		// spent there - on a half-built world, at scene call 1, with the
		// engine still rendering of its own accord. The pause menu, where the
		// engine has stopped entirely, comes minutes later and had no budget
		// left to be measured with.
		if (menuIsUp != g_placeProbeMenuWasUp) {
			g_placeProbeMenuWasUp = menuIsUp;
			if (menuIsUp) {
				g_placeProbesMenuLeft = 3;
			}
		}

		UInt32& budget = menuIsUp ? g_placeProbesMenuLeft : g_placeProbesLeft;
		if (budget > 0) {
			UInt32 draws = 0;
			UInt32 setup = 0;
			// The budget is spent on attempts that actually ran, not on ones
			// refused. The main menu draws its 2D long before any world render
			// has been seen, so there is no renderer instance to call and the
			// probe declines - and a budget decremented there is used up
			// before the game is even loaded, which is how the first run of
			// this measured nothing at all.
			if (RunMenuWorldProbe(draws, setup)) {
				--budget;
				OBVR_LOG("Place probe: from inside the 2D pass on a %s frame a self-initiated "
				         "world render ran and made %u draw call(s) with %u vertex setup "
				         "call(s) - %s (invocation %u, scene call %u)",
				         menuIsUp ? "MENU" : "world", draws, setup,
				         draws > 0 ? "IT DRAWS" : "empty", invocation, CurrentSceneCall());
			}
		}
	}
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

}  // namespace

bool RunHudPassBetweenScenes() {
	if (g_original == nullptr || g_lastSelf == nullptr || g_hudCapturedThisFrame) {
		return false;
	}

	// The same call the game makes at 0057929E: the interface manager in
	// ecx, a null texture argument for the frame's own 2D layer. Run through
	// the same redirect path as a hooked pass, so the layer lands in OBVR's
	// texture rather than in the back buffer the eyes were just copied from.
	ResetPassStats();
	const char* mode = RunInterfacePass(g_lastSelf, nullptr, nullptr, true);
	const UInt32 drew = g_statsDraws;

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

bool GetViewportDirect(void* viewportOut) {
	void* device = GetGameDevice();
	if (device == nullptr || viewportOut == nullptr) {
		return false;
	}
	auto getViewport = d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport);
	return getViewport != nullptr &&
	       getViewport(device, static_cast<d3d9::Viewport*>(viewportOut)) >= 0;
}

bool SetViewportDirect(const void* viewport) {
	void* device = GetGameDevice();
	if (device == nullptr || viewport == nullptr || g_originalSetViewport == nullptr) {
		return false;
	}
	return g_originalSetViewport(device, static_cast<const d3d9::Viewport*>(viewport)) >= 0;
}

void ArmBetweenTrace() {
	g_betweenTraceLeft = 12;
	// The per-pass trace answers the menu question the between trace cannot:
	// on a held frame the between pass never runs, and whether the game's own
	// 2D pass drew anything - and at which targets it aimed - is exactly what
	// "the menu is on the monitor and not in the headset" needs to know. Its
	// startup budget is long spent by the time anyone opens an inventory, so
	// a menu opening or closing rearms it.
	g_passTraceLeft = 12;
}

UInt32 TotalDrawCount() { return g_drawsTotal; }

StateCallCounts TotalStateCalls() { return g_stateCalls; }

void ArmPoolTimeline() {
	if (g_timelineDumped) {
		return;
	}
	g_timelineArmed = true;
	g_timelineCount = 0;
	g_vertexPeekCount = 0;
	g_timelinePhase = 0;
	g_boneMismatchCount = 0;
}

void MarkPoolTimeline(const char* label) {
	if (std::strcmp(label, "between the passes") == 0) {
		g_timelinePhase = 1;
	} else if (std::strcmp(label, "second pass begins") == 0) {
		g_timelinePhase = 2;
	}
	RecordTimeline('M', label, nullptr, 0, 0, 0);
}

void DumpPoolTimeline() {
	if (!g_timelineArmed) {
		return;
	}
	g_timelineArmed = false;
	g_timelineDumped = true;
	OBVR_LOG("Pool timeline: %u events, %u dynamic buffers known%s", g_timelineCount,
	         g_dynamicBuffers.count,
	         g_timelineCount >= kTimelineCapacity ? " (capacity reached, tail lost)" : "");
	for (UInt32 i = 0; i < g_timelineCount; ++i) {
		const TimelineEvent& e = g_timeline[i];
		switch (e.kind) {
			case 'M':
				OBVR_LOG("T ---- %s", e.label);
				break;
			case 'L':
				OBVR_LOG("T lock   %p o=%u s=%u f=%04X", e.buffer, e.a, e.b, e.c);
				break;
			case 'U':
				OBVR_LOG("T unlock %p sum=%08X", e.buffer, e.a);
				break;
			case 'D':
				OBVR_LOG("T draw   %p b=%u v=%u p=%u", e.buffer, e.a, e.b, e.c);
				break;
			default:
				break;
		}
	}
	for (UInt32 i = 0; i < g_vertexPeekCount; ++i) {
		const VertexPeek& p = g_vertexPeeks[i];
		const float* f = p.floats;
		OBVR_LOG("V %p sum=%08X | %g %g %g | %g %g %g | %g %g %g %g %g %g | %g %g %g "
		         "%g %g %g",
		         p.buffer, p.offset, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7],
		         f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15], f[16], f[17]);
	}
	OBVR_LOG("Bone compare: first render %u uploads, second compared %u, mismatches %u",
	         g_boneLogCount, g_boneCompareIndex, g_boneMismatchCount);
	if (g_boneMismatchCount > 0) {
		const float* a = g_boneMismatchFirst.floats;
		const float* b = g_boneMismatchSecond.floats;
		OBVR_LOG("Bone mismatch at upload %u: first c%u | %g %g %g %g | %g %g %g %g | "
		         "%g %g %g %g",
		         g_boneMismatchAt, g_boneMismatchFirst.startRegister, a[0], a[1], a[2],
		         a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]);
		OBVR_LOG("Bone mismatch at upload %u: second c%u | %g %g %g %g | %g %g %g %g | "
		         "%g %g %g %g",
		         g_boneMismatchAt, g_boneMismatchSecond.startRegister, b[0], b[1], b[2],
		         b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11]);
	}
}

bool PoolTimelineWasDumped() { return g_timelineDumped; }

void SetBonePassMode(BonePassMode mode) {
	const BonePassMode previous = g_boneMode;
	g_boneMode = mode;
	if (mode == BonePassMode::Capture || mode == BonePassMode::Replace) {
		// Both passes start on the world target; the SetRenderTarget hook
		// takes it from here. The world's width is measured once, from the
		// back buffer the HUD hook already holds - before either exists,
		// the width stays zero and every target counts as the world's.
		g_boneTargetIsMain = true;
		if (g_boneMainWidth == 0 && g_backBuffer != nullptr) {
			d3d9::SurfaceDesc desc{};
			auto getDesc =
				d3d9::Method<d3d9::GetDescFn>(g_backBuffer, d3d9::kSurfaceGetDesc);
			if (getDesc != nullptr && getDesc(g_backBuffer, &desc) >= 0 &&
			    desc.width > 0) {
				g_boneMainWidth = desc.width;
				OBVR_LOG("Bone lock: world target width %u - bone rows heading "
				         "for smaller targets (shadows, reflections) pass through",
				         g_boneMainWidth);
			}
		}
	}
	if (mode == BonePassMode::Capture) {
		g_boneLogCount = 0;
	} else if (mode == BonePassMode::Replace) {
		g_boneCompareIndex = 0;
		BeginEyeDeltaFrame(g_eyeDelta);
	}
	// A replace pass just finished: its measurement is complete, so judge
	// the palette convention's sign against the camera shift. Re-judged
	// every frame - a wrong one-off would heal - but reported once.
	if (previous == BonePassMode::Replace && mode == BonePassMode::Off) {
		const ShiftSign sign = CalibrateShiftSign(g_eyeDelta, g_boneEyeShift, 8);
		if (sign != ShiftSign::Unknown) {
			g_boneShiftSign = sign;
			if (!g_boneShiftSignReported) {
				g_boneShiftSignReported = true;
				float mean[3];
				CurrentEyeDelta(g_eyeDelta, mean);
				OBVR_LOG("Bone lock: shift sign calibrated %s - camera shift "
				         "%g %g %g, measured baseline %g %g %g from %u rows",
				         sign == ShiftSign::Positive ? "positive" : "negative",
				         g_boneEyeShift[0], g_boneEyeShift[1], g_boneEyeShift[2],
				         mean[0], mean[1], mean[2], g_eyeDelta.samples);
			}
		}
	}
}

void SetBoneEyeShift(float x, float y, float z) {
	g_boneEyeShift[0] = x;
	g_boneEyeShift[1] = y;
	g_boneEyeShift[2] = z;
}

void GetBoneEyeDelta(float out[3], UInt32& samples) {
	CurrentEyeDelta(g_eyeDelta, out);
	samples = g_eyeDelta.samples;
}

void GetBoneShiftState(float shift[3], int& sign) {
	shift[0] = g_boneEyeShift[0];
	shift[1] = g_boneEyeShift[1];
	shift[2] = g_boneEyeShift[2];
	sign = g_boneShiftSign == ShiftSign::Positive
	           ? 1
	           : (g_boneShiftSign == ShiftSign::Negative ? -1 : 0);
}

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
