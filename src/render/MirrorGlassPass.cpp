#include "render/MirrorGlassPass.h"

#include <cstdint>
#include <limits>

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/MenuMode.h"
#include "game/MirrorHead.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/SceneFrame.h"
#include "perf/Profiler.h"
#include "platform/Win32Min.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/HeadsetRenderer.h"
#include "render/InterfaceRenderHook.h"
#include "render/SceneRenderHook.h"
#include "render/ShadowShareHook.h"
#include "render/WaterReprojection.h"
#include "vr/MirrorDepthLogic.h"
#include "vr/MirrorGlassLogic.h"
#include "vr/MirrorGlassMap.h"
#include "vr/MirrorSteadyLogic.h"

namespace obvr::render {
namespace {

// D3DERR_DEVICELOST, D3DERR_DEVICEREMOVED and D3DERR_DEVICEHUNG (d3d9.h): a
// copy refused with one of these switches the pass off for the session.
bool DeviceGone(SInt32 result) {
	return result == static_cast<SInt32>(0x88760868u) || result == static_cast<SInt32>(0x88760870u) ||
	       result == static_cast<SInt32>(0x88760874u);
}

// The mirror's own texture on the game's device: a render target, the back
// buffer's format, MirrorPixels a side. Default pool, as the eye copies
// (EyeMirror): OBVR hooks no device reset, so a texture of a device that is no
// longer the game's is let go and made again on the new one.
struct MirrorTarget {
	void* device = nullptr;
	void* texture = nullptr;
	void* surface = nullptr;
	UInt32 side = 0;
	UInt32 format = 0;
	// The texture black outside this picture's size (the laid glass reads
	// black past its picture, vr/MirrorGlassMap.h): made black whole, then
	// blackened outside each new size.
	UInt32 blackOutsideWidth = 0;
	UInt32 blackOutsideHeight = 0;
};

struct MirrorState {
	// The textures: [0] the one picture for both eyes (MirrorDepth=0, stage 3)
	// or the left eye's; [1] the right eye's (stage 4 only). What each holds,
	// and the camera it was rendered with, in `pictures`.
	MirrorTarget targets[2];
	vr::MirrorEyePictures pictures;
	vr::MirrorFailures failures;
	vr::MirrorCostWindow cost;
	vr::MirrorTrace trace;
	// The steadied glass (vr/MirrorSteadyLogic.h) and the clock it steps on:
	// the real one, the game's stands still in a paused menu.
	vr::MirrorSteadyState steady;
	long long steadyTicks = 0;
	// Stage 4: the budget that makes the eyes take turns, the depth setting as
	// last seen, and the shadow share's mirror count at the last frame.
	vr::MirrorBudget budget;
	bool depth = false;
	bool depthKnown = false;
	unsigned shadowsKeptMark = 0;
	bool handedIn = false;    // the glass shows one of the targets' textures
	bool wantsBones = false;  // for the next single-pass render (MirrorWantsBones)
	bool onSaid = false;
};

MirrorState& State() {
	static MirrorState s;
	return s;
}

// The glass dark again (stage 1's), before anything it showed is let go; the
// pictures forgotten, so nothing old comes back on it.
void DarkGlass(MirrorState& s, HeadsetRenderer& renderer) {
	vr::ForgetMirrorEyes(s.pictures);
	if (s.handedIn) {
		renderer.SetMirrorGlass(nullptr, nullptr, MirrorGlassUv{});
		s.handedIn = false;
	}
}

// One texture let go; the glass first goes dark if it could show it.
void ReleaseTarget(MirrorState& s, HeadsetRenderer& renderer, int eye) {
	MirrorTarget& t = s.targets[eye];
	if (t.texture != nullptr) {
		DarkGlass(s, renderer);
	}
	d3d11::Release(t.surface);
	d3d11::Release(t.texture);
	t = MirrorTarget{};
}

// Off for the session: what was made let go, the glass dark, one line.
void SwitchOff(MirrorState& s, HeadsetRenderer& renderer, const char* why) {
	ReleaseTarget(s, renderer, 0);
	ReleaseTarget(s, renderer, 1);
	OBVR_LOG("Mirror: switched off for this session - %s; the glass stays dark", why);
}

MirrorGlassUv UvOf(const MirrorState& s, int eye) {
	MirrorGlassUv uv;
	for (int i = 0; i < 4; ++i) {
		uv.rect[i] = s.pictures.rect[eye][i];
	}
	uv.view = s.pictures.view[eye];
	return uv;
}

// The pictures there handed to the glass (vr::MirrorGlassSources): MirrorDepth=0
// the one picture for both eyes, as stage 3 handed it; with depth each eye its
// own, laid through the camera it was rendered with - a stale one too - or the
// other eye's while its own is not made yet; none, the dark glass.
void HandInGlass(MirrorState& s, HeadsetRenderer& renderer, bool depth) {
	const bool have[2] = {s.pictures.have[0] && s.targets[0].texture != nullptr,
	                      s.pictures.have[1] && s.targets[1].texture != nullptr};
	int source[2] = {-1, -1};
	vr::MirrorGlassSources(depth, have, source);
	if (source[0] < 0 || source[1] < 0) {
		DarkGlass(s, renderer);
		return;
	}
	if (!depth) {
		renderer.SetMirrorGlass(s.targets[0].texture, nullptr, UvOf(s, 0));
	} else {
		renderer.SetMirrorGlass(s.targets[source[0]].texture, s.targets[source[1]].texture, UvOf(s, source[0]),
		                        UvOf(s, source[1]));
	}
	s.handedIn = true;
}

// MirrorDepth turned off: the right eye's texture let go without a dark frame
// - the left's picture handed to both eyes first, until this frame's one pass.
void DropRightEye(MirrorState& s, HeadsetRenderer& renderer) {
	s.pictures.have[1] = false;
	if (s.handedIn) {
		HandInGlass(s, renderer, false);
	}
	MirrorTarget& t = s.targets[1];
	d3d11::Release(t.surface);
	d3d11::Release(t.texture);
	t = MirrorTarget{};
}

// The depth setting as this frame has it, against the last frame's: on a
// change the budget starts over, the trace logs the next passes, both eyes'
// pictures count as old, and turned off the right eye's texture goes. Read
// every frame (two compares), so a change while the page is down is taken too.
void NoteDepthSetting(MirrorState& s, HeadsetRenderer& renderer, bool depth) {
	if (s.depthKnown && s.depth == depth) {
		return;
	}
	const bool changed = s.depthKnown;
	s.depthKnown = true;
	s.depth = depth;
	s.budget = vr::MirrorBudget{};
	if (!changed) {
		return;
	}
	s.trace = vr::MirrorTrace{};
	s.pictures.age[0] = s.pictures.age[1] = vr::kMirrorNoPicture;
	if (!depth) {
		DropRightEye(s, renderer);
	}
	OBVR_LOG("Mirror: depth turned %s - %s", depth ? "on" : "off",
	         depth ? "each eye its own picture from the next pass, the right eye's texture made at its first"
	               : "one picture for both eyes again (stage 3), the right eye's texture let go");
}

bool ReadBackBuffer(void* device, d3d9::SurfaceDesc& out) {
	auto getBackBuffer = d3d9::Method<d3d9::GetBackBufferFn>(device, d3d9::kDeviceGetBackBuffer);
	void* surface = nullptr;
	if (getBackBuffer == nullptr ||
	    d3d11::Failed(getBackBuffer(device, 0, 0, d3d9::kBackBufferTypeMono, &surface)) || surface == nullptr) {
		return false;
	}
	auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
	const bool ok = getDesc != nullptr && !d3d11::Failed(getDesc(surface, &out));
	d3d11::Release(surface);
	return ok && out.width > 0 && out.height > 0;
}

// The texture made when it is not there, or is of another size, format or
// device; black until the first picture lands. False when it cannot be made.
bool EnsureTarget(MirrorState& s, HeadsetRenderer& renderer, int eye, void* device, UInt32 side, UInt32 format) {
	MirrorTarget& t = s.targets[eye];
	if (t.texture != nullptr && t.device == device && t.side == side && t.format == format) {
		return true;
	}
	ReleaseTarget(s, renderer, eye);
	auto create = d3d9::Method<d3d9::CreateTextureFn>(device, d3d9::kDeviceCreateTexture);
	if (create == nullptr || d3d11::Failed(create(device, side, side, 1, d3d9::kUsageRenderTarget, format,
	                                              d3d9::kPoolDefault, &t.texture, nullptr)) ||
	    t.texture == nullptr) {
		t.texture = nullptr;
		return false;
	}
	auto getSurface = d3d9::Method<d3d9::GetSurfaceLevelFn>(t.texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurface == nullptr || d3d11::Failed(getSurface(t.texture, 0, &t.surface)) || t.surface == nullptr) {
		t.surface = nullptr;
		d3d11::Release(t.texture);
		return false;
	}
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(device, d3d9::kDeviceColorFill);
	if (colorFill != nullptr) {
		colorFill(device, t.surface, nullptr, 0xFF000000u);
	}
	t.device = device;
	t.side = side;
	t.format = format;
	t.blackOutsideWidth = 0;
	t.blackOutsideHeight = 0;
	OBVR_LOG("Mirror: the reflection's texture made%s, %ux%u (format %u)", eye == 1 ? " for the right eye" : "", side,
	         side, format);
	return true;
}

// What one frame with the page up reads: the glass in the world, the camera
// behind it, and the inputs of the decision.
struct MirrorFrame {
	NiPoint3 corners[4] = {};   // the glass rendered through: the steadied one with its margin, or the held one
	vr::GlassPlane glass;
	vr::MirrorCamera camera;
	vr::MirrorPassInputs inputs;
	float longSidePixels = 0.0f;
	bool steadied = false;     // MirrorSteady on: the picture is laid through its camera
	float turnDegrees = 0.0f;  // the held glass's turn from the steadied one
	float seconds = 0.0f;      // since the last frame with the page up (none: not a number), for the budget
};

// Seconds since the last frame with the page up, on the real clock; none
// (not a number) for the first.
float SteadySeconds(MirrorState& s) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long now = ReadPerformanceCounter();
	const float seconds = s.steadyTicks != 0 && ticksPerSecond > 0
	                          ? static_cast<float>(static_cast<double>(now - s.steadyTicks) / static_cast<double>(ticksPerSecond))
	                          : std::numeric_limits<float>::quiet_NaN();
	s.steadyTicks = now;
	return seconds;
}

// The glass as held this frame through the steady filter (vr/MirrorSteadyLogic.h),
// grown by the margin: what the reflection is rendered through. Off, or no
// glass to steady, the held glass as it is.
void SteadyTheGlass(MirrorState& s, MirrorFrame& f, UInt32 setting) {
	const vr::MirrorSteadyTuning tuning = vr::MirrorSteadyTuningFor(static_cast<std::int32_t>(setting));
	const float seconds = SteadySeconds(s);
	f.seconds = seconds;
	if (!f.glass.valid) {
		return;
	}
	const vr::GlassFrame held = vr::GlassFrameOf(f.glass);
	vr::GlassFrame steady;
	vr::StepMirrorSteady(s.steady, held, seconds, tuning, steady);
	if (!tuning.on) {
		return;
	}
	NiPoint3 grown[4];
	vr::SteadyGlassCorners(f.corners, held, steady, tuning.margin, grown);
	const vr::GlassPlane plane = vr::GlassPlaneOf(grown);
	if (!plane.valid) {
		return;
	}
	for (int i = 0; i < 4; ++i) {
		f.corners[i] = grown[i];
	}
	f.glass = plane;
	f.steadied = true;
	f.turnDegrees = vr::GlassTurnDegrees(held.rot, steady.rot);
	f.longSidePixels *= 1.0f + 2.0f * tuning.margin;
}

MirrorFrame ReadMirrorFrame(MirrorState& s, const MirrorPageState& page, bool settingOn, bool eyesCaptured,
                            UInt32 steadySetting) {
	MirrorFrame f;
	for (int i = 0; i < 4; ++i) {
		f.corners[i] = vr::GlassCornerInWorld(page.handPos, page.handRot, page.unitsPerMetre, page.corners[i]);
	}
	f.glass = vr::GlassPlaneOf(f.corners);
	f.longSidePixels = vr::GlassLongSidePixels(page.eyes);
	SteadyTheGlass(s, f, steadySetting);
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
	if (vr::CyclopeanEye(page.eyes, eye)) {
		f.camera = vr::MirrorCameraFor(f.glass, f.corners, eye);
	}
	f.inputs.settingOn = settingOn;
	f.inputs.loading = game::LoadingThreadActive() ||
	                   (game::IsMenuMode() && game::TopVisibleMenu() == game::kMenuIdLoading);
	f.inputs.eyesCaptured = eyesCaptured;
	f.inputs.stopped = s.failures.stopped;
	f.inputs.view = f.camera.view;
	f.inputs.inView = vr::GlassInView(page.eyes[0]) || vr::GlassInView(page.eyes[1]);
	return f;
}

// The world rendered from the mirror's camera into the frame's top left
// corner. What changes for it, and where each goes back (the end of this
// function, in reverse order): (1) the main camera's local transform, its
// world transform recomputed from it - back to the saved local, recomputed;
// (2) its frustum - back to the saved one; (3) its port - back to the saved
// four floats (the engine itself resets it to 0, 1, 1, 0 after image space);
// (4) the device's viewport, which the render leaves at the mirror's corner -
// back to the saved one; (5) the bone lock's mode and shift - Off, and the
// shift the frame had; (6) the frame clock - zeroed and put back inside
// RunMirrorWorldRender. The water stays on its single pass (the reflection is
// rendered again for the mirror's camera, a small pass), the actor shadows
// keep the frame's maps (ShadowShare.h), the culling share and the leaf
// audit are closed by the frame's own render. (7) The player's head, for this
// render alone (game/MirrorHead.h): shown right before RunMirrorWorldRender,
// hidden again right after it - its true transform, the FaceGen bits, the
// palettes of the skins on its bones, each put back there.
//
// Stage 4 calls it once for each eye in a frame, everything above set and put
// back around each call: the bone lock replaces from the eyes' render's rows
// with each camera's own shift (Replace mode writes no row of the log), the
// head is shown and hidden per call (its skins' stamps turned each time, so it
// is built for each camera whatever the engine's stamp did), the shadows keep
// the frame's maps both times, and the water's reflection and the culling are
// each render's own (docs/mirror-spec.md 14.2 says why).
bool RenderFromMirror(const vr::MirrorCamera& camera, vr::MirrorPictureSize size, const d3d9::SurfaceDesc& back,
                      UInt32& draws, bool trace, game::MirrorHeadSwitch& head) {
	draws = 0;
	NiAVObject* const cam = game::MainCamera();
	float* const port = game::MainCameraPort();
	void* const device = GetGameDevice();
	if (cam == nullptr || port == nullptr || device == nullptr) {
		return false;
	}
	auto* const frustum = reinterpret_cast<game::NiFrustum*>(reinterpret_cast<UInt8*>(cam) + game::kNiCameraFrustumOffset);
	NiTransform parent{};
	parent.rot = NiMatrix33::Identity();
	parent.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	parent.scale = 1.0f;
	if (cam->parent != nullptr) {
		parent = cam->parent->worldTransform;
	}
	NiTransform wanted = cam->worldTransform;
	wanted.rot = camera.rotation;
	wanted.pos = camera.position;
	NiTransform local{};
	if (frustum->o || !(camera.nearUnits < frustum->f) || !BuildWaterCameraLocalFromParent(parent, wanted, local)) {
		return false;
	}
	const NiTransform savedLocal = cam->localTransform;
	const game::NiFrustum savedFrustum = *frustum;
	const float savedPort[4] = {port[0], port[1], port[2], port[3]};
	auto getViewport = d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport);
	auto setViewport = d3d9::Method<d3d9::SetViewportFn>(device, d3d9::kDeviceSetViewport);
	d3d9::Viewport savedViewport{};
	const bool haveViewport = getViewport != nullptr && setViewport != nullptr && !d3d11::Failed(getViewport(device, &savedViewport));
	float savedShift[3] = {0.0f, 0.0f, 0.0f};
	int sign = 0;
	GetBoneShiftState(savedShift, sign);
	// The frame's render drew with the camera where it stands now: the bone
	// rows it recorded are relative to here, and the mirror's are rebased by
	// the camera's move (render/BoneRebase.h: a translation, whatever the turn).
	const NiPoint3 shift = camera.position - cam->worldTransform.pos;

