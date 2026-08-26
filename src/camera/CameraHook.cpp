#include "camera/CameraHook.h"

#include "camera/CameraTrampoline.h"
#include "camera/LookControl.h"
#include "core/Config.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "core/MathFns.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "platform/Win32Min.h"
#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/HeadsetRenderer.h"
#include "render/PresentHook.h"
#include "render/ResolutionHook.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

KeyEdge g_recenterEdge;
FrameClock g_frameClock;
LookControl g_lookControl;
render::HeadsetRenderer g_headsetRenderer;

// What the camera hook decided about this frame, kept for Present to act on.
//
// Kept rather than rebuilt at the end, because the eye it names has to be the
// eye the camera was actually moved to. Reading the frame counter twice would
// be two chances to disagree, and disagreeing means each eye showing the
// other's viewpoint - a fault this project has already had once and does not
// need a second route to.
render::HeadsetRenderer::FrameRequest g_pendingRequest;

// Whether hooking the end of the frame was tried and failed. One attempt, not
// one per frame: a device whose table cannot be written this frame will not
// become writable on the next.
bool g_presentHookRefused = false;

// Whether BeginFrame opened a frame this pass. The submit at the end - here or
// from Present - is only owed when it did.
bool g_frameOpen = false;

// Watches Oblivion's own render frustum for a few frames, to say whether it is
// one view or several. See game::FrustumWatcher.
game::FrustumWatcher g_frustumWatcher;

float Abs(float value) { return value < 0.0f ? -value : value; }

// Runs from inside Present, with Oblivion's finished frame in the back buffer.
//
// Declared ahead of OnFrameEnd, which needs it: the recenter key has to work
// on frames where the camera hook does not run, which is every video, the main
// menu, and every menu opened in game.
bool PollRecenterEdge();

// Deliberately does nothing but pay the frame that BeginFrame opened. Anything
// else that wanted doing at the end of a frame would be tempting to put here,
// and this runs on the renderer's thread inside a call the game is waiting on.
void OnFrameEnd() {
	// Consumed, not merely read.
	//
	// This flag is set by the camera hook and nothing else, so on a frame
	// where that hook does not run - an inventory, an ESC menu, anything that
	// pauses the world - it would still be holding last frame's true. Present
	// would then take the normal path, EndFrame would find no frame open and
	// return at once, and nothing would reach the headset at all.
	//
	// Which is precisely what was reported: menus in game showed nothing,
	// while the main menu worked. The main menu works because the camera hook
	// has never run at that point, so the flag is still its initial false.
	const bool hadCameraPass = g_frameOpen;
	g_frameOpen = false;

	if (hadCameraPass) {
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), g_pendingRequest);
		return;
	}

	// No camera pass for this frame, so this is a menu or a loading screen.
	//
	// Deliver it anyway. Without this the headset shows nothing at all while
	// the main menu is up, which means loading a save requires taking the
	// headset off - and after ten frames without a submit the compositor drops
	// to its own Home scene, so it is not even a black screen, it is somebody
	// else's room.
	//
	// Flat, and deliberately so: there is no camera to give the eyes different
	// viewpoints from, and no pose the picture can be said to have been drawn
	// with. Menus in the world rather than on a plane are a separate piece of
	// work and a much larger one.
	if (!GetConfig().tracker.showMenus) {
		return;
	}

	// The recenter key, polled here because nothing else does on these frames.
	//
	// A flat picture is anchored where the head was when it appeared. If that
	// was mid-turn, or the wearer has since settled into a different position,
	// the picture hangs somewhere awkward and there is no way to move it - the
	// camera hook polls the key, and the camera hook is exactly what is not
	// running. An intro film that started while looking down stays down.
	if (PollRecenterEdge()) {
		g_headsetRenderer.ResetFlatAnchor();
		OBVR_LOG("Render: the flat picture was re-anchored on the recenter key (flat path)");
	}

	render::HeadsetRenderer::FrameRequest menu;
	menu.gameDevice = render::GetGameDevice();
	menu.submitGameFrame = GetConfig().tracker.submitGameFrame;
	menu.flatFrame = true;
	menu.backBufferIsThisFrame = true;
	menu.gameFovDegrees = GetConfig().tracker.gameFovDegrees;
	menu.gameFovIsFor4x3 = GetConfig().tracker.gameFovIsFor4x3;
	menu.cameraTanHalfWidth = g_state.cameraTanHalfWidth;
	menu.cameraTanHalfHeight = g_state.cameraTanHalfHeight;
	menu.menuScale = GetConfig().tracker.menuScale;
	menu.menuAspect = GetConfig().tracker.menuAspect;

	if (g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame())) {
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), menu);
	}
}

