#include <cstdio>
#include <limits>

#include "game/KickLogic.h"

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
using namespace obvr::game;

constexpr float kFrame = 1.0f / 90.0f;

KickGate Free() {
	KickGate g;
	g.inWorld = true;
	return g;
}

void TestGate() {
	std::printf("the gate: only in the world, on foot, standing, not exhausted\n");
	const KickSettings s;
	Check(KickAllowed(s, Free()), "in the world, nothing in the way: allowed");
	KickGate g = Free();
	g.inWorld = false;
	Check(!KickAllowed(s, g), "not in the world (a load, the main menu)");
	g = Free();
	g.menuUp = true;
	Check(!KickAllowed(s, g), "a menu up");
	g = Free();
	g.inDialogue = true;
	Check(!KickAllowed(s, g), "in a conversation");
	g = Free();
	g.mounted = true;
	Check(!KickAllowed(s, g), "on a horse");
	g = Free();
	g.swimming = true;
	Check(!KickAllowed(s, g), "swimming");
	g = Free();
	g.knockedDown = true;
	Check(!KickAllowed(s, g), "knocked down");
	g = Free();
	g.exhausted = true;
	Check(!KickAllowed(s, g), "exhausted");
	KickSettings off;
	off.enabled = false;
	Check(!KickAllowed(off, Free()), "[Hands] Kick=0: never");
}

// Steps `seconds` of frames, the button up; answers the last frame and the
// largest weight seen.
KickFrame Run(KickState& k, const KickSettings& s, float seconds, bool allowed, float* most = nullptr,
              int* striking = nullptr) {
	KickFrame f;
	const int frames = static_cast<int>(seconds / kFrame + 0.5f);
	for (int i = 0; i < frames; ++i) {
		f = StepKick(k, s, false, allowed, kFrame);
		if (most != nullptr && f.weight > *most) {
			*most = f.weight;
		}
		if (striking != nullptr && f.striking) {
			++*striking;
		}
	}
	return f;
}

void TestTiming() {
	std::printf("the press: out, held, back, then the cooldown\n");
	const KickSettings s;
	KickState k;
	KickFrame f = StepKick(k, s, false, true, kFrame);
	Check(!f.started && f.weight == 0.0f && k.phase == KickPhase::Ready, "no press: nothing");
	f = StepKick(k, s, true, true, kFrame);
	Check(f.started && k.phase == KickPhase::Out && k.serial == 1, "the press starts one kick");
	Check(f.weight > 0.0f && f.weight < 0.2f, "the foot just leaving");
	float most = 0.0f;
	f = Run(k, s, s.outSeconds, true, &most);
	Check(k.phase == KickPhase::Held && Near(most, 1.0f), "out in KickOut's time: all the way");
	f = Run(k, s, s.holdSeconds, true);
	Check(k.phase == KickPhase::Back, "held for its time, then back");
	f = Run(k, s, s.backSeconds + kFrame, true);
	Check(k.phase == KickPhase::Ready && f.weight == 0.0f, "back in its time: the leg the animation's again");

	KickState k2;
	StepKick(k2, s, true, true, kFrame);
	f = StepKick(k2, s, true, true, kFrame);
	Check(!f.started && k2.serial == 1, "the button held: still one kick");
	Run(k2, s, s.outSeconds + s.holdSeconds + s.backSeconds + 0.05f, true);
	f = StepKick(k2, s, true, true, kFrame);
	Check(!f.started && k2.phase == KickPhase::Ready, "a press within KickCooldownSeconds of the last: none");
	Run(k2, s, s.cooldownSeconds, true);
	f = StepKick(k2, s, true, true, kFrame);
	Check(f.started && k2.serial == 2, "after the cooldown: the next");

	KickState k3;
	f = StepKick(k3, s, true, false, kFrame);
	Check(!f.started && k3.phase == KickPhase::Ready, "a press the gate refuses: no kick");
	f = StepKick(k3, s, true, true, kFrame);
	Check(f.started, "and the refused press is no cooldown");

	KickState k4;
	StepKick(k4, s, true, true, kFrame);
	Run(k4, s, 0.05f, true);
	f = StepKick(k4, s, false, false, kFrame);
	Check(k4.phase == KickPhase::Ready && f.weight == 0.0f && !f.striking,
	      "the gate closing mid-kick (a menu): the leg given back at once");
}