	// The frustum before the transform's update, as the water capture orders
	// them: the camera's derived state is made from both.
	frustum->l = camera.left;
	frustum->r = camera.right;
	frustum->t = camera.top;
	frustum->b = camera.bottom;
	frustum->n = camera.nearUnits;
	cam->localTransform = local;
	game::UpdateNodeTransforms(cam);
	const vr::MirrorPort p = vr::MirrorPortFor(size, back.width, back.height);
	port[0] = p.left;
	port[1] = p.right;
	port[2] = p.top;
	port[3] = p.bottom;
	SetBoneEyeShift(shift.x, shift.y, shift.z);
	SetBonePassMode(BonePassMode::Replace);
	// Without antialiasing and with image space the frame's copy left the frame
	// open and the mirror's copy would draw into no target (game/SceneFrame.h):
	// closed as between the dual pass's eyes. Nothing on the other routes.
	game::CloseSceneFrameBetweenPasses(false);
	if (trace) {
		OBVR_LOG("Mirror trace: the camera, its frustum and port set, the bone lock replacing (shift %.1f %.1f %.1f) - "
		         "the world render starts",
		         static_cast<double>(shift.x), static_cast<double>(shift.y), static_cast<double>(shift.z));
	}
	game::ShowPlayerHeadForMirror(head);
	const bool ran = RunMirrorWorldRender(draws);
	game::HidePlayerHeadAfterMirror(head);
	const bool landed = ran && CaptureTargetIsBackBuffer();

