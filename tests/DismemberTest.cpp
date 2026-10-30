#include <cstdio>
#include <cstring>
#include <limits>

#include "game/DismemberLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

void TestEdge() {
	std::printf("an edge: a blade, or an axe by its model\n");
	Check(WeaponHasEdge(kCutWeaponBladeOneHand, "Weapons\\Iron\\LongSword.NIF"), "a one-handed blade");
	Check(WeaponHasEdge(kCutWeaponBladeTwoHand, "Weapons\\Steel\\Claymore.NIF"), "a two-handed blade");
	Check(WeaponHasEdge(kCutWeaponBluntOneHand, "Weapons\\Iron\\WarAxe.NIF"), "a war axe, typed blunt");
	Check(WeaponHasEdge(kCutWeaponBluntTwoHand, "weapons\\daedric\\battleaxe.nif"), "a battle axe, any case");
	Check(!WeaponHasEdge(kCutWeaponBluntOneHand, "Weapons\\Iron\\Mace.NIF"), "a mace: none");
	Check(!WeaponHasEdge(kCutWeaponBluntTwoHand, "Weapons\\Steel\\WarHammer.NIF"), "a warhammer: none");
	Check(!WeaponHasEdge(kCutWeaponBluntOneHand, nullptr), "blunt without a model path: none");
	Check(!WeaponHasEdge(kCutWeaponStaff, "Weapons\\Staff\\Axe.NIF"), "a staff never, whatever its model");
	Check(!WeaponHasEdge(kCutWeaponBow, "Weapons\\Bow\\Axe.NIF"), "a bow never");
	Check(!WeaponHasEdge(-1, "Weapons\\Axe.NIF"), "no weapon (fists): none");
}

void TestNeckTouched() {
	std::printf("the blade's sweep through the neck\n");
	const NiPoint3 spine{0.0f, 0.0f, 100.0f};
	const NiPoint3 head{0.0f, 0.0f, 115.0f};
	const float radius = 5.0f;
	// A blade across the neck from left to right, 60 units long.
	const NiPoint3 gripBefore{-30.0f, -40.0f, 108.0f};
	const NiPoint3 tipBefore{-30.0f, 20.0f, 108.0f};
	const NiPoint3 gripNow{30.0f, -40.0f, 108.0f};
	const NiPoint3 tipNow{30.0f, 20.0f, 108.0f};
	Check(NeckTouched(spine, head, radius, gripBefore, tipBefore, gripNow, tipNow),
	      "swept from one side of the neck to the other: through it");
	const NiPoint3 high{0.0f, 0.0f, 160.0f};
	Check(!NeckTouched(spine, head, radius, gripBefore + high - NiPoint3{0.0f, 0.0f, 108.0f},
	                   tipBefore + high - NiPoint3{0.0f, 0.0f, 108.0f}, gripNow + high - NiPoint3{0.0f, 0.0f, 108.0f},
	                   tipNow + high - NiPoint3{0.0f, 0.0f, 108.0f}),
	      "the same sweep over the head: not");
	Check(NeckTouched(spine, head, radius, gripNow, tipNow, gripNow, tipNow) == false,
	      "standing still beside the neck: not");
	const NiPoint3 inGrip{-30.0f, 0.0f, 108.0f};
	const NiPoint3 inTip{30.0f, 0.0f, 108.0f};
	Check(NeckTouched(spine, head, radius, inGrip, inTip, inGrip, inTip), "lying in the neck now: touched");
}

CutInput Lethal() {
	CutInput in;
	in.enabled = true;
	in.dead = true;
	in.onNeck = true;
	in.edge = true;
	in.speedMetres = 4.0f;
	return in;
}

