// Checks HeadTracker, the layer that turns a head orientation into the matrix
// laid onto Oblivion's camera.
//
// This is where the interesting behaviour lives: which source is read, what
// recentering means, and what happens when a source has nothing to report. All
// of it is checked against EulerToMatrix, the reference verified in the running
// game, so the results stay tied to established evidence.
//
// Windows only, because HeadTracker owns an OpenVRBackend and that calls
// LoadLibrary. The OpenVR source is exercised here in its unavailable state,
// which is the one that matters for anyone without SteamVR running.

#include <cmath>
#include <cstdio>

#include "core/Rotation.h"
#include "vr/HeadTracker.h"

namespace {

int g_failures = 0;

constexpr float kEpsilon = 1e-4f;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool MatrixNear(const obvr::NiMatrix33& a, const obvr::NiMatrix33& b) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			if (std::fabs(a.data[row][col] - b.data[row][col]) > kEpsilon) {
				return false;
			}
		}
	}
	return true;
}

void CheckMatrixNear(const obvr::NiMatrix33& actual, const obvr::NiMatrix33& expected,
                     const char* what) {
	if (MatrixNear(actual, expected)) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s\n", what);
		std::printf("        got      %8.4f %8.4f %8.4f\n",
		            actual.data[0][0], actual.data[0][1], actual.data[0][2]);
		std::printf("        expected %8.4f %8.4f %8.4f\n",
		            expected.data[0][0], expected.data[0][1], expected.data[0][2]);
		++g_failures;
	}
}

using obvr::vr::HeadTracker;
using obvr::vr::TrackerSettings;
using obvr::vr::TrackerSource;

TrackerSettings Fixed(float pitch, float roll, float yaw) {
	TrackerSettings settings;
	settings.source = TrackerSource::Fixed;
	settings.fixedPitch = pitch;
	settings.fixedRoll = roll;
	settings.fixedYaw = yaw;
	return settings;
}

void TestNoneIsNeutral() {
	std::printf("Source none\n");

	TrackerSettings settings;
	settings.source = TrackerSource::None;

	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "none leaves the camera untouched");

	tracker.Update(5000);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "and keeps doing so as frames pass");
}

void TestFixedAnglesUseOblivionAxes() {
	std::printf("Source fixed, angles in Oblivion axes\n");

	// The INI documents FixedPitch/FixedRoll/FixedYaw as being in Oblivion
	// axes. Internally they take the detour through OpenXR convention so that
	// they travel the same path as a real HMD orientation, and the point of
	// these checks is that the detour is invisible from outside.
	HeadTracker pitch;
	pitch.Configure(Fixed(20.0f, 0.0f, 0.0f));
	pitch.Update(1);
	CheckMatrixNear(pitch.GetCameraRotation(), obvr::EulerToMatrix(20.0f, 0.0f, 0.0f),
	                "FixedPitch=20 equals EulerToMatrix(20,0,0)");

	HeadTracker roll;
	roll.Configure(Fixed(0.0f, 20.0f, 0.0f));
	roll.Update(1);
	CheckMatrixNear(roll.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 20.0f, 0.0f),
	                "FixedRoll=20 equals EulerToMatrix(0,20,0)");

	HeadTracker yaw;
	yaw.Configure(Fixed(0.0f, 0.0f, 20.0f));
	yaw.Update(1);
	CheckMatrixNear(yaw.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 0.0f, 20.0f),
	                "FixedYaw=20 equals EulerToMatrix(0,0,20)");
}

void TestSimulatedMoves() {
	std::printf("Source simulated\n");

	TrackerSettings settings;
	settings.source = TrackerSource::Simulated;
	settings.simulatedYawAmplitude = 25.0f;
	settings.simulatedPitchAmplitude = 12.0f;
	settings.simulatedPeriodFrames = 600;

	HeadTracker tracker;
	tracker.Configure(settings);

	// Frame 0 sits at phase 0, where both sine terms vanish.
	tracker.Update(0);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "phase 0 is the rest position");

	tracker.Update(150);
	const obvr::NiMatrix33 quarter = tracker.GetCameraRotation();
	Check(!MatrixNear(quarter, obvr::NiMatrix33::Identity()),
	      "a quarter period in, the camera has moved");

	// Half a period puts the phase at pi, where both sines are zero again.
	// This is not a dropout, it is where the figure crosses its own centre.
	tracker.Update(300);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "half a period returns to the rest position");

	// A period of zero must not divide by zero; the code falls back to 600.
	settings.simulatedPeriodFrames = 0;
	HeadTracker guarded;
	guarded.Configure(settings);
	guarded.Update(300);
	CheckMatrixNear(guarded.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "a period of 0 falls back instead of dividing by zero");
}

