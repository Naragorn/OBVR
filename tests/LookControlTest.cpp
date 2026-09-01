// Checks what happens to the look controls once a headset has taken over.
//
// Everything here is about comfort rather than correctness, which is a bad
// reason to leave it untested: a fault does not crash anything and does not
// look wrong in a screenshot. It is felt, half an hour later, by somebody who
// then puts the mod down and cannot say why.
//
// The one that matters most is that the camera no longer tilts. A view that
// tilts while the inner ear insists it is level is the classic trigger, and a
// silent regression there - a matrix that keeps its pitch because the heading
// could not be read, say - would be invisible in every other check.
//
// Pure arithmetic, so this runs on Linux as well.

#include <cstdio>

#include "camera/LookControl.h"
#include "core/Rotation.h"
#include "core/Smoothing.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float actual, float expected, float tolerance) {
	const float difference = actual - expected;
	return difference <= tolerance && difference >= -tolerance;
}

void CheckNear(float actual, float expected, float tolerance, const char* what) {
	if (!Near(actual, expected, tolerance)) {
		std::printf("        expected %.4f, got %.4f\n", static_cast<double>(expected),
		            static_cast<double>(actual));
	}
	Check(Near(actual, expected, tolerance), what);
}

constexpr float kFrame = 1.0f / 60.0f;
constexpr bool kThirdPerson = true;
constexpr bool kFirstPerson = false;

// A camera turned by yaw degrees and tilted by pitch degrees, built the way
// the game builds one: EulerToMatrix with X as pitch and Z as yaw.
obvr::NiMatrix33 Camera(float yawDegrees, float pitchDegrees) {
	return obvr::EulerToMatrix(pitchDegrees, 0.0f, yawDegrees);
}

// How far a rotation tilts, in the same terms the game's matrix holds it.
float TiltOf(const obvr::NiMatrix33& rotation) { return obvr::SinPitchOf(rotation); }

obvr::camera::LookSettings Blocking() {
	obvr::camera::LookSettings settings;
	settings.blockVerticalLook = true;
	// Equal both ways, so that the tests that are not about the split are not
	// quietly testing it too.
	settings.verticalLookUpRange = 60.0f;
	settings.verticalLookDownRange = 60.0f;
	settings.smoothVerticalLook = false;
	settings.smoothTurning = false;
	return settings;
}

void TestTiltIsRemoved() {
	std::printf("The camera stops tilting\n");

	obvr::camera::LookControl control;
	control.Configure(Blocking());

	// 30 degrees up is well within what a stick reaches.
	const obvr::NiMatrix33 tilted = Camera(0.0f, 30.0f);
	CheckNear(TiltOf(tilted), 0.5f, 0.001f, "the input matrix really is tilted 30 degrees");

	control.Update(tilted, kThirdPerson, kFrame);
	CheckNear(TiltOf(control.GetRotation()), 0.0f, 0.001f, "the result is level");

	control.Update(Camera(0.0f, -40.0f), kThirdPerson, kFrame);
	CheckNear(TiltOf(control.GetRotation()), 0.0f, 0.001f, "looking down is levelled too");
}

void TestTurningSurvives() {
	std::printf("Turning still works\n");

	obvr::camera::LookControl control;
	control.Configure(Blocking());

	// Yaw has to come through untouched: it is the only way to face something
	// behind you, and taking it away would make the game unplayable rather
	// than comfortable.
	for (float yaw = -170.0f; yaw <= 170.0f; yaw += 85.0f) {
		const obvr::NiMatrix33 wanted = Camera(yaw, 0.0f);
		control.Update(Camera(yaw, 25.0f), kThirdPerson, kFrame);

		const obvr::NiMatrix33& got = control.GetRotation();
		const bool same = Near(got.data[0][0], wanted.data[0][0], 0.001f) &&
		                  Near(got.data[1][0], wanted.data[1][0], 0.001f);

		std::printf("    yaw %.0f\n", static_cast<double>(yaw));
		Check(same, "the heading survives the levelling");
	}
}

void TestVerticalOffset() {
	std::printf("Where the vertical look goes instead\n");

	obvr::camera::LookControl control;
	control.Configure(Blocking());

	// Third person: the tilt becomes height. sin(30 degrees) is 0.5, so half
	// of the 60 unit range.
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 30.0f, 0.01f, "looking up raises the camera");

	control.Update(Camera(0.0f, -30.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), -30.0f, 0.01f, "looking down lowers it");

	control.Update(Camera(0.0f, 0.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f, "level means no offset");

	// First person: nowhere sensible for it to go, so it does nothing at all.
	// This is the half of the request that is easy to forget, because in third
	// person the feature looks finished without it.
	control.Update(Camera(0.0f, 30.0f), kFirstPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f,
	          "in first person the vertical look does nothing");
}