void TestStriking() {
	std::printf("the foot strikes only through the second half of its way out and the hold\n");
	const KickSettings s;
	KickState k;
	KickFrame f = StepKick(k, s, true, true, kFrame);
	Check(!f.striking, "just leaving: not yet");
	int striking = 0;
	Run(k, s, s.outSeconds * 0.4f, true, nullptr, &striking);
	Check(striking == 0, "the first half of the way out: not yet");
	Run(k, s, s.outSeconds * 0.6f + s.holdSeconds, true, nullptr, &striking);
	const int expected = static_cast<int>((s.outSeconds * 0.5f + s.holdSeconds) / kFrame);
	Check(striking >= expected - 2 && striking <= expected + 2, "the second half and the hold: striking");
	striking = 0;
	Run(k, s, s.backSeconds, true, nullptr, &striking);
	Check(striking == 0, "on the way back: never");
}

void TestFootTarget() {
	std::printf("the foot's target: ahead of the hip along the facing, above hip height\n");
	const KickSettings s;
	const NiPoint3 hip{100.0f, 200.0f, 60.0f};
	const NiPoint3 resting{105.0f, 200.0f, 2.0f};
	const NiPoint3 north{0.0f, 1.0f, 0.0f};
	const float leg = 61.0f;  // thigh 33.67 + calf 27.33
	const NiPoint3 peak = KickPeak(hip, north, leg, s);
	const float ahead = peak.y - hip.y;
	const float peakDegrees = math::Atan2(peak.z - hip.z, ahead) * math::kRadiansToDegrees;
	Check(Near(peak.x, hip.x), "straight ahead: no step sideways");
	Check(ahead > 0.7f * leg && ahead < leg, "ahead by most of the leg's length");
	// Nadi, 2026-11-03: "auch wenn ich gerade schaue sehe ich keinen kick" -
	// at the hip's height the foot sat about 51 degrees under the eyes, past
	// the view's lower edge (about 49); 30 degrees above the hip puts it near 40.
	Check(s.dropDegrees == -30.0f && Near(peakDegrees, 30.0f, 0.1f), "looking ahead: 30 degrees above the hip");
	const float reach = ik::Length(peak - hip);
	Check(reach < leg * 0.99f, "short of the leg's full length: the knee keeps a bend to solve");
	Check(Near(reach, leg * s.reachShare, 0.05f), "at KickReachShare of the leg");

	// The foot goes up where the eyes look up (Nadi, 2026-11-01: "der kick
	// fuss sollte whl höher gehen je nachdem wo man hinschaut"): KickLookShare
	// of the look's pitch, up to KickHighestDegrees above the hip. Looking down
	// leaves it (Nadi, 2026-11-03: looking down to find the foot lowered it 1.5
	// times as far - 47 degrees under the hip in his log - so it fled the view).
	Check(s.lookShare == 1.5f && s.highestDegrees == 50.0f && s.lowestDegrees == 50.0f,
	      "the look up moves the foot by 1.5 times its pitch, at most 50 degrees above the hip");
	const float up10 = 10.0f * math::kDegreesToRadians;
	const NiPoint3 lookUp = KickPeak(hip, NiPoint3{0.0f, math::Cos(up10), math::Sin(up10)}, leg, s);
	const float upDegrees = math::Atan2(lookUp.z - hip.z, lookUp.y - hip.y) * math::kRadiansToDegrees;
	Check(lookUp.z > peak.z && Near(upDegrees, 45.0f, 0.1f), "looking 10 degrees up: the foot 45 degrees above the hip");
	const NiPoint3 steepUp = KickPeak(hip, NiPoint3{0.0f, 3.0f, 4.0f}, leg, s);
	const float steepDegrees = math::Atan2(steepUp.z - hip.z, steepUp.y - hip.y) * math::kRadiansToDegrees;
	Check(Near(steepDegrees, 50.0f, 0.1f), "looking far up: no higher than 50 degrees above the hip");
	const float down30 = -30.0f * math::kDegreesToRadians;
	const NiPoint3 lookDown = KickPeak(hip, NiPoint3{0.0f, math::Cos(down30), math::Sin(down30)}, leg, s);
	Check(Near(lookDown.y, peak.y) && Near(lookDown.z, peak.z), "looking 30 degrees down: as looking ahead");
	const NiPoint3 lookFarDown = KickPeak(hip, NiPoint3{0.0f, 1.0f, -3.0f}, leg, s);
	Check(Near(lookFarDown.y, peak.y) && Near(lookFarDown.z, peak.z), "looking steeply down: as looking ahead");
	// KickLowestDegrees still bounds a KickDropDegrees set low by hand.
	KickSettings low = s;
	low.dropDegrees = 70.0f;
	const NiPoint3 lowPeak = KickPeak(hip, north, leg, low);
	Check(Near(math::Atan2(hip.z - lowPeak.z, lowPeak.y - hip.y) * math::kRadiansToDegrees, 50.0f, 0.1f),
	      "KickDropDegrees 70: no lower than KickLowestDegrees, 50 below the hip");
	KickSettings high = s;
	high.dropDegrees = -70.0f;
	const NiPoint3 highPeak = KickPeak(hip, north, leg, high);
	Check(Near(math::Atan2(highPeak.z - hip.z, highPeak.y - hip.y) * math::kRadiansToDegrees, 50.0f, 0.1f),
	      "KickDropDegrees -70: no higher than KickHighestDegrees");
	KickSettings flat = s;
	flat.lookShare = 0.0f;
	const NiPoint3 level = KickPeak(hip, NiPoint3{0.0f, 3.0f, 4.0f}, leg, flat);
	Check(Near(level.x, peak.x) && Near(level.y, peak.y) && Near(level.z, peak.z),
	      "KickLookShare=0: a facing tilted up or down is taken level, as before");
	KickSettings odd = s;
	odd.lookShare = std::numeric_limits<float>::quiet_NaN();
	Check(Near(KickPeak(hip, NiPoint3{0.0f, 3.0f, 4.0f}, leg, odd).z, peak.z), "no share: level");
	const NiPoint3 none = KickPeak(hip, NiPoint3{0.0f, 0.0f, 1.0f}, leg, s);
	Check(Near(none.x, hip.x) && Near(none.y, hip.y) && none.z < hip.z, "no facing: straight down, no wild kick");

	Check(KickFootTarget(resting, peak, 0.0f).z == resting.z, "weight 0: where the animation has it");
	const NiPoint3 full = KickFootTarget(resting, peak, 1.0f);
	Check(Near(full.y, peak.y) && Near(full.z, peak.z), "weight 1: the peak");
	const NiPoint3 half = KickFootTarget(resting, peak, 0.5f);
	Check(Near(half.y, 0.5f * (resting.y + peak.y)) && Near(half.z, 0.5f * (resting.z + peak.z)), "between: between");
	Check(KickFootTarget(resting, peak, 2.0f).z == full.z && KickFootTarget(resting, peak, -1.0f).z == resting.z,
	      "the weight held to 0..1");
}

