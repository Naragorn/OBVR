#pragma once

#include "core/Types.h"
#include "render/PaletteBuckets.h"

namespace obvr::render {

// Redirects the pass that draws Oblivion's 2D layer into a surface of OBVR's
// own - addr::kRenderInterface, the one function every route to the HUD,
// menus, dialogues and loading screens funnels through.
//
// Two hooks working as one, because of how the pass behaves: it begins the
// default render target group *from inside itself*, so a wrapper that set a
// target first would be overridden immediately. The entry detour therefore
// only raises a flag and asks the callbacks for a surface; the actual
// substitution happens in the device's SetRenderTarget - the one method
// every Gamebryo target wrapper ends at, which is an API fact rather than a
// game one. While the flag is up, any target the pass sets for index 0 is
// replaced with the callback's surface; the last target it asked for is
// remembered and put back when the pass returns, so the device ends the call
// in the state the game believes it is in.
//
// The depth stencil surface is never touched: the pass clears depth and
// stencil on entry exactly as vanilla does, into a depth buffer the finished
// world no longer needs.

struct InterfaceRedirect {
	// Asked at the entry of every 2D pass. Return the surface the layer
	// should be drawn into, or null to leave this pass alone - null is the
	// answer for menu frames, for frames with no compositor, and for the
	// feature being off.
	void* (*beginRedirect)() = nullptr;

	// Called after a redirected pass returned, targets already restored.
	void (*endRedirect)() = nullptr;

	// Whether the diagnostic probe is on this frame. Optional - null means
	// never. While it answers true, the redirected pass starts from a
	// recognisable half-transparent clear issued through the render target
	// binding instead of an invisible one, and the first draw of a traced
	// pass logs the pipeline state it actually ran with.
	bool (*probeActive)() = nullptr;
};

// Verifies the entry bytes and patches the entry. The SetRenderTarget table
// entry is replaced lazily, on the first pass that actually redirects,
// because the device does not exist when this is installed. Install once.
bool InstallInterfaceRenderHook(const InterfaceRedirect& callbacks);

bool IsInterfaceRenderHooked();

// Draws since the process started, all of them - the world renders included.
// Read either side of a call to subtract: the difference is what that call
// drew. Wraps at 2^32 like any counter, and a subtraction of two readings
// survives the wrap.
UInt32 TotalDrawCount();

// The vertex pipeline setters since the process started, on the same terms:
// read either side of a call, subtract, and the difference is what that call
// set up. All six wrap at 2^32 and the subtraction survives it.
//
// Split out per method because they fail differently. Skinned bodies get
// their bone matrices through SetVertexShaderConstantF, so a render whose
// constant uploads sit far below its twin's while the draw counts match is
// drawing skinned geometry against stale registers. The other four say
// whether such a gap is the constants alone or the whole vertex setup.
struct StateCallCounts {
	UInt32 transforms = 0;
	UInt32 declarations = 0;
	UInt32 fvfs = 0;
	UInt32 vertexShaders = 0;
	UInt32 constantCalls = 0;
	UInt32 constantVectors = 0;
	UInt32 vbLocks = 0;         // vertex buffer Lock calls
	UInt32 vbDiscardLocks = 0;  // the subset that passed D3DLOCK_DISCARD
	UInt32 zeroUploads = 0;     // constant uploads whose leading floats are all zero
	UInt32 paletteCalls = 0;    // uploads of 12 vectors or more - bone palette candidates
	UInt32 paletteVectors = 0;  // float4 registers across those uploads
	UInt32 paletteSum = 0;      // their float bits summed - camera-independent data must match
	UInt32 largestUpload = 0;   // the biggest single upload seen (not a counter; no delta)

	// The same palette candidates, split by start register - see
	// PaletteBuckets.h for why the split is what makes the sums readable.
	PaletteRegisterBucket paletteRegisters[kPaletteBucketCount];
	UInt32 paletteOverflow = 0;  // palette uploads whose register found no bucket

	// Binding identities, not counts. The registers came back clean, so the
	// question moved to what each render binds: pointer sums are order-
	// independent fingerprints, and none of these have a camera in them -
	// two renders of the same scene must produce the same three sums.
	UInt32 declarationSum = 0;   // vertex declaration pointers, summed
	UInt32 vertexShaderSum = 0;  // vertex shader pointers, summed
	UInt32 streamSources = 0;    // SetStreamSource calls
	UInt32 streamSourceSum = 0;  // stream vertex buffer pointers, summed