void TestAimShareIsNotHeight() {
	std::printf("The aim's share of the tilt is not the stick's\n");

	obvr::camera::LookControl control;
	control.Configure(Blocking());

	// The camera tilted 30 degrees up, all of it the stick's: 30 units.
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame, 0.0f);
	CheckNear(control.GetVerticalOffset(), 30.0f, 0.01f, "with no aim share the tilt is all height");

	// The same camera, but the whole tilt is the aim's - a rotX of -30
	// degrees written for a shot at the sky and eased into the camera. Taking
	// it out leaves the stick level, so no height.
	const float thirtyDown = -30.0f * 3.14159265f / 180.0f;
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame, thirtyDown);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f,
	          "a tilt that is entirely the aim's share becomes no height at all");

	// Half of it: the stick's 15 degrees remain, sin(15) times 60.
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame, thirtyDown * 0.5f);
	CheckNear(control.GetVerticalOffset(), 15.529f, 0.01f,
	          "half the share leaves the stick's half of the tilt as height");

	// The other sign: a shot at the floor eased into a camera the stick
	// holds level reads as a tilt down; taking the share out restores level.
	control.Update(Camera(0.0f, -30.0f), kThirdPerson, kFrame, -thirtyDown);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f,
	          "and a share looking down is taken out the same way");

	// The rotation itself is untouched by the share - it is levelled either
	// way, so the share only ever decides the height.
	CheckNear(TiltOf(control.GetRotation()), 0.0f, 0.001f,
	          "the rotation is level whatever the share");
}

void TestRangeSign() {
	std::printf("The range, and its sign\n");

	obvr::camera::LookControl control;
	obvr::camera::LookSettings settings = Blocking();

	// Negative flips it. Oblivion's own third person camera swings vertically
	// as it tilts, and whether that swing agrees with this one is a question
	// for a headset - so the sign has to be reachable from the INI.
	settings.verticalLookUpRange = -60.0f;
	control.Configure(settings);
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), -30.0f, 0.01f, "a negative range flips the direction");

	settings.verticalLookUpRange = 0.0f;
	control.Configure(settings);
	control.Update(Camera(0.0f, 30.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f,
	          "a range of 0 leaves only the game's own swing");
}