float ElevationDegrees(const NiPoint3& hip, const NiPoint3& p) {
	const float dx = p.x - hip.x;
	const float dy = p.y - hip.y;
	return math::Atan2(p.z - hip.z, math::Sqrt(dx * dx + dy * dy)) * math::kRadiansToDegrees;
}

NiPoint3 LookDown(float degrees) {
	const float r = -degrees * math::kDegreesToRadians;
	return NiPoint3{0.0f, math::Cos(r), math::Sin(r)};
}

void TestAimDown() {
	std::printf("looking down past the foot: the foot goes under the look\n");
	// Looking down, the foot kept its height ahead and did not come to where
	// the eyes looked (2026-11-03, headset). Given the eyes, a look steeper
	// than the foot's own place puts the foot on the look's line, on its
	// reach round the hip.
	const KickSettings s;
	const NiPoint3 hip{0.0f, 0.0f, 60.0f};
	const NiPoint3 eye{0.0f, -5.0f, 122.0f};  // about as the harness measured: the foot level 28.6 under the eyes
	const float leg = 61.0f;
	const float reach = leg * s.reachShare;
	const NiPoint3 plain = KickPeak(hip, NiPoint3{0.0f, 1.0f, 0.0f}, leg, s);
	const NiPoint3 level = KickPeak(hip, NiPoint3{0.0f, 1.0f, 0.0f}, leg, s, &eye);
	Check(Near(level.y, plain.y) && Near(level.z, plain.z), "looking level: as without the eyes");
	const float footDepression = math::Atan2(eye.z - plain.z, plain.y - eye.y) * math::kRadiansToDegrees;
	const NiPoint3 shallow = KickPeak(hip, LookDown(footDepression - 3.0f), leg, s, &eye);
	Check(Near(shallow.y, plain.y) && Near(shallow.z, plain.z), "looking down less steeply than the foot sits: as level");
	const NiPoint3 justPast = KickPeak(hip, LookDown(footDepression + 0.5f), leg, s, &eye);
	Check(ik::Length(justPast - plain) < 2.0f, "just past the foot's place: no jump");
	const NiPoint3 down = KickPeak(hip, LookDown(55.0f), leg, s, &eye);
	const float lookDepression = math::Atan2(eye.z - down.z, down.y - eye.y) * math::kRadiansToDegrees;
	Check(Near(lookDepression, 55.0f, 0.2f), "looking 55 down: the foot on the look's line");
	Check(Near(ik::Length(down - hip), reach, 0.05f) && down.y > hip.y, "... at the leg's reach, ahead");
	Check(down.z < plain.z, "... lower than looking level");
	const NiPoint3 steep = KickPeak(hip, LookDown(88.0f), leg, s, &eye);
	Check(Near(ElevationDegrees(hip, steep), -50.0f, 0.1f) && steep.y > hip.y,
	      "looking straight down: no lower than KickLowestDegrees, still ahead");
	KickSettings flat = s;
	flat.lookShare = 0.0f;
	const NiPoint3 off = KickPeak(hip, LookDown(55.0f), leg, flat, &eye);
	const NiPoint3 offPlain = KickPeak(hip, NiPoint3{0.0f, 1.0f, 0.0f}, leg, flat);
	Check(Near(off.y, offPlain.y) && Near(off.z, offPlain.z), "KickLookShare=0: the look moves nothing, down neither");
	const NiPoint3 far{0.0f, -5.0f, 400.0f};
	const NiPoint3 missed = KickPeak(hip, LookDown(20.0f), leg, s, &far);
	Check(Near(missed.y, plain.y) && Near(missed.z, plain.z), "eyes so high the look misses the reach: as level");
	const NiPoint3 nan{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
	const NiPoint3 bad = KickPeak(hip, LookDown(55.0f), leg, s, &nan);
	Check(Near(bad.y, plain.y) && Near(bad.z, plain.z), "eyes no number: as level");
}

void TestAimAtLook() {
	std::printf("the kick aimed at the part of someone the eyes look at\n");
	const KickSettings s;
	Check(s.aimUnits == 110.0f, "KickAimUnits 110 by default: the look's reach for a target");
	const NiPoint3 eye{0.0f, 0.0f, 122.0f};
	const NiPoint3 ahead{0.0f, 1.0f, 0.0f};
	BladeBodies bodies;
	bodies.Add(NiPoint3{0.0f, 70.0f, 70.0f}, NiPoint3{0.0f, 70.0f, 110.0f}, 10.0f, 7u, BodyPart::Torso);
	bodies.Add(NiPoint3{0.0f, 70.0f, 118.0f}, NiPoint3{0.0f, 70.0f, 126.0f}, 7.0f, 7u, BodyPart::Head);
	bodies.Add(NiPoint3{0.0f, 72.0f, 5.0f}, NiPoint3{0.0f, 72.0f, 60.0f}, 6.0f, 7u, BodyPart::LeftLeg);
	const KickLookHit head = KickLookTarget(bodies, eye, ahead, s.aimUnits);
	Check(head.hit && head.actor == 7u && head.part == BodyPart::Head && Near(head.point.y, 63.0f, 0.05f),
	      "looking level at the head: the head, where the look enters it");
	const KickLookHit chest = KickLookTarget(bodies, eye, LookDown(25.0f), s.aimUnits);
	Check(chest.hit && chest.part == BodyPart::Torso, "looking 25 down: the torso");
	const KickLookHit knee = KickLookTarget(bodies, eye, LookDown(52.0f), s.aimUnits);
	Check(knee.hit && knee.part == BodyPart::LeftLeg, "looking 52 down: the leg");
	Check(!KickLookTarget(bodies, eye, NiPoint3{0.0f, -1.0f, 0.0f}, s.aimUnits).hit, "looking away: no one");
	Check(!KickLookTarget(bodies, eye, ahead, 50.0f).hit, "someone past the look's reach: no one");
	Check(!KickLookTarget(bodies, eye, ahead, 0.0f).hit, "KickAimUnits 0: no aim");
	BladeBodies around;
	around.Add(NiPoint3{0.0f, -5.0f, 100.0f}, NiPoint3{0.0f, 5.0f, 100.0f}, 30.0f, 9u, BodyPart::Torso);
	around.Add(NiPoint3{0.0f, 70.0f, 110.0f}, NiPoint3{0.0f, 70.0f, 130.0f}, 7.0f, 7u, BodyPart::Head);
	const KickLookHit past = KickLookTarget(around, eye, ahead, s.aimUnits);
	Check(past.hit && past.actor == 7u, "a capsule round the eyes (the player's own) is passed over");
	BladeBodies two = bodies;
	two.Add(NiPoint3{0.0f, 40.0f, 110.0f}, NiPoint3{0.0f, 40.0f, 130.0f}, 7.0f, 8u, BodyPart::Head);
	Check(KickLookTarget(two, eye, ahead, s.aimUnits).actor == 8u, "two in the look: the nearer");

	// The peak toward the target: along the hip's line to it, at the reach.
	const NiPoint3 hip{0.0f, 0.0f, 60.0f};
	const float reach = 61.0f * s.reachShare;
	NiPoint3 peak;
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 30.0f, 85.0f}, reach, s, peak), "a target ahead, close: a peak");
	const NiPoint3 toTarget{0.0f, 30.0f, 25.0f};
	const float cosine = (peak.y - hip.y) * toTarget.y / (reach * ik::Length(toTarget)) +
	                     (peak.z - hip.z) * toTarget.z / (reach * ik::Length(toTarget));
	Check(Near(ik::Length(peak - hip), reach, 0.05f) && Near(cosine, 1.0f, 1e-3f), "on the hip's line to it, at the reach");
	// Aimed high at someone further off (the harness, 2026-11-03: the look on
	// a bandit's neck 52 units away raised the foot so far it fell short of
	// him): the foot rises only as high as still reaches past where the look
	// met him, by KickAimDepthUnits.
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 48.0f, 120.0f}, reach, s, peak) &&
	          Near(peak.y - hip.y, 48.0f + kKickAimDepthUnits, 0.05f) && peak.z > hip.z,
	      "a neck 48 ahead and 60 up: the foot as high as still reaches 48 + the depth ahead");
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 48.0f, 0.0f}, reach, s, peak) &&
	          Near(peak.y - hip.y, 48.0f + kKickAimDepthUnits, 0.05f) && peak.z < hip.z,
	      "a knee as far and as low: the same reach, under the hip");
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 63.0f, 90.0f}, reach, s, peak) && Near(peak.z, hip.z, 0.05f) &&
	          Near(peak.y - hip.y, reach, 0.05f),
	      "someone past the reach: the foot level, as far as it goes");
	Check(KickPeakAt(hip, ahead, NiPoint3{30.0f, 60.0f, 60.0f}, reach, s, peak) && peak.x > 0.0f && Near(peak.z, hip.z, 0.05f),
	      "a target off to the side: the kick turns to it");
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 20.0f, 300.0f}, reach, s, peak) && Near(ElevationDegrees(hip, peak), 50.0f, 0.1f),
	      "a target high over the hip: no higher than KickHighestDegrees");
	Check(KickPeakAt(hip, ahead, NiPoint3{0.0f, 20.0f, -200.0f}, reach, s, peak) && Near(ElevationDegrees(hip, peak), -50.0f, 0.1f),
	      "a target far under it: no lower than KickLowestDegrees");
	Check(!KickPeakAt(hip, ahead, NiPoint3{0.0f, -40.0f, 60.0f}, reach, s, peak), "a target behind the hip: none, the kick by the look");
	Check(!KickPeakAt(hip, ahead, NiPoint3{0.0f, 0.0f, 120.0f}, reach, s, peak), "straight over the hip: none");
	Check(!KickPeakAt(hip, ahead, NiPoint3{std::numeric_limits<float>::quiet_NaN(), 60.0f, 60.0f}, reach, s, peak),
	      "no number: none");

	// KickPeak with an aim: the aim's peak; a refused aim: as without.
	const NiPoint3 target{0.0f, 63.0f, 90.0f};
	NiPoint3 want;
	KickPeakAt(hip, ahead, target, 61.0f * s.reachShare, s, want);
	const NiPoint3 aimed = KickPeak(hip, ahead, 61.0f, s, &eye, &target);
	Check(Near(aimed.y, want.y) && Near(aimed.z, want.z), "KickPeak given the aim: toward it");
	const NiPoint3 behind{0.0f, -40.0f, 60.0f};
	const NiPoint3 plainPeak = KickPeak(hip, ahead, 61.0f, s, &eye);
	const NiPoint3 refused = KickPeak(hip, ahead, 61.0f, s, &eye, &behind);
	Check(Near(refused.y, plainPeak.y) && Near(refused.z, plainPeak.z), "an aim behind: by the look, as without");
}