	SetBonePassMode(BonePassMode::Off);
	SetBoneEyeShift(savedShift[0], savedShift[1], savedShift[2]);
	if (haveViewport) {
		setViewport(device, &savedViewport);
	}
	port[0] = savedPort[0];
	port[1] = savedPort[1];
	port[2] = savedPort[2];
	port[3] = savedPort[3];
	*frustum = savedFrustum;
	cam->localTransform = savedLocal;
	game::UpdateNodeTransforms(cam);
	return landed;
}

// The picture from the frame's top left corner into the mirror's texture.
bool CopyToGlass(void* device, const MirrorTarget& t, vr::MirrorPictureSize size, SInt32& result) {
	result = 0;
	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(device, d3d9::kDeviceStretchRect);
	auto getBackBuffer = d3d9::Method<d3d9::GetBackBufferFn>(device, d3d9::kDeviceGetBackBuffer);
	void* source = nullptr;
	if (stretchRect == nullptr || getBackBuffer == nullptr || t.surface == nullptr ||
	    d3d11::Failed(getBackBuffer(device, 0, 0, d3d9::kBackBufferTypeMono, &source)) || source == nullptr) {
		return false;
	}
	const d3d9::Rect rect{0, 0, static_cast<SInt32>(size.width), static_cast<SInt32>(size.height)};
	result = stretchRect(device, source, &rect, t.surface, &rect, d3d9::kTexFilterNone);
	d3d11::Release(source);
	return !d3d11::Failed(result);
}

