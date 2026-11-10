// Checks the body's frame the holster places live in (vr/BodyFrame.h): a
// head turned or bowed leaves it where it was, a turn held drags it along as
// the torso does, and points go in and out of it unchanged.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/BodyFrame.h"
#include "vr/HolsterPlace.h"

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
bool Near(const NiPoint3& a, const NiPoint3& b, float within = 1e-3f) {
	return Near(a.x, b.x, within) && Near(a.y, b.y, within) && Near(a.z, b.z, within);
}

const game::TorsoYawSettings kFollow{};  // 40 degrees free, 1 s

// A head turned `yaw` degrees left (about tracking's up) and bowed `bow`
// degrees down (about its own right), its eyes where a neck 1.6 m up puts
// them: the eyes turn about the neck, as a real head does.
struct Head {
	Quaternion rot;
	NiPoint3 eyes;
};

Head HeadAt(float yaw, float bow) {
	const Quaternion turn = FromAxisAngle(0.0f, 1.0f, 0.0f, yaw);
	const Quaternion tilt = FromAxisAngle(1.0f, 0.0f, 0.0f, -bow);
	Head h;
	h.rot = (turn * tilt).Normalized();
	const NiPoint3 neck{0.0f, 1.6f, 0.0f};
	h.eyes = neck + ToMatrix(h.rot) * kEyesFromNeck;
	return h;
}

BodyFrame Step(BodyFrameState& s, const Head& h, float dt = 0.011f, bool hold = false) {
	return StepBodyFrame(s, true, h.rot, h.eyes, dt, kFollow, hold);
}

void TestHeldWhilePlacing() {
	std::printf("While the circles are up the heading is held\n");
	BodyFrameState s;
	BodyFrame f = Step(s, HeadAt(0.0f, 0.0f));
	const NiPoint3 hip = FromBodyFrame(f, NiPoint3{0.18f, 0.0f, -0.75f});
	for (int i = 1; i <= 18; ++i) {
		f = Step(s, HeadAt(5.0f * static_cast<float>(i), 0.0f), 0.011f, true);
	}
	for (int i = 0; i < 400; ++i) {
		f = Step(s, HeadAt(90.0f, 60.0f), 0.011f, true);
	}
	Check(Near(FromBodyFrame(f, NiPoint3{0.18f, 0.0f, -0.75f}), hip, 1e-3f),
	      "a quarter turn and a bow held 4.4 s: the hip has not moved");
	// Let go of the hold: the body then turns towards the head as ever,
	// starting from the held heading - no swing round at once.
	const BodyFrame next = Step(s, HeadAt(90.0f, 60.0f));
	const float yaw = std::atan2(-next.forwardX, -next.forwardZ) * 57.29578f;
	Check(yaw > 40.0f && yaw < 60.0f, "the hold let go: dragged to 40 behind the head, not swung round");
}

void TestLevelHeading() {
	std::printf("The head's heading on the level\n");
	float fx = 0.0f;
	float fz = 0.0f;
	LevelHeading(Quaternion::Identity(), fx, fz);
	Check(Near(fx, 0.0f) && Near(fz, -1.0f), "straight ahead: -z");
	for (float bow : {30.0f, 60.0f, 85.0f, 90.0f}) {
		LevelHeading(HeadAt(0.0f, bow).rot, fx, fz);
		char what[96];
		std::snprintf(what, sizeof(what), "bowed %.0f degrees: still ahead, not lost", static_cast<double>(bow));
		Check(Near(fx, 0.0f) && Near(fz, -1.0f), what);
	}
	LevelHeading(HeadAt(0.0f, -90.0f).rot, fx, fz);
	Check(Near(fx, 0.0f) && Near(fz, -1.0f), "looking straight up: still ahead");
	LevelHeading(HeadAt(90.0f, 90.0f).rot, fx, fz);
	Check(Near(fx, -1.0f) && Near(fz, 0.0f), "turned left a quarter and bowed all the way: left");
}

