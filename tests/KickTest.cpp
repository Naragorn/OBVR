#include <cstdio>

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
	std::printf("the foot's target: ahead of the hip along the facing, a little below hip height\n");
	const KickSettings s;
	const NiPoint3 hip{100.0f, 200.0f, 60.0f};
	const NiPoint3 resting{105.0f, 200.0f, 2.0f};
	const NiPoint3 north{0.0f, 1.0f, 0.0f};
	const float leg = 61.0f;  // thigh 33.67 + calf 27.33
	const NiPoint3 peak = KickPeak(hip, north, leg, s);
	const float ahead = peak.y - hip.y;
	const float below = hip.z - peak.z;
	Check(Near(peak.x, hip.x), "straight ahead: no step sideways");
	Check(ahead > 0.8f * leg && ahead < leg, "ahead by most of the leg's length");
	Check(below > 0.0f && below < 0.5f * leg, "below the hip, above the knee's height");
	const float reach = ik::Length(peak - hip);
	Check(reach < leg * 0.99f, "short of the leg's full length: the knee keeps a bend to solve");
	Check(Near(reach, leg * s.reachShare, 0.05f), "at KickReachShare of the leg");

	const NiPoint3 tilted{0.0f, 3.0f, 4.0f};
	const NiPoint3 level = KickPeak(hip, tilted, leg, s);
	Check(Near(level.x, peak.x) && Near(level.y, peak.y) && Near(level.z, peak.z),
	      "a facing tilted up or down is taken level");
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
	Check(KickHitSound(ShoveKind::Light) == kSoundHitHand, "a stagger: the fist's hit (WPNHitHand)");
	Check(KickHitSound(ShoveKind::Hard) == kSoundHitBluntFlesh, "a knockdown: the blunt weapon on flesh");
	Check(KickHitSound(ShoveKind::None) == 0, "no hit: none");
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
	const ShoveSettings k = KickShoveSettings(shove, s);
	Check(k.countsAsHit && k.lightCountsAsHit, "a kick is always a hit (decision 1), whatever ShoveCountsAsHit says");
	Check(k.fatigueLight == 0.0f && k.fatigueHard == 0.0f, "no fatigue per person: the kick's own, once a kick");
	Check(k.hardForce == shove.hardForce && k.distance == shove.distance, "the push and the throw the shove's");
}

}  // namespace

int main() {
	TestGate();
	TestTiming();
	TestStriking();
	TestFootTarget();
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
