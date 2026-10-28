// Checks when and where the places' sign shows (vr/HolsterSign.h): a hand has
// to stay at a place before it comes, one hand owns it until it leaves, a
// hint comes at once, it fades in and out, and it hangs low in the view on
// the place's side.

#include <cmath>
#include <cstdio>

#include "vr/HolsterSign.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 1e-3f) { return std::fabs(a - b) < within; }

constexpr float kFrame = 1.0f / 90.0f;

SignTimingInput At(HolsterSlot right, HolsterSlot left) {
	SignTimingInput in;
	in.rightSlot = right;
	in.leftSlot = left;
	in.allowed = true;
	in.seconds = kFrame;
	return in;
}

// Runs `frames` frames of the same input; the last output.
SignTimingOutput Run(SignTimingState& s, const SignTimingInput& in, int frames) {
	SignTimingOutput out;
	for (int i = 0; i < frames; ++i) {
		out = StepSignTiming(s, in);
	}
	return out;
}

int FramesFor(float seconds) { return static_cast<int>(std::ceil(seconds / kFrame)); }

void TestDwell() {
	std::printf("A hand has to stay at a place\n");
	SignTimingState s;
	SignTimingOutput out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignDwellSeconds) - 2);
	Check(!out.visible && out.alpha == 0.0f, "not before the dwell");
	out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::None), 3);
	Check(out.visible && out.slot == HolsterSlot::HipRight && !out.left, "after the dwell: the weapon hand's place");
	Check(out.alpha > 0.0f && out.alpha < 1.0f, "fading in, not at once");
	out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignFadeInSeconds) + 1);
	Check(out.alpha == 1.0f, "faded in");

	SignTimingState passing;
	SignTimingInput in = At(HolsterSlot::HipRight, HolsterSlot::None);
	out = Run(passing, in, FramesFor(kSignDwellSeconds) / 2);
	in.rightSlot = HolsterSlot::None;
	out = Run(passing, in, 2);
	in.rightSlot = HolsterSlot::HipRight;
	out = Run(passing, in, FramesFor(kSignDwellSeconds) / 2);
	Check(!out.visible, "a hand passing through, twice: the dwell starts again, nothing shown");

	SignTimingState moving;
	in = At(HolsterSlot::HipRight, HolsterSlot::None);
	out = Run(moving, in, FramesFor(kSignDwellSeconds) / 2);
	in.rightSlot = HolsterSlot::BackRight;
	out = Run(moving, in, FramesFor(kSignDwellSeconds) / 2 + 1);
	Check(!out.visible, "a hand going on to another place: its dwell starts there anew");
}

void TestOwner() {
	std::printf("One hand owns the sign\n");
	SignTimingState s;
	SignTimingOutput out = Run(s, At(HolsterSlot::None, HolsterSlot::HipLeft), FramesFor(kSignDwellSeconds) + 1);
	Check(out.visible && out.left && out.slot == HolsterSlot::HipLeft, "the other hand first: its place");
	out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::HipLeft), FramesFor(kSignDwellSeconds) * 3);
	Check(out.left && out.slot == HolsterSlot::HipLeft, "the weapon hand arriving later does not take it");

	out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignGraceSeconds) - 2);
	Check(out.left && out.slot == HolsterSlot::HipLeft && out.alpha == 1.0f, "the owner leaving: kept a moment");
	out = Run(s, At(HolsterSlot::HipRight, HolsterSlot::None), 3);
	Check(!out.left && out.slot == HolsterSlot::HipRight, "after the moment: the hand that stayed, already dwelt");

	SignTimingState both;
	out = Run(both, At(HolsterSlot::HipRight, HolsterSlot::HipLeft), FramesFor(kSignDwellSeconds) + 1);
	Check(out.visible && !out.left, "both hands arriving together: the weapon hand's");

	SignTimingState back;
	out = Run(back, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignDwellSeconds + kSignFadeInSeconds) + 2);
	out = Run(back, At(HolsterSlot::None, HolsterSlot::None), 1);
	out = Run(back, At(HolsterSlot::HipRight, HolsterSlot::None), 1);
	Check(out.visible && out.slot == HolsterSlot::HipRight && out.alpha == 1.0f,
	      "the owner back within the moment: kept, no new dwell, no new fade");
}

