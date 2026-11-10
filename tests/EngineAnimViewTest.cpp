// Checks the eyes held at the body's head through the engine's own third
// person (vr/EngineAnimViewLogic.h).

#include <cstdio>
#include <limits>

#include "vr/EngineAnimViewLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.01f) { return a - b < tolerance && b - a < tolerance; }

using namespace obvr;
using namespace obvr::vr;

EngineAnimViewInput Sitting() {
	EngineAnimViewInput in;
	in.enabled = true;
	in.headset = true;
	in.thirdPerson = true;
	in.sitMountBytes = 0x0001;
	in.sitSleepState = 3;
	return in;
}

void TestWanted() {
	std::printf("when the eyes go to the body's head\n");
	Check(EngineAnimViewWanted(Sitting()), "sitting down in the engine's third person: yes");
	EngineAnimViewInput in = Sitting();
	in.sitMountBytes = 0x0100;
	in.sitSleepState = 5;
	Check(EngineAnimViewWanted(in), "getting up: yes");
	in = Sitting();
	in.sitMountBytes = 0;
	in.sitSleepState = 0;
	in.knockState = 3;
	Check(EngineAnimViewWanted(in), "knocked down (the paralysis fall): yes");
	in.knockState = 5;
	Check(EngineAnimViewWanted(in), "getting up from it: yes");
	in.knockState = -1;
	Check(!EngineAnimViewWanted(in), "the knock state unread and nothing else: no");
	in = Sitting();
	in.sitMountBytes = 0;
	in.sitSleepState = 0;
	Check(!EngineAnimViewWanted(in), "third person with nothing of the engine's (the POV key, a mod): no");
	in = Sitting();
	in.sitSleepState = 4;
	in.sitMountBytes = 0;
	Check(!EngineAnimViewWanted(in), "seated, the engine back in first person's state: no animation of its own");
	in = Sitting();
	in.thirdPerson = false;
	Check(!EngineAnimViewWanted(in), "first person: no");
	in = Sitting();
	in.enabled = false;
	Check(!EngineAnimViewWanted(in), "[Look] FirstPersonAnimations=0: no");
	in = Sitting();
	in.headset = false;
	Check(!EngineAnimViewWanted(in), "no headset: no");
	in = Sitting();
	in.dead = true;
	Check(!EngineAnimViewWanted(in), "dead: the death view's own");
}

void TestStep() {
	std::printf("the eyes' place through the animation\n");
	EngineAnimViewState s;
	const NiPoint3 head{100.0f, 200.0f, 150.0f};
	NiPoint3 eyes{0.0f, 0.0f, 0.0f};
	Check(!StepEngineAnimView(s, false, head, 0.011f, eyes) && !s.active, "not wanted: nothing, nothing held");
	Check(StepEngineAnimView(s, true, head, 0.011f, eyes), "wanted: the eyes given");
	Check(Near(eyes.x, 100.0f) && Near(eyes.y, 200.0f) && Near(eyes.z, 150.0f - kEyesUnderFaceUnits),
	      "the first frame: at the head at once, the eyes a little under the face's centre");
	const NiPoint3 bobbed{100.0f, 200.0f, 160.0f};
	StepEngineAnimView(s, true, bobbed, 0.011f, eyes);
	Check(eyes.z > 150.0f - kEyesUnderFaceUnits && eyes.z < 160.0f - kEyesUnderFaceUnits - 5.0f,
	      "a 10-unit bob in one frame: damped, under half of it");
	for (int i = 0; i < 90; ++i) {
		StepEngineAnimView(s, true, bobbed, 0.011f, eyes);
	}
	Check(Near(eyes.z, 160.0f - kEyesUnderFaceUnits, 0.05f), "held there a second: caught up");
	const NiPoint3 far{400.0f, 200.0f, 160.0f};
	StepEngineAnimView(s, true, far, 0.011f, eyes);
	Check(Near(eyes.x, 400.0f), "a jump of 300 units (a teleport): taken at once, not swum to");
	Check(!StepEngineAnimView(s, false, far, 0.011f, eyes) && !s.active, "the engine back in first person: let go");
	StepEngineAnimView(s, true, head, 0.011f, eyes);
	Check(Near(eyes.x, 100.0f) && Near(eyes.z, 150.0f - kEyesUnderFaceUnits), "the next animation starts afresh");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!StepEngineAnimView(s, true, NiPoint3{nan, 0.0f, 0.0f}, 0.011f, eyes) && !s.active,
	      "a NaN head: nothing, the engine's camera stays");
}