bool ReadIsThirdPerson() {
	auto* player = *reinterpret_cast<UInt8**>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}
	return player[addr::kPlayerIsThirdPersonOffset] != 0;
}

void MaybeReloadConfig() {
	Config& config = GetConfig();
	if (!IsDue(g_state.frameCount, config.reloadEveryFrames)) {
		return;
	}

	if (config.Reload("OBVR.ini")) {
		g_headTracker.Configure(config.tracker);
		g_lookControl.Configure(config.look);
	}
}

// Polls the recenter key once per frame.
//
// Polled rather than hooked, and the reason is robustness, not speed. The
// obvious alternative would be a WH_KEYBOARD_LL hook, but Microsoft documents
// three properties that rule it out here:
//
//   * "This hook is called in the context of the thread that installed it.
//     The call is made by sending a message to the thread that installed the
//     hook. Therefore, the thread that installed the hook must have a message
//     loop." OBVR is a plugin inside Oblivion and owns no message loop it can
//     service on its own terms.
//   * The hook sits in the system-wide input path. Every keystroke in every
//     application waits for it to sign off before the key state updates.
//   * "If the hook procedure times out ... on Windows 7 and later, the hook is
//     silently removed without being called. There is no way for the
//     application to know whether the hook is removed." A game that stutters
//     is precisely where a timeout happens, and OBVR could not even detect
//     that recentering had stopped working.
//
// Microsoft's own advice is to prefer raw input over low-level hooks. That
// would work, but it needs a window to register against and a message queue to
// drain - considerably more machinery than one call per rendered frame.
//
// Cost is not the argument either way: this runs once per frame rather than in
// a spin loop, so it is a single user32 call every 8 to 16 milliseconds.
//
// The edge itself is detected by KeyEdge in FrameLogic.h, from a stored
// previous state. GetAsyncKeyState does carry a "pressed since the last call"
// bit, but that bit is consumed process wide by whoever reads it first, so it
// cannot be relied on next to the game's own input handling.
//
// The key state is global rather than per window. That is harmless here:
// Oblivion pauses when it loses focus, so this callback does not run at all
// while another application has the keyboard.
// Whether the recenter key went down this call, without acting on it.
//
// Separate from MaybePollRecenter because on a flat frame there is no camera
// to recenter - the key means "put the picture where I am looking now"
// instead. Both share one KeyEdge, which is what stops a single press from
// counting twice when a menu opens on the frame the key is pressed.
bool PollRecenterEdge() {
	const UInt32 key = GetConfig().recenterKey;
	if (key == 0) {
		g_recenterEdge.Reset();
		return false;
	}

	const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
	return g_recenterEdge.Update(isDown);
}

void MaybePollRecenter() {
	if (!PollRecenterEdge()) {
		return;
	}

	g_headTracker.Recenter();

	// Recentering is meant to take effect at once. Easing the camera into the
	// new zero would be the opposite of what the key is pressed for.
	g_lookControl.Reset();
	OBVR_LOG("Camera: recentered on key 0x%02X (frame %u, camera path)", GetConfig().recenterKey,
	         g_state.frameCount);
}

}  // namespace

