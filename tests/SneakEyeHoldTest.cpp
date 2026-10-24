// Checks the hold of the engine's eye against the sneak's drop
// (camera/SneakEyeHold.h): a real crouch in Full VR put the eyes the head's
// drop and the game's sneak's down (Nadi, 2026-10-24).

#include <cmath>
#include <cstdio>

#include "camera/SneakEyeHold.h"

using namespace obvr;
using camera::SneakEyeEvent;
using camera::SneakEyeHoldInput;
using camera::SneakEyeHoldState;
using camera::StepSneakEyeHold;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) {
	return std::fabs(a - b) < 0.001f;
}

constexpr float kFrame = 1.0f / 75.0f;

SneakEyeHoldInput Frame(bool sneaking, float eye) {
	SneakEyeHoldInput in;
	in.enabled = true;
	in.live = true;
	in.sneaking = sneaking;
	in.eyeAboveFeet = eye;
	in.dt = kFrame;
	return in;
}

// The engine's eye of crouch-camera.txt, 2026-10-24 19:32: frames 793 to 801
// as the sneak begins, 1082 to 1091 as it ends.
constexpr float kDown[] = {117.8f, 117.3f, 114.9f, 110.2f, 103.8f, 98.1f, 94.6f, 93.3f, 93.2f};
constexpr float kUp[] = {93.2f, 93.8f, 96.1f, 100.8f, 107.3f, 113.1f, 116.7f, 117.9f, 118.0f, 118.0f};

void TestStanding() {
	std::printf("Standing: the height is taken, nothing is lifted\n");
	SneakEyeHoldState s;
	const auto v = StepSneakEyeHold(s, Frame(false, 117.9f));
	Check(v.lift == 0.0f && v.event == SneakEyeEvent::None, "no lift, no event");
	Check(s.haveStanding && Near(s.standing, 117.9f), "the engine's eye above the feet is the height held next");
	StepSneakEyeHold(s, Frame(false, 117.8f));
	Check(Near(s.standing, 117.8f), "every standing frame takes it anew");
}

