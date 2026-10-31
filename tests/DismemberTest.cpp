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
	Check(std::memcmp(&kWalkersPowerStrikeMessage, "WGPS", 4) == 0, "'WGPS' in its bytes");
	Check(TellsPowerStrike(true, 0x20A319D8u), "a power strike on an actor: told before the engine's hit");
	Check(!TellsPowerStrike(false, 0x20A319D8u), "a light strike: not");
	Check(!TellsPowerStrike(true, 0u), "no actor: not");
	Check(TellsHeadCrushed(false, BodyPart::Head, false), "a mace's killing blow to the head: crushed, told");
	Check(!TellsHeadCrushed(true, BodyPart::Head, false), "a blade's: no (a blade cuts instead)");
	Check(!TellsHeadCrushed(false, BodyPart::Torso, false), "a mace's to the torso: no");
	Check(!TellsHeadCrushed(false, BodyPart::Neck, false), "a mace's to the neck: no, only the head's capsule");
	Check(!TellsHeadCrushed(false, BodyPart::Head, true), "the head already off: no");
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

void TestBlood() {
	std::printf("The blood at the cut\n");
	CutSettings s;
	Check(s.blood && s.bloodPulses == 3 && s.bloodPulseSeconds == 0.35f, "on by default: three spurts 0.35 s apart");
	Check(s.caps, "the wound caps on by default");
	Check(BloodPulseDue(s, 0.0f, 0), "the first spurt at once");
	Check(!BloodPulseDue(s, 0.2f, 1), "the second not before 0.35 s");
	Check(BloodPulseDue(s, 0.36f, 1), "the second at 0.35 s");
	Check(BloodPulseDue(s, 0.75f, 2), "the third at 0.7 s");
	Check(!BloodPulseDue(s, 5.0f, 3), "and no more after three");
	CutSettings off = s;
	off.blood = false;
	Check(!BloodPulseDue(off, 0.0f, 0), "DismemberBlood=0: none");
	CutSettings none = s;
	none.bloodPulses = 0;
	Check(!BloodPulseDue(none, 0.0f, 0), "no spurts asked: none");
	const NiPoint3 up = SprayDirection(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 5.0f});
	Check(up.z > 0.99f, "the spray goes from the body out of the neck, toward where the head was");
	const NiPoint3 still = SprayDirection(NiPoint3{1.0f, 1.0f, 1.0f}, NiPoint3{1.0f, 1.0f, 1.0f});
	Check(still.z > 0.99f, "no direction to tell: straight up");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const NiPoint3 bad = SprayDirection(NiPoint3{nan, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f});
	Check(bad.z > 0.99f, "not a number: straight up");
}

