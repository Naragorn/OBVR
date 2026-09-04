#include <cstdio>

#include "render/CullingSync.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
	if (!condition) {
		std::printf("FAIL: %s\n", message);
		++g_failures;
	}
}

bool SamePoint(const obvr::NiPoint3& a, const obvr::NiPoint3& b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

}  // namespace

int main() {
	using namespace obvr;
	using namespace obvr::render;

	CullingFrameSync sync;
	NiPoint3 replacement{9.0f, 9.0f, 9.0f};
	int cameraA = 0;
	int cameraB = 0;
	const NiPoint3 first{1.0f, 2.0f, 3.0f};
	const NiPoint3 second{4.0f, 5.0f, 6.0f};

	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::Inactive,
	      "an idle synchronizer leaves an ordinary render alone");

	sync.BeginCapture();
	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::Captured,
	      "the first culling call is captured");
	Check(sync.Visit(&cameraB, second, replacement) == CullingSyncResult::Captured,
	      "a later culling call is captured in order");
	Check(sync.CapturedCount() == 2, "capture reports its exact call count");

	sync.End();
	Check(sync.Visit(&cameraA, second, replacement) == CullingSyncResult::Inactive,
	      "work between the eye renders is not accidentally added to the capture");
	sync.BeginReplay();
	Check(sync.Visit(&cameraA, second, replacement) == CullingSyncResult::Reused,
	      "the same first camera reuses its captured position");
	Check(SamePoint(replacement, first), "replay returns the first pass's position");
	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::CameraMismatch,
	      "a different camera at the same ordinal is refused");
	Check(sync.Visit(&cameraB, second, replacement) == CullingSyncResult::NoCapturedCall,
	      "a second pass with extra calls is passed through after capture is exhausted");
	Check(sync.ReplayCount() == 3, "refused and overflow calls still advance the ordinal");

	sync.End();
	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::Inactive,
	      "ending the frame disables replay");

	sync.BeginCapture();
	for (UInt32 i = 0; i < CullingFrameSync::kCapacity; ++i) {
		Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::Captured,
		      "every in-capacity call is retained");
	}
	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::CaptureFull,
	      "capture refuses rather than overwrites when its fixed storage is full");
	Check(sync.CapturedCount() == CullingFrameSync::kCapacity,
	      "overflow does not change the retained count");

	sync.BeginCapture();
	Check(sync.CapturedCount() == 0, "a new frame discards the previous frame's samples");
	sync.BeginReplay();
	Check(sync.Visit(&cameraA, first, replacement) == CullingSyncResult::NoCapturedCall,
	      "a frame with no first-pass calls cannot invent a replay position");

	if (g_failures == 0) {
		std::printf("All culling synchronization tests passed.\n");
	}
	return g_failures == 0 ? 0 : 1;
}
