// Checks the Full VR teleport's pure logic: the stick that aims and lets
// go, the arc, what a landing may be, what it costs, and how the move runs.

#include <cstdio>

#include "vr/Teleport.h"
#include "vr/TeleportGeometry.h"

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

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

constexpr float kFrame = 1.0f / 90.0f;
constexpr float kUnitsPerMetre = 69.99125f;

void TestCone() {
	std::printf("The forward cone\n");
	Check(InTeleportCone(0.0f, 0.8f, 0.8f, 30.0f), "straight forward at the threshold");
	Check(!InTeleportCone(0.0f, 0.79f, 0.8f, 30.0f), "just short of it: no");
	Check(InTeleportCone(0.45f, 0.8f, 0.8f, 30.0f), "29 degrees off forward: yes");
	Check(!InTeleportCone(0.5f, 0.8f, 0.8f, 30.0f), "32 degrees off: a turn");
	Check(!InTeleportCone(0.0f, -0.9f, 0.8f, 30.0f), "back: no");
	Check(!InTeleportCone(0.01f, 0.9f, 0.8f, 0.0f), "a zero cone takes only dead ahead");
	Check(InTeleportCone(5.0f, 0.9f, 0.8f, 120.0f), "a cone past 89 degrees is held at 89");
}

void TestStick() {
	std::printf("The stick\n");
	TeleportSettings s;
	TeleportStickState st;
	TeleportStickVerdict v = StepTeleportStick(st, 0.0f, 0.5f, false, true, kFrame, s);
	Check(!v.aiming && !v.commit && !v.ownsStick, "half forward: nothing, the stick still turns");
	v = StepTeleportStick(st, 0.0f, 0.9f, false, true, kFrame, s);
	Check(v.aiming && v.ownsStick && !v.commit, "pushed forward: aiming, no turning");
	v = StepTeleportStick(st, 0.7f, 0.5f, false, true, kFrame, s);
	Check(v.aiming && v.ownsStick, "swung to the side while aiming: still aiming");
	v = StepTeleportStick(st, 0.1f, 0.1f, false, true, kFrame, s);
	Check(v.commit && !v.aiming && v.ownsStick, "let go: it goes, once");
	v = StepTeleportStick(st, 0.0f, 0.0f, false, true, kFrame, s);
	Check(!v.commit && v.ownsStick, "just after: the spring back does not turn");
	for (int i = 0; i < 30; ++i) {
		v = StepTeleportStick(st, 0.0f, 0.0f, false, true, kFrame, s);
	}
	Check(!v.ownsStick, "a quarter second later the stick turns again");

	std::printf("Cancelling\n");
	st = TeleportStickState{};
	StepTeleportStick(st, 0.0f, 0.9f, false, true, kFrame, s);
	v = StepTeleportStick(st, 0.0f, 0.9f, true, true, kFrame, s);
	Check(!v.aiming && v.ownsStick, "a grip cancels: no arc, but the stick stays its");
	v = StepTeleportStick(st, 0.0f, 0.9f, false, true, kFrame, s);
	Check(!v.aiming, "the grip let go again: still cancelled until released");
	v = StepTeleportStick(st, 0.0f, 0.0f, false, true, kFrame, s);
	Check(!v.commit, "released after a cancel: nothing");
	st = TeleportStickState{};
	StepTeleportStick(st, 0.0f, 0.9f, false, true, kFrame, s);
	v = StepTeleportStick(st, 0.0f, 0.9f, false, false, kFrame, s);
	Check(!v.aiming && !st.aiming, "no longer allowed (a menu): the aim is dropped");
	v = StepTeleportStick(st, 0.0f, 0.0f, false, true, kFrame, s);
	Check(!v.commit, "and nothing goes on the release");

	std::printf("Off, and a broken stick\n");
	TeleportSettings off = s;
	off.enabled = false;
	st = TeleportStickState{};
	v = StepTeleportStick(st, 0.0f, 1.0f, false, true, kFrame, off);
	Check(!v.aiming && !v.ownsStick, "switched off: the stick is the turn's");
	st = TeleportStickState{};
	volatile float zero = 0.0f;
	v = StepTeleportStick(st, zero / zero, zero / zero, false, true, kFrame, s);
	Check(!v.aiming && !v.commit, "a NaN stick is a centred stick");
	st = TeleportStickState{};
	v = StepTeleportStick(st, 0.6f, 0.85f, false, true, kFrame, s);
	Check(!v.aiming && !v.ownsStick, "pushed diagonally, as in a turn: no teleport");
}

