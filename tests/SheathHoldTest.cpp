#include <cmath>
#include <cstdio>
#include <limits>

#include "game/SheathHoldLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) < tolerance; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kPi = 3.14159265f;

// A turn about the node's own x by `degrees`, at `pos`: how the Unequip group tilts the hip's sheath.
NiTransform Tilted(float degrees, const NiPoint3& pos = NiPoint3{-14.6f, 3.6f, 8.6f}) {
	const float a = degrees * kPi / 180.0f;
	NiTransform t{};
	t.rot.data[0][0] = 1.0f;
	t.rot.data[1][1] = std::cos(a);
	t.rot.data[1][2] = -std::sin(a);
	t.rot.data[2][1] = std::sin(a);
	t.rot.data[2][2] = std::cos(a);
	t.pos = pos;
	t.scale = 1.0f;
	return t;
}

// The angle between two rotations, degrees.
float Between(const NiTransform& a, const NiTransform& b) {
	float c = 0.0f;
	for (int r = 0; r < 3; ++r) {
		for (int k = 0; k < 3; ++k) {
			c += a.rot.data[r][k] * b.rot.data[r][k];  // trace of a^T b
		}
	}
	c = (c - 1.0f) * 0.5f;
	c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
	return std::acos(c) * 180.0f / kPi;
}

bool Same(const NiTransform& a, const NiTransform& b) {
	return Between(a, b) < 0.05f && Near(a.pos.x, b.pos.x) && Near(a.pos.y, b.pos.y) && Near(a.pos.z, b.pos.z) &&
	       Near(a.scale, b.scale);
}

constexpr UInt32 kNode = 0x1E43E158;
constexpr float kFrame = 1.0f / 90.0f;

void TestWhichActions() {
	std::printf("Which of the player's actions keep the sheaths still\n");
	Check(SheathStillFor(kSheathActionEquip) && SheathStillFor(kSheathActionUnequip), "the draw and the sheathe");
	Check(!SheathStillFor(-1), "no action: the animation's");
	Check(!SheathStillFor(2) && !SheathStillFor(5) && !SheathStillFor(6), "an attack or the block: the animation's");
}

void TestCalm() {
	std::printf("No draw or sheathe: the animation's local as it is\n");
	SheathHoldState s;
	const NiTransform a = Tilted(3.0f);
	Check(Same(StepSheathHold(s, kNode, a, false, kFrame), a), "the animation's");
	const NiTransform b = Tilted(-2.0f, NiPoint3{-14.0f, 3.0f, 9.0f});
	Check(Same(StepSheathHold(s, kNode, b, false, kFrame), b), "and the next frame's, as it is");
}

// The measured case: the Unequip turns the hip's sheath 11.5, 38.9, 16.8, 22.4 and 19.8 degrees in five
// frames (h2-sheathe-katana 20261126-164256-slot4), then it hangs still until the action ends.
void TestSheatheKeepsStill() {
	std::printf("The sheathe: the sheath keeps the local of the frame before it\n");
	SheathHoldState s;
	const NiTransform idle = Tilted(0.0f);
	StepSheathHold(s, kNode, idle, false, kFrame);
	const float tilts[] = {0.0f, 11.5f, 50.4f, 33.6f, 56.0f, 36.2f, 0.0f, 0.0f};
	bool still = true;
	float most = 0.0f;
	for (float tilt : tilts) {
		const NiTransform out = StepSheathHold(s, kNode, Tilted(tilt), true, kFrame);
		const float off = Between(out, idle);
		most = off > most ? off : most;
		still = still && Same(out, idle);
	}
	Check(still && most < 0.05f, "every frame of the sheathe: the idle's local, the turns not passed on");
	const NiTransform after = StepSheathHold(s, kNode, idle, false, kFrame);
	Check(Same(after, idle), "the action over, the animation back at the idle: the animation's at once");
	Check(!s.easing, "nothing to ease: the two were the same");
}