void TestSneakBegins() {
	std::printf("The sneak begins: the drop is taken back frame for frame\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.8f));
	int began = 0;
	bool steady = true;
	for (float eye : kDown) {
		const auto v = StepSneakEyeHold(s, Frame(true, eye));
		began += v.event == SneakEyeEvent::Began ? 1 : 0;
		steady = steady && Near(eye + v.lift, 117.8f);
	}
	Check(began == 1, "one Began, on the sneak's first frame");
	Check(steady, "the eye plus the lift stays at 117.8 through the engine's ease");
	const auto held = StepSneakEyeHold(s, Frame(true, 93.2f));
	Check(Near(held.lift, 24.6f), "sneaking on: the whole drop, 24.6 units, lifted");
	Check(Near(s.standing, 117.8f), "the height held does not move while sneaking");
}

void TestSneakWithoutHeight() {
	std::printf("A sneak with no standing height seen: nothing to hold to\n");
	SneakEyeHoldState s;
	const auto v = StepSneakEyeHold(s, Frame(true, 93.2f));
	Check(v.lift == 0.0f && v.event == SneakEyeEvent::None, "no lift, no Began");
	Check(!s.holding, "not holding");
}

void TestLiftBounds() {
	std::printf("The lift's bounds\n");
	Check(camera::SneakEyeLiftFor(117.9f, 93.2f) > 24.6f, "below the height held: the difference");
	Check(camera::SneakEyeLiftFor(117.9f, 120.0f) == 0.0f, "above it: nothing, the eye is never pushed down");
	Check(camera::SneakEyeLiftFor(117.9f, 117.9f) == 0.0f, "at it: nothing");
	Check(camera::SneakEyeLiftFor(117.9f, 20.0f) == camera::kSneakEyeMaxLiftUnits,
	      "far below: at most kSneakEyeMaxLiftUnits");
	Check(camera::SneakEyeLiftFor(117.9f, std::nanf("")) == 0.0f, "an unread eye: nothing");
}

void TestSneakEnds() {
	std::printf("The sneak ends: the lift follows the engine's eye back up\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.9f));
	StepSneakEyeHold(s, Frame(true, 93.2f));
	bool steady = true;
	SneakEyeEvent last = SneakEyeEvent::None;
	int settled = 0;
	for (float eye : kUp) {
		const auto v = StepSneakEyeHold(s, Frame(false, eye));
		if (v.event == SneakEyeEvent::Settled) {
			Check(v.lift == 0.0f, "the settling frame lifts nothing");
		} else if (settled == 0) {
			steady = steady && Near(eye + v.lift, 117.9f);
		}
		settled += v.event == SneakEyeEvent::Settled ? 1 : 0;
		last = v.event;
	}
	Check(steady, "the eye plus the lift stays at 117.9 through the rise");
	Check(settled == 1, "one Settled, once the eye is within 1.5 units and still");
	Check(last == SneakEyeEvent::None, "standing after: no more events");
	Check(!s.holding && Near(s.standing, 118.0f), "the hold ends; the height is taken anew");
}

void TestSneakAgainDuringRise() {
	std::printf("A sneak again during the rise keeps the height held\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.9f));
	StepSneakEyeHold(s, Frame(true, 93.2f));
	StepSneakEyeHold(s, Frame(false, 100.8f));
	const auto v = StepSneakEyeHold(s, Frame(true, 100.0f));
	Check(v.event == SneakEyeEvent::None, "no second Began");
	Check(Near(v.lift, 17.9f) && Near(s.standing, 117.9f), "lifted to the same 117.9");
	Check(Near(s.release, camera::kSneakEyeReleaseSeconds), "the release time starts over");
}

void TestTimedOut() {
	std::printf("The eye stays low after the sneak: the hold gives up after the release time and fades out\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.9f));
	StepSneakEyeHold(s, Frame(true, 93.2f));
	int frames = 0;
	SneakEyeEvent event = SneakEyeEvent::None;
	float lastLift = -1.0f;
	while (frames < 200) {
		const auto v = StepSneakEyeHold(s, Frame(false, 93.2f));
		++frames;
		if (v.event != SneakEyeEvent::None) {
			event = v.event;
			lastLift = v.lift;
			break;
		}
		Check(frames > 1 || Near(v.lift, 24.7f), "the first frame after: still lifted");
	}
	Check(event == SneakEyeEvent::TimedOut, "TimedOut");
	Check(frames == 75 || frames == 76, "after 1 s of frames at 75 Hz, give or take the float sum's last frame");
	Check(Near(lastLift, 24.7f), "the frame it gives up keeps the lift - no step");
	Check(!s.holding && Near(s.standing, 93.2f), "the low eye is the new height");
	float before = lastLift;
	bool falling = true;
	int fadeFrames = 0;
	while (fadeFrames < 100) {
		const auto v = StepSneakEyeHold(s, Frame(false, 93.2f));
		++fadeFrames;
		falling = falling && v.lift <= before && before - v.lift <= 24.7f / (0.3f * 75.0f) + 0.01f;
		before = v.lift;
		if (v.lift == 0.0f) {
			break;
		}
	}
	Check(falling, "the lift fades, never more than a 0.3 s fade's frame at a time");
	Check(fadeFrames >= 22 && fadeFrames <= 24, "gone after 0.3 s");
	SneakEyeHoldInput bad = Frame(false, 93.2f);
	SneakEyeHoldState t;
	StepSneakEyeHold(t, Frame(false, 117.9f));
	StepSneakEyeHold(t, Frame(true, 93.2f));
	bad.dt = std::nanf("");
	StepSneakEyeHold(t, bad);
	bad.dt = -1.0f;
	StepSneakEyeHold(t, bad);
	Check(t.holding && Near(t.release, camera::kSneakEyeReleaseSeconds), "an unread or negative dt does not run the time");
}

// body-ik.txt, 2026-10-24 19:4x: a sneak begun at 119.9 (the head pitched),
// the eye back at 119.3 after it.
void TestResidualFades() {
	std::printf("The eye back a little under the height held: settled once still, the rest fades\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 119.9f));
	StepSneakEyeHold(s, Frame(true, 98.7f));
	const float rise[] = {94.9f, 97.2f, 101.9f, 108.4f, 114.2f, 117.8f, 119.1f, 119.3f, 119.3f, 119.3f};
	int settled = 0;
	float settleLift = -1.0f;
	bool steady = true;
	for (float eye : rise) {
		const auto v = StepSneakEyeHold(s, Frame(false, eye));
		if (v.event == SneakEyeEvent::Settled) {
			++settled;
			settleLift = v.lift;
		}
		if (settled == 0) {
			steady = steady && Near(eye + v.lift, 119.9f);
		}
	}
	Check(settled == 1, "one Settled, once the eye stopped moving");
	Check(Near(settleLift, 0.6f), "the settling frame keeps the 0.6 left - no step");
	Check(steady, "held at 119.9 until then");
	float before = 0.6f;
	bool falling = true;
	int frames = 0;
	while (frames < 100) {
		const auto v = StepSneakEyeHold(s, Frame(false, 119.3f));
		++frames;
		falling = falling && v.lift <= before;
		before = v.lift;
		if (v.lift == 0.0f) {
			break;
		}
	}
	Check(falling && frames >= 22 && frames <= 24, "the 0.6 fades out over 0.3 s");
}

void TestStillMatters() {
	std::printf("Within the settled band but still rising: held, not settled\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.9f));
	StepSneakEyeHold(s, Frame(true, 93.2f));
	StepSneakEyeHold(s, Frame(false, 113.1f));
	const auto v = StepSneakEyeHold(s, Frame(false, 116.7f));
	Check(v.event == SneakEyeEvent::None && Near(v.lift, 1.2f), "1.2 under, rising 3.6 a frame: lifted 1.2");
	const auto w = StepSneakEyeHold(s, Frame(false, 117.9f));
	Check(w.event == SneakEyeEvent::None && s.holding, "at the height, still rising 1.2: held on");
	const auto x = StepSneakEyeHold(s, Frame(false, 118.0f));
	Check(x.event == SneakEyeEvent::Settled && x.lift == 0.0f, "stopped: settled, nothing left to fade");
}

void TestSneakDuringFade() {
	std::printf("A sneak again while the last lift fades: no drop\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 119.9f));
	StepSneakEyeHold(s, Frame(true, 98.7f));
	StepSneakEyeHold(s, Frame(false, 119.3f));
	const auto settle = StepSneakEyeHold(s, Frame(false, 119.3f));
	Check(settle.event == SneakEyeEvent::Settled, "settled with 0.6 to fade");
	const auto fading = StepSneakEyeHold(s, Frame(false, 119.3f));
	const float shown = 119.3f + fading.lift;
	const auto again = StepSneakEyeHold(s, Frame(true, 119.3f));
	Check(again.event == SneakEyeEvent::Began, "a new Began");
	Check(Near(119.3f + again.lift, shown), "the view stays where the fade had it");
	Check(s.fade == 0.0f, "the fade is folded into the height held");
}

void TestOffAndNotLive() {
	std::printf("Off, not live or unread: no lift, the hold dropped\n");
	for (int which = 0; which < 3; ++which) {
		SneakEyeHoldState s;
		StepSneakEyeHold(s, Frame(false, 117.9f));
		StepSneakEyeHold(s, Frame(true, 93.2f));
		SneakEyeHoldInput in = Frame(true, 93.2f);
		if (which == 0) {
			in.enabled = false;
		} else if (which == 1) {
			in.live = false;
		} else {
			in.eyeAboveFeet = std::nanf("");
		}
		const auto v = StepSneakEyeHold(s, in);
		Check(v.lift == 0.0f && v.event == SneakEyeEvent::Reset,
		      which == 0 ? "off: Reset" : which == 1 ? "not live: Reset" : "the eye unread: Reset");
		Check(!s.holding, "the hold dropped");
		if (which != 0) {
			Check(s.haveStanding, "not live or unread: the standing height kept for when it is live again");
		} else {
			Check(!s.haveStanding, "nothing kept");
			const auto after = StepSneakEyeHold(s, Frame(true, 93.2f));
			Check(after.lift == 0.0f, "a sneak right after: no height to hold to");
		}
	}
	SneakEyeHoldState fading;
	StepSneakEyeHold(fading, Frame(false, 117.9f));
	StepSneakEyeHold(fading, Frame(true, 93.2f));
	for (int i = 0; i < 80; ++i) {
		StepSneakEyeHold(fading, Frame(false, 93.2f));
	}
	SneakEyeHoldInput third = Frame(false, 93.2f);
	third.live = false;
	Check(fading.fade > 0.0f && StepSneakEyeHold(fading, third).event == SneakEyeEvent::Reset,
	      "not live while a lift fades: Reset, the lift gone");
	SneakEyeHoldState idle;
	SneakEyeHoldInput off = Frame(false, 117.9f);
	off.enabled = false;
	Check(StepSneakEyeHold(idle, off).event == SneakEyeEvent::None, "off while not holding: no event");
	SneakEyeHoldState standing;
	StepSneakEyeHold(standing, Frame(false, 117.9f));
	SneakEyeHoldInput away = Frame(false, 117.9f);
	away.live = false;
	Check(StepSneakEyeHold(standing, away).event == SneakEyeEvent::None, "not live while not holding: no event");
}

void TestThirdPersonKeepsHeight() {
	std::printf("Not live a while (third person, riding): the hold drops, the standing height stays; a load forgets it\n");
	SneakEyeHoldState s;
	StepSneakEyeHold(s, Frame(false, 117.9f));
	StepSneakEyeHold(s, Frame(true, 93.2f));
	SneakEyeHoldInput third = Frame(true, 93.2f);
	third.live = false;
	const auto off = StepSneakEyeHold(s, third);
	Check(off.lift == 0.0f && off.event == SneakEyeEvent::Reset, "third person while sneaking: no lift, Reset");
	Check(s.haveStanding && Near(s.standing, 117.9f) && !s.holding, "the standing height kept");
	const auto back = StepSneakEyeHold(s, Frame(true, 93.2f));
	Check(back.event == SneakEyeEvent::Began && Near(back.lift, 24.7f), "first person again, still sneaking: held again");
	SneakEyeHoldInput load = Frame(true, 93.2f);
	load.live = false;
	load.loading = true;
	StepSneakEyeHold(s, load);
	Check(!s.haveStanding, "a load forgets it - the next character's height is its own");
	SneakEyeHoldState t;
	StepSneakEyeHold(t, Frame(false, 117.9f));
	SneakEyeHoldInput disabled = Frame(false, 117.9f);
	disabled.enabled = false;
	StepSneakEyeHold(t, disabled);
	Check(!t.haveStanding, "off forgets it too");
}

void TestWords() {
	std::printf("The log's words\n");
	Check(camera::SneakEyeEventWord(SneakEyeEvent::None) == nullptr, "None: no line");
	Check(camera::SneakEyeEventWord(SneakEyeEvent::Began) != nullptr &&
	          camera::SneakEyeEventWord(SneakEyeEvent::Settled) != nullptr &&
	          camera::SneakEyeEventWord(SneakEyeEvent::TimedOut) != nullptr &&
	          camera::SneakEyeEventWord(SneakEyeEvent::Reset) != nullptr,
	      "every event has its line");
}

}  // namespace

int main() {
	TestStanding();
	TestSneakBegins();
	TestSneakWithoutHeight();
	TestLiftBounds();
	TestSneakEnds();
	TestSneakAgainDuringRise();
	TestTimedOut();
	TestResidualFades();
	TestStillMatters();
	TestSneakDuringFade();
	TestOffAndNotLive();
	TestThirdPersonKeepsHeight();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