void TestDecide() {
	std::printf("the rule: the killing blow or a corpse, on the neck, with an edge, fast enough\n");
	const CutSettings s;
	Check(DecideCut(s, Lethal()) == CutVerdict::Cut, "dead, the neck, an edge, fast: the head comes off");
	CutInput in = Lethal();
	in.enabled = false;
	Check(DecideCut(s, in) == CutVerdict::Off, "Dismemberment=0: never");
	in = Lethal();
	in.alreadyCut = true;
	Check(DecideCut(s, in) == CutVerdict::AlreadyCut, "a head already off: nothing more");
	in = Lethal();
	in.dead = false;
	Check(DecideCut(s, in) == CutVerdict::Living, "alive: the living keep their parts");
	in = Lethal();
	in.edge = false;
	Check(DecideCut(s, in) == CutVerdict::NoEdge, "a mace or the fists: nothing cut");
	in = Lethal();
	in.onNeck = false;
	Check(DecideCut(s, in) == CutVerdict::NotOnNeck, "the torso: nothing cut");
	in = Lethal();
	in.speedMetres = s.minSpeedMetres * 0.9f;
	Check(DecideCut(s, in) == CutVerdict::TooSlow, "slower than DismemberMinSpeed: nothing cut");
	in.speedMetres = s.minSpeedMetres;
	Check(DecideCut(s, in) == CutVerdict::Cut, "at the threshold: cut");
	CutSettings kills = s;
	kills.cutKills = true;
	in = Lethal();
	in.dead = false;
	Check(DecideCut(kills, in) == CutVerdict::Cut, "CutKills=1: the living too");
	in.essential = true;
	Check(DecideCut(kills, in) == CutVerdict::Essential, "but never an essential one");
	Check(CutVerdictName(CutVerdict::TooSlow)[0] != '\0', "every verdict has a name for the log");
}

void TestKillingBlow() {
	std::printf("the killing blow: the neck touched alive, dead within the window\n");
	const CutSettings s;
	CutInput in = Lethal();
	in.dead = false;
	Check(ArmsKillingBlow(s, in), "alive, the neck, an edge, fast: the cut waits for the death");
	in.edge = false;
	Check(!ArmsKillingBlow(s, in), "no edge: nothing waits");
	in = Lethal();
	in.dead = false;
	in.speedMetres = s.minSpeedMetres * 0.5f;
	Check(!ArmsKillingBlow(s, in), "too slow: nothing waits");
	in = Lethal();
	in.dead = false;
	in.essential = true;
	Check(!ArmsKillingBlow(s, in), "essential: nothing waits (they never die)");
	in = Lethal();
	in.dead = false;
	in.alreadyCut = true;
	Check(!ArmsKillingBlow(s, in), "the head already off: nothing waits");
	in = Lethal();
	Check(!ArmsKillingBlow(s, in), "dead already: cut at once, not waited for");
	in.dead = false;
	in.enabled = false;
	Check(!ArmsKillingBlow(s, in), "Dismemberment=0: nothing waits");
	CutSettings kills = s;
	kills.cutKills = true;
	in = Lethal();
	in.dead = false;
	Check(!ArmsKillingBlow(kills, in), "CutKills=1 cuts at once: nothing waits");

	PendingCut p;
	Check(StepPendingCut(p, 0.016f, true) == PendingStep::None, "nothing armed: nothing happens");
	ArmPendingCut(p, 0x1234u, NiPoint3{1.0f, 2.0f, 3.0f});
	Check(p.armed && p.actor == 0x1234u, "armed for that actor");
	Check(StepPendingCut(p, 0.1f, false) == PendingStep::Wait, "still alive: wait");
	Check(StepPendingCut(p, 0.1f, true) == PendingStep::Cut && !p.armed, "dead inside the window: the head comes off");
	ArmPendingCut(p, 0x1234u, NiPoint3{});
	Check(StepPendingCut(p, kKillingBlowSeconds + 0.01f, false) == PendingStep::Expired && !p.armed,
	      "alive past the window: the blow did not kill, forgotten");
	ArmPendingCut(p, 0x1234u, NiPoint3{});
	Check(StepPendingCut(p, kKillingBlowSeconds + 0.01f, true) == PendingStep::Expired,
	      "dead only after the window: someone else's blow, no cut");
	ArmPendingCut(p, 0x1234u, NiPoint3{});
	ArmPendingCut(p, 0x5678u, NiPoint3{});
	Check(p.actor == 0x5678u && p.age == 0.0f, "a new neck re-arms for the new one");
}

