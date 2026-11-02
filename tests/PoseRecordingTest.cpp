#include <cstdio>
#include <limits>
#include <string>

#include "test/HandScript.h"
#include "test/PoseRecording.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.0001f) { return a - b < tolerance && b - a < tolerance; }

using namespace obvr;
using namespace obvr::test;

PoseSample Sample(float dt, float x) {
	PoseSample s;
	s.dt = dt;
	s.headValid = true;
	s.head = vr::Quaternion{0.1f, 0.2f, 0.3f, 0.927f};
	s.headPosition = NiPoint3{x, 1.6f, -0.25f};
	s.right.valid = true;
	s.right.kind = vr::ControllerKind::Index;
	s.right.orientation = vr::Quaternion{0.0f, 0.7071f, 0.0f, 0.7071f};
	s.right.position = NiPoint3{x + 0.2f, 1.1f, -0.4f};
	s.right.velocity = NiPoint3{3.5f, -0.25f, 1.0f};
	s.right.angularVelocity = NiPoint3{0.0f, 6.25f, 0.0f};
	s.right.trigger = 0.75f;
	s.right.thumbX = -0.5f;
	s.right.thumbY = 1.0f;
	s.right.gripPull = 0.9f;
	s.right.gripForce = 0.4f;
	s.right.buttonsPressed = 0x200000004ull;
	s.right.curlValid = true;
	s.right.curl[1] = 0.95f;
	s.right.curl[4] = 0.125f;
	s.right.thumbValid = true;
	s.right.thumb[2] = 0.5f;
	s.left.valid = false;
	s.left.kind = vr::ControllerKind::Unknown;
	return s;
}

void TestLine() {
	std::printf("A frame's line and back\n");
	const PoseSample s = Sample(0.0111f, 0.5f);
	const std::string line = PoseSampleLine(s);
	PoseSample back;
	Check(ParsePoseSample(obvr::test::detail::Tokens(line), back), "the written line reads back");
	Check(Near(back.dt, 0.0111f) && back.headValid && Near(back.head.w, 0.927f) && Near(back.headPosition.x, 0.5f),
	      "the head: its dt, its pose");
	const vr::HandPose& r = back.right;
	Check(r.valid && r.kind == vr::ControllerKind::Index && Near(r.orientation.y, 0.7071f) &&
	          Near(r.position.x, 0.7f),
	      "the right hand's pose and kind");
	Check(Near(r.velocity.x, 3.5f) && Near(r.angularVelocity.y, 6.25f), "SteamVR's own velocities kept, not derived");
	Check(Near(r.trigger, 0.75f) && Near(r.thumbX, -0.5f) && Near(r.thumbY, 1.0f) && Near(r.gripPull, 0.9f) &&
	          Near(r.gripForce, 0.4f),
	      "the trigger, the stick, the grip's pull and force");
	Check(r.buttonsPressed == 0x200000004ull, "the buttons' bits, 64 of them");
	Check(r.curlValid && Near(r.curl[1], 0.95f) && Near(r.curl[4], 0.125f) && r.thumbValid && Near(r.thumb[2], 0.5f),
	      "the fingers' curls and the thumb's joints");
	Check(r.actionInput && r.actionActiveMask == 0x1FF, "read back as the action path reports a bound controller");
	Check(!back.left.valid && back.left.kind == vr::ControllerKind::Unknown, "an untracked hand stays untracked");
	std::vector<std::string> broken = obvr::test::detail::Tokens(line);
	broken.pop_back();
	Check(!ParsePoseSample(broken, back), "a line short of a value is refused");
	broken = obvr::test::detail::Tokens(line);
	broken[5] = "x";
	Check(!ParsePoseSample(broken, back), "a word where a number belongs is refused");
	PoseSample nan = Sample(0.01f, 0.0f);
	nan.right.position.y = std::numeric_limits<float>::quiet_NaN();
	Check(ParsePoseSample(obvr::test::detail::Tokens(PoseSampleLine(nan)), back) && back.right.position.y == 0.0f,
	      "a NaN SteamVR gave is written as 0, so the file always reads back");
}

void TestRing() {
	std::printf("The ring keeps the last seconds\n");
	PoseRing ring;
	for (int i = 0; i < 100; ++i) {
		PushPoseSample(ring, Sample(0.1f, static_cast<float>(i)), 2.0f);
	}
	Check(ring.samples.size() == 20 && Near(ring.seconds, 2.0f, 0.01f), "2 s of 0.1 s frames: the last 20");
	Check(Near(ring.samples.front().headPosition.x, 80.0f) && Near(ring.samples.back().headPosition.x, 99.0f),
	      "the oldest dropped, the newest last");
	PoseRing off;
	PushPoseSample(off, Sample(0.1f, 0.0f), 0.0f);
	Check(off.samples.empty(), "0 seconds kept: nothing recorded");
	PoseRing one;
	PushPoseSample(one, Sample(5.0f, 0.0f), 1.0f);
	Check(one.samples.size() == 1, "a frame longer than the whole keep still stays, alone");
}

void TestScript() {
	std::printf("A recording is a hand script\n");
	PoseRing ring;
	PushPoseSample(ring, Sample(0.01f, 0.0f), 60.0f);
	PushPoseSample(ring, Sample(0.02f, 1.0f), 60.0f);
	const std::string text = PoseRecordingScript(ring, "# a test recording\n") + "right pos 0.2 -0.4 -0.3\nwait 1\n";
	HandScript script;
	ScriptParseError error;
	Check(ParseHandScript(text, script, error), "the saved recording parses as a script");
	Check(script.frames.size() == 2, "its two frames");
	Check(script.expects.size() == 1 && script.rejects.size() == 1, "with the runner's finished expect and warning reject");
	HandScriptRun run;
	HandScriptEvents events;
	StepHandScript(script, run, 12.0f, events);  // the wait 12 and the start mark
	Check(!events.said.empty() && events.said[0]->text == "replay_start", "the start mark");
	StepHandScript(script, run, 0.001f, events);
	Check(run.pose.recorded && run.pose.frame == 0, "the first frame plays");
	StepHandScript(script, run, 0.01f, events);
	Check(run.pose.recorded && run.pose.frame == 1, "its dt over, the second frame");
	StepHandScript(script, run, 0.02f, events);
	Check(!events.said.empty() && events.said[0]->text == "replay_end", "the end mark after the last frame's dt");
	Check(run.pose.recorded, "the last frame stands until a hand or head line");
	StepHandScript(script, run, 1.0f, events);
	Check(!run.pose.recorded, "a hand line ends the recorded pose");
	HandScript bad;
	Check(!ParseHandScript("frame 0.01 1 0 0 0 1\n", bad, error) && error.line == 1, "a frame line cut short: refused");
}

}  // namespace

int main() {
	TestLine();
	TestRing();
	TestScript();
	std::printf(g_failures == 0 ? "All pose recording checks passed.\n" : "%d pose recording check(s) FAILED.\n",
	            g_failures);
	return g_failures == 0 ? 0 : 1;
}
