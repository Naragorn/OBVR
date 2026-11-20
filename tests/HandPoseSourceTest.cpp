// Checks which pose a controller is drawn with (vr/HandPoseSource.h): this
// frame's render pose - the head's moment - or the pose at the moment of
// asking, in every flow; and the lead the log measures between the two.

#include <cmath>
#include <cstdio>

#include "vr/HandPoseSource.h"

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

bool Near(float a, float b, float tolerance = 1e-4f) { return std::fabs(a - b) <= tolerance; }

HandPoseChoice Ready() {
	HandPoseChoice c;
	c.predict = true;
	c.headFromFrame = true;
	c.frameHasDevices = true;
	c.device = 3;
	c.deviceCount = 64;
	c.frameValid = true;
	return c;
}

void TestChoice() {
	std::printf("Which pose a hand takes\n");
	Check(ChooseHandPose(Ready()) == HandPoseSource::Frame,
	      "the head from this frame, the frame's poses filled, the hand's valid: the frame's render pose");
	HandPoseChoice off = Ready();
	off.predict = false;
	Check(ChooseHandPose(off) == HandPoseSource::Now, "PredictHands=0: the moment of asking");
	HandPoseChoice noHead = Ready();
	noHead.headFromFrame = false;
	Check(ChooseHandPose(noHead) == HandPoseSource::Now,
	      "the head not drawn from this frame (no WaitGetPoses, rendering off): the moment of asking");
	HandPoseChoice noFrame = Ready();
	noFrame.frameHasDevices = false;
	Check(ChooseHandPose(noFrame) == HandPoseSource::Now, "WaitGetPoses failed this frame: the moment of asking");
	HandPoseChoice invalid = Ready();
	invalid.frameValid = false;
	Check(ChooseHandPose(invalid) == HandPoseSource::Now,
	      "the hand's frame pose not valid or not connected: the moment of asking");
	HandPoseChoice past = Ready();
	past.device = 64;
	Check(ChooseHandPose(past) == HandPoseSource::Now, "a device index past the frame's poses: the moment of asking");
	HandPoseChoice last = Ready();
	last.device = 63;
	Check(ChooseHandPose(last) == HandPoseSource::Frame, "the last index the frame holds: its render pose");
	HandPoseChoice none{};
	Check(ChooseHandPose(none) == HandPoseSource::Now, "nothing known: the moment of asking");
}

void TestLead() {
	std::printf("How far ahead the frame's pose is\n");
	float lead = 0.0f;
	// 3 m/s along x, the frame pose 7.5 cm ahead: 25 ms.
	Check(HandLeadSeconds(NiPoint3{1.075f, 1.0f, 0.0f}, NiPoint3{1.0f, 1.0f, 0.0f}, NiPoint3{3.0f, 0.0f, 0.0f}, lead) &&
	          Near(lead, 0.025f),
	      "7.5 cm ahead at 3 m/s: 25 ms");
	Check(HandLeadSeconds(NiPoint3{0.99f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}, lead) &&
	          Near(lead, -0.01f),
	      "behind the moment of asking: a negative lead");
	Check(HandLeadSeconds(NiPoint3{0.033f, 0.05f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}, lead) &&
	          Near(lead, 0.033f),
	      "a way across the motion does not count: only along it (33 ms)");
	lead = 7.0f;
	Check(!HandLeadSeconds(NiPoint3{0.1f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.4f, 0.0f, 0.0f}, lead) &&
	          Near(lead, 7.0f),
	      "slower than half a metre a second: no answer, the lead left alone");
	Check(HandLeadSeconds(NiPoint3{0.0125f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.5f, 0.0f, 0.0f}, lead) &&
	          Near(lead, 0.025f),
	      "exactly half a metre a second: answered");
	const float nan = std::nanf("");
	Check(!HandLeadSeconds(NiPoint3{nan, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}, lead),
	      "a pose that is not a number: no answer");
	Check(!HandLeadSeconds(NiPoint3{0.1f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{nan, 0.0f, 0.0f}, lead),
	      "a velocity that is not a number: no answer");
}

}  // namespace

int main() {
	TestChoice();
	TestLead();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
