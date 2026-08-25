#include "vr/OpenVRBackend.h"

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {
namespace {

// LoadLibraryA searches next to the running executable first. So the x86
// build of openvr_api.dll belongs next to Oblivion.exe - not the x64 build
// that ships with other games, and not the one inside SteamVR's own folder.
constexpr const char* kOpenVRLibrary = "openvr_api.dll";

template <typename Fn>
Fn Resolve(void* module, const char* name) {
	return reinterpret_cast<Fn>(GetProcAddress(static_cast<HMODULE>(module), name));
}

}  // namespace

void OpenVRBackend::LogOnce(bool& alreadyLogged, const char* message) const {
	if (!alreadyLogged) {
		alreadyLogged = true;
		OBVR_LOG("OpenVR: %s", message);
	}
}

bool OpenVRBackend::Connect(int applicationType) {
	auto initInternal = Resolve<openvr::VR_InitInternalFn>(m_module, "VR_InitInternal");
	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");
	auto errorText = Resolve<openvr::VR_GetVRInitErrorAsEnglishDescriptionFn>(
		m_module, "VR_GetVRInitErrorAsEnglishDescription");

	int error = openvr::kInitErrorNone;
	initInternal(&error, applicationType);
	if (error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: VR_InitInternal as type %d failed (%d: %s)", applicationType, error,
		         errorText != nullptr ? errorText(error) : "no description");
		return false;
	}

	error = openvr::kInitErrorNone;
	m_system = getInterface(openvr::kIVRSystemFnTableVersion, &error);
	if (m_system == nullptr || error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: %s unavailable (%d: %s)", openvr::kIVRSystemFnTableVersion, error,
		         errorText != nullptr ? errorText(error) : "no description");

		// Registered but unusable. Unregistering here rather than leaving it
		// is what makes a second attempt with a different application type
		// safe: SteamVR would otherwise still hold the first registration.
		if (auto shutdown =
				Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
			shutdown();
		}
		return false;
	}

	return true;
}

bool OpenVRBackend::Start(bool wantScene) {
	if (m_system != nullptr) {
		return true;
	}
	if (m_startAttempted) {
		// Retrying would print the same failure every frame. Anyone who
		// starts SteamVR afterwards can restart Oblivion.
		return false;
	}
	m_startAttempted = true;

	// Next to OBVR.dll first, which means Data/OBSE/Plugins. That is inside
	// the folder Mod Organizer 2 virtualises, so the library can ship as part
	// of the mod rather than needing the Root Builder plugin to place it in
	// the game root. The load happens long after USVFS has installed its
	// hooks, so the virtual path resolves.
	char path[512];
	if (platform::BuildPluginPath(kOpenVRLibrary, path, sizeof(path))) {
		m_module = LoadLibraryA(path);
	}

	// Otherwise the plain name, which LoadLibrary resolves next to
	// Oblivion.exe - the layout every non-MO2 installation uses.
	if (m_module == nullptr) {
		m_module = LoadLibraryA(kOpenVRLibrary);
	}

	if (m_module == nullptr) {
		OBVR_LOG("OpenVR: %s not found, camera stays unchanged", kOpenVRLibrary);
		OBVR_LOG("OpenVR: the x86 build belongs next to OBVR.dll or next to Oblivion.exe");
		return false;
	}

	auto isRuntimeInstalled =
		Resolve<openvr::VR_IsRuntimeInstalledFn>(m_module, "VR_IsRuntimeInstalled");
	auto initInternal = Resolve<openvr::VR_InitInternalFn>(m_module, "VR_InitInternal");
	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");

	if (initInternal == nullptr || getInterface == nullptr) {
		OBVR_LOG("OpenVR: %s does not have the expected exports", kOpenVRLibrary);
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	if (isRuntimeInstalled != nullptr && !isRuntimeInstalled()) {
		OBVR_LOG("OpenVR: no runtime installed, camera stays unchanged");
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	// The scene attempt first, when asked for. It is the only way to submit a
	// frame, and it is also the only way to take the headset away from
	// whatever else is using it - which is why it happens on request rather
	// than by default.
	if (wantScene && Connect(openvr::kApplicationScene)) {
		int error = openvr::kInitErrorNone;
		m_compositor = getInterface(openvr::kIVRCompositorFnTableVersion, &error);

		if (m_compositor != nullptr && error == openvr::kInitErrorNone) {
			OBVR_LOG("OpenVR: connected as a scene application through %s and %s",
			         openvr::kIVRSystemFnTableVersion, openvr::kIVRCompositorFnTableVersion);
			return true;
		}

		// Registered as a scene application but without a compositor to
		// submit to. Retreating to background costs the picture and keeps
		// head tracking, which is the better half to keep: a camera that
		// still follows the head is a working mod, a headset showing nothing
		// is not.
		OBVR_LOG("OpenVR: %s unavailable (%d), falling back to head tracking only",
		         openvr::kIVRCompositorFnTableVersion, error);
		m_compositor = nullptr;
		m_system = nullptr;
		if (auto shutdown =
				Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
			shutdown();
		}
	}

	// Background: reads poses and leaves the compositor alone. Everything up
	// to 0.0.4 works this way, and it stays the default until rendering is
	// asked for explicitly.
	if (!Connect(openvr::kApplicationBackground)) {
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	OBVR_LOG("OpenVR: connected through %s", openvr::kIVRSystemFnTableVersion);
	return true;
}

void OpenVRBackend::Stop() {
	if (m_module == nullptr) {
		return;
	}

	if (auto shutdown =
			Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
		shutdown();
	}

	FreeLibrary(static_cast<HMODULE>(m_module));
	m_module = nullptr;
	m_system = nullptr;
	m_compositor = nullptr;
	OBVR_LOG("OpenVR: disconnected");
}

bool OpenVRBackend::GetEyeProjection(int eye, float& left, float& right, float& top,
                                     float& bottom) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetProjectionRaw == nullptr) {
		return false;
	}

	float l = 0.0f;
	float r = 0.0f;
	float t = 0.0f;
	float b = 0.0f;
	table->GetProjectionRaw(eye, &l, &r, &t, &b);

	// A frustum with no width or no height is not a frustum. This catches a
	// runtime that answered without filling anything in, which would
	// otherwise turn into a division by zero downstream and be reported as a
	// projection fault rather than a missing answer.
	if (r - l == 0.0f || b - t == 0.0f) {
		return false;
	}

	left = l;
	right = r;
	top = t;
	bottom = b;
	return true;
}

bool OpenVRBackend::GetEyeOffset(int eye, NiPoint3& offsetMetres) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetEyeToHeadTransform == nullptr) {
		return false;
	}

	// Returns by value; see the note in OpenVRTypes.h about the hidden return
	// pointer. The translation is the fourth column, exactly as in a tracked
	// device pose, so it goes through the same reader.
	const openvr::HmdMatrix34 transform = table->GetEyeToHeadTransform(eye);
	offsetMetres = PositionFromOpenVRMatrix(transform.m);
	return true;
}