void TestArc() {
	std::printf("The arc\n");
	const float g = kArcGravityMetres * kUnitsPerMetre;
	const float range = 4.0f * kUnitsPerMetre;
	const float speed = ArcSpeed(range, g);
	Check(Near(speed, 6.264f * kUnitsPerMetre, 1.0f), "v = sqrt(g R): about 6.26 m/s for 4 m");
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};
	const NiPoint3 forty5{0.0f, 0.70710678f, 0.70710678f};
	const float flight = 2.0f * speed * 0.70710678f / g;
	const NiPoint3 down = ArcPoint(origin, forty5, speed, g, flight);
	Check(Near(down.y, range, 0.5f) && Near(down.z, 0.0f, 0.5f),
	      "thrown at 45 degrees it comes down at the range");
	const NiPoint3 top = ArcPoint(origin, forty5, speed, g, flight * 0.5f);
	Check(top.z > 0.0f && Near(top.y, range * 0.5f, 0.5f), "and is highest half way");
	const float step = ArcTimeStep(range, g);
	Check(Near(step * static_cast<float>(kArcMaxPoints - 1), 2.0f * flight, 0.001f),
	      "the points cover twice the flight");
	Check(ArcSpeed(-5.0f, g) == 0.0f, "a negative range throws nothing");
	Check(ArcSpeed(range, 0.0f) > 0.0f, "no gravity is not a division by zero");
}

LandingQuery FlatLanding() {
	LandingQuery q;
	q.hit = true;
	q.point = NiPoint3{0.0f, 3.0f * kUnitsPerMetre, 0.0f};
	q.normalUp = 1.0f;
	q.feet = NiPoint3{0.0f, 0.0f, 0.0f};
	q.rangeUnits = 4.0f * kUnitsPerMetre;
	q.jumpUnits = 64.0f;
	q.fatigueCost = 30.0f;
	q.fatigueNow = 100.0f;
	return q;
}

void TestLanding() {
	std::printf("Landing\n");
	LandingQuery q = FlatLanding();
	Check(JudgeLanding(q) == TeleportRefusal::None, "flat ground in range: yes");
	q.allowedNow = false;
	Check(JudgeLanding(q) == TeleportRefusal::NotNow, "not now (a menu, combat off): no");
	q = FlatLanding();
	q.hit = false;
	Check(JudgeLanding(q) == TeleportRefusal::NoGround, "nothing hit: no");
	q = FlatLanding();
	q.normalUp = 0.5f;
	Check(JudgeLanding(q) == TeleportRefusal::TooSteep, "a wall or steep slope: no");
	q.normalUp = kLandingMinNormalUp;
	Check(JudgeLanding(q) == TeleportRefusal::None, "45 degrees: yes");
	q = FlatLanding();
	q.point.y = 4.3f * kUnitsPerMetre;
	Check(JudgeLanding(q) == TeleportRefusal::TooFar, "past the range: no");
	q.point.y = 4.15f * kUnitsPerMetre;
	Check(JudgeLanding(q) == TeleportRefusal::None, "a hair past it (the arc's step): yes");

	std::printf("Heights, without and with Blink\n");
	q = FlatLanding();
	q.point.z = 60.0f;
	Check(JudgeLanding(q) == TeleportRefusal::None, "onto a crate a jump reaches: yes");
	q.point.z = 100.0f;
	Check(JudgeLanding(q) == TeleportRefusal::TooHigh, "onto a roof: no");
	q.blink = true;
	Check(JudgeLanding(q) == TeleportRefusal::None, "with Blink: yes");
	q = FlatLanding();
	q.point.y = 1.0f * kUnitsPerMetre;
	q.point.z = -3.9f * kUnitsPerMetre;
	Check(JudgeLanding(q) == TeleportRefusal::None, "down a few metres in range: yes");
	q.rangeUnits = 3.0f * kUnitsPerMetre;
	q.point.y = 0.0f;
	q.point.z = -3.5f * kUnitsPerMetre;
	Check(JudgeLanding(q) == TeleportRefusal::TooFar, "further down than the range and its slack: too far");

	std::printf("Fatigue\n");
	q = FlatLanding();
	q.fatigueNow = 29.0f;
	Check(JudgeLanding(q) == TeleportRefusal::NotEnoughFatigue, "short of the price: locked");
	q.fatigueNow = 30.0f;
	Check(JudgeLanding(q) == TeleportRefusal::None, "exactly the price: yes");
	q.fatigueNow = 0.0f;
	q.fatigueCost = 0.0f;
	Check(JudgeLanding(q) == TeleportRefusal::None, "free (the setting at 0) at no fatigue: yes");
	Check(TeleportRefusalName(TeleportRefusal::TooHigh)[0] != '?', "every refusal has a name");
}

