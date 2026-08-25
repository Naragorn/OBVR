#pragma once

#include "core/Types.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Reads the head orientation from SteamVR through OpenVR.
//
// Why OpenVR and not OpenXR: Oblivion.exe is 32 bit, so OBVR.dll is too.
// openvr_api.dll has always shipped for x86, and OpenVR is out-of-process by
// design (vrclient against vrserver.exe) - Valve already built the bridge
// between 32 and 64 bit there. SteamVR's OpenXR only supports 32-bit
// applications from beta 2.17.2 onwards, and under Proton wineopenxr is not
// built for i386 at all.
//
// The library is loaded at runtime rather than linked. That is not a matter
// of taste: OBVR has to load cleanly on machines without SteamVR, otherwise
// starting Oblivion would fail for every user who does not own a headset. If
// any step fails, the vanilla camera stays in place and OBVR says so in the
// log.
class OpenVRBackend {
public:
	// Loads openvr_api.dll, registers OBVR with SteamVR and fetches the
	// system interface. Calling it more than once is harmless.
	//
	// Returns false when SteamVR cannot be reached - a normal state, not an
	// error.
	bool Start();

	// Unregisters OBVR from SteamVR. Deliberately not called from DllMain:
	// the loader holds its lock there, and VR_ShutdownInternal loads
	// libraries of its own.
	void Stop();

	bool IsRunning() const { return m_system != nullptr; }

	// Current head orientation in OpenVR convention (X right, Y up,
	// -Z forward), which is the same as OpenXR's. The caller still has to
	// pass it through FromOpenXR.
	//
	// Returns false while no valid pose is available - for instance because
	// tracking has not picked up yet. The caller should then keep the last
	// valid orientation instead of letting the camera jump.
	bool ReadHeadOrientation(Quaternion& out) const;

private:
	// Logs a message once. The flag is diagnostic state rather than part of
	// the backend's state, which is why it is mutable and usable from a const
	// method.
	void LogOnce(bool& alreadyLogged, const char* message) const;

	void* m_module = nullptr;   // openvr_api.dll
	void* m_system = nullptr;   // IVRSystemFnTable*
	bool m_startAttempted = false;
	mutable bool m_loggedNoPose = false;
};

}  // namespace obvr::vr
