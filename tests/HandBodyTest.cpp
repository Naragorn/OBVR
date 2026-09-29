// Checks the decisions behind the hands' and the weapon's Havok bodies
// (game/HandBodyLogic.h): the filter, the capsules, the hard keyframe drive,
// the lifecycle and the physics rate.

#include <cmath>
#include <cstdio>

#include "game/HandBodyLogic.h"

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

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

NiMatrix33 AboutZ(float radians) {
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = std::cos(radians);
	m.data[0][1] = -std::sin(radians);
	m.data[1][0] = std::sin(radians);
	m.data[1][1] = std::cos(radians);
	return m;
}

void TestFilter() {
	std::printf("The filter\n");
	Check(HandBodyFilter(9, true) == ((9u << 16) | 22u), "the player's group, layer 22");
	Check(HandBodyFilter(9, false) == ((9u << 16) | 22u | 0x4000u), "off: bit 14 set");
	Check(HandBodyFilter(0, true) == ((9u << 16) | 22u), "group 0 read: the fallback 9");
	Check(HandBodyFilter(0x12345, true) >> 16 == 0x2345u, "a group wider than 16 bits keeps its low word");
	Check(HandBodyFilter(9, true, false) == ((9u << 16) | 23u), "not pushing people: layer 23");
	Check(HandBodyFilter(9, false, false) == ((9u << 16) | 23u | 0x4000u), "not pushing people, and off");
	Check(FilterLayer(HandBodyFilter(9, true, true)) == kHandBodyLayer &&
	          FilterLayer(HandBodyFilter(9, false, false)) == kHandBodyQuietLayer,
	      "the layer read back, with or without the off bit");
	Check(HandBodyRefilterNeeded(HandBodyFilter(9, true, true), HandBodyFilter(9, true, false)) &&
	          !HandBodyRefilterNeeded(HandBodyFilter(9, true, false), HandBodyFilter(9, true, false)),
	      "a new filter only when it differs");
}

void TestMeasuring() {
	std::printf("Measuring\n");
	Check(Near(PassThroughTravelUnits(1.0f, 1.5f), 4.0f), "a plate 1 unit thick, the blade 1.5 round: 4 units a step");
	Check(Near(TravelPerStep(12.0f, 3), 4.0f) && Near(TravelPerStep(12.0f, 1), 12.0f), "a frame's travel over its steps");
	Check(Near(TravelPerStep(12.0f, 0), 12.0f) && Near(TravelPerStep(12.0f, 40), 12.0f),
	      "no step, or an absurd count: the whole frame's travel");
	const AlongAxis a = MeasureAlongAxis(NiPoint3{1.0f, 1.0f, 1.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{1.0f, 11.0f, 4.0f});
	Check(Near(a.ahead, 10.0f) && Near(a.aside, 3.0f), "10 ahead along the axis, 3 to the side");
	const AlongAxis b = MeasureAlongAxis(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, -2.0f, 0.0f});
	Check(Near(b.ahead, -2.0f) && Near(b.aside, 0.0f), "behind the grip: negative");
	const BoxGaps g = HavokBeyondMesh(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{10.0f, 10.0f, 2.0f},
	                                  NiPoint3{-1.0f, -0.5f, 0.0f}, NiPoint3{10.0f, 12.0f, 2.5f});
	Check(Near(g.most, 2.0f) && Near(g.least, 0.0f), "Havok 2 beyond the mesh on one side, flush on others");
	const BoxGaps inside = HavokBeyondMesh(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{10.0f, 10.0f, 10.0f},
	                                       NiPoint3{1.0f, 1.0f, 1.0f}, NiPoint3{9.0f, 9.0f, 9.0f});
	Check(Near(inside.most, -1.0f) && Near(inside.least, -1.0f), "Havok inside the mesh all round: negative");
}