	// The draws wired to a discard-locked buffer - the dynamic pool the
	// software-skinned bodies are packed into - and the writes into that
	// pool. Together they say, per render, whether the skinned draws read
	// the regions this render wrote, the regions the other render wrote,
	// or regions nobody wrote at all.
	UInt32 skinnedDraws = 0;      // draws whose stream 0 is a discard-locked buffer
	UInt32 skinnedVertexSum = 0;  // their NumVertices summed - camera-free, must match
	UInt32 skinnedPrimSum = 0;    // their primitive counts summed - must match
	UInt32 skinnedOffsetSum = 0;  // stream-0 byte offsets at those draws
	UInt32 dynamicLocks = 0;      // locks landing on those buffers
	UInt32 dynamicLockOffsetSum = 0;  // their byte offsets summed
	UInt32 dynamicLockSizeSum = 0;    // their byte sizes summed
	UInt32 dynamicBufferOverflow = 0;  // discard-locked buffers the set could not hold

	// The bytes those locks actually wrote, fingerprinted at unlock time -
	// the first moment the written data is complete and the last it is
	// still mapped. Camera-free like everything else about skinning: two
	// renders packing the same bodies must write the same leading bytes.
	UInt32 dynamicWrites = 0;       // pool unlocks whose bytes were fingerprinted
	UInt32 dynamicWriteSum = 0;     // their leading bytes summed
	UInt32 dynamicWriteSkipped = 0;  // pool locks whose bytes could not be followed

	// Every sum above is order-independent, which is exactly the blind
	// spot: two renders can upload the same bytes and still interleave
	// them differently against the draws. This one is order-sensitive -
	// a skinned draw issued while register c0 (ModelViewProj in every
	// shader of the active package) holds an all-zero matrix is a draw
	// that collapses onto one clip-space point.
	UInt32 skinnedZeroMatrixDraws = 0;

	// One frame's pool timeline, recorded on demand. Every aggregate above
	// is blind to order and to which buffer an event hit; the timeline is
	// the raw sequence - each lock with its flags, each unlock, each
	// skinned draw with its source - for exactly one dual frame, so a
	// standalone reproduction can replay the game's true access pattern.
	// (Counters continue below; the timeline itself lives in the hooks.)

	// The index side of vertex fetch, mirrored from the vertex side once
	// that side ran out of suspects. Indices decide which vertex each
	// triangle corner reads: zeroed indices build every triangle out of
	// vertex zero, and a stride of zero reads one vertex forever - both
	// are bodies collapsed onto a point.
	UInt32 streamStrideSum = 0;   // stream-0 strides summed
	UInt32 streamFreqCalls = 0;   // SetStreamSourceFreq calls - instancing changes fetch
	UInt32 indexBinds = 0;        // SetIndices calls
	UInt32 indexBindSum = 0;      // index buffer pointers, summed
	UInt32 ibLocks = 0;           // index buffer Lock calls
	UInt32 ibDiscardLocks = 0;    // the subset that passed DISCARD
	UInt32 ibWrites = 0;          // fingerprinted index writes
	UInt32 ibWriteSum = 0;        // their leading bytes summed
	UInt32 ibWriteSkipped = 0;    // index writes that could not be followed
	UInt32 skinnedZeroStrideDraws = 0;  // skinned draws bound with stream-0 stride zero

	// The vertex processing mode, the one axis every counter above was
	// blind to. A MIXED device may flip SetSoftwareVertexProcessing per
	// mesh, and a software-processed draw takes a different path through
	// the D3D9 implementation than a hardware one.
	UInt32 swvpToggles = 0;      // SetSoftwareVertexProcessing calls
	UInt32 swvpOnDraws = 0;      // skinned draws issued while software processing is on