void TestTooLowWithoutBlink() {
	std::printf("Too far down\n");
	LandingQuery q = FlatLanding();
	q.rangeUnits = 10.0f * kUnitsPerMetre;
	q.point = NiPoint3{0.0f, 1.0f * kUnitsPerMetre, -9.0f * kUnitsPerMetre};
	Check(JudgeLanding(q) == TeleportRefusal::None, "nine metres down within a ten metre range");
	q.rangeUnits = 4.0f * kUnitsPerMetre;
	q.point = NiPoint3{0.0f, 0.0f, -4.1f * kUnitsPerMetre};
	q.rangeUnits = 4.0f * kUnitsPerMetre;
	q.feet = NiPoint3{0.0f, 0.0f, 0.0f};
	// Straight down 4.1 m against a 4 m range: inside the 5 % slack, but
	// more than the range down.
	Check(JudgeLanding(q) == TeleportRefusal::TooLow, "just over the range straight down: no");
	q.blink = true;
	Check(JudgeLanding(q) == TeleportRefusal::None, "with Blink: yes");

	std::printf("Something in the way\n");
	q = FlatLanding();
	q.pathClear = false;
	Check(JudgeLanding(q) == TeleportRefusal::Blocked, "a wall between: no");
	q.blink = true;
	Check(JudgeLanding(q) == TeleportRefusal::None, "with Blink the glide goes over it: yes");
	q.blink = false;
	q.pathClear = true;
	Check(JudgeLanding(q) == TeleportRefusal::None, "a clear line: yes");
}

void TestCost() {
	std::printf("The price\n");
	TeleportSettings s;
	Check(Near(TeleportFatigueCost(30.0f, 0.0f, s), 30.0f), "the dodge's price");
	s.fatigueMult = 2.0f;
	Check(Near(TeleportFatigueCost(30.0f, 0.0f, s), 60.0f), "times the setting");
	s.fatigueMult = -1.0f;
	Check(TeleportFatigueCost(30.0f, 0.0f, s) == 0.0f, "a negative setting is free, not a gift");
	s.fatigueMult = 1.0f;
	Check(Near(TeleportFatigueCost(30.0f, 3.0f, s), 30.0f), "a climb costs nothing extra without Blink");
	s.blink = true;
	Check(Near(TeleportFatigueCost(30.0f, 3.0f, s), 60.0f), "with Blink, ten a metre up");
	Check(Near(TeleportFatigueCost(30.0f, -3.0f, s), 30.0f), "and nothing for going down");
	Check(TeleportFatigueCost(-5.0f, 0.0f, s) == 0.0f, "a negative dodge price is none");

	std::printf("The jump's price\n");
	Check(Near(JumpFatigueCost(30.0f, 0.0f, 50.0f, 100.0f, false, 0.5f), 30.0f), "vanilla: 30");
	Check(Near(JumpFatigueCost(30.0f, 0.0f, 50.0f, 100.0f, true, 0.5f), 15.0f),
	      "Acrobatics Expert: half");
	Check(Near(JumpFatigueCost(30.0f, 20.0f, 50.0f, 100.0f, false, 0.5f), 40.0f),
	      "a load multiplier a mod sets counts");
	Check(Near(JumpFatigueCost(30.0f, 20.0f, 50.0f, 0.0f, false, 0.5f), 30.0f),
	      "no capacity: no load term");
	Check(JumpFatigueCost(-30.0f, 0.0f, 0.0f, 100.0f, false, 0.5f) == 0.0f, "never negative");
}

void TestGlide() {
	std::printf("The glide\n");
	TeleportSettings s;
	const NiPoint3 from{0.0f, 0.0f, 0.0f};
	const NiPoint3 to{0.0f, 3.0f * kUnitsPerMetre, 0.0f};
	TeleportMove m = StartTeleport(from, to, s, kUnitsPerMetre);
	Check(m.phase == TeleportPhase::Gliding, "the default is the glide");
	Check(Near(m.duration, 0.2f), "3 m at 15 m/s: 0.2 s");
	TeleportMoveStep step = StepTeleportMove(m, 0.1f);
	Check(step.place && step.invulnerable && !step.finished, "half way: placed, untouchable");
	Check(Near(step.at.y, to.y * 0.5f, 0.01f), "at the middle");
	step = StepTeleportMove(m, 0.15f);
	Check(step.place && step.finished && step.invulnerable, "the end: placed at the target, over");
	Check(Near(step.at.y, to.y, 0.01f), "exactly there");
	step = StepTeleportMove(m, kFrame);
	Check(!step.place && !step.invulnerable, "afterwards: nothing, hurtable again");

	std::printf("The glide's speed limits\n");
	Check(Near(GlideSeconds(kUnitsPerMetre, 1.0f, kUnitsPerMetre), 0.2f), "under 5 m/s is 5");
	Check(Near(GlideSeconds(40.0f * kUnitsPerMetre, 100.0f, kUnitsPerMetre), 1.0f),
	      "over 40 m/s is 40");
	Check(Near(GlideSeconds(0.0f, 15.0f, kUnitsPerMetre), kGlideMinSeconds), "never shorter than a blink");
	volatile float zero = 0.0f;
	Check(Near(GlideSeconds(kUnitsPerMetre, zero / zero, kUnitsPerMetre), 0.2f), "a NaN speed is the slowest");
}

