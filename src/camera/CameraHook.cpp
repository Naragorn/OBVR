#include "camera/CameraHook.h"

#include "camera/CameraTrampoline.h"
#include "camera/LookControl.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "platform/Win32Min.h"
#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/HeadsetRenderer.h"

namespace obvr::camera {
namespace {

State g_state;
vr::HeadTracker g_headTracker;

constexpr UInt32 kTrampolineSize = 64;

KeyEdge g_recenterEdge;
FrameClock g_frameClock;
LookControl g_lookControl;
render::HeadsetRenderer g_headsetRenderer;

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
void MaybePollRecenter() {
	const UInt32 key = GetConfig().recenterKey;
	if (key == 0) {
		// Explicitly disabled in the configuration. Forgetting the previous
		// state matters here: a key held down while recentering is switched
		// off would otherwise fire an edge the moment it is switched back on.
		g_recenterEdge.Reset();
		return;
	}

	const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;

	if (g_recenterEdge.Update(isDown)) {
		g_headTracker.Recenter();

		// Recentering is meant to take effect at once. Easing the camera into
		// the new zero would be the opposite of what the key is pressed for.
		g_lookControl.Reset();
		OBVR_LOG("Camera: recentered on key 0x%02X (frame %u)", key, g_state.frameCount);
	}
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

	g_headsetRenderer.Update(g_headTracker.GetBackend(), request);
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
	return true;
}

}  // namespace obvr::camera