void TestFirstFrame() {
	std::printf("The first frame starts on the head\n");
	BodyFrameState s;
	const Head h = HeadAt(25.0f, 0.0f);
	const BodyFrame f = Step(s, h);
	float fx = 0.0f;
	float fz = 0.0f;
	LevelHeading(h.rot, fx, fz);
	Check(f.valid && Near(f.forwardX, fx) && Near(f.forwardZ, fz), "facing where the head faces");
	Check(Near(f.origin, h.eyes), "at the eyes, the head upright");
}

void TestGlanceStays() {
	std::printf("A glance aside leaves the places where they are\n");
	BodyFrameState s;
	const Head ahead = HeadAt(0.0f, 0.0f);
	BodyFrame f = Step(s, ahead);
	const NiPoint3 hip = FromBodyFrame(f, NiPoint3{0.18f, 0.0f, -0.75f});
	// A look 35 degrees aside (turned over a tenth of a second, as a head
	// turns), held a third of a second: within the free 40 degrees the body
	// has hardly turned, so the hip is about where it was.
	for (int i = 1; i <= 7; ++i) {
		f = Step(s, HeadAt(5.0f * static_cast<float>(i), 0.0f));
	}
	for (int i = 0; i < 30; ++i) {
		f = Step(s, HeadAt(35.0f, 0.0f));
	}
	Check(Near(FromBodyFrame(f, NiPoint3{0.18f, 0.0f, -0.75f}), hip, 0.04f),
	      "looked 35 degrees aside for a third of a second: the hip within 4 cm of its place");
	// Under the old frame (the head's own heading) the same look moved it.
	const NiPoint3 oldWay = BodyToTracking(HeadAt(35.0f, 0.0f).rot, HeadAt(35.0f, 0.0f).eyes, NiPoint3{0.18f, 0.0f, -0.75f});
	const NiPoint3 moved = oldWay - hip;
	Check(moved.LengthSquared() > 0.1f * 0.1f, "the head's heading alone took it more than 10 cm along");
}

void TestBowStays() {
	std::printf("Looking down at the places leaves them where they are\n");
	BodyFrameState s;
	BodyFrame f = Step(s, HeadAt(0.0f, 0.0f));
	const NiPoint3 hipRel{0.18f, 0.0f, -0.75f};
	const NiPoint3 hip = FromBodyFrame(f, hipRel);
	for (float bow : {20.0f, 45.0f, 70.0f, 85.0f, 90.0f}) {
		f = Step(s, HeadAt(0.0f, bow));
		char what[96];
		std::snprintf(what, sizeof(what), "bowed %.0f degrees: the hip stays", static_cast<double>(bow));
		Check(Near(FromBodyFrame(f, hipRel), hip, 1e-3f), what);
	}
	// The old frame: the eyes' own position, which goes forward and down as
	// the head bows about the neck.
	const Head down = HeadAt(0.0f, 80.0f);
	const NiPoint3 oldWay = BodyToTracking(down.rot, down.eyes, hipRel);
	Check((oldWay - hip).LengthSquared() > 0.05f * 0.05f, "the eyes alone took it more than 5 cm along");
}

void TestTurnHeldFollows() {
	std::printf("A turn held takes the body along\n");
	BodyFrameState s;
	BodyFrame f = Step(s, HeadAt(0.0f, 0.0f));
	for (int i = 0; i < 400; ++i) {
		f = Step(s, HeadAt(90.0f, 0.0f));
	}
	float fx = 0.0f;
	float fz = 0.0f;
	LevelHeading(HeadAt(90.0f, 0.0f).rot, fx, fz);
	Check(Near(f.forwardX, fx, 0.02f) && Near(f.forwardZ, fz, 0.02f), "90 degrees held 4.4 s: the body faces there");
	// Past the free 40 degrees at once the body is dragged to 40 behind.
	BodyFrameState t;
	Step(t, HeadAt(0.0f, 0.0f));
	f = StepBodyFrame(t, true, HeadAt(29.0f, 0.0f).rot, HeadAt(29.0f, 0.0f).eyes, 0.0f, kFollow);
	f = StepBodyFrame(t, true, HeadAt(58.0f, 0.0f).rot, HeadAt(58.0f, 0.0f).eyes, 0.0f, kFollow);
	const float bodyYaw = std::atan2(-f.forwardX, -f.forwardZ) * 57.29578f;
	Check(Near(bodyYaw, 18.0f, 0.1f), "turned 58 degrees with no time passing: the body 40 behind, at 18");
}