// The texture black outside the picture just copied, so the laid glass reads
// black wherever it reaches past the picture (vr/MirrorGlassMap.h): two fills,
// only when the picture shrank in a direction since the texture was last black
// outside it (the whole texture is black when made).
void BlackOutsidePicture(void* device, MirrorTarget& t, vr::MirrorPictureSize size) {
	if (size.width >= t.blackOutsideWidth && size.height >= t.blackOutsideHeight) {
		t.blackOutsideWidth = size.width;
		t.blackOutsideHeight = size.height;
		return;
	}
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(device, d3d9::kDeviceColorFill);
	if (colorFill == nullptr || t.surface == nullptr) {
		return;
	}
	const SInt32 side = static_cast<SInt32>(t.side);
	const SInt32 w = static_cast<SInt32>(size.width);
	const SInt32 h = static_cast<SInt32>(size.height);
	const d3d9::Rect right{w, 0, side, side};
	const d3d9::Rect below{0, h, w, side};
	const bool done = (w >= side || !d3d11::Failed(colorFill(device, t.surface, &right, 0xFF000000u))) &&
	                  (h >= side || !d3d11::Failed(colorFill(device, t.surface, &below, 0xFF000000u)));
	if (done) {
		t.blackOutsideWidth = size.width;
		t.blackOutsideHeight = size.height;
	}
}