void TestFootInView() {
	std::printf("the foot's place in the view: how far under the eyes' level and under the look\n");
	const NiPoint3 eye{0.0f, 0.0f, 130.0f};
	// 40 ahead, 40 down: 45 degrees under the eyes' level.
	const NiPoint3 foot{0.0f, 40.0f, 90.0f};
	const KickFootView ahead = KickFootViewFor(eye, NiPoint3{0.0f, 1.0f, 0.0f}, foot);
	Check(Near(ahead.belowLevelDegrees, 45.0f, 0.1f) && Near(ahead.belowLookDegrees, 45.0f, 0.1f),
	      "looking level: 45 under the level and under the look");
	const float down20 = -20.0f * math::kDegreesToRadians;
	const KickFootView lookingDown =
		KickFootViewFor(eye, NiPoint3{0.0f, math::Cos(down20), math::Sin(down20)}, foot);
	Check(Near(lookingDown.belowLevelDegrees, 45.0f, 0.1f) && Near(lookingDown.belowLookDegrees, 25.0f, 0.1f),
	      "looking 20 down: still 45 under the level, 25 under the look");
	const NiPoint3 aboveFoot{0.0f, 40.0f, 170.0f};
	Check(Near(KickFootViewFor(eye, NiPoint3{0.0f, 1.0f, 0.0f}, aboveFoot).belowLevelDegrees, -45.0f, 0.1f),
	      "a foot above the eyes: negative");
	const KickFootView under = KickFootViewFor(eye, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 50.0f});
	Check(Near(under.belowLevelDegrees, 90.0f, 0.1f), "straight under the eyes: 90");
	const KickFootView noLook = KickFootViewFor(eye, NiPoint3{0.0f, 0.0f, 0.0f}, foot);
	Check(Near(noLook.belowLookDegrees, noLook.belowLevelDegrees, 0.1f), "no facing: the look taken level");
}