void TestRecenter() {
	std::printf("Recenter\n");

	HeadTracker tracker;
	tracker.Configure(Fixed(0.0f, 30.0f, 0.0f));
	tracker.Update(1);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 30.0f, 0.0f),
	                "before recentering the full angle is applied");

	tracker.Recenter();
	tracker.Update(2);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "after recentering the same pose is the new zero");

	// Moving on from the new zero has to yield the difference, not the
	// absolute angle.
	tracker.Configure(Fixed(0.0f, 50.0f, 0.0f));
	tracker.Update(3);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 20.0f, 0.0f),
	                "50 degrees after a zero at 30 leaves 20 degrees");
}

void TestReconfigureKeepsTheRecenterReference() {
	std::printf("Hot reload must not undo a recenter\n");

	// The regression this guards against: Configure used to reset the recenter
	// reference on every call, and CameraHook calls it every ReloadEveryFrames.
	// With the fixed and simulated sources that goes unnoticed, because their
	// reference is the identity anyway. With a real headset it would have
	// undone every recenter within two seconds, and the cause would have been
	// very hard to see from inside the headset.
	const TrackerSettings settings = Fixed(0.0f, 30.0f, 0.0f);

	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);
	tracker.Recenter();
	tracker.Update(2);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "recentered");

	// The same settings again, as a hot reload delivers them.
	tracker.Configure(settings);
	tracker.Update(3);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "reconfiguring with unchanged settings keeps the zero");

	// An actual change of source is a different matter and does reset, because
	// a reference taken from one source means nothing to another.
	TrackerSettings simulated;
	simulated.source = TrackerSource::Simulated;
	tracker.Configure(simulated);
	Check(tracker.GetRawOrientation().w > 1.0f - kEpsilon,
	      "changing the source resets the stored orientation");
}

void TestOpenVrWithoutRuntime() {
	std::printf("Source openvr without SteamVR\n");

	// The state most users meet first. HeadTracker must not invent an
	// orientation, and must not crash.
	TrackerSettings settings;
	settings.source = TrackerSource::OpenVR;

	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "no runtime means the vanilla camera, not a guess");

	tracker.Update(2);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "and it stays that way on later frames");

	// Recentering on a source that reports nothing must also be harmless.
	tracker.Recenter();
	tracker.Update(3);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "recentering without a runtime is harmless");
}

void TestOutputStaysNormalised() {
	std::printf("Normalisation\n");

	// A camera matrix that scales would stretch the whole view. The rows of a
	// pure rotation are unit length, so checking one of them is enough to
	// catch a quaternion that drifted off the unit sphere.
	HeadTracker tracker;
	tracker.Configure(Fixed(13.0f, -27.0f, 61.0f));
	tracker.Update(1);

	const obvr::NiMatrix33& m = tracker.GetCameraRotation();
	const float rowLength = std::sqrt(m.data[0][0] * m.data[0][0] +
	                                  m.data[0][1] * m.data[0][1] +
	                                  m.data[0][2] * m.data[0][2]);
	Check(std::fabs(rowLength - 1.0f) <= kEpsilon, "the camera matrix does not scale");

	Check(std::fabs(tracker.GetRawOrientation().LengthSquared() - 1.0f) <= kEpsilon,
	      "the raw orientation is a unit quaternion");
}

}  // namespace

int main() {
	std::printf("OBVR head tracker test\n\n");

	TestNoneIsNeutral();
	std::printf("\n");
	TestFixedAnglesUseOblivionAxes();
	std::printf("\n");
	TestSimulatedMoves();
	std::printf("\n");
	TestRecenter();
	std::printf("\n");
	TestReconfigureKeepsTheRecenterReference();
	std::printf("\n");
	TestOpenVrWithoutRuntime();
	std::printf("\n");
	TestOutputStaysNormalised();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
