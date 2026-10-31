// Checks a two-hander held in one hand (vr/OneHandTwoHander.h): the percent
// as a factor, which weapons count, the cost scaled and nothing else, the
// body's frame from the camera, the places' scale, the shoulder rest - on
// either shoulder, not upright before it, not pointing ahead, not with the
// fist alone, not too short - and the hold's step: nothing drawn, both
// hands, raised and low with the band at the hip, resting on a shoulder and
// staying there, the hand lost, the burn switched off or set to nothing, a
// frame without time, the totals of a hold that ended, the percents at 0,
// the hip moved.

#include <cstdio>
#include <limits>

#include "vr/OneHandTwoHander.h"

namespace {

using namespace obvr::vr;
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

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kFrame = 1.0f / 90.0f;

void TestPercent() {
	std::printf("The percent\n");
	Check(Near(PercentMore(40.0f), 1.4f), "40 % more is x1.4");
	Check(PercentMore(0.0f) == 1.0f, "0 % is as it was");
	Check(PercentMore(-20.0f) == 1.0f, "a negative one too");
	Check(PercentMore(kNaN) == 1.0f, "and one that is not a number");
	Check(Near(PercentMore(250.0f), 3.0f), "over 200 % is x3");
}

void TestWeaponTypes() {
	std::printf("The weapons\n");
	Check(IsSwungTwoHanderType(1), "a two-handed blade counts");
	Check(IsSwungTwoHanderType(3), "a two-handed blunt weapon counts");
	Check(!IsSwungTwoHanderType(4), "a staff does not");
	Check(!IsSwungTwoHanderType(0) && !IsSwungTwoHanderType(2), "nor a one-hander");
	Check(!IsSwungTwoHanderType(5) && !IsSwungTwoHanderType(-1), "nor a bow, nor nothing");
}

void TestScaledChange() {
	std::printf("The cost scaled\n");
	Check(Near(ScaledFatigueChange(-10.0f, 1.4f), -14.0f), "a cost of 10 at x1.4 is 14");
	Check(ScaledFatigueChange(5.0f, 1.4f) == 5.0f, "fatigue given back is not");
	Check(ScaledFatigueChange(0.0f, 1.4f) == 0.0f, "nothing stays nothing");
	Check(ScaledFatigueChange(-10.0f, 1.0f) == -10.0f, "at x1 as it is");
	Check(ScaledFatigueChange(-10.0f, 0.5f) == -10.0f, "a scale under 1 makes nothing cheaper");
	Check(ScaledFatigueChange(-10.0f, kNaN) == -10.0f, "a scale that is not a number: as it is");
	const float nan = ScaledFatigueChange(kNaN, 1.4f);
	Check(nan != nan, "a change that is not a number is passed on as it came");
}

NiMatrix33 Yawed(float degrees) {
	// The camera's columns: x right, y forward, z up; turned left by degrees.
	const float r = degrees * obvr::math::kDegreesToRadians;
	const float c = obvr::math::Cos(r);
	const float s = obvr::math::Sin(r);
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = c;
	m.data[1][0] = s;
	m.data[0][1] = -s;
	m.data[1][1] = c;
	return m;
}

NiMatrix33 Pitched(const NiMatrix33& yaw, float degreesDown) {
	// Tipped down about the camera's own x.
	const float r = degreesDown * obvr::math::kDegreesToRadians;
	const float c = obvr::math::Cos(r);
	const float s = obvr::math::Sin(r);
	NiMatrix33 pitch = NiMatrix33::Identity();
	pitch.data[1][1] = c;
	pitch.data[2][1] = -s;
	pitch.data[1][2] = s;
	pitch.data[2][2] = c;
	return yaw * pitch;
}

void TestBodyFrame() {
	std::printf("The body's frame\n");
	const NiPoint3 eyes{100.0f, 200.0f, 300.0f};
	NiPoint3 p = BodyFrameOf(NiMatrix33::Identity(), eyes, NiPoint3{170.0f, 270.0f, 230.0f}, 70.0f);
	Check(Near(p.x, 1.0f) && Near(p.y, 1.0f) && Near(p.z, -1.0f),
	      "looking along y: 70 units right, ahead and down are a metre each");
	p = BodyFrameOf(Yawed(90.0f), eyes, NiPoint3{100.0f, 270.0f, 300.0f}, 70.0f);
	Check(Near(p.x, 1.0f) && Near(p.y, 0.0f), "turned left a quarter: the old forward is the right");
	p = BodyFrameOf(Pitched(NiMatrix33::Identity(), 45.0f), eyes, NiPoint3{100.0f, 270.0f, 230.0f}, 70.0f);
	Check(Near(p.x, 0.0f) && Near(p.y, 1.0f) && Near(p.z, -1.0f),
	      "the head tipped down: the heading is on the level, up is the room's");
	p = BodyFrameOf(Pitched(NiMatrix33::Identity(), 90.0f), eyes, NiPoint3{100.0f, 270.0f, 300.0f}, 70.0f);
	Check(p.x == p.x && p.y == p.y, "looking straight down: a heading all the same, no number that is none");
	p = BodyFrameOf(NiMatrix33::Identity(), eyes, NiPoint3{170.0f, 270.0f, 230.0f}, 0.0f);
	Check(p.x == 0.0f && p.y == 0.0f && p.z == 0.0f, "no units a metre: nowhere");
}

void TestScale() {
	std::printf("The places' scale\n");
	Check(Near(HoldScaleOf(0.65f), 1.0f), "the default hip: 1");
	Check(Near(HoldScaleOf(0.78f), 1.2f), "a hip 0.78 under the eyes: 1.2");
	Check(Near(HoldScaleOf(1.0f), 1.25f), "held to 1.25 at most");
	Check(Near(HoldScaleOf(0.4f), 0.8f), "and 0.8 at least");
	Check(HoldScaleOf(0.0f) == 1.0f && HoldScaleOf(kNaN) == 1.0f, "none, or not a number: 1");
}

// A claymore of a metre: the grip in front of the right shoulder at the
// chest, the blade back and up over it.
const NiPoint3 kRestGrip{0.20f, 0.12f, -0.35f};
const NiPoint3 kRestTip{0.12f, -0.55f, 0.35f};

void TestShoulderRest() {
	std::printf("The shoulder rest\n");
	ShoulderRest r = ShoulderRestOf(kRestGrip, kRestTip, 1.0f, kShoulderRestMetres);
	Check(r.resting && r.right, "the blade back over the right shoulder rests on it");
	Check(r.distance < 0.05f && r.along > kShoulderRestFromGripMetres, "its blade near the shoulder's top");
	const NiPoint3 leftGrip{-kRestGrip.x, kRestGrip.y, kRestGrip.z};
	const NiPoint3 leftTip{-kRestTip.x, kRestTip.y, kRestTip.z};
	r = ShoulderRestOf(leftGrip, leftTip, 1.0f, kShoulderRestMetres);
	Check(r.resting && !r.right, "the same over the left shoulder");
	// Upright before the shoulder, the hand at its height ("vom Tag").
	r = ShoulderRestOf(NiPoint3{0.20f, 0.10f, -0.20f}, NiPoint3{0.20f, 0.10f, 0.80f}, 1.0f, kShoulderRestMetres);
	Check(!r.resting, "upright before the shoulder: not resting");
	// Pointing ahead from behind the shoulder, through its place.
	r = ShoulderRestOf(NiPoint3{0.15f, -0.40f, -0.15f}, NiPoint3{0.15f, 0.60f, -0.20f}, 1.0f, kShoulderRestMetres);
	Check(!r.resting && r.distance <= kShoulderRestMetres, "through the shoulder's place but pointing ahead: not");
	// The fist on the shoulder, the blade out behind: only the handle there.
	r = ShoulderRestOf(NiPoint3{0.15f, -0.08f, -0.17f}, NiPoint3{0.15f, -1.08f, -0.17f}, 1.0f, kShoulderRestMetres);
	Check(!r.resting, "the fist on the shoulder and the blade behind: the handle does not rest");
	r = ShoulderRestOf(kRestGrip, kRestGrip + NiPoint3{0.0f, -0.1f, 0.0f}, 1.0f, kShoulderRestMetres);
	Check(!r.resting, "a weapon no longer than the grip's share rests on nothing");
	r = ShoulderRestOf(kRestGrip, kRestTip, 0.0f, kShoulderRestMetres);
	Check(!r.resting, "no scale: nothing");
	// A taller player's shoulder is further out and down: the same pose
	// scaled with it rests; the small one does not reach so far.
	const NiPoint3 tallGrip = kRestGrip * 1.2f;
	const NiPoint3 tallTip = kRestTip * 1.2f;
	r = ShoulderRestOf(tallGrip, tallTip, 1.2f, kShoulderRestMetres * 1.2f);
	Check(r.resting && r.right, "a taller player's pose rests on a taller player's shoulder");
	r = ShoulderRestOf(NiPoint3{0.30f, 0.12f, -0.35f}, NiPoint3{0.32f, -0.55f, 0.35f}, 1.0f, kShoulderRestMetres);
	Check(!r.resting && r.distance > kShoulderRestMetres, "back past the shoulder's outside: not on it");
}

void TestCostKinds() {
	std::printf("Which costs are dearer\n");
	// Nadi, 2026-10-26: "nur auf angriff block, swung, parade, schubsen, packen".
	Check(OneHandScales(FatigueCostKind::Attack) && OneHandScales(FatigueCostKind::Block),
	      "the game's attack and block");
	Check(OneHandScales(FatigueCostKind::Swing) && OneHandScales(FatigueCostKind::Parry),
	      "a swing and a parry");
	Check(OneHandScales(FatigueCostKind::Shove) && OneHandScales(FatigueCostKind::Grab), "a shove and a grab");
	Check(!OneHandScales(FatigueCostKind::Teleport), "not the teleport");
	Check(!OneHandScales(FatigueCostKind::Hold), "not the hold's own burn, already the one hand's");
	Check(!OneHandScales(FatigueCostKind::Sprint), "not the sprint: legs, not the arms");
	Check(!OneHandScales(FatigueCostKind::Count), "nothing past the kinds");
	bool named = true;
	for (UInt32 k = 0; k <= static_cast<UInt32>(FatigueCostKind::Count); ++k) {
		named = named && FatigueCostKindWords(static_cast<FatigueCostKind>(k))[0] != '\0';
	}
	Check(named, "every kind has its words");
}

TwoHanderHoldInput Drawn(float handUp, bool bothHands = false) {
	TwoHanderHoldInput in;
	in.twoHanderDrawn = true;
	in.bothHands = bothHands;
	in.handValid = true;
	in.handUp = handUp;
	in.weaponValid = true;
	// Pointing ahead from the hand, nowhere near a shoulder.
	in.grip = NiPoint3{0.20f, 0.30f, handUp};
	in.tip = NiPoint3{0.20f, 1.30f, handUp};
	in.dtSeconds = kFrame;
	return in;
}

void TestHold() {
	std::printf("The hold\n");
	const OneHandTwoHanderSettings set;
	TwoHanderHoldState s;
	TwoHanderHoldInput none;
	none.dtSeconds = kFrame;
	TwoHanderHoldVerdict v = StepTwoHanderHold(s, none, set);
	Check(v.hold == TwoHanderHold::None && !v.oneHanded && v.costScale == 1.0f && v.weightFactor == 1.0f &&
	          v.burn == 0.0f,
	      "nothing drawn: nothing scaled, nothing burnt");

	v = StepTwoHanderHold(s, Drawn(-0.40f, true), set);
	Check(v.hold == TwoHanderHold::BothHands && !v.oneHanded && v.costScale == 1.0f && v.weightFactor == 1.0f &&
	          v.burn == 0.0f && v.changed,
	      "both hands, over the hip: no burn, the costs and the weight as before");
	v = StepTwoHanderHold(s, Drawn(0.10f, true), set);
	Check(v.hold == TwoHanderHold::BothHands && v.burn == 0.0f, "both hands over the head: still none");

	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.40f), set);
	Check(v.hold == TwoHanderHold::Raised && v.oneHanded, "in one hand 0.40 under the eyes: raised over the hip");
	Check(Near(v.costScale, 1.4f) && Near(v.weightFactor, 1.4f), "every cost x1.4, the weight x1.4");
	Check(Near(v.burn, 5.0f * kFrame) && Near(v.burnPerSecond, 5.0f),
	      "the bar falls 5 a second: with nothing given back, 5 a second burns");
	Check(Near(v.overHip, 0.25f), "0.25 over the hip at 0.65");

	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.85f), set);
	Check(v.hold == TwoHanderHold::Low && v.oneHanded && v.burn == 0.0f && Near(v.costScale, 1.4f),
	      "in one hand under the hip: no burn, the costs still x1.4");

	std::printf("The band at the hip\n");
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.64f), set);
	Check(v.hold == TwoHanderHold::Raised, "taken up a centimetre over the hip: raised");
	v = StepTwoHanderHold(s, Drawn(-0.665f), set);
	Check(v.hold == TwoHanderHold::Raised && !v.changed, "a centimetre and a half under it: still raised");
	v = StepTwoHanderHold(s, Drawn(-0.68f), set);
	Check(v.hold == TwoHanderHold::Low && v.changed, "three under it: low");
	v = StepTwoHanderHold(s, Drawn(-0.635f), set);
	Check(v.hold == TwoHanderHold::Low, "a centimetre and a half over it: still low");
	v = StepTwoHanderHold(s, Drawn(-0.62f), set);
	Check(v.hold == TwoHanderHold::Raised, "three over it: raised");

	std::printf("Resting on a shoulder\n");
	s = TwoHanderHoldState{};
	TwoHanderHoldInput rest = Drawn(-0.35f);
	rest.grip = kRestGrip;
	rest.tip = kRestTip;
	v = StepTwoHanderHold(s, rest, set);
	Check(v.hold == TwoHanderHold::Shoulder && v.shoulder.right && v.burn == 0.0f && v.oneHanded &&
	          Near(v.costScale, 1.4f),
	      "the hand over the hip, the blade on the shoulder: resting, no burn, the costs x1.4");
	// Lifted 12 cm: past the radius to begin, inside the one to stay.
	TwoHanderHoldInput lifted = rest;
	lifted.grip = kRestGrip + NiPoint3{0.0f, 0.0f, 0.12f};
	lifted.tip = kRestTip + NiPoint3{0.0f, 0.0f, 0.12f};
	v = StepTwoHanderHold(s, lifted, set);
	Check(v.hold == TwoHanderHold::Shoulder, "lifted 12 cm: still resting");
	TwoHanderHoldState fresh;
	v = StepTwoHanderHold(fresh, lifted, set);
	Check(v.hold == TwoHanderHold::Raised, "the same pose coming from nowhere: raised, not resting");
	TwoHanderHoldInput gone = rest;
	gone.grip = kRestGrip + NiPoint3{0.0f, 0.0f, 0.20f};
	gone.tip = kRestTip + NiPoint3{0.0f, 0.0f, 0.20f};
	v = StepTwoHanderHold(s, gone, set);
	Check(v.hold == TwoHanderHold::Raised && v.burn > 0.0f, "lifted 20 cm off it: raised, burning");
	TwoHanderHoldInput noWeapon = rest;
	noWeapon.weaponValid = false;
	fresh = TwoHanderHoldState{};
	v = StepTwoHanderHold(fresh, noWeapon, set);
	Check(v.hold == TwoHanderHold::Raised, "no weapon read: the hand's height alone decides");

	std::printf("The hand lost\n");
	s = TwoHanderHoldState{};
	StepTwoHanderHold(s, Drawn(-0.40f), set);
	TwoHanderHoldInput lost = Drawn(-0.40f);
	lost.handValid = false;
	v = StepTwoHanderHold(s, lost, set);
	Check(v.hold == TwoHanderHold::Raised && v.burn > 0.0f, "raised, then no hand: raised as it was");
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, lost, set);
	Check(v.hold == TwoHanderHold::Low && v.burn == 0.0f, "no hand from the start: low, nothing burnt");

	std::printf("The burn set\n");
	OneHandTwoHanderSettings off = set;
	off.holdCosts = false;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.40f), off);
	Check(v.hold == TwoHanderHold::Raised && v.burn == 0.0f && Near(v.costScale, 1.4f),
	      "OneHandTwoHanderHoldCosts off: raised burns nothing, the costs still x1.4");
	OneHandTwoHanderSettings zero = set;
	zero.holdPerSecond = 0.0f;
	s = TwoHanderHoldState{};
	Check(StepTwoHanderHold(s, Drawn(-0.40f), zero).burn == 0.0f, "a burn of 0 a second: none");
	zero.holdPerSecond = kNaN;
	s = TwoHanderHoldState{};
	Check(StepTwoHanderHold(s, Drawn(-0.40f), zero).burn == 0.0f, "one that is not a number: none");
	s = TwoHanderHoldState{};
	TwoHanderHoldInput noTime = Drawn(-0.40f);
	noTime.dtSeconds = kNaN;
	Check(StepTwoHanderHold(s, noTime, set).burn == 0.0f, "a frame whose time is not a number burns nothing");
	noTime.dtSeconds = 2.0f;
	Check(StepTwoHanderHold(s, noTime, set).burn == 0.0f, "nor does a frame two seconds long (a load)");

	std::printf("The game's return added\n");
	s = TwoHanderHoldState{};
	TwoHanderHoldInput given = Drawn(-0.40f);
	given.gameReturnPerSecond = 5.0f;
	v = StepTwoHanderHold(s, given, set);
	Check(Near(v.burnPerSecond, 10.0f) && Near(v.burn, 10.0f * kFrame),
	      "the game gives back 5 a second: 10 burn, so the bar still falls 5 (Nadi's install)");
	given.gameReturnPerSecond = 10.0f;
	Check(Near(StepTwoHanderHold(s, given, set).burnPerSecond, 15.0f), "vanilla's 10 given back: 15 burn");
	given.handUp = -0.85f;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, given, set);
	Check(v.hold == TwoHanderHold::Low && v.burn == 0.0f && v.burnPerSecond == 0.0f,
	      "held low: nothing burns, whatever the game gives back");
	TwoHanderHoldInput bothGiven = Drawn(-0.40f, true);
	bothGiven.gameReturnPerSecond = 5.0f;
	s = TwoHanderHoldState{};
	Check(StepTwoHanderHold(s, bothGiven, set).burn == 0.0f, "both hands: nothing, whatever the game gives back");
	OneHandTwoHanderSettings offGiven = set;
	offGiven.holdCosts = false;
	given.handUp = -0.40f;
	s = TwoHanderHoldState{};
	Check(StepTwoHanderHold(s, given, offGiven).burn == 0.0f, "the hold's cost off: the game's return adds nothing");

	std::printf("The burn a second\n");
	Check(Near(HoldBurnPerSecond(5.0f, 5.0f), 10.0f), "a fall of 5, 5 given back: 10");
	Check(Near(HoldBurnPerSecond(5.0f, 0.0f), 5.0f), "nothing given back: the fall alone");
	Check(Near(HoldBurnPerSecond(5.0f, -3.0f), 5.0f), "a return under 0 (unread): the fall alone");
	Check(Near(HoldBurnPerSecond(5.0f, kNaN), 5.0f), "a return that is not a number: the fall alone");
	Check(Near(HoldBurnPerSecond(5.0f, 5.0e3f), 5.0f), "an absurd return (no sane game's): the fall alone");
	Check(HoldBurnPerSecond(0.0f, 5.0f) == 0.0f, "a fall of 0: nothing burns, the return is not paid alone");
	Check(HoldBurnPerSecond(-1.0f, 5.0f) == 0.0f, "a fall under 0: nothing");
	Check(HoldBurnPerSecond(kNaN, 5.0f) == 0.0f, "a fall that is not a number: nothing");
	Check(HoldBurnPerSecond(2.0e4f, 5.0f) == 0.0f, "an absurd fall: nothing");

	std::printf("The game's return\n");
	Check(Near(GameFatigueReturnOf(10.0f, 0.0f, 50), 10.0f), "vanilla: 10 a second, Endurance times 0");
	Check(Near(GameFatigueReturnOf(5.0f, 0.1f, 40), 9.0f), "5 plus 0.1 a point of Endurance at 40: 9");
	Check(GameFatigueReturnOf(5.0f, 0.0f, -1) < 0.0f, "no Endurance read: unknown");
	Check(GameFatigueReturnOf(kNaN, 0.0f, 50) < 0.0f, "a base that is not a number: unknown");
	Check(GameFatigueReturnOf(5.0f, kNaN, 50) < 0.0f, "a multiplier that is not a number: unknown");

	std::printf("The totals\n");
	s = TwoHanderHoldState{};
	for (int i = 0; i < 90; ++i) {
		StepTwoHanderHold(s, Drawn(-0.40f), set);
	}
	v = StepTwoHanderHold(s, Drawn(-0.85f), set);
	Check(v.changed && v.was == TwoHanderHold::Raised && v.hold == TwoHanderHold::Low, "raised, then low");
	Check(Near(v.endedSeconds, 1.0f, 0.02f) && Near(v.endedBurned, 5.0f, 0.1f),
	      "the raised second said: 1 s, 5 burnt");

	std::printf("The percents and the hip\n");
	OneHandTwoHanderSettings plain = set;
	plain.weightPercent = 0.0f;
	plain.fatiguePercent = 0.0f;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.40f), plain);
	Check(v.oneHanded && v.costScale == 1.0f && v.weightFactor == 1.0f && v.burn > 0.0f,
	      "both percents 0: in one hand, nothing scaled, the hold still burns");
	OneHandTwoHanderSettings tall = set;
	tall.hipBelowEyes = 0.78f;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.70f), tall);
	Check(v.hold == TwoHanderHold::Raised && Near(v.overHip, 0.08f), "a hip 0.78 under the eyes: 0.70 is over it");
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.70f), set);
	Check(v.hold == TwoHanderHold::Low, "the default hip: 0.70 is under it");
	TwoHanderHoldInput tallRest = Drawn(-0.40f);
	tallRest.grip = kRestGrip * 1.2f;
	tallRest.tip = kRestTip * 1.2f;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, tallRest, tall);
	Check(v.hold == TwoHanderHold::Shoulder, "the taller player's shoulder from the hip's scale");
	OneHandTwoHanderSettings noHip = set;
	noHip.hipBelowEyes = kNaN;
	s = TwoHanderHoldState{};
	v = StepTwoHanderHold(s, Drawn(-0.60f), noHip);
	Check(v.hold == TwoHanderHold::Raised && Near(v.overHip, 0.05f), "a hip that is not a number: the default");

	std::printf("The words\n");
	Check(TwoHanderHoldWords(TwoHanderHold::Raised)[0] != '\0' && TwoHanderHoldWords(TwoHanderHold::Shoulder)[0] != '\0' &&
	          TwoHanderHoldWords(TwoHanderHold::Low)[0] != '\0' && TwoHanderHoldWords(TwoHanderHold::BothHands)[0] != '\0' &&
	          TwoHanderHoldWords(TwoHanderHold::None)[0] != '\0',
	      "every hold has its words");
}

}  // namespace

int main() {
	TestPercent();
	TestWeaponTypes();
	TestScaledChange();
	TestBodyFrame();
	TestScale();
	TestShoulderRest();
	TestCostKinds();
	TestHold();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