void TestKind() {
	std::printf("standing it staggers, running it knocks down\n");
	const KickSettings s;
	Check(KickKindFor(s, false, false) == ShoveKind::Light, "standing or walking: the stagger");
	Check(KickKindFor(s, true, false) == ShoveKind::Hard, "running: the knockdown");
	Check(KickKindFor(s, false, true) == ShoveKind::Hard, "in a jump: the knockdown (decision 3)");
	std::printf("the character controller's state: in the air, swimming\n");
	Check(!KickAirborne(kCharacterStateOnGround) && KickAirborne(kCharacterStateJumping) &&
	          KickAirborne(kCharacterStateInAir),
	      "jumping or in the air: airborne; on the ground: not");
	Check(!KickAirborne(kCharacterStateSwimming) && !KickAirborne(-1), "swimming or unread: not airborne");
	Check(KickSwimming(kCharacterStateSwimming) && !KickSwimming(kCharacterStateOnGround) && !KickSwimming(-1),
	      "swimming read from the state; unread: not swimming");
	std::printf("the sounds: the game's own\n");
	Check(KickHitSound(ShoveKind::Light, false) == kSoundHitHand, "a stagger: the fist's hit (WPNHitHand)");
	Check(KickHitSound(ShoveKind::Hard, false) == kSoundHitBluntFlesh, "a knockdown: the blunt weapon on flesh");
	Check(KickHitSound(ShoveKind::None, false) == 0, "no hit: none");
	Check(KickHitSound(ShoveKind::Light, true) == 0,
	      "the engine's fist hit landed: its own sound, none of ours on top");
	Check(KickHitSound(ShoveKind::Hard, true) == kSoundHitBluntFlesh, "a knockdown keeps the heavy one on top");
	std::printf("the engine's hit as a fist: the weapon asked of the player's process answered none\n");
	Check(KickAnswersFists(0x5000, 0x5000), "the kicking player's process while the kick strikes: no weapon");
	Check(!KickAnswersFists(0x6000, 0x5000), "anyone else's process: their weapon as ever");
	Check(!KickAnswersFists(0x5000, 0), "no kick striking: the player's weapon as ever");
}