void TestInvalid() {
	std::printf("No head\n");
	BodyFrameState s;
	BodyFrame f = StepBodyFrame(s, false, Quaternion::Identity(), NiPoint3{}, 0.011f, kFollow);
	Check(!f.valid, "never had one: no frame");
	f = Step(s, HeadAt(10.0f, 0.0f));
	const BodyFrame kept = StepBodyFrame(s, false, Quaternion::Identity(), NiPoint3{}, 0.011f, kFollow);
	Check(kept.valid && Near(kept.origin, f.origin) && Near(kept.forwardX, f.forwardX),
	      "lost for a frame: the last frame kept");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const BodyFrame bad = StepBodyFrame(s, true, Quaternion{nan, 0.0f, 0.0f, 1.0f}, NiPoint3{}, 0.011f, kFollow);
	Check(bad.valid && Near(bad.origin, f.origin), "a head that is not a number: the last frame kept");
}

void TestRoundTrip() {
	std::printf("In and out of the frame\n");
	BodyFrameState s;
	const BodyFrame f = Step(s, HeadAt(123.0f, 30.0f));
	const NiPoint3 p{0.3f, -0.2f, -0.6f};
	Check(Near(ToBodyFrame(f, FromBodyFrame(f, p)), p), "a place out and back in: the same place");
	const NiPoint3 q{1.0f, 0.5f, -2.0f};
	Check(Near(FromBodyFrame(f, ToBodyFrame(f, q)), q), "a point in and back out: the same point");
	BodyFrameState u;
	const BodyFrame up = Step(u, HeadAt(0.0f, 0.0f));
	Check(Near(ToBodyFrame(up, up.origin + NiPoint3{0.0f, 0.0f, -1.0f}), NiPoint3{0.0f, 1.0f, 0.0f}),
	      "a metre ahead of the eyes: forward 1 (x right, y forward, z up)");
	Check(Near(ToBodyFrame(up, up.origin + NiPoint3{1.0f, -0.5f, 0.0f}), NiPoint3{1.0f, 0.0f, -0.5f}),
	      "right and down: x 1, z -0.5");
	// Upright and ahead the frame is the one the places were fitted in (the
	// head's heading at the eyes): fitted places are where they were.
	Check(Near(ToBodyFrame(up, FromBodyFrame(up, p)), BodyRelative(HeadAt(0.0f, 0.0f).rot, HeadAt(0.0f, 0.0f).eyes,
	                                                                 FromBodyFrame(up, p))),
	      "upright, ahead: the same as the head's own frame");
}

