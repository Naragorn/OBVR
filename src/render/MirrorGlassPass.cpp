#include "render/MirrorGlassPass.h"

#include "core/Config.h"
#include "core/Log.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/MenuMode.h"
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
#include "render/WaterReprojection.h"
#include "vr/MirrorGlassLogic.h"

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
};

struct MirrorState {
	MirrorTarget target;
	vr::MirrorFailures failures;
	vr::MirrorCostWindow cost;
	vr::MirrorTrace trace;
	bool handedIn = false;    // the glass shows target.texture
	bool wantsBones = false;  // for the next single-pass render (MirrorWantsBones)
	bool onSaid = false;
};

MirrorState& State() {
	static MirrorState s;
	return s;
}

// The glass dark again (stage 1's), before anything it showed is let go.
void DarkGlass(MirrorState& s, HeadsetRenderer& renderer) {
	if (s.handedIn) {
		renderer.SetMirrorGlass(nullptr, nullptr, MirrorGlassUv{});
		s.handedIn = false;
	}
}

void ReleaseTarget(MirrorState& s, HeadsetRenderer& renderer) {
	DarkGlass(s, renderer);
	d3d11::Release(s.target.surface);
	d3d11::Release(s.target.texture);
	s.target = MirrorTarget{};
}

// Off for the session: what was made let go, the glass dark, one line.
void SwitchOff(MirrorState& s, HeadsetRenderer& renderer, const char* why) {
	ReleaseTarget(s, renderer);
	OBVR_LOG("Mirror: switched off for this session - %s; the glass stays dark", why);
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
bool EnsureTarget(MirrorState& s, HeadsetRenderer& renderer, void* device, UInt32 side, UInt32 format) {
	MirrorTarget& t = s.target;
	if (t.texture != nullptr && t.device == device && t.side == side && t.format == format) {
		return true;
	}
	ReleaseTarget(s, renderer);
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
	OBVR_LOG("Mirror: the reflection's texture made, %ux%u (format %u)", side, side, format);
	return true;
}

// What one frame with the page up reads: the glass in the world, the camera
// behind it, and the inputs of the decision.
struct MirrorFrame {
	NiPoint3 corners[4] = {};
	vr::GlassPlane glass;
	vr::MirrorCamera camera;
	vr::MirrorPassInputs inputs;
	float longSidePixels = 0.0f;
};

MirrorFrame ReadMirrorFrame(const MirrorPageState& page, bool settingOn, bool stopped, bool eyesCaptured) {
	MirrorFrame f;
	for (int i = 0; i < 4; ++i) {
		f.corners[i] = vr::GlassCornerInWorld(page.handPos, page.handRot, page.unitsPerMetre, page.corners[i]);
	}
	f.glass = vr::GlassPlaneOf(f.corners);
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
	if (vr::CyclopeanEye(page.eyes, eye)) {
		f.camera = vr::MirrorCameraFor(f.glass, f.corners, eye);
	}
	f.inputs.settingOn = settingOn;
	f.inputs.loading = game::LoadingThreadActive() ||
	                   (game::IsMenuMode() && game::TopVisibleMenu() == game::kMenuIdLoading);
	f.inputs.eyesCaptured = eyesCaptured;
	f.inputs.stopped = stopped;
	f.inputs.view = f.camera.view;
	f.inputs.inView = vr::GlassInView(page.eyes[0]) || vr::GlassInView(page.eyes[1]);
	f.longSidePixels = vr::GlassLongSidePixels(page.eyes);
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
// audit are closed by the frame's own render, the head stays hidden.
bool RenderFromMirror(const vr::MirrorCamera& camera, vr::MirrorPictureSize size, const d3d9::SurfaceDesc& back,
                      UInt32& draws, bool trace) {
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
	const bool ran = RunMirrorWorldRender(draws);
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

// The bone lock's work in the frame's single-pass capture and the mirror's
// replace, marked as one span, so a capture's bone_lock spans stay per frame.
void MarkBoneLock(const perf::EventContext& context) {
	const UInt64 locked = TakeBoneLockTicks();
	if (locked != 0) {
		const UInt64 now = static_cast<UInt64>(ReadPerformanceCounter());
		perf::Profiler::Instance().Mark(perf::EventType::BoneLock, now - locked, now, context);
	}
}

struct MirrorPassResult {
	bool rendered = false;
	double ms = 0.0;
	UInt32 draws = 0;
	vr::MirrorPictureSize size;
};

void TraceFrameStart(const MirrorFrame& f, vr::MirrorPictureSize size, UInt32 side) {
	const vr::MirrorCamera& c = f.camera;
	OBVR_LOG("Mirror trace: scene call %u - the glass at %.1f %.1f %.1f (%.1f x %.1f units), the eye %.1f units in "
	         "front (facing %.2f), the mirror's camera at %.1f %.1f %.1f, frustum %.3f %.3f %.3f %.3f near %.1f, "
	         "picture %ux%u of %u",
	         CurrentSceneCall(), static_cast<double>(f.glass.centre.x), static_cast<double>(f.glass.centre.y),
	         static_cast<double>(f.glass.centre.z), static_cast<double>(f.glass.width),
	         static_cast<double>(f.glass.height), static_cast<double>(c.eyeDistance), static_cast<double>(c.facing),
	         static_cast<double>(c.position.x), static_cast<double>(c.position.y), static_cast<double>(c.position.z),
	         static_cast<double>(c.left), static_cast<double>(c.right), static_cast<double>(c.top),
	         static_cast<double>(c.bottom), static_cast<double>(c.nearUnits), size.width, size.height, side);
}

MirrorPassResult RenderMirror(MirrorState& s, HeadsetRenderer& renderer, const MirrorFrame& f, UInt32 pixels) {
	MirrorPassResult out;
	void* const device = GetGameDevice();
	d3d9::SurfaceDesc back{};
	if (device == nullptr || !ReadBackBuffer(device, back)) {
		if (vr::NoteMirrorOutcome(s.failures, false)) {
			SwitchOff(s, renderer, "the frame's back buffer could not be read");
		}
		return out;
	}
	if (!EnsureTarget(s, renderer, device, pixels, back.format)) {
		if (vr::StopMirror(s.failures)) {
			SwitchOff(s, renderer, "the reflection's texture could not be made");
		}
		return out;
	}
	out.size = vr::MirrorPictureSizeFor(f.longSidePixels, f.glass.width / f.glass.height, s.target.side, back.width,
	                                    back.height);
	const bool trace = s.trace.On();
	if (trace) {
		TraceFrameStart(f, out.size, s.target.side);
	}
	perf::EventContext context{};
	context.sceneId = CurrentSceneCall();
	context.passIndex = 2;
	const long long start = ReadPerformanceCounter();
	bool landed = false;
	{
		perf::Profiler::Instance().GpuBegin(perf::EventType::MirrorPass, context, device);
		perf::Profiler::ScopedSpan span(perf::Profiler::Instance(), perf::EventType::MirrorPass, context);
		landed = RenderFromMirror(f.camera, out.size, back, out.draws, trace);
		SInt32 result = 0;
		const bool copied = landed && CopyToGlass(device, s.target, out.size, result);
		perf::Profiler::Instance().GpuEnd();
		if (trace) {
			OBVR_LOG("Mirror trace: the world render returned (%u draws), put back; the picture %s, %s", out.draws,
			         landed ? "in the frame" : "NOT in the frame", copied ? "copied to the glass" : "not copied");
		}
		if (!copied && DeviceGone(result)) {
			if (vr::StopMirror(s.failures)) {
				SwitchOff(s, renderer, "the device was lost at the copy");
			}
			return out;
		}
		if (vr::NoteMirrorOutcome(s.failures, copied)) {
			SwitchOff(s, renderer, landed ? "the copy to the glass was refused three frames in a row"
			                              : "the mirror's picture did not reach the frame three frames in a row");
		}
		if (copied) {
			MirrorGlassUv uv;
			vr::MirrorGlassUvFor(out.size, s.target.side, uv.rect);
			renderer.SetMirrorGlass(s.target.texture, nullptr, uv);
			s.handedIn = true;
		}
	}
	MarkBoneLock(context);
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	out.ms = ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - start) /
	                                  static_cast<double>(ticksPerSecond)
	                            : 0.0;
	out.rendered = true;
	s.trace.FrameDone();
	return out;
}

// The cost line (docs/mirror-spec.md 12, "Reading the cost line").
void LogMirrorCost(const vr::MirrorCostWindow& w, UInt32 side) {
	const auto skipped = [&w](vr::MirrorSkip k) { return w.skipped[static_cast<int>(k)]; };
	OBVR_LOG("Mirror: %u frames up - %u passes, %.2f ms mean, %.2f ms worst (CPU), %u draws a pass, picture %ux%u of "
	         "%u; skipped %u (out of view %u, facing away %u, edge-on %u, eyes not captured %u, loading %u, no glass "
	         "%u, stopped %u, switched off %u)",
	         w.frames, w.passes, MeanMs(w), w.worstMs,
	         w.passes > 0 ? static_cast<UInt32>(w.draws / w.passes) : 0u, w.width, w.height, side,
	         vr::SkippedTotal(w), skipped(vr::MirrorSkip::OutOfView), skipped(vr::MirrorSkip::FacingAway),
	         skipped(vr::MirrorSkip::EdgeOn), skipped(vr::MirrorSkip::NotCaptured), skipped(vr::MirrorSkip::Loading),
	         skipped(vr::MirrorSkip::NoGlass), skipped(vr::MirrorSkip::Stopped), skipped(vr::MirrorSkip::Off));
}

}  // namespace

void RunMirrorGlassPass(HeadsetRenderer& renderer, bool stereoFrame) {
	MirrorState& s = State();
	const MirrorPageState page = renderer.MirrorPage();
	if (!page.up) {
		s.wantsBones = false;
		DarkGlass(s, renderer);
		return;
	}
	const vr::TabletSettings& t = GetConfig().hands.tablet;
	const bool settingOn = t.mirror && t.mirrorWorld;
	s.wantsBones = vr::MirrorWantsBoneCapture(true, settingOn, s.failures.stopped);
	const bool eyesCaptured = stereoFrame && renderer.BothEyesCaptured() && BonesKeptThisFrame();
	const MirrorFrame f = ReadMirrorFrame(page, settingOn, s.failures.stopped, eyesCaptured);
	const vr::MirrorSkip skip = vr::DecideMirrorPass(f.inputs);
	MirrorPassResult result;
	if (skip == vr::MirrorSkip::None) {
		if (!s.onSaid) {
			s.onSaid = true;
			OBVR_LOG("Mirror: the reflection is on - one world render a frame while the glass is in view, after both "
			         "eyes' captures, on the glass a frame later; one picture for both eyes, the head not in it");
		}
		result = RenderMirror(s, renderer, f, t.mirrorPixels);
	} else if (skip == vr::MirrorSkip::Off || skip == vr::MirrorSkip::FacingAway) {
		DarkGlass(s, renderer);  // the glass's back, or no reflection wanted: no old picture on it
	}
	// A pass that could not start (no back buffer read, no texture) counts as stopped.
	const vr::MirrorSkip counted = result.rendered || skip != vr::MirrorSkip::None ? skip : vr::MirrorSkip::Stopped;
	if (vr::NoteMirrorFrame(s.cost, counted, result.ms, result.draws, result.size)) {
		LogMirrorCost(s.cost, s.target.side);
		vr::ResetMirrorCost(s.cost);
	}
}

bool MirrorWantsBones() { return State().wantsBones; }

}  // namespace obvr::render
