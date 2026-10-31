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

namespace obvr::test {
extern bool g_stubHeadActive;
extern NiPoint3 g_stubHeadPosition;
}  // namespace obvr::test

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

void TestHeadLookOffIsNeutral() {
	std::printf("Head look off\n");

	// The same fixed angles that turn the camera with head look on turn
	// nothing with it off: the rotation stays the identity and the offset
	// zero, frame after frame, while the source still reads.
	TrackerSettings settings = Fixed(20.0f, 0.0f, 30.0f);
	settings.headLook = false;
	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "head look off: the fixed angles reach nothing");
	tracker.Update(500);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(), "and keep reaching nothing");

	TrackerSettings simulated;
	simulated.source = TrackerSource::Simulated;
	simulated.headLook = false;
	HeadTracker moving;
	moving.Configure(simulated);
	moving.Update(150);
	CheckMatrixNear(moving.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "a moving source with head look off: still nothing");

	// Back on through a reconfigure, the angles arrive again.
	settings.headLook = true;
	tracker.Configure(settings);
	tracker.Update(2);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(20.0f, 0.0f, 30.0f),
	                "head look on again: the fixed angles are back");
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

	// Yaw, because that is what recentring is for: deciding which way is
	// forward. Pitch and roll are posture, and the test below is that they
	// survive it.
	HeadTracker tracker;
	tracker.Configure(Fixed(0.0f, 0.0f, 30.0f));
	tracker.Update(1);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 0.0f, 30.0f),
	                "before recentering the full angle is applied");

	tracker.Recenter();
	tracker.Update(2);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::NiMatrix33::Identity(),
	                "after recentering the same heading is the new zero");

	// Moving on from the new zero has to yield the difference, not the
	// absolute angle.
	tracker.Configure(Fixed(0.0f, 0.0f, 50.0f));
	tracker.Update(3);
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 0.0f, 20.0f),
	                "50 degrees after a zero at 30 leaves 20 degrees");
}

void TestRecenterLeavesTheHorizon() {
	std::printf("Recentering must not tilt the world\n");

	// The fault this guards against was reported from a headset: press the key
	// with the head tilted and the horizon tilts with it, for as long as the
	// reference stands. Every later pose is measured against a tilted zero, so
	// it never settles - and a horizon that is not level is one of the
	// reliable ways to make somebody ill, because the inner ear keeps
	// insisting and the picture keeps disagreeing.
	//
	// So the reference keeps the heading and drops the tilt.
	HeadTracker tracker;
	tracker.Configure(Fixed(0.0f, 25.0f, 0.0f));
	tracker.Update(1);
	tracker.Recenter();
	tracker.Update(2);

	// The roll is still there, because it was never the key's business.
	CheckMatrixNear(tracker.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 25.0f, 0.0f),
	                "a roll of 25 survives recentring");

	HeadTracker pitched;
	pitched.Configure(Fixed(20.0f, 0.0f, 0.0f));
	pitched.Update(1);
	pitched.Recenter();
	pitched.Update(2);
	CheckMatrixNear(pitched.GetCameraRotation(), obvr::EulerToMatrix(20.0f, 0.0f, 0.0f),
	                "and so does a pitch of 20");

	// Heading and tilt together: the heading goes, the tilt stays.
	HeadTracker both;
	both.Configure(Fixed(0.0f, 15.0f, 40.0f));
	both.Update(1);
	both.Recenter();
	both.Update(2);
	CheckMatrixNear(both.GetCameraRotation(), obvr::EulerToMatrix(0.0f, 15.0f, 0.0f),
	                "with both, the heading is zeroed and the roll is kept");
}
void TestReconfigureKeepsTheRecenterReference() {
	std::printf("Hot reload must not undo a recenter\n");

	// The regression this guards against: Configure used to reset the recenter
	// reference on every call, and CameraHook calls it every ReloadEveryFrames.
	// With the fixed and simulated sources that goes unnoticed, because their
	// reference is the identity anyway. With a real headset it would have
	// undone every recenter within two seconds, and the cause would have been
	// very hard to see from inside the headset.
	const TrackerSettings settings = Fixed(0.0f, 0.0f, 30.0f);

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

void TestFollowedLeavesTheRest() {
	std::printf("What the body followed through the room is no longer a camera offset\n");
	TrackerSettings settings;
	settings.source = TrackerSource::OpenVR;
	obvr::test::g_stubHeadActive = true;
	obvr::test::g_stubHeadPosition = {0.0f, 1.7f, 0.0f};
	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);
	obvr::test::g_stubHeadPosition = {0.0f, 1.7f, -0.5f};  // half a metre ahead (OpenVR's -z)
	tracker.Update(2);
	const obvr::NiPoint3 raw = tracker.GetRawOffset();
	const float lead = std::sqrt(raw.x * raw.x + raw.y * raw.y + raw.z * raw.z);
	Check(std::fabs(lead - 0.5f * settings.EffectiveUnitsPerMetre()) < 0.01f, "the raw offset: half a metre");
	tracker.SetFollowed(raw);
	const obvr::NiPoint3 rest = tracker.GetCameraOffset();
	Check(std::fabs(rest.x) < kEpsilon && std::fabs(rest.y) < kEpsilon && std::fabs(rest.z) < kEpsilon,
	      "all of it followed: no camera offset at once, before the next update");
	tracker.Update(3);
	Check(std::fabs(tracker.GetCameraOffset().x) < kEpsilon && std::fabs(tracker.GetCameraOffset().y) < kEpsilon,
	      "and the next update keeps it so");
	tracker.SetFollowed(raw * 0.5f);
	const obvr::NiPoint3 half = tracker.GetCameraOffset();
	Check(std::fabs(std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z) - 0.5f * lead) < 0.01f,
	      "half followed: half left as a lean");
	tracker.Recenter();
	const obvr::NiPoint3 cleared = tracker.GetFollowed();
	Check(cleared.x == 0.0f && cleared.y == 0.0f && cleared.z == 0.0f, "a recenter clears what was followed");
	obvr::test::g_stubHeadActive = false;
}

