// Checks the fallback path of the OpenVR backend on a machine without
// SteamVR.
//
// This is not a side issue: the vast majority of users will start OBVR at
// least once without SteamVR running. If OBVR crashed there or kept Oblivion
// from loading, the mod would be useless - for people who have not even been
// in VR yet.
//
// The test also compiles the whole backend for x86, which is what arms the
// static_asserts in OpenVRTypes.h.

#include <cstddef>
#include <cstdio>

#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	std::printf("OBVR OpenVR backend, fallback without SteamVR\n\n");

	std::printf("Struct layout\n");
	Check(sizeof(obvr::vr::openvr::HmdMatrix34) == 48, "HmdMatrix34 is 48 bytes");
	Check(sizeof(obvr::vr::openvr::TrackedDevicePose) == 80, "TrackedDevicePose is 80 bytes");

	// The typed entry has to sit exactly behind the twelve unused ones. The
	// check is written against sizeof(void*) rather than a fixed 48 because
	// the tests build natively, which is 64 bit on this machine, while
	// OBVR.dll itself is always x86. What matters is the index, not the byte
	// offset - and the index is what a wrong listing of the interface would
	// get wrong.
	Check(offsetof(obvr::vr::openvr::IVRSystemFnTable, GetDeviceToAbsoluteTrackingPose) ==
	          12 * sizeof(void*),
	      "GetDeviceToAbsoluteTrackingPose sits at index 12");

	std::printf("\nStarting without SteamVR\n");
	obvr::vr::OpenVRBackend backend;

	const bool started = backend.Start();
	Check(!started, "Start reports false instead of crashing");
	Check(!backend.IsRunning(), "backend reports itself as not running");

	obvr::vr::Quaternion orientation = obvr::vr::Quaternion::Identity();
	obvr::NiPoint3 position{0.0f, 0.0f, 0.0f};
	const bool read = backend.ReadHeadPose(orientation, position);
	Check(!read, "ReadHeadPose reports false without a connection");

	// A second attempt must neither crash nor flood the log.
	Check(!backend.Start(), "second Start stays without effect as well");

	// Stop on a backend that never started has to be harmless too.
	backend.Stop();
	Check(true, "Stop without a prior Start does not crash");

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