	// The draw addressing itself - the one pair of parameters never
	// compared. BaseVertexIndex and StartIndex decide which part of a
	// correct buffer a draw reads; a second render issuing the same draw
	// with the wrong base reads the wrong vertices out of the right
	// buffer, and every sum above stays equal while it happens.
	UInt32 skinnedBaseVertexSum = 0;  // BaseVertexIndex of skinned draws, summed
	UInt32 skinnedStartIndexSum = 0;  // StartIndex of skinned draws, summed
	UInt32 skinnedMinVertexSum = 0;   // MinVertexIndex of skinned draws, summed

	// Uploads into the bone register range (c40 and up, where the active
	// package's shaders keep their Bones arrays), at ANY size. Every
	// palette probe so far filtered at twelve vectors and would have been
	// blind to an engine that uploads bones three vectors at a time - the
	// one hardware-skinning shape never yet measured.
	UInt32 boneRangeCalls = 0;
	UInt32 boneRangeVectors = 0;
	UInt32 boneRangeSum = 0;  // float bits summed - bone matrices carry no camera

	// The bone lock at work: how many second-render bone uploads were
	// replaced with the first render's values, and how many arrived after
	// a sequence mismatch stopped the replacing for the rest of the frame.
	UInt32 boneLockReplaced = 0;
	UInt32 boneLockPassthrough = 0;
};

StateCallCounts TotalStateCalls();

// The one-shot pool timeline. Arm it and every lock, unlock and skinned
// draw on the dynamic pool is recorded in order until Dump writes the
// sequence to the log and retires the recorder for the rest of the run.
// One frame is the intended span: the scene hook arms it at the top of a
// dual frame whose predecessor was busy with skinned bodies, drops
// markers at the pass boundaries, and dumps at the frame's end - the raw
// access pattern a standalone reproduction has to replay.
void ArmPoolTimeline();
void MarkPoolTimeline(const char* label);
void DumpPoolTimeline();
bool PoolTimelineWasDumped();

// The bone lock. Oblivion's second world render of a frame re-evaluates
// its skeletons and uploads partly different bone palettes - and they are
// wrong: palettes are world-space (the probes showed the first uploads
// bit-identical between the renders, not eye-offset), so both renders
// must upload the same ones, and the collapsed bodies are the draws that
// got the re-evaluated ones. BioShock VR hit the identical failure on its
// sequential stereo and cured it the same way: keep the first render's
// bone transforms and reapply them on the second. The scene hook drives
// the mode per frame: capture during the first render, replace during the
// second, off in between and outside dual frames.
enum class BonePassMode { Off, Capture, Replace };
void SetBonePassMode(BonePassMode mode);

// How many times the 2D pass was entered since this was last asked, and how
// many primitives it drew in those passes; both zero afterwards. The scene
// render hook asks once per world render, so the two numbers land in the
// same line as the probe rung that frame ran under - which is what turns
// three runs of the game into one.
//
// Two numbers rather than one because they fail differently. Zero passes
// means Oblivion never entered the pass, and the cause is one of the three
// gates in the wrapper at 00579260. One pass with zero draws means it was
// entered and left without drawing, which is where the dual pass puts it.
void TakeInterfaceStats(UInt32& passes, UInt32& draws);

// Runs the 2D pass now, redirected, and says whether anything was captured.
//
// For the moment between the two world renders of a dual frame, which the
// probe sweep showed is the only moment in such a frame when the pass draws
// at all: with the world rendered once it draws its 21 primitives, with it
// rendered twice the same pass walks past every open gate and draws and
// clears nothing, and cutting the second render back off restores it inside
// the same run. So the layer is taken where it exists rather than waited for
// where it does not.
//
// It also happens to be the right moment for the overlay: the eye pictures
// are captured just before this, so a layer drawn here is OBVR's alone and
// never lands in the world picture.
//
// True means the game's own later pass will be left alone rather than
// redirected, so it cannot clear what this one filled. False means nothing
// was captured and that pass keeps the layer, exactly as before.
bool RunHudPassBetweenScenes();

// Reopens the between-render trace for a few more runs.
//
// The standing budget is spent within the first second of play, which is
// exactly the wrong second: the question that needs it is what this pass draws
// when a menu opens, and nobody reaches an inventory that fast. So the moment a
// menu opens or closes, the window is opened again, and the log gets the draw
// counts from the frames that matter instead of from the frames that happened
// to be first.
void ArmBetweenTrace();

}  // namespace obvr::render