void TestInstant() {
	std::printf("The instant mode\n");
	TeleportSettings s;
	s.instant = true;
	s.fadeSeconds = 0.1f;
	const NiPoint3 from{0.0f, 0.0f, 0.0f};
	const NiPoint3 to{0.0f, 200.0f, 10.0f};
	TeleportMove m = StartTeleport(from, to, s, kUnitsPerMetre);
	Check(m.phase == TeleportPhase::FadingOut, "it starts by fading out");
	TeleportMoveStep step = StepTeleportMove(m, kFrame);
	Check(step.fadeOut && !step.place && step.invulnerable, "the first frame starts the fade");
	step = StepTeleportMove(m, kFrame);
	Check(!step.fadeOut && !step.place, "not twice");
	step = StepTeleportMove(m, 0.1f);
	Check(step.place && step.fadeIn && !step.finished, "dark: moved there, fading back");
	Check(Near(step.at.y, 200.0f) && Near(step.at.z, 10.0f), "straight to the target");
	step = StepTeleportMove(m, 0.05f);
	Check(!step.finished && step.invulnerable, "still fading in, still untouchable");
	step = StepTeleportMove(m, 0.06f);
	Check(step.finished && !step.place, "faded in: over");
	s.fadeSeconds = 0.0f;
	m = StartTeleport(from, to, s, kUnitsPerMetre);
	step = StepTeleportMove(m, kFrame);
	Check(step.fadeOut && step.place && step.fadeIn, "no fade time: all in one frame");
	step = StepTeleportMove(m, kFrame);
	Check(step.finished, "and over the next");
}

NiPoint3 Column(const vr::openvr::HmdMatrix34& m, int c) {
	return NiPoint3{m.m[0][c], m.m[1][c], m.m[2][c]};
}

bool Orthonormal(const vr::openvr::HmdMatrix34& m) {
	const NiPoint3 r = Column(m, 0);
	const NiPoint3 u = Column(m, 1);
	const NiPoint3 f = Column(m, 2);
	const NiPoint3 rxu = Cross(r, u);
	return Near(r.LengthSquared(), 1.0f) && Near(u.LengthSquared(), 1.0f) &&
	       Near(f.LengthSquared(), 1.0f) && Near(Dot(r, u), 0.0f) && Near(Dot(u, f), 0.0f) &&
	       Near(rxu.x, f.x) && Near(rxu.y, f.y) && Near(rxu.z, f.z);
}

void TestGeometry() {
	std::printf("Where the arc and the ring hang\n");
	const NiPoint3 eyes{0.0f, 1.7f, 0.0f};
	vr::openvr::HmdMatrix34 m =
		ArcSegmentMatrix(NiPoint3{0.0f, 1.0f, -1.0f}, NiPoint3{0.0f, 1.0f, -2.0f}, eyes);
	Check(Orthonormal(m), "a piece: a right-handed frame");
	Check(Near(Column(m, 1).z, -1.0f), "its up runs along the piece");
	Check(Near(m.m[2][3], -1.5f) && Near(m.m[1][3], 1.0f), "centred on it");
	Check(Column(m, 2).y > 0.9f, "facing up to the eyes above it");
	m = ArcSegmentMatrix(NiPoint3{0.0f, 1.7f, -1.0f}, NiPoint3{0.0f, 1.7f, -2.0f},
	                     NiPoint3{0.0f, 1.7f, 0.0f});
	Check(Orthonormal(m), "looked at straight along it: still a frame");
	m = ArcSegmentMatrix(NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, eyes);
	Check(Orthonormal(m), "a piece of no length: still a frame");

	m = GroundRingMatrix(NiPoint3{0.0f, 0.0f, -3.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, eyes);
	Check(Orthonormal(m), "the ring: a right-handed frame");
	Check(Near(Column(m, 2).y, 1.0f), "lying on flat ground, facing up");
	Check(Near(Column(m, 1).z, -1.0f), "its up pointing away from the eyes");
	Check(Near(m.m[1][3], kTeleportRingLiftMetres), "a hair above the ground");
	m = GroundRingMatrix(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f},
	                     NiPoint3{0.0f, 1.7f, 0.0f});
	Check(Orthonormal(m), "straight below the eyes: still a frame");
	m = GroundRingMatrix(NiPoint3{0.0f, 0.0f, -3.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, eyes);
	Check(Orthonormal(m) && Near(Column(m, 2).y, 1.0f), "no normal: taken as flat ground");
}

}  // namespace

int main() {
	TestCone();
	TestStick();
	TestArc();
	TestLanding();
	TestTooLowWithoutBlink();
	TestCost();
	TestGlide();
	TestInstant();
	TestGeometry();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