// The bone lock's work in the frame's single-pass capture and the mirror's
// replace, marked as one span, so a capture's bone_lock spans stay per frame.
void MarkBoneLock(const perf::EventContext& context) {
	const UInt64 locked = TakeBoneLockTicks();
	if (locked != 0) {
		const UInt64 now = static_cast<UInt64>(ReadPerformanceCounter());
		perf::Profiler::Instance().Mark(perf::EventType::BoneLock, now - locked, now, context);
	}
}

// The palette builder's stamp source (docs/mirror-spec.md 14.2): the word at
// [[renderer+0x8B0]+0x3C], which 0x007655F0 compares with each skin's stamp.
// Read in the EXE: counted up by 0x00777A40, called only at 0x0076262B inside
// 0x00762600, the renderer's vtable +0x134 - the end of a scene frame (device
// EndScene, then the add); no other writer was found (a search for increments,
// not for plain stores). For the trace and the cost line: whether a frame's two
// passes see different stamps (each skin built again in the second) or the
// same. 0 when not readable.
UInt32 SkinBuildStamp() {
	const UInt32 renderer = *reinterpret_cast<const UInt32*>(addr::kRendererPointer);
	if (!mem::LooksLikeObjectAddress(renderer)) {
		return 0;
	}
	const UInt32 manager = *reinterpret_cast<const UInt32*>(renderer + 0x8B0);
	return mem::LooksLikeObjectAddress(manager) ? *reinterpret_cast<const UInt32*>(manager + 0x3C) : 0;
}

// The renderer's two scene-frame flags (+0x200 the default group's, +0x204 an
// offscreen one's; 0x007D71C0, 0x007D7280, 0x007D72D0): a world render ends
// the frame it finds open at its close, which counts the stamp up. Trace only.
void RendererFrameFlags(UInt32& defaultOpen, UInt32& offscreenOpen) {
	defaultOpen = offscreenOpen = 0xFFFFFFFFu;
	const UInt32 renderer = *reinterpret_cast<const UInt32*>(addr::kRendererPointer);
	if (mem::LooksLikeObjectAddress(renderer)) {
		defaultOpen = *reinterpret_cast<const UInt32*>(renderer + 0x200);
		offscreenOpen = *reinterpret_cast<const UInt32*>(renderer + 0x204);
	}
}

struct MirrorPassResult {
	bool rendered = false;  // the pass started: a back buffer read, a texture there
	bool copied = false;    // its picture reached the texture
	double ms = 0.0;
	UInt32 draws = 0;
	UInt32 stamp = 0;  // SkinBuildStamp before the render
	vr::MirrorPictureSize size;
	game::MirrorHeadSwitch head;
};

void TraceFrameStart(const MirrorFrame& f, const vr::MirrorCamera& c, const char* which, vr::MirrorPictureSize size,
                     UInt32 side) {
	UInt32 defaultOpen = 0;
	UInt32 offscreenOpen = 0;
	RendererFrameFlags(defaultOpen, offscreenOpen);
	OBVR_LOG("Mirror trace: scene call %u, %s - the glass at %.1f %.1f %.1f (%.1f x %.1f units), the eye %.1f units "
	         "in front (facing %.2f), the mirror's camera at %.1f %.1f %.1f, frustum %.3f %.3f %.3f %.3f near %.1f, "
	         "picture %ux%u of %u; skin stamp %u, renderer frames open %u %u",
	         CurrentSceneCall(), which, static_cast<double>(f.glass.centre.x), static_cast<double>(f.glass.centre.y),
	         static_cast<double>(f.glass.centre.z), static_cast<double>(f.glass.width),
	         static_cast<double>(f.glass.height), static_cast<double>(c.eyeDistance), static_cast<double>(c.facing),
	         static_cast<double>(c.position.x), static_cast<double>(c.position.y), static_cast<double>(c.position.z),
	         static_cast<double>(c.left), static_cast<double>(c.right), static_cast<double>(c.top),
	         static_cast<double>(c.bottom), static_cast<double>(c.nearUnits), size.width, size.height, side,
	         SkinBuildStamp(), defaultOpen, offscreenOpen);
}

// The picture copied: the texture black outside it, and noted as this eye's
// with the part it fills and, steadied, the camera it was rendered with.
void NotePicture(MirrorState& s, void* device, const MirrorFrame& f, const vr::MirrorCamera& camera, int eye,
                 vr::MirrorPictureSize size) {
	float rect[4];
	vr::MirrorGlassUvFor(size, s.targets[eye].side, rect);
	BlackOutsidePicture(device, s.targets[eye], size);  // after every copy: MirrorSteady may come on
	vr::NoteMirrorEyeRendered(s.pictures, eye, f.steadied ? vr::PictureViewOf(camera, rect) : vr::MirrorPictureView{},
	                          rect);
}