void TestEaseAfter() {
	std::printf("The action over with the animation elsewhere: eased back to it\n");
	SheathHoldState s;
	const NiTransform before = Tilted(0.0f, NiPoint3{-14.6f, 3.6f, 8.6f});
	StepSheathHold(s, kNode, before, false, kFrame);
	StepSheathHold(s, kNode, Tilted(30.0f), true, kFrame);
	const NiTransform idle = Tilted(10.0f, NiPoint3{-14.6f, 5.6f, 8.6f});
	const NiTransform first = StepSheathHold(s, kNode, idle, false, kSheathEaseSeconds * 0.5f);
	Check(s.easing, "eased");
	Check(Near(Between(first, before), 5.0f, 0.05f) && Near(first.pos.y, 4.6f), "half the ease's time: half way");
	const NiTransform done = StepSheathHold(s, kNode, idle, false, kSheathEaseSeconds);
	Check(Same(done, idle) && !s.easing, "the rest of it and more: the animation's, the ease over");
	Check(Same(StepSheathHold(s, kNode, Tilted(12.0f), false, kFrame), Tilted(12.0f)), "then the animation's again");
}

void TestEaseInterrupted() {
	std::printf("A draw while it eases: held where it was eased to\n");
	SheathHoldState s;
	StepSheathHold(s, kNode, Tilted(0.0f), false, kFrame);
	StepSheathHold(s, kNode, Tilted(40.0f), true, kFrame);
	const NiTransform half = StepSheathHold(s, kNode, Tilted(20.0f), false, kSheathEaseSeconds * 0.5f);
	const NiTransform held = StepSheathHold(s, kNode, Tilted(70.0f), true, kFrame);
	Check(Same(held, half) && !s.easing, "the new action keeps the eased local, no jump to either end");
}

void TestNothingToHold() {
	std::printf("Nothing to hold yet, or another node\n");
	SheathHoldState s;
	const NiTransform mid = Tilted(25.0f);
	Check(Same(StepSheathHold(s, kNode, mid, true, kFrame), mid), "a sheathe in the first frame seen: the animation's");
	StepSheathHold(s, kNode, Tilted(0.0f), false, kFrame);
	const NiTransform other = Tilted(33.0f);
	Check(Same(StepSheathHold(s, kNode + 4, other, true, kFrame), other),
	      "another node (a new 3D) mid-sheathe: its own, the old node's local forgotten");
	Check(Same(StepSheathHold(s, kNode + 4, Tilted(50.0f), true, kFrame), other), "and held from there");
	Check(Same(StepSheathHold(s, 0, Tilted(7.0f), true, kFrame), Tilted(7.0f)), "no node: passed through");
}

void TestOddTime() {
	std::printf("A frame's time that does not read\n");
	SheathHoldState s;
	StepSheathHold(s, kNode, Tilted(0.0f), false, kFrame);
	StepSheathHold(s, kNode, Tilted(30.0f), true, kFrame);
	const NiTransform a = StepSheathHold(s, kNode, Tilted(10.0f), false, kNaN);
	Check(Same(a, Tilted(0.0f)) && s.easing, "no number: the ease does not move");
	const NiTransform b = StepSheathHold(s, kNode, Tilted(10.0f), false, -1.0f);
	Check(Same(b, Tilted(0.0f)) && s.easing, "a negative one: neither");
	const NiTransform c = StepSheathHold(s, kNode, Tilted(10.0f), false, 1.0f);
	Check(Same(c, Tilted(10.0f)) && !s.easing, "a long one: done");
}

void TestTiltWords() {
	std::printf("The trace's verdict on a sheathe\n");
	Check(SheathTiltStill(0.0f) && SheathTiltStill(kSheathStillDegrees - 0.01f), "under the limit: still");
	Check(!SheathTiltStill(kSheathStillDegrees) && !SheathTiltStill(38.9f), "the measured 38.9 a frame: swung");
	Check(!SheathTiltStill(kNaN), "no number: not still");
}

}  // namespace

int main() {
	TestWhichActions();
	TestCalm();
	TestSheatheKeepsStill();
	TestEaseAfter();
	TestEaseInterrupted();
	TestNothingToHold();
	TestOddTime();
	TestTiltWords();
	std::printf(g_failures == 0 ? "All sheath hold checks passed\n" : "%d sheath hold checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