int OpenVRBackend::WaitGetPoses() const {
	if (m_compositor == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	auto* table = static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
	if (table->WaitGetPoses == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	// One entry rather than kMaxTrackedDeviceCount, for the same reason
	// ReadHeadPose asks for one: the HMD is index 0 and the array is filled
	// from the front, so the rest would be five kilobytes of controllers and
	// base stations on the stack every frame.
	//
	// The game poses are declined outright. They are the poses to run game
	// logic against, and Oblivion's logic knows nothing about a headset.
	openvr::TrackedDevicePose renderPose{};
	return table->WaitGetPoses(&renderPose, 1, nullptr, 0);
}

int OpenVRBackend::SubmitEye(int eye, void* handle, int textureType,
                             const openvr::VRTextureBounds* bounds) const {
	if (m_compositor == nullptr || handle == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	auto* table = static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
	if (table->Submit == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	openvr::Texture description{};
	description.handle = handle;
	description.type = textureType;

	// Auto rather than a stated colour space. The texture is
	// R8G8B8A8_UNORM, and letting the compositor apply its own rule for that
	// format is more likely to be right than OBVR asserting one - the ramp in
	// the test pattern is there precisely so that a wrong guess here is
	// visible rather than merely suspected.
	description.colorSpace = openvr::kColorSpaceAuto;

	return table->Submit(eye, &description, bounds, openvr::kSubmitDefault);
}

bool OpenVRBackend::GetRecommendedRenderTargetSize(UInt32& width, UInt32& height) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetRecommendedRenderTargetSize == nullptr) {
		return false;
	}

	UInt32 w = 0;
	UInt32 h = 0;
	table->GetRecommendedRenderTargetSize(&w, &h);

	// A zero from a runtime that answered anyway would otherwise become a
	// texture of no size, and Direct3D reports that as a generic failure with
	// nothing pointing back to here.
	if (w == 0 || h == 0) {
		return false;
	}

	width = w;
	height = h;
	return true;
}

bool OpenVRBackend::ReadHeadPose(Quaternion& orientation, NiPoint3& position) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetDeviceToAbsoluteTrackingPose == nullptr) {
		return false;
	}

	// One entry instead of kMaxTrackedDeviceCount: the HMD is always index 0
	// and the call fills the array from the front. That saves a good five
	// kilobytes of stack per frame which would only describe controllers and
	// base stations.
	openvr::TrackedDevicePose pose{};

	// No prediction: predictedSecondsFromNow stays zero because OBVR
	// has no frame loop of its own and therefore does not know when the image
	// will be presented. That belongs to the milestone that brings a frame loop.
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, 0.0f, &pose, 1);

	if (!pose.poseIsValid || !pose.deviceIsConnected) {
		// Happens routinely right after startup while tracking has not picked
		// up yet. The caller keeps the last valid pose instead of letting the
		// camera snap back to rest.
		LogOnce(m_loggedNoPose, "no valid HMD pose yet");
		return false;
	}

	orientation = FromOpenVRMatrix(pose.deviceToAbsoluteTracking.m);
	position = PositionFromOpenVRMatrix(pose.deviceToAbsoluteTracking.m);
	return true;
}

}  // namespace obvr::vr