void TestHeld() {
	std::printf("the hold over the engine's last third-person frames\n");
	EngineAnimViewState s;
	EngineAnimViewInput in = Sitting();
	NiPoint3 eyes{0.0f, 0.0f, 0.0f};
	const NiPoint3 head{1.0f, 2.0f, 3.0f};
	Check(EngineAnimViewHeld(s, in, 0.011f), "the get-up's bytes: held");
	StepEngineAnimView(s, true, head, 0.011f, eyes);
	in.sitMountBytes = 0;
	in.sitSleepState = 0;
	Check(EngineAnimViewHeld(s, in, 0.011f), "the bytes gone, still third person: held on (the engine switches next)");
	in.thirdPerson = false;
	Check(!EngineAnimViewHeld(s, in, 0.011f), "first person again: let go");
	StepEngineAnimView(s, false, head, 0.011f, eyes);
	in.thirdPerson = true;
	Check(!EngineAnimViewHeld(s, in, 0.011f), "a third person afterwards with no reason (the POV key): not held");
	in = Sitting();
	EngineAnimViewHeld(s, in, 0.011f);
	StepEngineAnimView(s, true, head, 0.011f, eyes);
	in.sitMountBytes = 0;
	in.sitSleepState = 0;
	bool held = true;
	for (int i = 0; i < 60; ++i) {
		held = EngineAnimViewHeld(s, in, 0.011f);
	}
	Check(!held, "a third person that stays after the animation: let go after half a second");
	EngineAnimViewState fresh;
	in.thirdPerson = true;
	Check(!EngineAnimViewHeld(fresh, in, 0.011f), "never held: no hold to start from");
	in = Sitting();
	in.dead = true;
	EngineAnimViewState dying;
	dying.active = true;
	dying.holdSeconds = 0.4f;
	Check(!EngineAnimViewHeld(dying, in, 0.011f), "dead in the middle of it: let go");
}

void TestMoment() {
	std::printf("the moment, in either view\n");
	EngineAnimMomentState m;
	EngineAnimViewInput in = Sitting();
	in.enabled = false;
	Check(EngineAnimMomentHeld(m, in, 0.011f), "FirstPersonAnimations=0 (third person for it): still the moment");
	in.enabled = true;
	Check(EngineAnimMomentHeld(m, in, 0.011f), "FirstPersonAnimations=1: the moment");
	in.sitMountBytes = 0;
	in.sitSleepState = 0;
	Check(EngineAnimMomentHeld(m, in, 0.011f), "the reasons gone, still third person: held on");
	bool held = true;
	for (int i = 0; i < 60; ++i) {
		held = EngineAnimMomentHeld(m, in, 0.011f);
	}
	Check(!held && !m.active, "past the hold: over");
	Check(!EngineAnimMomentHeld(m, in, 0.011f), "a third person with no reason afterwards (the POV key): no moment");
	in = Sitting();
	EngineAnimMomentHeld(m, in, 0.011f);
	in.thirdPerson = false;
	Check(!EngineAnimMomentHeld(m, in, 0.011f) && m.holdSeconds == 0.0f, "first person again: over at once");
	in = Sitting();
	in.headset = false;
	Check(!EngineAnimMomentHeld(m, in, 0.011f), "no headset: none");
	in = Sitting();
	in.dead = true;
	Check(!EngineAnimMomentHeld(m, in, 0.011f), "dead: none");
}

void TestVignette() {
	std::printf("the vignette over the moment\n");
	float a = 0.0f;
	Check(StepAnimVignette(a, false, 0.7f, 0.011f) == 0.0f, "no moment: none");
	StepAnimVignette(a, true, 0.7f, 0.1f);
	Check(a > 0.3f && a < 0.4f, "half the fade-in: half the strength");
	StepAnimVignette(a, true, 0.7f, 0.1f);
	StepAnimVignette(a, true, 0.7f, 0.1f);
	Check(Near(a, 0.7f), "held at the strength, never over it");
	StepAnimVignette(a, false, 0.7f, 0.1f);
	Check(a > 0.5f && a < 0.55f, "the moment over: fading out, slower than in");
	for (int i = 0; i < 10; ++i) {
		StepAnimVignette(a, false, 0.7f, 0.1f);
	}
	Check(a == 0.0f, "gone after the fade-out, never below 0");
	a = 0.0f;
	for (int i = 0; i < 5; ++i) {
		StepAnimVignette(a, true, 3.0f, 0.1f);
	}
	Check(a == 1.0f, "a strength over 1: held to 1");
	a = 0.5f;
	Check(StepAnimVignette(a, true, -1.0f, 0.011f) == 0.0f, "a strength below 0: none");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	a = nan;
	Check(StepAnimVignette(a, true, nan, 0.011f) == 0.0f, "not a number: none");
	a = 0.4f;
	Check(StepAnimVignette(a, true, 0.7f, -1.0f) == 0.4f, "a negative frame time: unchanged");
	a = 0.0f;
	StepAnimVignette(a, true, 0.7f, 5.0f);
	Check(a > 0.3f && a < 0.4f, "a long frame (a hitch): counted as a tenth of a second");
}

}  // namespace

int main() {
	TestWanted();
	TestStep();
	TestHeld();
	TestMoment();
	TestVignette();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