void TestTouches() {
	std::printf("the foot's sweep meets everyone in its way, each once a kick (the cleave)\n");
	BladeBodies bodies;
	const UInt32 a = 0x1000;
	const UInt32 b = 0x2000;
	const UInt32 c = 0x3000;
	// a and b side by side 50 units ahead, c far off; upright torsos.
	bodies.Add(NiPoint3{-10.0f, 50.0f, 20.0f}, NiPoint3{-10.0f, 50.0f, 60.0f}, 12.0f, a);
	bodies.Add(NiPoint3{-10.0f, 50.0f, 0.0f}, NiPoint3{-10.0f, 50.0f, 20.0f}, 6.0f, a);
	bodies.Add(NiPoint3{12.0f, 52.0f, 20.0f}, NiPoint3{12.0f, 52.0f, 60.0f}, 12.0f, b);
	bodies.Add(NiPoint3{0.0f, 300.0f, 20.0f}, NiPoint3{0.0f, 300.0f, 60.0f}, 12.0f, c);
	KickMet met;
	UInt32 hit[kKickMetMax] = {};
	UInt32 n = KickTouches(bodies, NiPoint3{0.0f, 0.0f, 40.0f}, NiPoint3{0.0f, 20.0f, 40.0f}, 6.0f, met, 1, hit,
	                       kKickMetMax);
	Check(n == 0, "the foot still short of them: no one");
	n = KickTouches(bodies, NiPoint3{0.0f, 20.0f, 40.0f}, NiPoint3{0.0f, 45.0f, 40.0f}, 6.0f, met, 1, hit,
	                kKickMetMax);
	Check(n == 2 && ((hit[0] == a && hit[1] == b) || (hit[0] == b && hit[1] == a)),
	      "swept into the two side by side: both, each once though a has two capsules");
	n = KickTouches(bodies, NiPoint3{0.0f, 45.0f, 40.0f}, NiPoint3{0.0f, 46.0f, 40.0f}, 6.0f, met, 1, hit,
	                kKickMetMax);
	Check(n == 0, "still in them the next frame: not again this kick");
	n = KickTouches(bodies, NiPoint3{0.0f, 45.0f, 40.0f}, NiPoint3{0.0f, 46.0f, 40.0f}, 6.0f, met, 2, hit,
	                kKickMetMax);
	Check(n == 2, "the next kick: met anew");
	n = KickTouches(bodies, NiPoint3{0.0f, 45.0f, 40.0f}, NiPoint3{0.0f, 46.0f, 40.0f}, 6.0f, met, 3, hit, 1);
	Check(n == 1, "no more than the caller has room for");
	KickMet none;
	BladeBodies empty;
	Check(KickTouches(empty, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 50.0f, 0.0f}, 6.0f, none, 1, hit,
	                  kKickMetMax) == 0,
	      "no one near: no one");
}

