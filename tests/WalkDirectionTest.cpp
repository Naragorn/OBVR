// Checks which way the left stick walks (vr/WalkDirection.h): the setting's
// words, the direction each choice picks, the fallbacks, and when the body
// is steered at all.

#include <cmath>
#include <cstdio>

#include "vr/WalkDirection.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

constexpr float kDegree = 0.01745329252f;

WalkYaws AllTracked() {
	WalkYaws y;
	y.headValid = true;
	y.head = 10.0f * kDegree;
	y.rightValid = true;
	y.right = 60.0f * kDegree;
	y.leftValid = true;
	y.left = -30.0f * kDegree;
	return y;
}

void TestWords() {
	std::printf("The setting's words\n");
	WalkDirection d = WalkDirection::Head;
	Check(ParseWalkDirection("blend", d) && d == WalkDirection::HeadAndHand, "blend: head and hand");
	Check(ParseWalkDirection("Right", d) && d == WalkDirection::RightHand, "any case: Right");
	Check(ParseWalkDirection("LEFT", d) && d == WalkDirection::LeftHand, "LEFT");
	Check(ParseWalkDirection("head", d) && d == WalkDirection::Head, "head");
	d = WalkDirection::LeftHand;
	Check(!ParseWalkDirection("hand", d) && d == WalkDirection::LeftHand, "not a word of it: refused, left alone");
	Check(!ParseWalkDirection("heads", d) && !ParseWalkDirection("hea", d), "longer or shorter: refused");
	Check(!ParseWalkDirection("", d) && !ParseWalkDirection(nullptr, d), "empty or none: refused");
	Check(kWalkDirectionDefault == WalkDirection::HeadAndHand, "the default is head and hand (the tester)");
	Check(WalkDirectionFromIndex(0.0f) == WalkDirection::Head && WalkDirectionFromIndex(1.0f) == WalkDirection::RightHand &&
	          WalkDirectionFromIndex(2.0f) == WalkDirection::LeftHand &&
	          WalkDirectionFromIndex(3.0f) == WalkDirection::HeadAndHand,
	      "the row's values in the words' order");
	Check(WalkDirectionFromIndex(2.6f) == WalkDirection::HeadAndHand, "a value between rounds");
	Check(WalkDirectionFromIndex(-1.0f) == kWalkDirectionDefault && WalkDirectionFromIndex(9.0f) == kWalkDirectionDefault,
	      "out of range: the default");
}

void TestTargets() {
	std::printf("The direction each choice picks\n");
	const WalkYaws y = AllTracked();
	float out = 0.0f;
	Check(WalkTargetYaw(WalkDirection::Head, y, out) && Near(out, 10.0f * kDegree), "head: the head");
	Check(WalkTargetYaw(WalkDirection::RightHand, y, out) && Near(out, 60.0f * kDegree), "right: the right hand");
	Check(WalkTargetYaw(WalkDirection::LeftHand, y, out) && Near(out, -30.0f * kDegree), "left: the left hand");
	Check(WalkTargetYaw(WalkDirection::HeadAndHand, y, out) && Near(out, -10.0f * kDegree),
	      "blend: halfway between the head and the stick's (left) hand");
	WalkYaws lefty = y;
	lefty.stickHandRight = true;
	Check(WalkTargetYaw(WalkDirection::HeadAndHand, lefty, out) && Near(out, 35.0f * kDegree),
	      "left-handed: halfway to the right hand, which has the stick");

	std::printf("The fallbacks\n");
	WalkYaws noRight = y;
	noRight.rightValid = false;
	Check(WalkTargetYaw(WalkDirection::RightHand, noRight, out) && Near(out, 10.0f * kDegree),
	      "the right hand not tracked: the head");
	WalkYaws noLeft = y;
	noLeft.leftValid = false;
	Check(WalkTargetYaw(WalkDirection::LeftHand, noLeft, out) && Near(out, 10.0f * kDegree),
	      "the left hand not tracked: the head");
	Check(WalkTargetYaw(WalkDirection::HeadAndHand, noLeft, out) && Near(out, 10.0f * kDegree),
	      "blend without the stick's hand: the head");
	WalkYaws noHead = y;
	noHead.headValid = false;
	Check(WalkTargetYaw(WalkDirection::RightHand, noHead, out) && Near(out, 60.0f * kDegree),
	      "a tracked hand does not need the head");
	out = 5.0f;
	Check(!WalkTargetYaw(WalkDirection::HeadAndHand, noHead, out) && out == 5.0f,
	      "blend without the head: nothing, and nothing written");
	Check(!WalkTargetYaw(WalkDirection::Head, noHead, out), "head not known: nothing");
	WalkYaws none;
	Check(!WalkTargetYaw(WalkDirection::LeftHand, none, out), "nothing tracked: nothing");
	Check(!WalkTargetYaw(static_cast<WalkDirection>(7), noHead, out),
	      "a value not in the list, and no head: nothing");
	Check(WalkTargetYaw(static_cast<WalkDirection>(7), y, out) && Near(out, 10.0f * kDegree),
	      "a value not in the list: the head");

	std::printf("Halfway, the short way round\n");
	Check(Near(HalfwayYaw(170.0f * kDegree, -170.0f * kDegree), 3.14159265f, 1e-3f) ||
	          Near(HalfwayYaw(170.0f * kDegree, -170.0f * kDegree), -3.14159265f, 1e-3f),
	      "across the back: behind, not in front");
	Check(Near(HalfwayYaw(0.0f, 90.0f * kDegree), 45.0f * kDegree), "a quarter apart: the eighth");
	Check(Near(HalfwayYaw(20.0f * kDegree, 200.0f * kDegree), 20.0f * kDegree), "opposite: the first is kept");
}

void TestGate() {
	std::printf("When the body is steered\n");
	Check(WalkSteerWanted(true, true, true, false, true, false), "Full VR, headset, first person, walking: yes");
	Check(!WalkSteerWanted(false, true, true, false, true, false), "not Full VR: no");
	Check(!WalkSteerWanted(true, false, true, false, true, false), "no headset: no");
	Check(!WalkSteerWanted(true, true, false, false, true, false), "third person: no");
	Check(!WalkSteerWanted(true, true, true, true, true, false), "a menu: no");
	Check(!WalkSteerWanted(true, true, true, false, false, false), "standing: no, the body stays where it is");
	Check(!WalkSteerWanted(true, true, true, false, true, true), "the aim turns the body this frame: no");
	Check(!WalkStepWorthWriting(0.0f) && !WalkStepWorthWriting(0.003f) && !WalkStepWorthWriting(-0.003f),
	      "a turn under a quarter degree: not written");
	Check(WalkStepWorthWriting(0.01f) && WalkStepWorthWriting(-0.01f), "a larger one either way: written");
}

}  // namespace

int main() {
	TestWords();
	TestTargets();
	TestGate();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