void TestRecenterTakesTheHeadAsItIsNow() {
	std::printf("Recenter takes the head where it is now, not where the last update saw it\n");

	// Nadi, 2026-10-28/29: floating over the ground in every session. The
	// first start's T-pose recenters from the main menu, where no frame runs
	// Update: the reference was the position read long before, so standing
	// up afterwards read as the head 44 units above its zero and the body was
	// lifted to put its head there. The headset here is the test stub's head.
	TrackerSettings settings;
	settings.source = TrackerSource::OpenVR;
	obvr::test::g_stubHeadActive = true;
	obvr::test::g_stubHeadPosition = {0.0f, 1.1f, 0.0f};  // seated, OpenVR's y up

	HeadTracker tracker;
	tracker.Configure(settings);
	tracker.Update(1);  // the zero taken at the first read

	// Stood up with no Update in between (the main menu), then recentered.
	obvr::test::g_stubHeadPosition = {0.0f, 1.73f, 0.0f};
	tracker.Recenter();
	tracker.Update(2);
	const obvr::NiPoint3 offset = tracker.GetCameraOffset();
	Check(std::fabs(offset.x) < kEpsilon && std::fabs(offset.y) < kEpsilon && std::fabs(offset.z) < kEpsilon,
	      "standing where the recenter was made: no offset");

	// And measured from there: half a metre down is half a metre down.
	obvr::test::g_stubHeadPosition = {0.0f, 1.23f, 0.0f};
	tracker.Update(3);
	Check(std::fabs(tracker.GetCameraOffset().z + 0.5f * settings.EffectiveUnitsPerMetre()) < 0.01f,
	      "half a metre lower reads half a metre lower");

	// No pose at the recenter (no SteamVR here): the last one read stands, as
	// before - the zero moves to 1.23, where the head last was.
	obvr::test::g_stubHeadActive = false;
	tracker.Recenter();
	obvr::test::g_stubHeadActive = true;
	obvr::test::g_stubHeadPosition = {0.0f, 1.73f, 0.0f};
	tracker.Update(4);
	Check(std::fabs(tracker.GetCameraOffset().z - 0.5f * settings.EffectiveUnitsPerMetre()) < 0.01f,
	      "without a pose the recenter keeps the last one read");
	obvr::test::g_stubHeadActive = false;
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
	TestHeadLookOffIsNeutral();
	TestSimulatedMoves();
	std::printf("\n");
	TestRecenter();
	std::printf("\n");
	TestRecenterLeavesTheHorizon();
	std::printf("\n");
	TestReconfigureKeepsTheRecenterReference();
	std::printf("\n");
	TestOpenVrWithoutRuntime();
	std::printf("\n");
	TestRecenterTakesTheHeadAsItIsNow();
	std::printf("\n");
	TestFollowedLeavesTheRest();
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
