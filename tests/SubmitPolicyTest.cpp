// Checks what OBVR does about what the compositor said.
//
// The fault this guards against is a specific and nasty one. WaitGetPoses
// blocks until the compositor wants the next frame, so calling it from
// Oblivion's thread puts the game on the compositor's clock - which is
// correct, and is how the picture stays in step with the world. But without
// focus that call throttles itself to 10 Hz. A game thread blocking on it
// runs at ten frames a second, and nothing on screen points at VR; it reads
// as a driver problem, and the person debugging it is not looking anywhere
// near here.
//
// So the answers have to be acted on, and the acting-on is pure logic. Pure
// arithmetic, so this runs on Linux as well.

#include <cstdio>

#include "render/SubmitPolicy.h"
#include "vr/OpenVRTypes.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::SubmitDecision;
using obvr::render::SubmitPolicy;

namespace codes = obvr::vr::openvr;

void TestSuccessIsBoring() {
	std::printf("When nothing is wrong\n");

	SubmitPolicy policy;
	for (int frame = 0; frame < 1000; ++frame) {
		if (policy.Observe(codes::kCompositorErrorNone) != SubmitDecision::Continue) {
			Check(false, "a thousand good frames stay good");
			return;
		}
	}
	Check(true, "a thousand good frames stay good");
	Check(!policy.HasStopped(), "and rendering is still on");
	Check(policy.GetConsecutiveFailures() == 0, "with no failures counted");
}

void TestPermanentFailuresStopAtOnce() {
	std::printf("Failures that waiting cannot fix\n");

	// Submitting as a background application. The application type is fixed
	// at registration, so the next frame's answer is identical - retrying it
	// sixty times a second only fills the log.
	SubmitPolicy notScene;
	Check(notScene.Observe(codes::kCompositorErrorIsNotSceneApplication) ==
	          SubmitDecision::StopRendering,
	      "submitting as a background application stops rendering immediately");
	Check(notScene.GetStopReason() == codes::kCompositorErrorIsNotSceneApplication,
	      "and the reason is kept, because \"stopped rendering\" alone helps nobody");

	// Both texture faults are properties of how the texture was created, so
	// the next one will be the same.
	SubmitPolicy wrongDevice;
	Check(wrongDevice.Observe(codes::kCompositorErrorTextureIsOnWrongDevice) ==
	          SubmitDecision::StopRendering,
	      "a texture from the wrong device stops rendering immediately");

	SubmitPolicy badFormat;
	Check(badFormat.Observe(codes::kCompositorErrorTextureUsesUnsupportedFormat) ==
	          SubmitDecision::StopRendering,
	      "an unsupported format stops rendering immediately");

	Check(!obvr::render::IsRecoverable(codes::kCompositorErrorIsNotSceneApplication),
	      "none of those three count as recoverable");
}

void TestLostFocusIsWaitedOut() {
	std::printf("Losing focus, which passes on its own - up to a point\n");

	SubmitPolicy policy;

	// Another application took the scene. It may give it back, so the first
	// frames only skip.
	Check(policy.Observe(codes::kCompositorErrorDoNotHaveFocus) == SubmitDecision::Skip,
	      "the first lost frame is skipped rather than fatal");
	Check(!policy.HasStopped(), "and rendering stays on");

	// But not for ever. This is the case that would otherwise hold the game
	// at ten frames a second indefinitely.
	SubmitPolicy patient;
	SubmitDecision last = SubmitDecision::Continue;
	for (UInt32 i = 0; i < SubmitPolicy::kMaxConsecutiveFailures; ++i) {
		last = patient.Observe(codes::kCompositorErrorDoNotHaveFocus);
	}
	Check(last == SubmitDecision::StopRendering,
	      "a long enough run of lost frames gives up rather than dragging the frame rate");
	Check(patient.GetStopReason() == codes::kCompositorErrorDoNotHaveFocus,
	      "and says which condition ran out of patience");

	// The threshold has to be long enough to survive a headset being put
	// down and picked up, and short enough that nobody plays a whole scene
	// at ten frames a second.
	Check(SubmitPolicy::kMaxConsecutiveFailures >= 30,
	      "the threshold is not so short that a brief interruption ends rendering");
	Check(SubmitPolicy::kMaxConsecutiveFailures <= 600,
	      "and not so long that a stuck compositor is endured for minutes");
}

void TestRunsOfFailuresMustBeConsecutive() {
	std::printf("A bad frame here and there\n");

	// The decisive property, and the one an implementation counting total
	// failures instead of consecutive ones would get wrong: an occasional
	// lost frame must never accumulate into a shutdown. Over a long session
	// it certainly would.
	SubmitPolicy policy;
	for (int round = 0; round < 100; ++round) {
		for (UInt32 i = 0; i < SubmitPolicy::kMaxConsecutiveFailures - 1; ++i) {
			policy.Observe(codes::kCompositorErrorDoNotHaveFocus);
		}
		policy.Observe(codes::kCompositorErrorNone);
	}

	Check(!policy.HasStopped(),
	      "failures separated by a good frame never add up to a shutdown");
	Check(policy.GetConsecutiveFailures() == 0, "one good frame clears the run");
}

void TestStoppingIsFinal() {
	std::printf("Once it has given up\n");

	SubmitPolicy policy;
	policy.Observe(codes::kCompositorErrorIsNotSceneApplication);

	// Nothing here turns rendering back on. Everything that reaches a stop is
	// either permanent or has already been waited out, and a policy that
	// resumed by itself would go back to blocking the game thread on the very
	// call it just escaped.
	Check(policy.Observe(codes::kCompositorErrorNone) == SubmitDecision::StopRendering,
	      "even a successful frame does not restart rendering");
	Check(policy.HasStopped(), "it stays stopped");

	policy.Reset();
	Check(!policy.HasStopped(), "only an explicit reset clears it");
	Check(policy.GetStopReason() == 0, "and the reason goes with it");
	Check(policy.Observe(codes::kCompositorErrorNone) == SubmitDecision::Continue,
	      "after which a good frame is good again");
}

void TestUnknownCodesAreForgiven() {
	std::printf("Codes this build has never heard of\n");

	// Treating an unfamiliar code as fatal would mean a future SteamVR
	// release could switch OBVR off over something transient. Treating it as
	// recoverable is the forgiving reading, and the consecutive counter stops
	// it becoming an endless one.
	Check(obvr::render::IsRecoverable(9999), "an unknown code is treated as recoverable");

	SubmitPolicy policy;
	Check(policy.Observe(9999) == SubmitDecision::Skip,
	      "so it skips the frame rather than ending rendering");

	SubmitPolicy stubborn;
	SubmitDecision last = SubmitDecision::Continue;
	for (UInt32 i = 0; i < SubmitPolicy::kMaxConsecutiveFailures; ++i) {
		last = stubborn.Observe(9999);
	}
	Check(last == SubmitDecision::StopRendering, "but it is still not endured for ever");
}

}  // namespace

int main() {
	std::printf("OBVR submit policy test\n\n");

	TestSuccessIsBoring();
	std::printf("\n");
	TestPermanentFailuresStopAtOnce();
	std::printf("\n");
	TestLostFocusIsWaitedOut();
	std::printf("\n");
	TestRunsOfFailuresMustBeConsecutive();
	std::printf("\n");
	TestStoppingIsFinal();
	std::printf("\n");
	TestUnknownCodesAreForgiven();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