void TestFadeOut() {
	std::printf("It fades out\n");
	SignTimingState s;
	SignTimingOutput out = Run(s, At(HolsterSlot::BackLeft, HolsterSlot::None), FramesFor(kSignDwellSeconds + kSignFadeInSeconds) + 2);
	Check(out.alpha == 1.0f, "shown");
	out = Run(s, At(HolsterSlot::None, HolsterSlot::None), FramesFor(kSignGraceSeconds) + 2);
	Check(out.visible && out.slot == HolsterSlot::BackLeft && out.alpha < 1.0f && out.alpha > 0.0f,
	      "fading out at the place it showed");
	out = Run(s, At(HolsterSlot::None, HolsterSlot::None), FramesFor(kSignFadeOutSeconds) + 1);
	Check(!out.visible && out.alpha == 0.0f, "gone");

	SignTimingState menu;
	out = Run(menu, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignDwellSeconds + kSignFadeInSeconds) + 2);
	SignTimingInput in = At(HolsterSlot::HipRight, HolsterSlot::None);
	in.allowed = false;
	out = StepSignTiming(menu, in);
	Check(!out.visible && out.alpha == 0.0f, "not allowed (a menu, a load): gone at once");
	in.allowed = true;
	out = StepSignTiming(menu, in);
	Check(!out.visible, "allowed again: a new dwell");

	SignTimingState broken;
	in = At(HolsterSlot::HipRight, HolsterSlot::None);
	in.seconds = std::nanf("");
	out = Run(broken, in, 1000);
	Check(!out.visible, "no time passing (NaN): never shown");
	in.seconds = 5.0f;
	out = Run(broken, in, 1);
	Check(!out.visible, "one long frame counts at most a frame's worth");
}

void TestHint() {
	std::printf("A hint comes at once\n");
	SignTimingState s;
	SignTimingInput in = At(HolsterSlot::None, HolsterSlot::HipRight);
	in.hint = true;
	in.hintSlot = HolsterSlot::HipRight;
	in.hintLeft = true;
	SignTimingOutput out = StepSignTiming(s, in);
	Check(out.visible && out.left && out.slot == HolsterSlot::HipRight, "no dwell for a hint");
	Check(out.alpha == 1.0f, "no fade in either: the reach did something wrong just now");

	SignTimingState owned;
	out = Run(owned, At(HolsterSlot::HipRight, HolsterSlot::None), FramesFor(kSignDwellSeconds) + 1);
	in = At(HolsterSlot::HipRight, HolsterSlot::BackLeft);
	in.hint = true;
	in.hintSlot = HolsterSlot::BackLeft;
	in.hintLeft = true;
	out = StepSignTiming(owned, in);
	Check(out.left && out.slot == HolsterSlot::BackLeft, "a hint takes the sign from the owner");
	in.hint = false;
	out = Run(owned, in, FramesFor(kSignDwellSeconds) * 2);
	Check(out.left && out.slot == HolsterSlot::BackLeft, "and keeps it after, while that hand stays");
}

void TestPlace() {
	std::printf("Where it hangs\n");
	const NiPoint3 eyes{0.0f, 1.6f, 0.0f};
	const NiPoint3 right = SignPlaceInView(Quaternion::Identity(), eyes, HolsterSlot::HipRight);
	const NiPoint3 d = right - eyes;
	const float distance = std::sqrt(d.LengthSquared());
	Check(Near(distance, kSignDistanceMetres), "at reading distance");
	Check(d.z < 0.0f && d.y < 0.0f && d.x > 0.0f, "ahead, below the eyes, on the right for a right place");
	const float down = std::atan2(-d.y, -d.z) * 57.29578f;
	const float across = std::atan2(d.x, -d.z) * 57.29578f;
	Check(Near(down, kSignDownDegrees, 0.5f) && Near(across, kSignAcrossDegrees, 0.5f), "the angles below and across");
	const NiPoint3 left = SignPlaceInView(Quaternion::Identity(), eyes, HolsterSlot::BackLeft);
	Check(left.x < eyes.x && Near(left.y, right.y) && Near(left.z, right.z), "a left place: the mirror");

	// Looking down 50 degrees: the sign comes down with the eyes.
	const Quaternion lookDown = FromAxisAngle(1.0f, 0.0f, 0.0f, -50.0f);
	const NiPoint3 low = SignPlaceInView(lookDown, eyes, HolsterSlot::HipRight) - eyes;
	const float lowDown = std::atan2(-low.y, std::sqrt(low.x * low.x + low.z * low.z)) * 57.29578f;
	Check(lowDown > 50.0f, "it follows the gaze down");

	NiPoint3 at{0.0f, 0.0f, 0.0f};
	const NiPoint3 target{1.0f, 0.0f, 0.0f};
	at = FollowSign(at, target, kSignFollowSeconds, true);
	Check(Near(at.x, 1.0f), "a sign just shown: at its place at once");
	at = NiPoint3{0.0f, 0.0f, 0.0f};
	at = FollowSign(at, target, kFrame, false);
	Check(at.x > 0.0f && at.x < 0.5f, "shown: follows the head softly");
	at = FollowSign(at, target, std::nanf(""), false);
	Check(at.x > 0.0f && at.x < 0.5f, "no time passing: stays");
	at = FollowSign(at, target, 10.0f, false);
	Check(Near(at.x, 1.0f, 0.01f), "a long time: arrived");
}

}  // namespace

int main() {
	TestDwell();
	TestOwner();
	TestFadeOut();
	TestHint();
	TestPlace();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