void TestCutMemory() {
	std::printf("The heads cut, kept over a save and a reload\n");
	CutMemory m;
	Check(!RememberCut(m, 0), "no form id: nothing kept");
	Check(RememberCut(m, 0x0001C45Bu) && m.count == 1 && CutRemembered(m, 0x0001C45Bu), "a cut kept");
	Check(!RememberCut(m, 0x0001C45Bu) && m.count == 1, "the same again: kept once");
	CutMemory full;
	for (UInt32 i = 1; i <= kCutMemoryMost; ++i) {
		RememberCut(full, i);
	}
	Check(full.count == kCutMemoryMost && CutRemembered(full, 1), "full to the last");
	RememberCut(full, 0xFF000001u);
	Check(full.count == kCutMemoryMost && !CutRemembered(full, 1) && CutRemembered(full, 0xFF000001u) &&
	          CutRemembered(full, 2),
	      "one more: the oldest let go, the newest kept");

	const CutHeadsRecord r = RecordOfCuts(m);
	Check(r.count == 1 && r.ids[0] == 0x0001C45Bu, "the record holds the ids");
	auto same = [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	};
	CutMemory back;
	Check(CutsOfRecord(&r, sizeof(r), kCutHeadsVersion, same, back) && back.count == 1 &&
	          CutRemembered(back, 0x0001C45Bu),
	      "read back: the same heads");
	CutHeadsRecord two = RecordOfCuts(m);
	two.count = 2;
	two.ids[1] = 0xFF000ABCu;
	auto movedOrGone = [](UInt32 id, UInt32& out) {
		if (id == 0xFF000ABCu) {
			return false;  // a created reference that is gone
		}
		out = id + 1;  // the load order moved
		return true;
	};
	CutMemory moved;
	Check(CutsOfRecord(&two, sizeof(two), kCutHeadsVersion, movedOrGone, moved) && moved.count == 1 &&
	          CutRemembered(moved, 0x0001C45Cu),
	      "an id the load moved is followed, one that is gone dropped");
	CutMemory refused;
	Check(!CutsOfRecord(&r, sizeof(r) - 4, kCutHeadsVersion, same, refused) && refused.count == 0,
	      "a record of another length: none read");
	Check(!CutsOfRecord(&r, sizeof(r), kCutHeadsVersion + 1, same, refused), "another version: none read");
	Check(!CutsOfRecord(nullptr, sizeof(r), kCutHeadsVersion, same, refused), "no record: none read");
	CutHeadsRecord wild = r;
	wild.count = kCutMemoryMost + 5;
	Check(!CutsOfRecord(&wild, sizeof(wild), kCutHeadsVersion, same, refused), "a count past the most: none read");

	CutMemory hands;
	Check(RememberCut(hands, 0x0001C45Bu, CutPart::LeftHand) && RememberCut(hands, 0x0001C45Bu, CutPart::Head) &&
	          hands.count == 2,
	      "a hand and the head of one body: both kept");
	Check(CutRemembered(hands, 0x0001C45Bu, CutPart::LeftHand) && !CutRemembered(hands, 0x0001C45Bu, CutPart::RightHand),
	      "the left hand kept, the right not");
	const CutHeadsRecord hr = RecordOfCuts(hands);
	CutMemory handsBack;
	Check(CutsOfRecord(&hr, sizeof(hr), kCutHeadsVersion, same, handsBack) && handsBack.count == 2 &&
	          CutRemembered(handsBack, 0x0001C45Bu, CutPart::LeftHand),
	      "read back with their parts");
	CutMemory v1;
	CutHeadsRecord old = RecordOfCuts(m);
	Check(CutsOfRecord(&old, kCutHeadsRecordV1Bytes, kCutHeadsVersionHeadsOnly, same, v1) &&
	          CutRemembered(v1, 0x0001C45Bu, CutPart::Head),
	      "a version 1 record (heads only) read as heads");
	Check(std::strcmp(CutPartName(CutPart::RightHand), "right hand") == 0, "the parts have names");
	Check(SwapBoneOnPartSide(true, false, false, false, false), "a head cut's part side: every bone not the head's goes to the part");
	Check(SwapBoneOnPartSide(false, false, false, true, false), "a hand cut: the cut arm's forearm goes to the part");
	Check(!SwapBoneOnPartSide(false, false, false, true, true), "a hand cut: the spine both hands hang from stays");
	Check(!SwapBoneOnPartSide(false, false, false, false, false), "a hand cut: the other hand's bones stay");
	Check(!SwapBoneOnPartSide(false, true, false, false, false) && !SwapBoneOnPartSide(false, false, true, true, false),
	      "a hand cut: the hand and its fingers stay its own");
	NiPoint3 wlow{};
	NiPoint3 whigh{};
	WristSegment(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, wlow, whigh);
	Check(wlow.x > 13.9f && wlow.x < 14.1f && whigh.x > 21.9f && whigh.x < 22.1f,
	      "the wrist: the forearm's last 30 % to a tenth past the hand, not the whole forearm");

	Check(!RedoDue(0.5f) && RedoDue(1.0f) && RedoDue(3.0f), "the cut redone once its 3D has stood a second");
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
	TestCutMemory();
	TestBlood();
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