void TestShoveSettings() {
	std::printf("the kick's shove: told as a hit standing too, the fatigue its own\n");
	ShoveSettings shove;
	shove.countsAsHit = false;
	const KickSettings s;
	const ShoveSettings k = KickShoveSettings(shove, s, false);
	Check(k.countsAsHit && k.lightCountsAsHit, "a kick is always a hit (decision 1), whatever ShoveCountsAsHit says");
	const ShoveSettings engine = KickShoveSettings(shove, s, true);
	Check(!engine.countsAsHit && !engine.lightCountsAsHit,
	      "the engine's own hit landed: it told them already, the shove does not again");
	Check(k.fatigueLight == 0.0f && k.fatigueHard == 0.0f, "no fatigue per person: the kick's own, once a kick");
	Check(k.hardForce == shove.hardForce && k.distance == shove.distance, "the push and the throw the shove's");
}

// Steps `seconds` of frames with the B held (`held`), answers the last frame
// and counts the striking frames.
KickFrame RunHeld(KickState& k, const KickSettings& s, float seconds, bool held, int* striking = nullptr) {
	KickFrame f;
	const int frames = static_cast<int>(seconds / kFrame + 0.5f);
	for (int i = 0; i < frames; ++i) {
		f = StepKick(k, s, false, true, kFrame, held);
		if (striking != nullptr && f.striking) {
			++*striking;
		}
	}
	return f;
}