// Called from the trampoline after Oblivion has finished computing the
// camera. eax held the CameraNode there; the trampoline passes it through as
// the single argument.
//
// All registers are saved at this point, so this function may be ordinary
// C++. It does have to stay fast and free of exceptions though - it runs on
// every rendered frame.
extern "C" void __cdecl OBVR_OnCameraUpdated(NiAVObject* cameraNode) {
	if (cameraNode == nullptr) {
		return;
	}

	const bool isThirdPerson = ReadIsThirdPerson();

	switch (g_state.ObservePointOfView(isThirdPerson)) {
	case PovEvent::FirstPass: {
		OBVR_LOG("Camera: first hook pass, CameraNode=%08X, %s",
		         reinterpret_cast<UInt32>(cameraNode),
		         isThirdPerson ? "third person" : "first person");

		// The first look at Oblivion's own renderer, and the question 0.1.0
		// turns on: is Direct3D 9 here being served by DXVK, which hands out
		// the Vulkan objects behind a texture, or by Microsoft's own, which
		// does not? Asked here because the renderer certainly exists by the
		// time a frame is being drawn, and asked once because the answer
		// cannot change within a run.
		void* device = render::GetGameDevice();
		const render::DeviceKind kind = render::IdentifyDevice(device);
		OBVR_LOG("Render: Oblivion's D3D9 device %08X is %s",
		         reinterpret_cast<UInt32>(device), render::DeviceKindName(kind));

		// Five of the ten fields OpenVR wants for a Vulkan texture, and the
		// reason the DXVK route exists. Logged before anything is submitted,
		// because a handle that arrives null here fails inside the compositor
		// later - reported as a bad texture, which would send the search to
		// entirely the wrong place.
		if (kind == render::DeviceKind::Dxvk) {
			render::VulkanContext vulkan;
			if (render::GetVulkanContext(device, vulkan)) {
				OBVR_LOG("Render: Vulkan instance=%08X physical=%08X device=%08X",
				         reinterpret_cast<UInt32>(vulkan.instance),
				         reinterpret_cast<UInt32>(vulkan.physicalDevice),
				         reinterpret_cast<UInt32>(vulkan.device));
				OBVR_LOG("Render: Vulkan queue=%08X index=%u family=%u",
				         reinterpret_cast<UInt32>(vulkan.queue), vulkan.queueIndex,
				         vulkan.queueFamilyIndex);
			} else {
				OBVR_LOG("Render: DXVK did not hand over a complete set of Vulkan handles");
			}

			// The other five fields, and the first look at Oblivion's own
			// picture as something OpenVR could take. The size is the check
			// worth reading: it should match the game's window, and anything
			// else means this is not the surface it appears to be.
			render::BackBufferImage backBuffer;
			if (render::GetBackBufferImage(device, backBuffer)) {
				OBVR_LOG("Render: back buffer image=%08X%08X %ux%u format=%u samples=%u layout=%u",
				         static_cast<UInt32>(backBuffer.image >> 32),
				         static_cast<UInt32>(backBuffer.image), backBuffer.width,
				         backBuffer.height, backBuffer.format, backBuffer.sampleCount,
				         backBuffer.layout);

				// The line that decides whether Oblivion's own frame can go
				// to the compositor as it stands. Usage is the part that
				// cannot be repaired afterwards: it is fixed when the image
				// is created, so missing bits mean copying the frame rather
				// than handing it over.
				OBVR_LOG("Render: back buffer usage=%08X transfer_src=%d sampled=%d, %s",
				         backBuffer.usage,
				         (backBuffer.usage & render::dxvk::kImageUsageTransferSrc) != 0 ? 1 : 0,
				         (backBuffer.usage & render::dxvk::kImageUsageSampled) != 0 ? 1 : 0,
				         render::IsSubmittableImage(backBuffer)
				             ? "submittable once transitioned"
				             : "NOT submittable as it stands");
			} else {
				OBVR_LOG("Render: no Vulkan image behind the back buffer");
			}
		}
		break;
	}
	case PovEvent::Switched:
		// First and third person put the camera in entirely different places,
		// so there is no continuity for the easing to preserve across the
		// change.
		g_lookControl.Reset();
		OBVR_LOG("Camera: switched to %s (frame %u)",
		         isThirdPerson ? "third person" : "first person",
		         g_state.frameCount);
		break;
	case PovEvent::Unchanged:
		break;
	}

	++g_state.frameCount;
	MaybeReloadConfig();

	// The counter frequency is fixed for the lifetime of the process, so it
	// is read once rather than every frame.
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const float deltaSeconds = g_frameClock.Tick(ReadPerformanceCounter(), ticksPerSecond);

	// The compositor first, before anything asks the tracker where the head
	// is. This blocks until the headset wants the next frame and hands back
	// the pose to draw that frame with - and it has to come first, because
	// the tracker is about to be read and the pose from here is the answer it
	// should give.
	//
	// The order is the one OpenVR's own overview specifies: WaitGetPoses,
	// render, submit. It used to be render, submit, WaitGetPoses, which
	// meant the poses arrived a frame after they were wanted and were thrown
	// away instead. See OpenVRBackend::WaitGetPoses for what that cost.
	//
	// Does nothing when rendering is off or the compositor was never reached,
	// so the cost on a machine without a headset is one comparison.
	g_frameOpen = g_headsetRenderer.BeginFrame(g_headTracker.GetBackendForFrame());


	// Watch Oblivion's own render frustum, a handful of times.
	//
	// Reading it once at startup found a view 1.458 times wider in tangents
	// than the projection matrix reported - the same factor in both axes. That
	// is one view at the wrong size rather than a different view, and the two
	// candidates are a camera meant for something else or the right camera at
	// the wrong moment. Whether it moves is what separates them.
	if (GetConfig().tracker.renderToHeadset) {
		game::NiFrustum frustum{};
		if (game::ReadGameCameraFrustum(frustum)) {
			// The headset's own view first, because it outranks an angle: an
			// eye is not a 4:3 frustum and forcing it through one throws away
			// the shape that is the entire point of matching it.
			float headsetW = 0.0f;
			float headsetH = 0.0f;
			const bool matching = GetConfig().tracker.matchHeadsetFov &&
			                      g_headsetRenderer.GetHeadsetFrustum(headsetW, headsetH);

			if (matching) {
				game::SetFrustumTangents(frustum, headsetW, headsetH);
				if (!game::WriteGameCameraFrustum(frustum)) {
					OBVR_LOG("Camera: the headset frustum could not be written");
				}
			} else {
				const float override = GetConfig().tracker.gameFovOverride;
				if (override > 1.0f && override < 179.0f) {
					game::SetFrustumFov(frustum, override);
					if (!game::WriteGameCameraFrustum(frustum)) {
						// Cannot happen after a successful read, but silence
						// here would mean the world quietly kept its old field
						// of view.
						OBVR_LOG("Camera: the field of view override could not be written");
					}
				}
			}

			// Kept for the placement. Absolute values, because which edge
			// carries which sign is not settled and the half-width does not
			// depend on it.
			const float halfWidth = (Abs(frustum.l) + Abs(frustum.r)) * 0.5f;
			const float halfHeight = (Abs(frustum.t) + Abs(frustum.b)) * 0.5f;
			if (halfWidth > 0.0f && halfHeight > 0.0f) {
				g_state.cameraTanHalfWidth = halfWidth;
				g_state.cameraTanHalfHeight = halfHeight;
			}
		}

		if (game::ReadGameCameraFrustum(frustum) && g_frustumWatcher.Observe(frustum)) {
			OBVR_LOG("Camera: frustum #%u l=%.4f r=%.4f t=%.4f b=%.4f n=%.4f f=%.1f "
			         "(%.1f deg across)",
			         g_frustumWatcher.Reported(), static_cast<double>(frustum.l),
			         static_cast<double>(frustum.r), static_cast<double>(frustum.t),
			         static_cast<double>(frustum.b), static_cast<double>(frustum.n),
			         static_cast<double>(frustum.f),
			         static_cast<double>(2.0f * math::Atan(frustum.r) *
			                             math::kRadiansToDegrees));
		}
	}

	g_headTracker.Update(g_state.frameCount);

	// After Update, so that the recenter reference is this frame's
	// orientation rather than the previous one. The new zero therefore takes
	// effect from the next frame - a single frame of delay that nobody can
	// see, in exchange for the reference being exactly the pose the user was
	// holding when they pressed the key.
	MaybePollRecenter();

	const Config& config = GetConfig();
	if (IsDue(g_state.frameCount, config.logEveryFrames)) {
		const vr::Quaternion& raw = g_headTracker.GetRawOrientation();
		const NiPoint3& pos = cameraNode->localTransform.pos;
		const NiPoint3& offset = g_headTracker.GetCameraOffset();
		// The lean is reported twice over: the offset actually applied, and
		// how far it reached for before MaxLeanUnits cut it. Equal means the
		// limit never came into play; a raw figure stuck at MaxLeanUnits
		// across several lines means it is the limit doing the deciding, not
		// the head - and that is not something the headset can show you.
		OBVR_LOG("Camera: frame %u, %s, %.1f ms, pos=(%.1f, %.1f, %.1f), "
		         "head=(%.3f, %.3f, %.3f, %.3f), lean=(%.1f, %.1f, %.1f) raw=%.1f",
		         g_state.frameCount,
		         isThirdPerson ? "3rd" : "1st",
		         static_cast<double>(deltaSeconds) * 1000.0,
		         static_cast<double>(pos.x),
		         static_cast<double>(pos.y),
		         static_cast<double>(pos.z),
		         static_cast<double>(raw.x),
		         static_cast<double>(raw.y),
		         static_cast<double>(raw.z),
		         static_cast<double>(raw.w),
		         static_cast<double>(offset.x),
		         static_cast<double>(offset.y),
		         static_cast<double>(offset.z),
		         static_cast<double>(g_headTracker.GetRawOffsetUnits()));
	}

	// The heart of it: the vanilla rotation stays the base, the head rotation
	// acts in local camera space.
	//
	// The order matters. The head offset is measured in the camera's own
	// space, so it has to be carried over by the vanilla rotation - the one
	// the game computed, without the head laid on top. Taking the product
	// instead would tie leaning to where the head is looking, and leaning
	// forward while glancing sideways would slide the camera sideways.
	// The look controls are only taken away from the player while a headset is
	// actually delivering poses. Without one there is nothing to hand them to,
	// and somebody starting Oblivion without SteamVR has to get the game they
	// had before.
	NiMatrix33 baseRotation = cameraNode->localTransform.rot;
	float verticalOffset = 0.0f;

	if (g_headTracker.IsHeadsetConnected()) {
		g_lookControl.Update(baseRotation, isThirdPerson, deltaSeconds);
		baseRotation = g_lookControl.GetRotation();
		verticalOffset = g_lookControl.GetVerticalOffset();
	} else {
		g_lookControl.Reset();
	}

	// The head offset is measured in the camera's own space, so it is carried
	// over by the base rotation. The vertical look is not: it is a height, and
	// heights are along the world up axis whichever way the camera faces.
	cameraNode->localTransform.pos =
		cameraNode->localTransform.pos + baseRotation * g_headTracker.GetCameraOffset();
	cameraNode->localTransform.pos.z += verticalOffset;

	const NiMatrix33 finalRotation = baseRotation * g_headTracker.GetCameraRotation();

	// Alternate eye rendering: the camera steps to one eye, this frame is drawn
	// from there, and it goes to that eye alone. The next frame does the other.
	// Depth without drawing the world twice.
	//
	// Carried by the final rotation rather than the base one, and the
	// difference matters. The head offset above is measured in the frame the
	// wearer recentered in, so the levelled rotation is what belongs under it.
	// The eyes are attached to the head: where "right" is for them depends on
	// where the head is looking, which is what the head rotation adds.
	if (config.tracker.stereo == vr::StereoMode::AlternateEyes &&
	    g_headTracker.IsHeadsetConnected()) {
		const float half = g_headTracker.GetHalfEyeSeparationUnits();
		const float sign = IsLeftEyeFrame(g_state.frameCount) ? -1.0f : 1.0f;
		const NiPoint3 eyeOffset{sign * half, 0.0f, 0.0f};
		cameraNode->localTransform.pos =
			cameraNode->localTransform.pos + finalRotation * eyeOffset;
	}

	cameraNode->localTransform.rot = finalRotation;

	// Last, and on purpose. This blocks until the compositor wants the next
	// frame, so from here Oblivion runs on the compositor's clock rather than
	// its own - which is what keeps the picture in step with the headset, and
	// is the arrangement 0.1.0 needs, since the texture submitted then has to
	// be the frame the game has just drawn.
	//
	// It does nothing at all unless rendering was asked for and the
	// compositor was reached, so the cost on every other machine is one
	// comparison.
	render::HeadsetRenderer::FrameRequest request;
	request.gameDevice = render::GetGameDevice();
	request.submitGameFrame = config.tracker.submitGameFrame;
	request.alternateEyes = config.tracker.stereo == vr::StereoMode::AlternateEyes;

	// The same call the camera offset above used, so the eye the camera moved
	// to and the eye the picture is given to cannot drift apart.
	request.isLeftEye = IsLeftEyeFrame(g_state.frameCount);
	request.gameFovDegrees = config.tracker.gameFovDegrees;
	request.gameFovIsFor4x3 = config.tracker.gameFovIsFor4x3;

	// Read at the start of this pass, not here at the end. The frustum differs
	// between the two: 1.0231 across when the camera is computed, 1.1188 by the
	// time Present runs. Which of those the frame was drawn with is not settled,
	// but the first is the one that matches fDefaultFOV exactly, and the second
	// is read after every pass of the frame has had its turn with the camera.
	request.cameraTanHalfWidth = g_state.cameraTanHalfWidth;
	request.cameraTanHalfHeight = g_state.cameraTanHalfHeight;
	request.menuScale = config.tracker.menuScale;
	request.menuAspect = config.tracker.menuAspect;

	if (!g_frameOpen) {
		// BeginFrame declined at the top of this pass: rendering is off, the
		// compositor was never reached, or it asked us to stop. Nothing is
		// owed, and submitting anyway would be a Submit with no WaitGetPoses
		// in front of it.
		return;
	}

	if (!config.tracker.submitAtFrameEnd) {
		// Submitted here, which means the picture is whatever the back buffer
		// held from last time. One frame of latency, and the arrangement that
		// is known to work.
		request.backBufferIsThisFrame = false;
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), request);
		return;
	}

	// The other arrangement: the wait already happened at the top of this
	// pass, Oblivion is about to draw, and Present hands the finished picture
	// over.
	//
	// The request is kept rather than rebuilt at the end, because the eye it
	// names has to be the eye the camera was moved to a few lines above. Two
	// separate readings of the frame counter would be two chances to disagree,
	// and disagreeing means each eye showing the other's viewpoint.
	request.backBufferIsThisFrame = true;
	g_pendingRequest = request;

	if (!render::IsPresentHooked() && !g_presentHookRefused) {
		if (!render::InstallPresentHook(request.gameDevice, &OnFrameEnd)) {
			// Said once. Without the hook the end of the frame never arrives,
			// so falling back to submitting here is better than a headset that
			// quietly stops being fed.
			g_presentHookRefused = true;
			OBVR_LOG("Render: the frame end could not be hooked, so the submit stays at the "
			         "start of the frame and the picture is one frame old");
		}
	}

	if (g_presentHookRefused) {
		g_pendingRequest.backBufferIsThisFrame = false;
		g_headsetRenderer.EndFrame(g_headTracker.GetBackend(), g_pendingRequest);
	}
}