void TestPlacingWhileLooking() {
	std::printf("A circle placed while looking around lands where the hand let it go\n");
	BodyFrameState s;
	BodyFrame f = Step(s, HeadAt(0.0f, 0.0f));
	HolsterPlaceState place;
	HolsterSettings settings{};
	HolsterPlaceInput in;
	in.command = HolsterPlaceCommand::Start;
	in.rightValid = true;
	StepHolsterPlace(place, in, settings);
	// The hand at the right hip's circle, in the room.
	const HolsterPlaces start = PlacesOf(settings);
	const NiPoint3 handRoom = FromBodyFrame(f, start.at[1]);
	in = HolsterPlaceInput{};
	in.rightValid = true;
	in.right = ToBodyFrame(f, handRoom);
	in.rightGrip = true;
	HolsterPlaceVerdict v = StepHolsterPlace(place, in, settings);
	Check(v.grabbed && v.circle == 1, "the grip on the right hip takes it");
	// The hand 10 cm forward in the room while the head looks down at it and
	// aside: let go, the circle is 10 cm forward of where it was, in the room.
	const NiPoint3 to = handRoom + NiPoint3{0.0f, 0.0f, -0.10f};
	for (int i = 1; i <= 6; ++i) {
		f = Step(s, HeadAt(5.0f * static_cast<float>(i), 12.0f * static_cast<float>(i)), 0.011f, true);
	}
	in.right = ToBodyFrame(f, to);
	StepHolsterPlace(place, in, settings);
	in.rightGrip = false;
	v = StepHolsterPlace(place, in, settings);
	Check(v.dropped, "let go");
	Check(Near(FromBodyFrame(f, v.places.at[1]), to, 0.01f), "the circle where the hand let it go, in the room");
	for (int i = 1; i <= 10; ++i) {
		f = Step(s, HeadAt(30.0f - 5.0f * static_cast<float>(i), 10.0f), 0.011f, true);
	}
	Check(Near(FromBodyFrame(f, v.places.at[1]), to, 0.01f), "and still there with the head turned the other way");
}

}  // namespace

// Standing, the heading holds through a look aside exactly as the drawn
// body's torso does (game/BodyTorsoLogic.h: within 15 degrees never, past
// it after a second); walking it follows at once.
void TestStandingLook() {
	std::printf("Standing, a look aside leaves the heading as the body's\n");
	BodyFrameState s;
	game::TorsoYawState torso;
	const float dt = 0.011f;
	StepBodyFrame(s, true, HeadAt(0.0f, 0.0f).rot, HeadAt(0.0f, 0.0f).eyes, dt, kFollow, false, false);
	game::StepTorsoYaw(torso, 0.0f, dt, kFollow, false);
	BodyFrame f;
	float body = 0.0f;
	for (int i = 0; i < 60; ++i) {
		const Head h = HeadAt(25.0f, 0.0f);
		f = StepBodyFrame(s, true, h.rot, h.eyes, dt, kFollow, false, false);
		body = 25.0f * 0.01745329f + game::StepTorsoYaw(torso, 25.0f * 0.01745329f, dt, kFollow, false);
	}
	Check(Near(f.forwardX, 0.0f, 1e-4f) && Near(f.forwardZ, -1.0f, 1e-4f),
	      "a look of 25 degrees held 0.66 s standing: the heading has not turned");
	Check(Near(body, 0.0f, 1e-4f), "the drawn body's torso the same");
	for (int i = 0; i < 90; ++i) {
		const Head h = HeadAt(25.0f, 0.0f);
		f = StepBodyFrame(s, true, h.rot, h.eyes, dt, kFollow, false, false);
		body = 25.0f * 0.01745329f + game::StepTorsoYaw(torso, 25.0f * 0.01745329f, dt, kFollow, false);
	}
	Check(Near(math::Atan2(-f.forwardX, -f.forwardZ), body, 1e-3f) && body > 0.0f,
	      "held past the second: both follow, alike");
	BodyFrameState w;
	StepBodyFrame(w, true, HeadAt(0.0f, 0.0f).rot, HeadAt(0.0f, 0.0f).eyes, dt, kFollow, false, true);
	const Head h = HeadAt(25.0f, 0.0f);
	f = StepBodyFrame(w, true, h.rot, h.eyes, dt, kFollow, false, true);
	Check(f.forwardX < 0.0f, "walking: the heading follows at once");
}

int main() {
	TestStandingLook();
	TestLevelHeading();
	TestFirstFrame();
	TestGlanceStays();
	TestBowStays();
	TestTurnHeldFollows();
	TestHeldWhilePlacing();
	TestInvalid();
	TestRoundTrip();
	TestPlacingWhileLooking();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