void TestHeld() {
	std::printf("the B held: the foot stays out longer, up to KickHoldMaxSeconds\n");
	const KickSettings s;
	Check(s.holdMaxSeconds > s.holdSeconds, "the held kick's hold is longer than a tap's");

	// Held past the tap's hold: still out, still striking.
	KickState k;
	StepKick(k, s, true, true, kFrame, true);
	RunHeld(k, s, s.outSeconds + s.holdSeconds + 0.1f, true);
	KickFrame f = RunHeld(k, s, kFrame, true);
	Check(k.phase == KickPhase::Held && f.weight == 1.0f && f.striking, "held past KickHoldSeconds: the foot out");
	// Let go: back at once, the whole way back, not skipped.
	f = RunHeld(k, s, kFrame, false);
	Check(k.phase == KickPhase::Back && f.weight > 0.9f, "let go: the way back begins");
	int striking = 0;
	RunHeld(k, s, s.backSeconds * 0.5f, false, &striking);
	Check(k.phase == KickPhase::Back && striking == 0, "half way back, not striking");
	RunHeld(k, s, s.backSeconds * 0.5f + kFrame, false);
	Check(k.phase == KickPhase::Ready, "back in KickBackSeconds");

	// Held on and on: back at KickHoldMaxSeconds.
	KickState m;
	StepKick(m, s, true, true, kFrame, true);
	RunHeld(m, s, s.outSeconds + s.holdMaxSeconds - 0.05f, true);
	Check(m.phase == KickPhase::Held, "just short of KickHoldMaxSeconds: still out");
	RunHeld(m, s, 0.1f, true);
	Check(m.phase == KickPhase::Back, "past KickHoldMaxSeconds, still held: back");
	RunHeld(m, s, s.backSeconds + kFrame, true);
	Check(m.phase == KickPhase::Ready && m.serial == 1, "held through: one kick, no second");

	// A tap: let go before the peak keeps the tap's hold.
	KickState t;
	StepKick(t, s, true, true, kFrame, true);
	RunHeld(t, s, s.outSeconds + s.holdSeconds + 2.0f * kFrame, false);
	Check(t.phase == KickPhase::Back, "a tap: back after KickHoldSeconds");

	// KickHoldMaxSeconds below KickHoldSeconds: the hold never shorter than a tap's.
	KickSettings shortMax;
	shortMax.holdMaxSeconds = 0.0f;
	KickState n;
	StepKick(n, shortMax, true, true, kFrame, true);
	RunHeld(n, shortMax, shortMax.outSeconds + shortMax.holdSeconds - 2.0f * kFrame, true);
	Check(n.phase == KickPhase::Held, "KickHoldMaxSeconds=0: the tap's hold still");
	RunHeld(n, shortMax, 4.0f * kFrame, true);
	Check(n.phase == KickPhase::Back, "and then back, held or not");

	// The gate closing while held: given back at once.
	KickState g;
	StepKick(g, s, true, true, kFrame, true);
	RunHeld(g, s, s.outSeconds + 0.2f, true);
	f = StepKick(g, s, false, false, kFrame, true);
	Check(g.phase == KickPhase::Ready && f.weight == 0.0f, "held, the gate closes: the leg given back");
}

}  // namespace

int main() {
	TestGate();
	TestTiming();
	TestHeld();
	TestStriking();
	TestFootTarget();
	TestAimDown();
	TestAimAtLook();
	TestFootInView();
	TestKind();
	TestTouches();
	TestShoveSettings();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
