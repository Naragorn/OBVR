#include "vr/OpenVRBackend.h"

#include "core/Log.h"
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

bool OpenVRBackend::Start() {
	if (m_system != nullptr) {
		return true;
	}
	if (m_startAttempted) {
		// Retrying would print the same failure every frame. Anyone who
		// starts SteamVR afterwards can restart Oblivion.
		return false;
	}
	m_startAttempted = true;

	m_module = LoadLibraryA(kOpenVRLibrary);
	if (m_module == nullptr) {
		OBVR_LOG("OpenVR: %s not found, camera stays unchanged", kOpenVRLibrary);
		OBVR_LOG("OpenVR: the x86 build belongs next to Oblivion.exe");
		return false;
	}

	auto isRuntimeInstalled =
		Resolve<openvr::VR_IsRuntimeInstalledFn>(m_module, "VR_IsRuntimeInstalled");
	auto initInternal = Resolve<openvr::VR_InitInternalFn>(m_module, "VR_InitInternal");
	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");
	auto errorText = Resolve<openvr::VR_GetVRInitErrorAsEnglishDescriptionFn>(
		m_module, "VR_GetVRInitErrorAsEnglishDescription");

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

	// Background rather than Scene: in 0.0.3 OBVR only reads poses and must
	// not take the scene away from the compositor.
	int error = openvr::kInitErrorNone;
	initInternal(&error, openvr::kApplicationBackground);
	if (error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: VR_InitInternal failed (%d: %s)", error,
		         errorText != nullptr ? errorText(error) : "no description");
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	error = openvr::kInitErrorNone;
	m_system = getInterface(openvr::kIVRSystemFnTableVersion, &error);
	if (m_system == nullptr || error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: %s unavailable (%d: %s)", openvr::kIVRSystemFnTableVersion,
		         error, errorText != nullptr ? errorText(error) : "no description");
		Stop();
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
	OBVR_LOG("OpenVR: disconnected");
}

bool OpenVRBackend::ReadHeadOrientation(Quaternion& out) const {
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

	// No prediction: predictedSecondsFromNow stays zero because in 0.0.3 OBVR
	// has no frame loop of its own and therefore does not know when the image
	// will be presented. That belongs to 0.0.4.
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, 0.0f, &pose, 1);

	if (!pose.poseIsValid || !pose.deviceIsConnected) {
		// Happens routinely right after startup while tracking has not picked
		// up yet. The caller keeps the last valid orientation instead of
		// letting the camera snap back to rest.
		LogOnce(m_loggedNoPose, "no valid HMD pose yet");
		return false;
	}

	out = FromOpenVRMatrix(pose.deviceToAbsoluteTracking.m);
	return true;
}

}  // namespace obvr::vr
