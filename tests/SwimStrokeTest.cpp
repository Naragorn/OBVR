// Checks swimming with the arms (vr/SwimStroke.h): a hand that holds the water
// and is pulled through it carries the swimmer the other way; both held hands
// are one stroke; recovery with an open hand, a busy hand, no swimming, walking
// about the room and a stroke turned back pull nothing; the glide eases out; a
// stroke never slows except a clear push back; the stick and the arms never
// go faster than the faster alone.

#include <cstdio>
#include <limits>

#include "vr/SwimStroke.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance) { return a - b < tolerance && b - a < tolerance; }

const float kDt = 1.0f / 90.0f;
const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kSwim = 60.0f;  // the game's swim speed in these tests, units a second

float Speed(const NiPoint3& v) { return math::Sqrt(v.LengthSquared()); }

vr::SwimInput Frame(bool swimming = true) {
	vr::SwimInput in;
	in.swimming = swimming;
	in.dt = kDt;
	in.swimSpeed = kSwim;
	return in;
}

void Hold(vr::SwimInput& in, int hand, const NiPoint3& velocity, bool grip = true, bool busy = false) {
	in.hand[hand].tracked = true;
	in.hand[hand].grip = grip;
	in.hand[hand].busy = busy;
	in.hand[hand].velocity = velocity;
}

// Frames of the same input; the last verdict.
vr::SwimVerdict Run(vr::SwimState& s, const vr::SwimSettings& settings, const vr::SwimInput& in, int frames) {
	vr::SwimVerdict v;
	for (int i = 0; i < frames; ++i) {
		v = vr::StepSwim(s, settings, in);
	}
	return v;
}

void TestStrokeSpeed() {
	std::printf("The stroke's speed\n");
	Check(Near(vr::StrokeSpeed(1.0f, kSwim, 1.0f), 3.0f * kSwim, 0.01f), "the hands at 1 m/s: 3 swim speeds");
	const float light = vr::StrokeSpeed(0.3f, kSwim, 1.0f);
	Check(light > 1.5f * kSwim, "a light stroke (0.3 m/s) already swims faster than the game's swimmer");
	Check(vr::StrokeSpeed(2.0f, kSwim, 1.0f) > vr::StrokeSpeed(1.0f, kSwim, 1.0f), "faster hands swim faster");
	Check(vr::StrokeSpeed(2.0f, kSwim, 1.0f) < 2.0f * vr::StrokeSpeed(1.0f, kSwim, 1.0f),
	      "but less than in step with them");
	Check(Near(vr::StrokeSpeed(20.0f, kSwim, 1.0f), 4.0f * kSwim, 0.01f), "never more than 4 swim speeds");
	Check(vr::StrokeSpeed(0.1f, kSwim, 1.0f) == 0.0f, "under 0.15 m/s is the tracking's wander");
	Check(Near(vr::StrokeSpeed(1.0f, kSwim, 2.0f), 6.0f * kSwim, 0.05f), "the strength scales it");
	Check(vr::StrokeSpeed(1.0f, kSwim, 0.0f) == 0.0f, "strength 0: no stroke");
	Check(vr::StrokeSpeed(1.0f, 0.0f, 1.0f) > 0.0f, "the game's swim speed unread: the fallback");
	Check(vr::StrokeSpeed(kNaN, kSwim, 1.0f) == 0.0f, "a hand speed that is no number: none");
	Check(Near(vr::SwimPowerStep(1.0f), 1.0f, 1e-5f) && Near(vr::SwimPowerStep(0.25f), 0.5946f, 0.001f),
	      "the 0.375 power: 1 and 0.25 (0.5946)");
}

void TestRelativeVelocity() {
	std::printf("Against the head: walking about in the room swims nothing\n");
	const NiPoint3 walk{0.0f, 1.2f, 0.0f};
	const NiPoint3 rel = vr::SwimRelativeVelocity(walk, walk);
	Check(Speed(rel) < 1e-6f, "hand and head moving alike: no hand motion");
	const NiPoint3 stroke = vr::SwimRelativeVelocity(NiPoint3{0.0f, 0.4f, 0.0f}, NiPoint3{0.0f, 1.2f, 0.0f});
	Check(Near(stroke.y, -0.8f, 1e-5f), "a hand slower than the walking head moves back against it");
	const NiPoint3 jump = vr::SwimRelativeVelocity(NiPoint3{0.0f, 0.5f, 0.0f}, NiPoint3{0.0f, 50.0f, 0.0f});
	Check(Near(jump.y, 0.5f, 1e-5f), "a head jump (a recenter) is no head motion: the hand's own stands");
		Check(Speed(vr::SwimRelativeVelocity(NiPoint3{kNaN, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f})) == 0.0f,
	      "a hand that is no number: none");
}