void TestKillingStrike() {
	std::printf("the killing strike: the part the blade touched decides, at the touch\n");
	BladeBodies bodies;
	bodies.Add(NiPoint3{0.0f, 0.0f, 100.0f}, NiPoint3{0.0f, 0.0f, 140.0f}, 10.0f, 7u, BodyPart::Torso);
	bodies.Add(NiPoint3{0.0f, 0.0f, 143.0f}, NiPoint3{0.0f, 0.0f, 150.0f}, 4.0f, 7u, BodyPart::Neck);
	bodies.Add(NiPoint3{0.0f, 0.0f, 156.0f}, NiPoint3{0.0f, 0.0f, 164.0f}, 7.0f, 7u, BodyPart::Head);
	bodies.Add(NiPoint3{0.0f, 0.0f, 143.0f}, NiPoint3{0.0f, 0.0f, 150.0f}, 4.0f, 9u, BodyPart::Neck);
	BodyPart part = BodyPart::Column;
	Check(NearestTouchedPart(bodies, 7u, NiPoint3{-30.0f, 2.0f, 146.0f}, NiPoint3{30.0f, 2.0f, 146.0f}, 1.0f, part) &&
	          part == BodyPart::Neck,
	      "across the neck: the neck, though the torso's top and the head are near");
	Check(NearestTouchedPart(bodies, 7u, NiPoint3{-30.0f, 2.0f, 120.0f}, NiPoint3{30.0f, 2.0f, 120.0f}, 1.0f, part) &&
	          part == BodyPart::Torso,
	      "across the chest: the torso");
	Check(!NearestTouchedPart(bodies, 7u, NiPoint3{-30.0f, 40.0f, 146.0f}, NiPoint3{30.0f, 40.0f, 146.0f}, 1.0f, part),
	      "past them all: nothing");
	Check(!NearestTouchedPart(bodies, 8u, NiPoint3{-30.0f, 2.0f, 146.0f}, NiPoint3{30.0f, 2.0f, 146.0f}, 1.0f, part),
	      "another actor's capsules never count");
	Check(StrikeKilled(74.0f, -401.0f), "health from above zero to below: this blow killed");
	Check(StrikeKilled(5.0f, 0.0f), "to exactly zero: killed");
	Check(!StrikeKilled(74.0f, 60.0f), "still above zero: alive");
	Check(!StrikeKilled(-3.0f, -20.0f), "dead before the blow: not this blow's kill");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!StrikeKilled(nan, -1.0f) && !StrikeKilled(10.0f, nan), "an unreadable health: no kill");
	Check(PartTakesHead(BodyPart::Neck), "the neck takes the head");
	Check(!PartTakesHead(BodyPart::Head) && !PartTakesHead(BodyPart::Torso) && !PartTakesHead(BodyPart::Column),
	      "the skull, the chest, a column body: not");
}

void TestKillingSweep() {
	std::printf("the killing strike's sweep: the way the blade was going, the body flung off\n");
	const NiPoint3 spine{0.0f, 0.0f, 100.0f};
	const NiPoint3 head{0.0f, 0.0f, 115.0f};
	// The blade points at the neck (along y) and moves along x: one frame from
	// x 30 to x 20, the shoulder 15 off the neck where the strike killed.
	const NiPoint3 gripBefore{30.0f, -50.0f, 108.0f};
	const NiPoint3 tipBefore{30.0f, 10.0f, 108.0f};
	const NiPoint3 gripNow{20.0f, -50.0f, 108.0f};
	const NiPoint3 tipNow{20.0f, 10.0f, 108.0f};
	const float frame = 1.0f / 75.0f;
	Check(!NeckTouched(spine, head, 6.0f, gripBefore, tipBefore, gripNow, tipNow),
	      "the frame's own sweep stops short of the neck (the shoulder met first)");
	Check(KillingSweepMeetsNeck(spine, head, 6.0f, gripBefore, tipBefore, gripNow, tipNow, frame, kKillingSweepSeconds),
	      "its way on for kKillingSweepSeconds goes through the neck: cut");
	Check(!KillingSweepMeetsNeck(spine, head, 6.0f, gripBefore, tipBefore, gripNow, tipNow, frame, 0.0f),
	      "without the look ahead: not");
	const NiPoint3 away{0.0f, 0.0f, -60.0f};
	Check(!KillingSweepMeetsNeck(spine, head, 6.0f, gripBefore + away, tipBefore + away, gripNow + away, tipNow + away,
	                             frame, kKillingSweepSeconds),
	      "the same sweep at the hips: not");
	Check(!KillingSweepMeetsNeck(spine, head, 6.0f, gripNow, tipNow, gripBefore, tipBefore, frame, kKillingSweepSeconds),
	      "going away from the neck: not");
	Check(!KillingSweepMeetsNeck(spine, head, 6.0f, gripBefore, tipBefore, gripNow, tipNow, 0.0f, kKillingSweepSeconds),
	      "no frame time: not");
}

