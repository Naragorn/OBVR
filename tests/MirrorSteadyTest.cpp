// Checks the mirror's steadied glass (vr/MirrorSteadyLogic.h,
// docs/mirror-spec.md 13): the setting's tuning, the turn between two frames,
// the One Euro step - off, the first frame, a still hand's tremor, a
// deliberate turn and move, a jump taken at once, a frame time of no number -
// and the corners moved to the steadied glass with the margin.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/MirrorSteadyLogic.h"

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

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

NiPoint3 P(float x, float y, float z) { return NiPoint3{x, y, z}; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

constexpr float kFrame = 1.0f / 90.0f;

// The glass at `centre`, turned `degrees` about the vertical (z).
GlassFrame Glass(const NiPoint3& centre, float degrees) {
	GlassFrame f;
	f.centre = centre;
	f.rot = MirrorAxisTurn(P(0.0f, 0.0f, 1.0f), degrees * math::kDegreesToRadians);
	return f;
}

float YawOf(const NiMatrix33& m) { return std::atan2(m.data[1][0], m.data[0][0]) * math::kRadiansToDegrees; }

void TestTuning() {
	std::printf("The setting's tuning\n");
	Check(!MirrorSteadyTuningFor(0).on, "0 is off");
	Check(!MirrorSteadyTuningFor(-5).on, "below 0 is off");
	const MirrorSteadyTuning weak = MirrorSteadyTuningFor(1);
	const MirrorSteadyTuning def = MirrorSteadyTuningFor(kMirrorSteadyDefault);
	const MirrorSteadyTuning strong = MirrorSteadyTuningFor(100);
	Check(weak.on && def.on && strong.on, "1 to 100 are on");
	Check(Near(strong.stillCutoffHz, 0.4f, 0.01f) && Near(strong.margin, 0.10f), "100: 0.4 Hz, a 10 percent margin");
	Check(Near(def.stillCutoffHz, 1.33f, 0.02f) && Near(def.margin, 0.076f), "the default 60: 1.33 Hz, a 7.6 percent margin");
	Check(weak.stillCutoffHz > 7.5f && weak.stillCutoffHz < 8.0f, "1: nearly 8 Hz, nearly nothing held back");
	Check(Near(MirrorSteadyTuningFor(250).stillCutoffHz, strong.stillCutoffHz), "above 100 is 100");
	Check(ClampMirrorSteady(-1) == 0 && ClampMirrorSteady(101) == 100 && ClampMirrorSteady(42) == 42, "the clamp");
}

void TestTurnBetween() {
	std::printf("The turn between two frames\n");
	const NiMatrix33 a = MirrorAxisTurn(P(0.0f, 0.0f, 1.0f), 0.2f);
	const NiMatrix33 b = MirrorAxisTurn(P(0.0f, 0.0f, 1.0f), 0.5f);
	NiPoint3 axis{0.0f, 0.0f, 0.0f};
	Check(Near(TurnBetween(a, b, axis), 0.3f) && NearPoint(axis, P(0.0f, 0.0f, 1.0f)), "0.3 rad about +z");
	Check(Near(TurnBetween(b, a, axis), 0.3f) && NearPoint(axis, P(0.0f, 0.0f, -1.0f)), "and back about -z");
	Check(Near(TurnBetween(a, a, axis), 0.0f) && NearPoint(axis, P(0.0f, 0.0f, 1.0f)), "none: 0, the axis +z");
	const NiMatrix33 tilt = MirrorAxisTurn(P(1.0f, 0.0f, 0.0f), 30.0f * math::kDegreesToRadians);
	Check(Near(GlassTurnDegrees(NiMatrix33::Identity(), tilt), 30.0f, 0.01f), "30 degrees of tilt reads 30");
	const NiMatrix33 half = MirrorAxisTurn(P(0.0f, 1.0f, 0.0f), math::kPi);
	Check(Near(GlassTurnDegrees(NiMatrix33::Identity(), half), 180.0f, 0.05f), "a half turn reads 180");
}

void TestOffAndStart() {
	std::printf("Off, and the first frame\n");
	MirrorSteadyState s;
	GlassFrame out;
	const GlassFrame now = Glass(P(10.0f, 20.0f, 30.0f), 15.0f);
	Check(StepMirrorSteady(s, now, kFrame, MirrorSteadyTuningFor(0), out) == MirrorSteadyStep::Off, "off says so");
	Check(NearPoint(out.centre, now.centre) && !s.have, "off: the glass as it is, nothing kept");
	Check(StepMirrorSteady(s, now, kFrame, MirrorSteadyTuningFor(60), out) == MirrorSteadyStep::Started, "the first frame starts");
	Check(NearPoint(out.centre, now.centre) && Near(YawOf(out.rot), 15.0f, 0.01f), "and takes the glass as it is");
	Check(StepMirrorSteady(s, now, kFrame, MirrorSteadyTuningFor(0), out) == MirrorSteadyStep::Off && !s.have,
	      "turned off again: the state is forgotten");
}

void TestStillHand() {
	std::printf("A still hand's tremor: a few tenths of a degree at 10 Hz\n");
	const MirrorSteadyTuning t = MirrorSteadyTuningFor(kMirrorSteadyDefault);
	MirrorSteadyState s;
	GlassFrame out;
	float worstIn = 0.0f;
	float worstOut = 0.0f;
	float worstMove = 0.0f;
	for (int frame = 0; frame < 270; ++frame) {
		const float time = static_cast<float>(frame) * kFrame;
		const float wobble = std::sin(2.0f * math::kPi * 10.0f * time);
		const GlassFrame now = Glass(P(100.0f + 0.07f * wobble, 50.0f, 80.0f), 0.3f * wobble);
		StepMirrorSteady(s, now, kFrame, t, out);
		if (frame >= 90) {  // past the first second
			worstIn = std::fabs(YawOf(now.rot)) > worstIn ? std::fabs(YawOf(now.rot)) : worstIn;
			worstOut = std::fabs(YawOf(out.rot)) > worstOut ? std::fabs(YawOf(out.rot)) : worstOut;
			const float m = std::fabs(out.centre.x - 100.0f);
			worstMove = m > worstMove ? m : worstMove;
		}
	}
	std::printf("        the glass turned up to %.3f degrees, the steadied up to %.3f; moved %.4f units\n",
	            static_cast<double>(worstIn), static_cast<double>(worstOut), static_cast<double>(worstMove));
	Check(worstIn > 0.29f, "the hand's glass shakes by 0.3 degrees");
	Check(worstOut < 0.3f * 0.25f, "the steadied glass by under a quarter of it");
	Check(worstMove < 0.07f * 0.25f, "and its 1 mm shake of place by under a quarter");
}

void TestDeliberateTurn() {
	std::printf("A deliberate turn: 90 degrees a second for half a second\n");
	const MirrorSteadyTuning t = MirrorSteadyTuningFor(kMirrorSteadyDefault);
	MirrorSteadyState s;
	GlassFrame out;
	float lag = 0.0f;
	for (int frame = 0; frame <= 45; ++frame) {
		const float yaw = 90.0f * static_cast<float>(frame) * kFrame;
		StepMirrorSteady(s, Glass(P(0.0f, 0.0f, 0.0f), yaw), kFrame, t, out);
		lag = yaw - YawOf(out.rot);
	}
	std::printf("        behind by %.2f degrees at the end\n", static_cast<double>(lag));
	Check(lag > 0.0f, "the steadied glass follows behind");
	Check(lag < 3.0f, "within three degrees of a quick turn: a turn on purpose stays immediate");
	for (int frame = 0; frame < 90; ++frame) {
		StepMirrorSteady(s, Glass(P(0.0f, 0.0f, 0.0f), 45.0f), kFrame, t, out);
	}
	Check(Near(YawOf(out.rot), 45.0f, 0.1f), "held still after it, the glass settles where it was turned to");
	Check(Near(Determinant(out.rot), 1.0f), "and stays a rotation");
}

void TestDeliberateMove() {
	std::printf("A deliberate move: half a metre a second\n");
	const MirrorSteadyTuning t = MirrorSteadyTuningFor(kMirrorSteadyDefault);
	MirrorSteadyState s;
	GlassFrame out;
	float lag = 0.0f;
	for (int frame = 0; frame <= 45; ++frame) {
		const float x = 35.0f * static_cast<float>(frame) * kFrame;
		StepMirrorSteady(s, Glass(P(x, 0.0f, 0.0f), 0.0f), kFrame, t, out);
		lag = x - out.centre.x;
	}
	std::printf("        behind by %.2f units at the end\n", static_cast<double>(lag));
	Check(lag > 0.0f && lag < 1.6f, "within the default's margin of a 21-unit glass (1.6 units)");
}

void TestJumps() {
	std::printf("A jump is taken at once\n");
	const MirrorSteadyTuning t = MirrorSteadyTuningFor(100);
	MirrorSteadyState s;
	GlassFrame out;
	StepMirrorSteady(s, Glass(P(0.0f, 0.0f, 0.0f), 0.0f), kFrame, t, out);
	const GlassFrame far = Glass(P(25.0f, 0.0f, 0.0f), 0.0f);
	Check(StepMirrorSteady(s, far, kFrame, t, out) == MirrorSteadyStep::Jumped, "25 units in a frame: a jump");
	Check(NearPoint(out.centre, far.centre), "the glass taken where it is now");
	const GlassFrame turned = Glass(P(25.0f, 0.0f, 0.0f), 40.0f);
	Check(StepMirrorSteady(s, turned, kFrame, t, out) == MirrorSteadyStep::Jumped && Near(YawOf(out.rot), 40.0f, 0.01f),
	      "40 degrees in a frame: a jump, taken as it is");
	const GlassFrame step = Glass(P(26.0f, 0.0f, 0.0f), 41.0f);
	Check(StepMirrorSteady(s, step, kFrame, t, out) == MirrorSteadyStep::Filtered, "a small step after it filters again");
	Check(out.centre.x > 25.0f && out.centre.x < 26.0f, "from the jumped-to place");
	GlassFrame broken = step;
	broken.centre.x = std::numeric_limits<float>::quiet_NaN();
	Check(StepMirrorSteady(s, broken, kFrame, t, out) == MirrorSteadyStep::Jumped, "a centre of no number: a jump, not a poisoned filter");
}

void TestNoTime() {
	std::printf("A frame time of no number\n");
	const MirrorSteadyTuning t = MirrorSteadyTuningFor(60);
	MirrorSteadyState s;
	GlassFrame out;
	StepMirrorSteady(s, Glass(P(0.0f, 0.0f, 0.0f), 0.0f), kFrame, t, out);
	const GlassFrame now = Glass(P(2.0f, 0.0f, 0.0f), 2.0f);
	Check(StepMirrorSteady(s, now, std::numeric_limits<float>::quiet_NaN(), t, out) == MirrorSteadyStep::NoTime,
	      "no number: no time");
	Check(NearPoint(out.centre, now.centre) && NearPoint(s.frame.centre, now.centre), "the glass taken as it is, the filter from there");
	Check(StepMirrorSteady(s, now, 0.0f, t, out) == MirrorSteadyStep::NoTime, "zero seconds: no time");
	Check(StepMirrorSteady(s, now, -0.01f, t, out) == MirrorSteadyStep::NoTime, "time backwards: no time");
	Check(StepMirrorSteady(s, now, 0.5f, t, out) == MirrorSteadyStep::NoTime, "half a second (a hitch, a pause): no time");
	Check(StepMirrorSteady(s, now, std::numeric_limits<float>::infinity(), t, out) == MirrorSteadyStep::NoTime, "endless: no time");
}

void TestCorners() {
	std::printf("The corners on the steadied glass, with the margin\n");
	const NiPoint3 corners[4] = {P(-10.0f, 0.0f, 5.0f), P(10.0f, 0.0f, 5.0f), P(-10.0f, 0.0f, -5.0f), P(10.0f, 0.0f, -5.0f)};
	GlassFrame frame;
	frame.centre = P(0.0f, 0.0f, 0.0f);
	frame.rot = MirrorColumns(P(1.0f, 0.0f, 0.0f), P(0.0f, 0.0f, 1.0f), P(0.0f, -1.0f, 0.0f));
	NiPoint3 out[4];
	SteadyGlassCorners(corners, frame, frame, 0.0f, out);
	Check(NearPoint(out[0], corners[0]) && NearPoint(out[3], corners[3]), "the same frame, no margin: the same corners");
	SteadyGlassCorners(corners, frame, frame, 0.1f, out);
	Check(NearPoint(out[0], P(-12.0f, 0.0f, 6.0f)) && NearPoint(out[3], P(12.0f, 0.0f, -6.0f)),
	      "a 10 percent margin: each side 20 percent longer, about the same centre");
	GlassFrame moved = frame;
	moved.centre = P(1.0f, 2.0f, 3.0f);
	moved.rot = MirrorAxisTurn(P(0.0f, 0.0f, 1.0f), math::kPi * 0.5f) * frame.rot;
	SteadyGlassCorners(corners, frame, moved, 0.0f, out);
	Check(NearPoint(out[1], P(1.0f, 12.0f, 8.0f)), "moved and turned a quarter: the top right corner goes with it");
	GlassPlane g = GlassPlaneOf(out);
	Check(g.valid && Near(g.width, 20.0f) && Near(g.height, 10.0f), "and they still make a valid glass of its size");
	const GlassFrame of = GlassFrameOf(g);
	Check(NearPoint(of.centre, P(1.0f, 2.0f, 3.0f)) && Near(GlassTurnDegrees(of.rot, moved.rot), 0.0f, 0.01f),
	      "the glass's frame read back from it");
}

}  // namespace

int main() {
	TestTuning();
	TestTurnBetween();
	TestOffAndStart();
	TestStillHand();
	TestDeliberateTurn();
	TestDeliberateMove();
	TestJumps();
	TestNoTime();
	TestCorners();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