void TestPushingPeople() {
	std::printf("Pushing people\n");
	Check(HandBodyPushesActors(true, false, false), "a hand, out of combat, open: pushes");
	Check(!HandBodyPushesActors(true, false, true), "a hand made a fist: does not");
	Check(!HandBodyPushesActors(true, true, false) && !HandBodyPushesActors(true, true, true),
	      "a hand in combat: does not, open or fist");
	Check(HandBodyPushesActors(false, false, false) && HandBodyPushesActors(false, false, true),
	      "the weapon out of combat: pushes (a fist round a handle is no fist)");
	Check(!HandBodyPushesActors(false, true, false), "the weapon in combat: does not");

	const float open[5] = {0.9f, 0.1f, 0.1f, 0.1f, 0.1f};
	const float fist[5] = {0.0f, 0.95f, 0.92f, 0.91f, 0.93f};
	const float between[5] = {0.0f, 0.95f, 0.6f, 0.95f, 0.95f};
	const float oneLow[5] = {0.0f, 0.95f, 0.95f, 0.95f, 0.5f};
	Check(!HandBodyFist(false, true, open, 0.9f, 0.75f), "open: no fist (the thumb does not count)");
	Check(HandBodyFist(false, true, fist, 0.9f, 0.75f), "four fingers past the close limit: a fist");
	Check(!HandBodyFist(false, true, oneLow, 0.9f, 0.75f), "one finger short of it: not yet");
	Check(HandBodyFist(true, true, between, 0.9f, 0.75f), "a fist stays one while a finger is above the open limit");
	Check(!HandBodyFist(true, true, open, 0.9f, 0.75f), "all below the open limit: open again");
	Check(!HandBodyFist(true, false, fist, 0.9f, 0.75f) && !HandBodyFist(false, true, nullptr, 0.9f, 0.75f),
	      "no curls: no fist");
}

void TestCapsules() {
	std::printf("The capsules\n");
	const CapsuleSpec hand = HandCapsule(kHandBodyRadiusUnits);
	Check(Near(hand.a.y, -4.0f * kHavokPerUnit) && Near(hand.b.y, 9.0f * kHavokPerUnit) && hand.a.x == 0.0f &&
	          hand.b.z == 0.0f,
	      "the hand: 4 units behind the grip to 9 ahead, along y, in Havok units");
	Check(Near(hand.radius, 2.5f * kHavokPerUnit), "2.5 units round");
	CapsuleSpec blade;
	Check(BladeCapsule(60.0f, kBladeBodyRadiusUnits, blade) && Near(blade.b.y, 60.0f * kHavokPerUnit) &&
	          blade.a.y == 0.0f && Near(blade.radius, 1.5f * kHavokPerUnit),
	      "a longsword: the grip to 60 units ahead");
	Check(!BladeCapsule(4.9f, kBladeBodyRadiusUnits, blade) && !BladeCapsule(0.0f, 1.0f, blade),
	      "shorter than 5 units, or nothing: no blade");
	Check(!BladeCapsule(60.0f, 0.0f, blade), "no radius: refused");
	volatile float zero = 0.0f;
	Check(!BladeCapsule(zero / zero, 1.0f, blade), "not a number: refused");
	Check(!BladeNeedsRebuild(60.0f, 62.5f) && BladeNeedsRebuild(60.0f, 70.0f) && BladeNeedsRebuild(70.0f, 60.0f),
	      "a new body only for another weapon's blade, not a wobble");
}