void TestNoSwimming() {
	std::printf("Not swimming\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	vr::SwimInput in = Frame();
	Hold(in, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Check(Speed(Run(s, settings, in, 20).velocity) > 0.0f, "the same stroke swims in the water");
	vr::SwimInput dry = Frame(false);
	Hold(dry, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	const vr::SwimVerdict v = vr::StepSwim(s, settings, dry);
	Check(Speed(v.velocity) == 0.0f && !v.holding && !v.pulling, "out of the water: nothing, the glide gone");
	Check(Speed(s.glide) == 0.0f && !s.caught[0], "and the state cleared");
	vr::SwimSettings off;
	off.enabled = false;
	vr::SwimState s2;
	Check(Speed(Run(s2, off, in, 20).velocity) == 0.0f, "switched off: nothing");
}

void TestOneHand() {
	std::printf("One hand\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	vr::SwimInput in = Frame();
	Hold(in, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	const vr::SwimVerdict first = vr::StepSwim(s, settings, in);
	Check(first.caughtNow[0] && !first.caughtNow[1], "the catch is told once, for that hand");
	Check(!vr::StepSwim(s, settings, in).caughtNow[0], "and not again while it holds");
	const vr::SwimVerdict v = Run(s, settings, in, 10);
	Check(v.holding && v.pulling, "holding and pulling");
	Check(v.velocity.y > 0.0f && Near(v.velocity.x, 0.0f, 1e-3f) && Near(v.velocity.z, 0.0f, 1e-3f),
	      "a hand pulled back swims the player forward");
	Check(Near(Speed(v.velocity), 3.0f * kSwim, 1.0f), "at 3 swim speeds with the hand at 1 m/s");
	Check(v.effort > 0.9f && v.effort <= 1.0f, "the effort is the stroke against the full pull");
	vr::SwimState up;
	vr::SwimInput rise = Frame();
	Hold(rise, 1, NiPoint3{0.0f, 0.0f, -0.8f});
	const vr::SwimVerdict w = Run(up, settings, rise, 10);
	Check(w.velocity.z > 0.0f, "a hand pulled down lifts the swimmer: any direction works");
}

void TestTwoHands() {
	std::printf("Two hands\n");
	vr::SwimSettings settings;
	vr::SwimState both;
	vr::SwimInput crawl = Frame();
	Hold(crawl, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Hold(crawl, 1, NiPoint3{0.0f, -1.0f, 0.0f});
	const vr::SwimVerdict v = Run(both, settings, crawl, 10);
	Check(Near(Speed(v.velocity), 3.0f * kSwim, 1.0f), "both hands pulling alike: one stroke, not two");
	vr::SwimState sweep;
	vr::SwimInput out = Frame();
	Hold(out, 0, NiPoint3{1.0f, 0.0f, 0.0f});
	Hold(out, 1, NiPoint3{-1.0f, 0.0f, 0.0f});
	const vr::SwimVerdict o = Run(sweep, settings, out, 20);
	Check(Speed(o.velocity) == 0.0f && !o.pulling, "a breaststroke's sweep out: the hands' mean is nothing");
	vr::SwimState pull;
	vr::SwimInput in = Frame();
	Hold(in, 0, NiPoint3{-0.6f, -1.0f, 0.0f});
	Hold(in, 1, NiPoint3{0.6f, -1.0f, 0.0f});
	const vr::SwimVerdict p = Run(pull, settings, in, 10);
	Check(p.velocity.y > 0.0f && Near(p.velocity.x, 0.0f, 1e-3f),
	      "the pull back with the hands coming together: forward, the sideways part cancelled");
	vr::SwimState one;
	vr::SwimInput single = Frame();
	Hold(single, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Hold(single, 1, NiPoint3{0.0f, 1.0f, 0.0f}, false);
	const vr::SwimVerdict c = Run(one, settings, single, 10);
	Check(c.velocity.y > 0.0f, "the crawl: one hand holds and pulls while the other reaches out open");
	Check(Near(Speed(c.velocity), 3.0f * kSwim, 1.0f), "the open hand's reach does not count against it");
}

void TestRecovery() {
	std::printf("Recovery, busy hands, a stroke turned back\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	vr::SwimInput reach = Frame();
	Hold(reach, 0, NiPoint3{0.0f, 1.0f, 0.0f}, false);
	Check(Speed(Run(s, settings, reach, 30).velocity) == 0.0f, "a hand reaching out with an open grip: nothing");
	vr::SwimState busy;
	vr::SwimInput held = Frame();
	Hold(held, 0, NiPoint3{0.0f, -1.0f, 0.0f}, true, true);
	const vr::SwimVerdict b = Run(busy, settings, held, 30);
	Check(Speed(b.velocity) == 0.0f && !b.holding && !b.caughtNow[0],
	      "a closed hand that holds something (busy): no catch, no pulse");
	vr::SwimState mixed;
	vr::SwimInput two = Frame();
	Hold(two, 0, NiPoint3{0.0f, -1.0f, 0.0f}, true, true);
	Hold(two, 1, NiPoint3{0.0f, -1.0f, 0.0f});
	Check(Near(Speed(Run(mixed, settings, two, 10).velocity), 3.0f * kSwim, 1.0f),
	      "one hand busy: the other still swims");
	vr::SwimState lost;
	vr::SwimInput gone = Frame();
	Hold(gone, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	gone.hand[0].tracked = false;
	Check(Speed(Run(lost, settings, gone, 10).velocity) == 0.0f, "a hand not tracked: nothing");
	// A hand held on through its recovery: the way since the catch is forward of its start.
	vr::SwimState back;
	vr::SwimInput pull = Frame();
	Hold(pull, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Run(back, settings, pull, 12);
	vr::SwimInput recover = Frame();
	Hold(recover, 0, NiPoint3{0.0f, 1.0f, 0.0f});
	const float glide = Speed(back.glide);
	const vr::SwimVerdict r = vr::StepSwim(back, settings, recover);
	Check(!r.pulling, "the same hand turned back with the grip still closed: not counted");
	Check(Speed(r.velocity) > 0.9f * glide && r.velocity.y > 0.0f,
	      "and no push back from it: the swimmer glides on forward");
	// The stroke's way is measured from the catch.
	vr::SwimInput open = Frame();
	Hold(open, 0, NiPoint3{0.0f, 1.0f, 0.0f}, false);
	Run(back, settings, open, 3);
	Hold(pull, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Check(vr::StepSwim(back, settings, pull).pulling, "a new catch is a new stroke: pulling again");
}

void TestGlide() {
	std::printf("The glide\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	vr::SwimInput pull = Frame();
	Hold(pull, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Run(s, settings, pull, 30);
	const float top = Speed(s.glide);
	vr::SwimInput rest = Frame();
	float last = top;
	bool falling = true;
	for (int i = 0; i < 90 * 3; ++i) {
		const float now = Speed(vr::StepSwim(s, settings, rest).velocity);
		falling = falling && now <= last;
		last = now;
	}
	Check(falling && last < 0.7f * top && last > 0.0f, "after the stroke the speed falls steadily and is still gliding after 3 s");
	for (int i = 0; i < 90 * 40; ++i) {
		last = Speed(vr::StepSwim(s, settings, rest).velocity);
	}
	Check(last == 0.0f, "and stops, the swimmer hangs where it ended");
	// Slow glides last: e over six seconds.
	vr::SwimState slow;
	slow.glide = NiPoint3{0.0f, 0.2f * kSwim, 0.0f};
	for (int i = 0; i < 90 * 6; ++i) {
		vr::StepSwim(slow, settings, rest);
	}
	const float ratio = slow.glide.y / (0.2f * kSwim);
	Check(ratio > 0.3f && ratio < 0.42f, "a slow glide falls by roughly e (0.37) over six seconds");
	// Fast ones fall sooner.
	vr::SwimState fast;
	fast.glide = NiPoint3{0.0f, 4.0f * kSwim, 0.0f};
	for (int i = 0; i < 90 * 6; ++i) {
		vr::StepSwim(fast, settings, rest);
	}
	Check(fast.glide.y / (4.0f * kSwim) < ratio, "a fast glide falls more in the same time");
}

void TestNeverSlows() {
	std::printf("A stroke never slows\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	s.glide = NiPoint3{0.0f, 6.0f * kSwim, 0.0f};
	vr::SwimInput weak = Frame();
	Hold(weak, 0, NiPoint3{0.0f, -0.3f, 0.0f});
	const float before = Speed(s.glide);
	const vr::SwimVerdict v = vr::StepSwim(s, settings, weak);
	Check(Speed(v.velocity) <= before && Speed(v.velocity) > 0.95f * before,
	      "a light stroke the same way while gliding fast: the glide only eases, never pulled down to the stroke");
	vr::SwimState turn;
	turn.glide = NiPoint3{0.0f, 3.0f * kSwim, 0.0f};
	vr::SwimInput side = Frame();
	Hold(side, 0, NiPoint3{-1.0f, 0.0f, 0.0f});
	const vr::SwimVerdict t = Run(turn, settings, side, 90);
	Check(t.velocity.x > 0.0f && Near(t.velocity.y, 0.0f, 6.0f), "a stroke sideways turns the glide its way");
}

void TestPushBack() {
	std::printf("A push back\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	s.glide = NiPoint3{0.0f, 3.0f * kSwim, 0.0f};
	vr::SwimInput push = Frame();
	Hold(push, 0, NiPoint3{0.0f, 1.0f, 0.0f});  // a hand pushed forward: the swimmer goes back
	const vr::SwimVerdict v = vr::StepSwim(s, settings, push);
	Check(v.velocity.y < 0.0f, "one hand pushed forward against the glide takes the swimmer back");
	vr::SwimState both;
	both.glide = NiPoint3{0.0f, 3.0f * kSwim, 0.0f};
	vr::SwimInput sweep = Frame();
	Hold(sweep, 0, NiPoint3{2.0f, 0.3f, 0.0f});
	Hold(sweep, 1, NiPoint3{-2.0f, 0.3f, 0.0f});
	const vr::SwimVerdict w = vr::StepSwim(both, settings, sweep);
	Check(w.velocity.y > 0.0f && !w.pulling, "a breaststroke's sweep out, a little forward: more sweep than push, no slowing");
	vr::SwimState firm;
	firm.glide = NiPoint3{0.0f, 3.0f * kSwim, 0.0f};
	vr::SwimInput together = Frame();
	Hold(together, 0, NiPoint3{0.3f, 1.0f, 0.0f});
	Hold(together, 1, NiPoint3{-0.3f, 1.0f, 0.0f});
	Check(vr::StepSwim(firm, settings, together).velocity.y < 0.0f,
	      "both hands pushed forward, more push than sweep: back it goes");
	vr::SwimState rest;
	vr::SwimInput start = Frame();
	Hold(start, 0, NiPoint3{0.0f, 1.0f, 0.0f});
	Check(vr::StepSwim(rest, settings, start).velocity.y < 0.0f, "from rest any stroke swims, a push forward goes back");
}

void TestCombine() {
	std::printf("The stick and the arms\n");
	const NiPoint3 none{0.0f, 0.0f, 0.0f};
	const NiPoint3 stick{0.0f, 2.0f, 0.0f};
	const NiPoint3 arms{0.0f, 5.0f, 0.0f};
	Check(vr::CombineSwimStep(stick, none).y == 2.0f, "the stick alone: its own step");
	Check(vr::CombineSwimStep(none, arms).y == 5.0f, "the arms alone: theirs");
	Check(Near(Speed(vr::CombineSwimStep(stick, arms)), 5.0f, 1e-4f), "both the same way: the faster of the two, not the sum");
	const NiPoint3 side{3.0f, 0.0f, 0.0f};
	const NiPoint3 c = vr::CombineSwimStep(side, arms);
	Check(Speed(c) <= 5.0f + 1e-4f && c.x > 0.0f && c.y > 0.0f, "at an angle: between them, no longer than the faster");
	const NiPoint3 opposite = vr::CombineSwimStep(NiPoint3{0.0f, -2.0f, 0.0f}, arms);
	Check(Near(opposite.y, 3.0f, 1e-4f), "against each other: they cancel as vectors");
}

void TestBusy() {
	std::printf("Which hands are busy\n");
	vr::SwimBusyInput in;
	Check(!vr::SwimHandBusy(in, false) && !vr::SwimHandBusy(in, true), "nothing in either hand: both free");
	in.weaponDrawn = true;
	Check(vr::SwimHandBusy(in, false) && !vr::SwimHandBusy(in, true), "a drawn weapon holds the weapon hand only");
	in.twoHanderOrBow = true;
	Check(vr::SwimHandBusy(in, true), "a two-hander or a bow wants the other hand too");
	vr::SwimBusyInput torch;
	torch.torchHeld = true;
	Check(vr::SwimHandBusy(torch, true) && !vr::SwimHandBusy(torch, false), "a torch holds the other hand");
	vr::SwimBusyInput grab;
	grab.grabHolds = true;
	grab.grabWithLeft = true;
	Check(vr::SwimHandBusy(grab, true) && !vr::SwimHandBusy(grab, false), "a grab holds the hand that took it (left)");
	grab.grabWithLeft = false;
	Check(vr::SwimHandBusy(grab, false) && !vr::SwimHandBusy(grab, true), "and the right");
	vr::SwimBusyInput reach;
	reach.claimed[1] = true;
	Check(vr::SwimHandBusy(reach, true) && !vr::SwimHandBusy(reach, false), "a grip that is a holster's reach is busy");
	reach.claimed[0] = true;
	Check(vr::SwimHandBusy(reach, false), "either hand's");
}

void TestGate() {
	std::printf("When the arms swim\n");
	vr::SwimSettings on;
	vr::SwimGate g;
	g.fullVr = g.inWorld = g.cameraKnown = g.gameSwimming = true;
	Check(vr::SwimAllowed(on, g), "Full VR in the world, the game swimming: on");
	vr::SwimSettings off;
	off.enabled = false;
	Check(!vr::SwimAllowed(off, g), "the setting off: no");
	vr::SwimGate g2 = g;
	g2.fullVr = false;
	Check(!vr::SwimAllowed(on, g2), "the hands not running: no");
	g2 = g;
	g2.inWorld = false;
	Check(!vr::SwimAllowed(on, g2), "not in the world: no");
	g2 = g;
	g2.menuUp = true;
	Check(!vr::SwimAllowed(on, g2), "a menu up: no");
	g2 = g;
	g2.riding = true;
	Check(!vr::SwimAllowed(on, g2), "on a horse: no");
	g2 = g;
	g2.flyCam = true;
	Check(!vr::SwimAllowed(on, g2), "the fly camera: no");
	g2 = g;
	g2.cameraKnown = false;
	Check(!vr::SwimAllowed(on, g2), "the view not known: no");
	g2 = g;
	g2.gameSwimming = false;
	Check(!vr::SwimAllowed(on, g2), "the game does not have the player swimming: no");
	Check(vr::SwimDropsReadyClick(0.0f) && vr::SwimDropsReadyClick(0.4f) && !vr::SwimDropsReadyClick(0.6f),
	      "a ready click is dropped while a hand holds the water and for half a second after");
}

void TestNudge() {
	std::printf("The direction the game is shown\n");
	float x = 0.0f;
	float y = 0.0f;
	vr::SwimNudgeDirection(NiPoint3{0.0f, 50.0f, 0.0f}, 0.0f, x, y);
	Check(Near(x, 0.0f, 1e-5f) && Near(y, 1.0f, 1e-5f), "swimming north, facing north (heading 0): ahead");
	vr::SwimNudgeDirection(NiPoint3{50.0f, 0.0f, 0.0f}, 0.0f, x, y);
	Check(Near(x, 1.0f, 1e-5f) && Near(y, 0.0f, 1e-5f), "swimming east, facing north: to the right");
	vr::SwimNudgeDirection(NiPoint3{50.0f, 0.0f, 0.0f}, math::kHalfPi, x, y);
	Check(Near(x, 0.0f, 1e-4f) && Near(y, 1.0f, 1e-4f), "swimming east, facing east (heading 90 degrees): ahead");
	vr::SwimNudgeDirection(NiPoint3{0.0f, -50.0f, 0.0f}, 0.0f, x, y);
	Check(Near(y, -1.0f, 1e-5f), "swimming back: back");
	vr::SwimNudgeDirection(NiPoint3{0.0f, 0.0f, -80.0f}, 0.0f, x, y);
	Check(x == 0.0f && y == 1.0f, "straight down: ahead (the game just has to see a swimmer)");
	vr::SwimNudgeDirection(NiPoint3{0.0f, 50.0f, 0.0f}, kNaN, x, y);
	Check(x == 0.0f && y == 1.0f, "a heading that is no number: ahead");
}

void TestBadInput() {
	std::printf("Bad input\n");
	vr::SwimSettings settings;
	vr::SwimState s;
	vr::SwimInput in = Frame();
	in.dt = 0.0f;
	Hold(in, 0, NiPoint3{0.0f, -1.0f, 0.0f});
	Check(Speed(Run(s, settings, in, 5).velocity) >= 0.0f, "a frame of no time: no crash, the pull still taken");
	vr::SwimInput bad = Frame();
	Hold(bad, 0, NiPoint3{kNaN, 0.0f, 0.0f});
	vr::SwimState t;
	const vr::SwimVerdict v = vr::StepSwim(t, settings, bad);
	Check(v.velocity.x == v.velocity.x && v.velocity.y == v.velocity.y && v.velocity.z == v.velocity.z,
	      "a hand velocity that is no number leaves no NaN in the swimmer");
}

}  // namespace

int main() {
	TestStrokeSpeed();
	TestRelativeVelocity();
	TestNoSwimming();
	TestOneHand();
	TestTwoHands();
	TestRecovery();
	TestGlide();
	TestNeverSlows();
	TestPushBack();
	TestCombine();
	TestBusy();
	TestGate();
	TestNudge();
	TestBadInput();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