// One world render from `camera` into texture `eye` (0: the one picture or
// the left eye's, 1: the right eye's). Not handed to the glass here: the
// caller hands in once a frame, after its passes (HandInGlass).
MirrorPassResult RenderMirror(MirrorState& s, HeadsetRenderer& renderer, const MirrorFrame& f,
                              const vr::MirrorCamera& camera, int eye, const char* which, UInt32 pixels) {
	MirrorPassResult out;
	void* const device = GetGameDevice();
	d3d9::SurfaceDesc back{};
	if (device == nullptr || !ReadBackBuffer(device, back)) {
		if (vr::NoteMirrorOutcome(s.failures, false)) {
			SwitchOff(s, renderer, "the frame's back buffer could not be read");
		}
		return out;
	}
	if (!EnsureTarget(s, renderer, eye, device, pixels, back.format)) {
		if (vr::StopMirror(s.failures)) {
			SwitchOff(s, renderer, "the reflection's texture could not be made");
		}
		return out;
	}
	out.size = vr::MirrorPictureSizeFor(f.longSidePixels, f.glass.width / f.glass.height, s.targets[eye].side,
	                                    back.width, back.height);
	const bool trace = s.trace.On();
	if (trace) {
		TraceFrameStart(f, camera, which, out.size, s.targets[eye].side);
	}
	perf::EventContext context{};
	context.sceneId = CurrentSceneCall();
	context.passIndex = 2 + eye;
	const long long start = ReadPerformanceCounter();
	out.stamp = SkinBuildStamp();
	{
		perf::Profiler::Instance().GpuBegin(perf::EventType::MirrorPass, context, device);
		perf::Profiler::ScopedSpan span(perf::Profiler::Instance(), perf::EventType::MirrorPass, context);
		const bool landed = RenderFromMirror(camera, out.size, back, out.draws, trace, out.head);
		SInt32 result = 0;
		out.copied = landed && CopyToGlass(device, s.targets[eye], out.size, result);
		perf::Profiler::Instance().GpuEnd();
		if (trace) {
			OBVR_LOG("Mirror trace: the world render returned (%u draws), put back; the picture %s, %s", out.draws,
			         landed ? "in the frame" : "NOT in the frame", out.copied ? "copied to the glass" : "not copied");
		}
		if (!out.copied && DeviceGone(result)) {
			if (vr::StopMirror(s.failures)) {
				SwitchOff(s, renderer, "the device was lost at the copy");
			}
			return out;
		}
		if (vr::NoteMirrorOutcome(s.failures, out.copied)) {
			SwitchOff(s, renderer, landed ? "the copy to the glass was refused three frames in a row"
			                              : "the mirror's picture did not reach the frame three frames in a row");
		}
		if (out.copied && !s.failures.stopped) {
			NotePicture(s, device, f, camera, eye, out.size);
		}
	}
	MarkBoneLock(context);
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	out.ms = ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - start) /
	                                  static_cast<double>(ticksPerSecond)
	                            : 0.0;
	out.rendered = true;
	return out;
}

// What a frame rendered: its mode and each texture's pass (index 0 the one
// picture or the left eye's).
struct MirrorFrameResult {
	vr::MirrorDepthMode mode = vr::MirrorDepthMode::OneForBoth;
	MirrorPassResult pass[2];
	UInt32 planned = 0;
	bool Rendered() const { return pass[0].rendered || pass[1].rendered; }
	double Ms() const { return pass[0].ms + pass[1].ms; }
	UInt32 Passes() const { return (pass[0].rendered ? 1u : 0u) + (pass[1].rendered ? 1u : 0u); }
};

// MirrorDepth=0: stage 3's pass - the cyclopean camera, texture 0, one picture
// handed to both eyes.
MirrorFrameResult RenderOneForBoth(MirrorState& s, HeadsetRenderer& renderer, const MirrorFrame& f, UInt32 pixels) {
	MirrorFrameResult r;
	r.planned = 1;
	r.pass[0] = RenderMirror(s, renderer, f, f.camera, 0, "one picture for both eyes", pixels);
	if (r.pass[0].copied && !s.failures.stopped) {
		HandInGlass(s, renderer, false);
	}
	return r;
}