vr::HeadTracker& GetHeadTracker() { return g_headTracker; }

const State& GetState() { return g_state; }

bool Install() {
	const Config& config = GetConfig();
	g_headTracker.Configure(config.tracker);
	g_lookControl.Configure(config.look);

	// Check first, patch second. If something other than the expected bytes
	// sits there, it is a different game version or another mod got there
	// first - in either case patching would be a shot in the dark.
	if (!mem::Verify(addr::kHookCameraUpdate, kOriginalBytes, addr::kHookCameraUpdatePatchSize)) {
		OBVR_LOG("Camera: bytes at %08X differ, hook will not be installed",
		         addr::kHookCameraUpdate);
		return false;
	}

	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Camera: no executable memory for the trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = BuildTrampoline(
		trampoline, kTrampolineSize, trampolineAddress,
		reinterpret_cast<UInt32>(&OBVR_OnCameraUpdated));

	if (trampolineSize == 0) {
		OBVR_LOG("Camera: trampoline does not fit into %u bytes", kTrampolineSize);
		return false;
	}

	UInt8 patch[addr::kHookCameraUpdatePatchSize];
	const UInt32 patchSize =
		BuildPatch(patch, sizeof(patch), addr::kHookCameraUpdate, trampolineAddress);

	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Camera: patch has unexpected length %u", patchSize);
		return false;
	}

	if (!mem::SafeWrite(addr::kHookCameraUpdate, patch, patchSize)) {
		OBVR_LOG("Camera: SafeWrite to %08X failed", addr::kHookCameraUpdate);
		return false;
	}

	OBVR_LOG("Camera: hook installed at %08X, trampoline at %08X (%u bytes)",
	         addr::kHookCameraUpdate, trampolineAddress, trampolineSize);

	// The end of the frame, hooked as soon as there is a device to hook it on
	// rather than when the first world camera runs.
	//
	// The difference is everything before that camera: the intro videos, the
	// main menu, and the dialogue that loads a save. Installing from the camera
	// hook meant none of those reached the headset - which was reported as
	// seeing only the loading screen, and only because by then the hook had
	// gone in.
	if (GetConfig().tracker.renderToHeadset && GetConfig().tracker.submitAtFrameEnd) {
		render::InstallPresentHookWhenReady(&OnFrameEnd);
	}

	// Oblivion's frame size, set where it is decided.
	//
	// This used to write iSize into Oblivion.ini and let the game read it next
	// time - a detour past the place the decision is made, costing two
	// restarts and editing a file that belongs to the user. The device
	// creation is where the size actually lives, so that is where this is now.
	if (!GetConfig().tracker.setRenderSize) {
		OBVR_LOG("Config: SetGameResolution is off, so the frame is the game's own size");
	} else {
		UInt32 width = GetConfig().tracker.renderWidth;
		UInt32 height = GetConfig().tracker.renderHeight;

		if (width == 0 || height == 0) {
			// Whatever the headset asks for. That figure already accounts for
			// the distortion margin the compositor needs, so it is the honest
			// answer to "what can this headset use".
			if (!g_headTracker.GetBackend().GetRecommendedRenderTargetSize(width, height)) {
				OBVR_LOG("Config: the headset reported no render size, so the frame is the "
				         "game's own");
				width = 0;
				height = 0;
			}
		}

		if (width != 0 && height != 0) {
			render::SetWantedResolution(width, height);
			if (render::InstallResolutionHook()) {
				OBVR_LOG("Config: the frame will be created at %ux%u", width, height);
			} else {
				// Not a silent fallback. Either the import is not there, or -
				// far more likely - the device was already created before this
				// plugin loaded, and then nothing here can reach it.
				OBVR_LOG("Config: Direct3DCreate9 could not be intercepted, so the frame "
				         "stays at the game's own size. If the device was already made by "
				         "the time OBVR loaded, this is why.");
			}
		}
	}

	return true;

}

}  // namespace obvr::camera