void TestDrive() {
	std::printf("The hard keyframe\n");
	Check(Near(HandBodyStepSeconds(1.0f / 90.0f), 1.0f / 90.0f) && Near(HandBodyStepSeconds(0.0f), 1.0f / 60.0f) &&
	          Near(HandBodyStepSeconds(0.5f), 1.0f / 60.0f),
	      "the planner's step, or 1/60 when it is none or absurd");
	NiPoint3 v = HardKeyframeLinear(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}, 1.0f / 60.0f);
	Check(Near(v.x, 60.0f) && v.y == 0.0f, "one Havok unit in a 1/60 s step: 60 a second");
	v = HardKeyframeLinear(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 100.0f, 0.0f}, 1.0f / 60.0f);
	Check(Near(v.y, kHandBodyMaxHavokPerSecond), "too fast: clamped to 30 m/s");
	v = HardKeyframeLinear(NiPoint3{2.0f, 2.0f, 2.0f}, NiPoint3{2.0f, 2.0f, 2.0f}, 1.0f / 60.0f);
	Check(v.LengthSquared() == 0.0f, "there already: still");
	Check(Near(HandBodyDriveSeconds(1, 0.0072f), 0.0072f) && Near(HandBodyDriveSeconds(2, 1.0f / 60.0f), 2.0f / 60.0f),
	      "the steps the physics runs this frame, times their length");
	Check(Near(HandBodyDriveSeconds(0, 1.0f / 60.0f), 1.0f / 60.0f) && Near(HandBodyDriveSeconds(7, 1.0f / 60.0f), 1.0f / 60.0f),
	      "no step this frame, or an absurd count: one step's time");
	Check(!HandBodyPlaceInstead(10.0f) && HandBodyPlaceInstead(50.0f), "far from the hand (a teleport): placed");
	volatile float zero = 0.0f;
	Check(HandBodyPlaceInstead(zero / zero), "a gap that is not a number: placed rather than flung");

	NiPoint3 w = HardKeyframeAngular(NiMatrix33::Identity(), NiMatrix33::Identity(), 1.0f / 60.0f);
	Check(w.LengthSquared() == 0.0f, "no turn: no angular velocity");
	w = HardKeyframeAngular(NiMatrix33::Identity(), AboutZ(0.1f), 1.0f / 60.0f);
	Check(Near(w.z, 6.0f, 1e-2f) && Near(w.x, 0.0f) && Near(w.y, 0.0f), "0.1 rad about z in a step: 6 rad/s about z");
	w = HardKeyframeAngular(AboutZ(0.3f), AboutZ(0.2f), 1.0f / 60.0f);
	Check(Near(w.z, -6.0f, 1e-2f), "back the other way: negative");
	w = HardKeyframeAngular(NiMatrix33::Identity(), AboutZ(3.0f), 1.0f / 60.0f);
	Check(Near(w.z, kHandBodyMaxRadiansPerSecond, 1e-2f), "a large turn: clamped");
	w = HardKeyframeAngular(AboutZ(3.0f), AboutZ(-3.0f), 1.0f / 60.0f);
	Check(w.z > 0.0f && w.z <= kHandBodyMaxRadiansPerSecond + 1e-3f,
	      "across 180 degrees: the short way round (on through pi), not back");
}

void TestLifecycle() {
	std::printf("The lifecycle\n");
	HandBodyState s;
	Check(DecideHandBody(s) == HandBodyAction::None, "not wanted, not made: nothing");
	s.wanted = true;
	Check(DecideHandBody(s) == HandBodyAction::None, "wanted but no world (the main menu): nothing");
	s.playerWorld = 0x100;
	Check(DecideHandBody(s) == HandBodyAction::Create, "wanted, a world, not made: made");
	s.made = true;
	Check(DecideHandBody(s) == HandBodyAction::Enter, "made, in no world: into the player's");
	s.bodyWorld = 0x200;
	Check(DecideHandBody(s) == HandBodyAction::Enter, "in another world (through a door): moved");
	s.bodyWorld = 0x100;
	Check(DecideHandBody(s) == HandBodyAction::Drive, "in the player's world: driven");
	s.wanted = false;
	Check(DecideHandBody(s) == HandBodyAction::Leave, "no longer wanted (a menu, a load): out of the world");
	s.bodyWorld = 0;
	Check(DecideHandBody(s) == HandBodyAction::None, "not wanted, already out: nothing");
	s.wanted = true;
	s.playerWorld = 0;
	s.bodyWorld = 0x100;
	Check(DecideHandBody(s) == HandBodyAction::Leave, "wanted but the player has no world: out");
	s.bodyWorldSound = false;
	Check(DecideHandBody(s) == HandBodyAction::Abandon, "its world no longer looks like one: abandoned");
	s.bodyWorld = 0;
	Check(DecideHandBody(s) == HandBodyAction::None, "unsound but in no world: not abandoned");
}

void TestRate() {
	std::printf("The physics rate\n");
	Check(PhysicsStepFor(0.0f) == 0.0f && PhysicsStepFor(-5.0f) == 0.0f, "0 or less: the game's own");
	Check(Near(PhysicsStepFor(90.0f), 1.0f / 90.0f), "90 Hz");
	Check(Near(PhysicsStepFor(10.0f), 1.0f / 30.0f) && Near(PhysicsStepFor(500.0f), 1.0f / 180.0f),
	      "clamped to 30..180 Hz");
	volatile float zero = 0.0f;
	Check(PhysicsStepFor(zero / zero) == 0.0f, "not a number: the game's own");
}

}  // namespace

int main() {
	TestFilter();
	TestPushingPeople();
	TestMeasuring();
	TestCapsules();
	TestDrive();
	TestLifecycle();
	TestRate();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