// Stage 4: each eye's camera (its own place reflected in the same steadied
// glass), the eyes this frame renders (both, or the older one while taking
// turns - vr::PlanMirrorEyes), one pass each as a plain repeat of stage 3's
// steps with that camera (docs/mirror-spec.md 14.2), then both pictures handed
// in, each with the camera it was rendered with. An eye not rendered this
// frame keeps its picture, laid through its own older camera.
MirrorFrameResult RenderEachEye(MirrorState& s, HeadsetRenderer& renderer, const MirrorFrame& f,
                                const MirrorPageState& page, UInt32 pixels) {
	MirrorFrameResult r;
	const vr::MirrorEyeCameras cameras = vr::MirrorEyeCamerasFor(f.glass, f.corners, page.eyes, true);
	const bool can[2] = {cameras.eye[0].view == vr::MirrorView::Ok && vr::GlassInView(page.eyes[0]),
	                     cameras.eye[1].view == vr::MirrorView::Ok && vr::GlassInView(page.eyes[1])};
	r.mode = vr::MirrorDepthModeFor(true, s.budget);
	vr::AgeMirrorEyes(s.pictures);
	const vr::MirrorEyePlan plan = vr::PlanMirrorEyes(r.mode, can, s.pictures.age);
	r.planned = plan.passes;
	bool copied = false;
	for (int eye = 0; eye < 2; ++eye) {
		if (plan.render[eye] && !s.failures.stopped) {
			r.pass[eye] =
				RenderMirror(s, renderer, f, cameras.eye[eye], eye, eye == 0 ? "the left eye" : "the right eye", pixels);
			copied = copied || r.pass[eye].copied;
		}
	}
	if (copied && !s.failures.stopped) {
		HandInGlass(s, renderer, true);
	}
	return r;
}

// The cost line (docs/mirror-spec.md 12.5, 13.4, 14.5).
void LogMirrorCost(const MirrorState& s, UInt32 steady, float budgetMs) {
	const vr::MirrorCostWindow& w = s.cost;
	const auto skipped = [&w](vr::MirrorSkip k) { return w.skipped[static_cast<int>(k)]; };
	const auto frames = [&w](vr::MirrorDepthMode m) { return w.modeFrames[static_cast<int>(m)]; };
	OBVR_LOG("Mirror: %u frames up - %u passes, %.2f ms mean, %.2f ms worst (CPU), %u draws a pass, picture %ux%u of "
	         "%u; skipped %u (out of view %u, facing away %u, edge-on %u, eyes not captured %u, loading %u, no glass "
	         "%u, stopped %u, switched off %u); head shown in %u of %u passes (%.3f ms a pass); steady %u: the glass "
	         "turned %.2f degrees mean, %.2f most from the steadied in %u frames, laid on %u eye draws, a corner past "
	         "the picture in %u, not mapped in %u; depth %s: both eyes %u, taking turns %u, one for both %u frames, "
	         "passes left %u right %u, together %.2f ms a frame mean, %.2f worst; budget %.2f ms a pass (last second "
	         "%.2f); kept: actor shadow maps %u times; skin stamp moved between the two passes in %u of %u frames",
	         w.frames, w.passes, MeanMs(w), w.worstMs, w.passes > 0 ? static_cast<UInt32>(w.draws / w.passes) : 0u,
	         w.width, w.height, s.targets[0].side, vr::SkippedTotal(w), skipped(vr::MirrorSkip::OutOfView),
	         skipped(vr::MirrorSkip::FacingAway), skipped(vr::MirrorSkip::EdgeOn), skipped(vr::MirrorSkip::NotCaptured),
	         skipped(vr::MirrorSkip::Loading), skipped(vr::MirrorSkip::NoGlass), skipped(vr::MirrorSkip::Stopped),
	         skipped(vr::MirrorSkip::Off), w.headShown, w.headAsked, vr::MeanHeadMs(w), steady, vr::MeanTurnDegrees(w),
	         static_cast<double>(w.turnMostDegrees), w.turned, w.glassDraws, w.glassOutside, w.glassUnmapped,
	         s.depth ? "on" : "off", frames(vr::MirrorDepthMode::Both), frames(vr::MirrorDepthMode::Turns),
	         frames(vr::MirrorDepthMode::OneForBoth), w.eyePasses[0], w.eyePasses[1], vr::MeanFrameMs(w),
	         w.frameWorstMs, static_cast<double>(budgetMs), s.budget.lastMeanMs, w.shadowsKept, w.stampChanged,
	         w.stampAsked);
}

// The frame into the cost window: each pass's head switch, the glass's turn,
// the frame (its first pass with it), its second pass, the mode and the
// passes together, the shadow maps the mirror kept since the last frame. True
// when the window's line is due.
bool NoteFrameCost(MirrorState& s, const MirrorFrame& f, vr::MirrorSkip counted, const MirrorFrameResult& r) {
	for (const MirrorPassResult& p : r.pass) {
		if (p.head.asked) {
			vr::NoteMirrorHead(s.cost, p.head.plan.refusal == game::MirrorHeadRefusal::None, p.head.ms);
		}
	}
	if (r.Rendered() && f.steadied) {
		vr::NoteMirrorTurn(s.cost, f.turnDegrees);
	}
	const MirrorPassResult& first = r.pass[0].rendered || !r.pass[1].rendered ? r.pass[0] : r.pass[1];
	const bool due = vr::NoteMirrorFrame(s.cost, counted, first.ms, first.draws, first.size);
	if (r.pass[0].rendered && r.pass[1].rendered) {
		vr::NoteMirrorExtraPass(s.cost, r.pass[1].ms, r.pass[1].draws);
	}
	if (r.Rendered()) {
		vr::NoteMirrorDepthFrame(s.cost, r.mode, r.Ms(), r.pass[0].rendered, r.pass[1].rendered,
		                         r.pass[0].stamp != r.pass[1].stamp);
	}
	const unsigned kept = MirrorShadowMapsKept();
	vr::NoteMirrorShadowsKept(s.cost, kept - s.shadowsKeptMark);
	s.shadowsKeptMark = kept;
	return due;
}