void TestWalkersMessages() {
	std::printf("telling Oblivion Walkers (its spec 4.17, agreed 2026-10-30)\n");
	Check(std::memcmp(&kWalkersDismemberMessage, "WGDM", 4) == 0, "'WGDM' in its bytes");
	Check(std::memcmp(&kWalkersHeadMessage, "WGHD", 4) == 0, "'WGHD' in its bytes");
	const WalkersHeadWords head = HeadMessageFor(0x0001C45Bu, kHeadCut);
	Check(sizeof(head) == 8 && head.refId == 0x0001C45Bu && head.kind == 1u, "'WGHD': the reference, then 1 for cut");
	Check(kHeadCrushed == 2u, "2 is a crushed head");
	WalkersDismemberSay say;
	Check(WalkersDismemberDue(say, true), "nothing taken yet: say it");
	NoteWalkersDismemberSaid(say, true, false);
	Check(WalkersDismemberDue(say, true), "not taken (Walkers not listening yet): say it again");
	NoteWalkersDismemberSaid(say, true, true);
	Check(!WalkersDismemberDue(say, true), "taken, the same: nothing");
	Check(WalkersDismemberDue(say, false), "switched off since: say it");
	NoteWalkersDismemberSaid(say, false, true);
	Check(!WalkersDismemberDue(say, false), "taken off: nothing");
}

void TestSides() {
	std::printf("which bone pointers each side's shapes give up\n");
	Check(SwapBoneOnBodySide(true, false), "the body's shapes: the head bone collapses at the neck");
	Check(SwapBoneOnBodySide(false, true), "and anything under the head");
	Check(!SwapBoneOnBodySide(false, false), "the body's own bones stay");
	Check(SwapBoneOnHeadSide(false, false), "the head's shapes: the neck, spine and clavicles collapse under it");
	Check(!SwapBoneOnHeadSide(true, false) && !SwapBoneOnHeadSide(false, true), "the head and its own bones stay");
	std::printf("which side a shape is, by its bones\n");
	Check(ShapeIsHeadSide(3, 3), "the head's bones only (a hood, hair): the head's");
	Check(ShapeIsHeadSide(2, 5), "FaceGenFace, the head and neck of its five bones: the head's");
	Check(ShapeIsHeadSide(1, 3), "a third: the head's");
	Check(!ShapeIsHeadSide(2, 10) && !ShapeIsHeadSide(2, 19),
	      "a shirt with the neck in its collar: the body's (the harness's beggar of 2026-10-30, its bound's middle a "
	      "hair nearer the head)");
	Check(!ShapeIsHeadSide(1, 4), "under a third: the body's");
	Check(!ShapeIsHeadSide(0, 0), "no bones read: not by its bones");
	std::printf("the head's push\n");
	const NiPoint3 slow = CutPush(NiPoint3{100.0f, 0.0f, 0.0f});
	Check(slow.x == 100.0f, "a push under kCutPushMostUnits stays as it is");
	const NiPoint3 fast = CutPush(NiPoint3{0.0f, 1300.0f, 0.0f});
	Check(fast.y > kCutPushMostUnits - 0.01f && fast.y < kCutPushMostUnits + 0.01f && fast.x == 0.0f,
	      "a harness thrust's 1300 units/s: cut down to the most, the way kept");
}

}  // namespace

int main() {
	TestEdge();
	TestNeckTouched();
	TestDecide();
	TestKillingBlow();
	TestKillingStrike();
	TestKillingSweep();
	TestWalkersMessages();
	TestSides();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