void TestAsymmetricRange() {
	std::printf("Different ranges for up and down\n");

	// The camera sits at head height rather than halfway along its travel, so
	// the way down is a whole body long and the way up is bounded by nothing.
	// A single range sized for looking up stops around the hips going down,
	// which is what a real headset reported before this was split in two.
	obvr::camera::LookControl control;
	obvr::camera::LookSettings settings = Blocking();
	settings.verticalLookUpRange = 60.0f;
	settings.verticalLookDownRange = 120.0f;
	control.Configure(settings);

	control.Update(Camera(0.0f, 90.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 60.0f, 0.01f, "full tilt up uses the up range");

	control.Update(Camera(0.0f, -90.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), -120.0f, 0.01f,
	          "full tilt down uses the down range, and still goes down");

	// Half tilt, to show the range scales rather than merely switching.
	control.Update(Camera(0.0f, -30.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), -60.0f, 0.01f, "half tilt down is half the down range");

	// The seam. Level is the one tilt both ranges could claim, and it has to
	// come out at zero either way - otherwise the camera would jump as the
	// stick crossed the middle.
	control.Update(Camera(0.0f, 0.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.001f, "level is zero whichever range applies");

	// One direction switched off while the other stays.
	settings.verticalLookDownRange = 0.0f;
	control.Configure(settings);
	control.Update(Camera(0.0f, -90.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.01f, "a down range of 0 disables only down");
	control.Update(Camera(0.0f, 90.0f), kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 60.0f, 0.01f, "and leaves up working");
}

void TestSwitchedOff() {
	std::printf("Switched off\n");

	obvr::camera::LookControl control;
	obvr::camera::LookSettings settings;
	settings.blockVerticalLook = false;
	control.Configure(settings);

	const obvr::NiMatrix33 tilted = Camera(45.0f, 30.0f);
	control.Update(tilted, kThirdPerson, kFrame);

	CheckNear(TiltOf(control.GetRotation()), 0.5f, 0.001f,
	          "with the feature off the tilt is left alone");
	CheckNear(control.GetVerticalOffset(), 0.0f, 0.001f, "and nothing is added to the height");
}

void TestVerticalSmoothing() {
	std::printf("Smoothing the vertical movement\n");

	obvr::camera::LookControl control;
	obvr::camera::LookSettings settings = Blocking();
	settings.smoothVerticalLook = true;
	settings.verticalLookSpeed = 8.0f;
	control.Configure(settings);

	// Starting from zero, one frame at 60 fps covers 8/60 of the way.
	const obvr::NiMatrix33 lookingUp = Camera(0.0f, 90.0f);
	control.Update(lookingUp, kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 60.0f * 8.0f / 60.0f, 0.1f,
	          "one frame covers speed times delta of the way");

	// And it keeps closing the gap rather than stopping short.
	for (int frame = 0; frame < 300; ++frame) {
		control.Update(lookingUp, kThirdPerson, kFrame);
	}
	CheckNear(control.GetVerticalOffset(), 60.0f, 0.1f, "it arrives given enough frames");

	// Reset drops the eased state, for the moments where continuity would be
	// wrong: a load, a change of view, a recenter.
	control.Reset();
	control.Update(lookingUp, kThirdPerson, kFrame);
	CheckNear(control.GetVerticalOffset(), 60.0f * 8.0f / 60.0f, 0.1f,
	          "after a reset it eases in from zero again");
}

void TestTurnSmoothing() {
	std::printf("Smoothing the turn\n");

	obvr::camera::LookControl control;
	obvr::camera::LookSettings settings = Blocking();
	settings.smoothTurning = true;
	settings.turnSpeed = 12.0f;
	control.Configure(settings);

	// The first frame has nothing to ease from and takes the game's heading
	// as it is. Without this a load would swing the camera round from
	// whatever heading happened to be stored.
	control.Update(Camera(90.0f, 0.0f), kThirdPerson, kFrame);
	const obvr::NiMatrix33 wanted = Camera(90.0f, 0.0f);
	Check(Near(control.GetRotation().data[0][0], wanted.data[0][0], 0.001f),
	      "the first frame lands on the game's heading");

	// Now the game turns instantly. The camera must not.
	control.Update(Camera(0.0f, 0.0f), kThirdPerson, kFrame);
	const float cosine = control.GetRotation().data[0][0];
	Check(cosine > 0.0f && cosine < 0.99f, "an instant turn is followed only part of the way");

	// It gets there in the end.
	for (int frame = 0; frame < 600; ++frame) {
		control.Update(Camera(0.0f, 0.0f), kThirdPerson, kFrame);
	}
	CheckNear(control.GetRotation().data[0][0], 1.0f, 0.001f, "and arrives given enough frames");
	CheckNear(control.GetRotation().data[1][0], 0.0f, 0.001f, "on exactly the right heading");
}

void TestHeadingSmoothingWraps() {
	std::printf("Turning across the seam\n");

	// A heading held as a cosine and a sine has no seam at 360 degrees, which
	// is the whole reason it is stored that way rather than as an angle. Here
	// the target is 10 degrees short of a full turn, so the short way round is
	// backwards - an implementation working on angles would send the camera
	// the long way unless it took special care, which is why UEVR needs a
	// dedicated lerp_angle.
	const obvr::Heading almostFullTurn{0.9848f, -0.1736f};  // -10 degrees
	const obvr::Heading start{1.0f, 0.0f};                  // 0 degrees

	const obvr::Heading stepped = obvr::Approach(start, almostFullTurn, 12.0f, kFrame);

	Check(stepped.sine < 0.0f, "it turns the short way, backwards, not 350 degrees forwards");
	Check(stepped.sine > -0.1736f, "and only part of the way");

	// Two headings exactly opposite have no short way round. Snapping to the
	// target is the only answer that is not arbitrary.
	const obvr::Heading behind{-1.0f, 0.0f};
	const obvr::Heading snapped = obvr::Approach(start, behind, 12.0f, kFrame);
	CheckNear(snapped.cosine, -1.0f, 0.001f, "an exact about-face snaps rather than guessing");

	// A heading always comes back with length one, whatever the step.
	const obvr::Heading eased = obvr::Approach(start, obvr::Heading{0.0f, 1.0f}, 30.0f, kFrame);
	CheckNear(eased.cosine * eased.cosine + eased.sine * eased.sine, 1.0f, 0.001f,
	          "the result is renormalised");
}

}  // namespace

int main() {
	std::printf("OBVR look control test\n\n");

	TestTiltIsRemoved();
	std::printf("\n");
	TestTurningSurvives();
	std::printf("\n");
	TestVerticalOffset();
	std::printf("\n");
	TestAimShareIsNotHeight();
	std::printf("\n");
	TestRangeSign();
	std::printf("\n");
	TestAsymmetricRange();
	std::printf("\n");
	TestSwitchedOff();
	std::printf("\n");
	TestVerticalSmoothing();
	std::printf("\n");
	TestTurnSmoothing();
	std::printf("\n");
	TestHeadingSmoothingWraps();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