// With depth: the frame's passes into the budget (vr::StepMirrorBudget); a
// change of mode is one line, and the trace logs the passes after it.
void StepBudget(MirrorState& s, const MirrorFrame& f, const MirrorFrameResult& r, float budgetMs) {
	const vr::MirrorBudgetChange change = vr::StepMirrorBudget(s.budget, budgetMs, f.seconds, r.Ms(), r.Passes());
	if (change == vr::MirrorBudgetChange::None) {
		return;
	}
	s.trace = vr::MirrorTrace{};
	if (change == vr::MirrorBudgetChange::ToTurns) {
		OBVR_LOG("Mirror: the eyes take turns now - a pass cost %.2f ms on average over the last second, over the "
		         "budget of %.2f ms (MirrorBudgetMs); one pass a frame, each eye's picture renewed every second frame",
		         s.budget.lastMeanMs, static_cast<double>(budgetMs));
	} else {
		OBVR_LOG("Mirror: both eyes every frame again - %.2f ms a pass over the last second, under %.2f ms "
		         "(MirrorBudgetMs %.2f, or 0)",
		         s.budget.lastMeanMs, static_cast<double>(budgetMs * vr::kMirrorBudgetBackShare),
		         static_cast<double>(budgetMs));
	}
}

void SayMirrorOn(MirrorState& s, bool depth, float budgetMs) {
	if (s.onSaid) {
		return;
	}
	s.onSaid = true;
	if (!depth) {
		OBVR_LOG("Mirror: the reflection is on - one world render a frame while the glass is in view, after both "
		         "eyes' captures, on the glass a frame later; one picture for both eyes, the head shown for it alone");
		return;
	}
	OBVR_LOG("Mirror: the reflection is on - each eye its own picture: two world renders a frame while the glass is "
	         "in view, one a frame with the eyes taking turns while a pass costs over %.2f ms; after both eyes' "
	         "captures, on the glass a frame later; the head shown for them alone",
	         static_cast<double>(budgetMs));
}

}  // namespace

void RunMirrorGlassPass(HeadsetRenderer& renderer, bool stereoFrame) {
	MirrorState& s = State();
	const vr::TabletSettings& t = GetConfig().hands.tablet;
	NoteDepthSetting(s, renderer, t.mirrorDepth);
	const MirrorPageState page = renderer.MirrorPage();
	if (!page.up) {
		s.wantsBones = false;
		s.steady = vr::MirrorSteadyState{};  // the next time up starts from the glass as it is
		s.steadyTicks = 0;
		s.budget = vr::MirrorBudget{};      // and with both eyes
		DarkGlass(s, renderer);
		return;
	}
	const bool settingOn = t.mirror && t.mirrorWorld;
	s.wantsBones = vr::MirrorWantsBoneCapture(true, settingOn, s.failures.stopped);
	const bool eyesCaptured = stereoFrame && renderer.BothEyesCaptured() && BonesKeptThisFrame();
	const MirrorFrame f = ReadMirrorFrame(s, page, settingOn, eyesCaptured, t.mirrorSteady);
	vr::NoteGlassDraws(s.cost, page.glassDraws, page.glassOutside, page.glassUnmapped);
	const vr::MirrorSkip skip = vr::DecideMirrorPass(f.inputs);
	MirrorFrameResult result;
	if (skip == vr::MirrorSkip::None) {
		SayMirrorOn(s, t.mirrorDepth, t.mirrorBudgetMs);
		result = t.mirrorDepth ? RenderEachEye(s, renderer, f, page, t.mirrorPixels)
		                       : RenderOneForBoth(s, renderer, f, t.mirrorPixels);
		if (result.Rendered()) {
			s.trace.FrameDone();
		}
	} else if (skip == vr::MirrorSkip::Off || skip == vr::MirrorSkip::FacingAway) {
		DarkGlass(s, renderer);  // the glass's back, or no reflection wanted: no old picture on it
	}
	// A pass that could not start (no back buffer read, no texture) counts as
	// stopped; with depth, a frame neither eye could render (its own camera
	// refused, or the glass out of its view) as out of view.
	vr::MirrorSkip counted = skip;
	if (skip == vr::MirrorSkip::None && !result.Rendered()) {
		counted = result.planned == 0 ? vr::MirrorSkip::OutOfView : vr::MirrorSkip::Stopped;
	}
	if (t.mirrorDepth) {
		StepBudget(s, f, result, t.mirrorBudgetMs);
	}
	if (NoteFrameCost(s, f, counted, result)) {
		LogMirrorCost(s, t.mirrorSteady, t.mirrorBudgetMs);
		vr::ResetMirrorCost(s.cost);
	}
}

bool MirrorWantsBones() { return State().wantsBones; }

}  // namespace obvr::render
