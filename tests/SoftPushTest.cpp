// Checks the soft push (game/SoftPushLogic.h): the breast bones' names; how
// far a point is pushed by the hands - out of reach, at the edge, inside,
// both hands at once, clamped, a hand exactly on the point, unread hands,
// numbers that are not numbers, the switch's values; the offset's step - the
// hand drives it at once, it eases back after, a long frame arrives, no time
// stays, numbers that are not numbers; and a world offset carried into the
// parent's frame - turned, scaled, a parent with no scale.

#include <cstdio>
#include <cstring>
#include <limits>

#include "game/SoftPushLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiMatrix33;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }
float Length(const NiPoint3& p) { return obvr::math::Sqrt(p.LengthSquared()); }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestNames() {
	std::printf("The bones\n");
	Check(std::strcmp(kSoftPushBoneNames[0], "Bip01 R Op1") == 0 && std::strcmp(kSoftPushBoneNames[1], "Bip01 L Op1") == 0,
	      "the chain's roots, right then left: the BBB breast bones of the installed body");
	Check(std::strcmp(kSoftPushTipNames[0], "Bip01 R Op2") == 0 && std::strcmp(kSoftPushTipNames[1], "Bip01 L Op2") == 0,
	      "the point measured: the next bone out");
}

void TestWanted() {
	std::printf("How far the hands push\n");
	const NiPoint3 point{0.0f, 10.0f, 0.0f};
	NiPoint3 hands[2] = {{0.0f, 30.0f, 0.0f}, {0.0f, -40.0f, 0.0f}};
	bool valid[2] = {true, true};
	NiPoint3 d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "both hands out of reach: nothing");

	hands[0] = NiPoint3{0.0f, 17.0f, 0.0f};
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "a hand exactly at the reach's edge: nothing");

	hands[0] = NiPoint3{0.0f, 15.0f, 0.0f};
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(d.y, -2.0f) && Near(d.x, 0.0f) && Near(d.z, 0.0f), "a hand 5 units off a reach of 7: pushed 2 away from it");

	hands[1] = NiPoint3{5.0f, 10.0f, 0.0f};
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(d.x, -2.0f) && Near(d.y, -2.0f), "both hands in: their pushes add");

	valid[1] = false;
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(d.x, 0.0f) && Near(d.y, -2.0f), "a hand not read this frame: left out");
	valid[1] = true;

	hands[0] = NiPoint3{0.0f, 11.0f, 0.0f};
	hands[1] = NiPoint3{0.0f, -40.0f, 0.0f};
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 4.0f) && d.y < 0.0f, "deep in (6 of 7): no further than the most it may give");

	hands[0] = point;
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "a hand exactly on the point: no direction, nothing");

	hands[0] = NiPoint3{kNaN, 15.0f, 0.0f};
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "a hand that is not a number: left out");
	hands[0] = NiPoint3{0.0f, 15.0f, 0.0f};
	d = SoftPushWanted(NiPoint3{kNaN, 0.0f, 0.0f}, hands, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "a point that is not a number: nothing");
	d = SoftPushWanted(point, hands, valid, 2, kNaN, 4.0f);
	Check(Near(Length(d), 0.0f), "a reach that is not a number: nothing");
	d = SoftPushWanted(point, hands, valid, 2, 0.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "a reach of 0 (off): nothing");
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, 0.0f);
	Check(Near(Length(d), 0.0f), "the most it may give 0: nothing");
	d = SoftPushWanted(point, hands, valid, 2, 7.0f, kNaN);
	Check(Near(Length(d), 0.0f), "the most it may give not a number: nothing");
	d = SoftPushWanted(point, nullptr, valid, 2, 7.0f, 4.0f);
	Check(Near(Length(d), 0.0f), "no hands at all: nothing");
}

void TestStep() {
	std::printf("The offset's step\n");
	const NiPoint3 zero{0.0f, 0.0f, 0.0f};
	NiPoint3 o = StepSoftPushOffset(zero, NiPoint3{0.0f, -3.0f, 0.0f}, 0.011f, 10.0f);
	Check(Near(o.y, -3.0f), "pressed further: the hand drives it at once");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, zero, 0.011f, 10.0f);
	Check(o.y < 0.0f && o.y > -3.0f && Near(o.y, -3.0f + 3.0f * 0.11f), "the hand gone: it eases back a step");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, NiPoint3{0.0f, -1.0f, 0.0f}, 0.011f, 10.0f);
	Check(o.y < -1.0f && o.y > -3.0f, "the hand drawn back a little: eases towards its push, not jumping");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, zero, 5.0f, 10.0f);
	Check(Near(o.y, 0.0f), "a long frame: it arrives, never past");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, zero, 0.0f, 10.0f);
	Check(Near(o.y, -3.0f), "no time: it stays");
	o = StepSoftPushOffset(NiPoint3{kNaN, -3.0f, 0.0f}, zero, 0.011f, 10.0f);
	Check(Near(Length(o), 0.0f), "an offset that is not a number: the wanted one");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, NiPoint3{kNaN, 0.0f, 0.0f}, 0.011f, 10.0f);
	Check(Near(Length(o), 0.0f), "a wanted one that is not a number: nothing");
	o = StepSoftPushOffset(NiPoint3{0.0f, -3.0f, 0.0f}, zero, kNaN, 10.0f);
	Check(Near(o.y, -3.0f), "a frame time that is not a number: it stays");
}

void TestLocal() {
	std::printf("Into the parent's frame\n");
	const NiPoint3 d{1.0f, 2.0f, 3.0f};
	NiPoint3 l = WorldOffsetToLocal(NiMatrix33::Identity(), 1.0f, d);
	Check(Near(l.x, 1.0f) && Near(l.y, 2.0f) && Near(l.z, 3.0f), "an unturned parent: the same");
	NiMatrix33 turn{};  // 90 degrees about z: the parent's x is the world's y
	turn.data[0][1] = -1.0f;
	turn.data[1][0] = 1.0f;
	turn.data[2][2] = 1.0f;
	l = WorldOffsetToLocal(turn, 1.0f, NiPoint3{0.0f, 1.0f, 0.0f});
	Check(Near(l.x, 1.0f) && Near(l.y, 0.0f), "a turned parent: turned back");
	l = WorldOffsetToLocal(NiMatrix33::Identity(), 2.0f, d);
	Check(Near(l.x, 0.5f) && Near(l.z, 1.5f), "a parent scaled 2: half as far in its units");
	l = WorldOffsetToLocal(NiMatrix33::Identity(), 0.0f, d);
	Check(Near(Length(l), 0.0f), "a parent with no scale: nothing");
	l = WorldOffsetToLocal(NiMatrix33::Identity(), kNaN, d);
	Check(Near(Length(l), 0.0f), "a scale that is not a number: nothing");
}

}  // namespace

int main() {
	TestNames();
	TestWanted();
	TestStep();
	TestLocal();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
