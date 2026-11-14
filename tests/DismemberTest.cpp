#include <cstdio>
#include <cmath>
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
	in.speedMetres = 8.0f;
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
	// Nadi, 2026-11-05: cuts came too easily - the neck wants a real blow, a
	// limb more, a two-handed blade a little less.
	Check(s.minSpeedMetres >= 4.9f && s.limbMinSpeedMetres > s.minSpeedMetres, "a limb wants a harder blow than the neck");
	Check(CutSpeedNeeded(s, false, false) == s.minSpeedMetres, "the neck with one hand: DismemberMinSpeed");
	Check(CutSpeedNeeded(s, true, false) == s.limbMinSpeedMetres, "a limb with one hand: DismemberLimbMinSpeed");
	Check(CutSpeedNeeded(s, false, true) == s.minSpeedMetres * s.twoHandedShare &&
	          CutSpeedNeeded(s, true, true) == s.limbMinSpeedMetres * s.twoHandedShare && s.twoHandedShare < 1.0f,
	      "a two-handed blade: that share of either");
	in = Lethal();
	in.limb = true;
	in.speedMetres = s.limbMinSpeedMetres * 0.95f;
	Check(DecideCut(s, in) == CutVerdict::TooSlow, "a limb at a speed the neck would take: too slow");
	in.twoHanded = true;
	Check(DecideCut(s, in) == CutVerdict::Cut, "the same with a two-handed blade: cut");
	in = Lethal();
	in.speedMetres = s.minSpeedMetres * 0.9f;
	in.twoHanded = true;
	Check(DecideCut(s, in) == CutVerdict::Cut, "the neck just under the threshold with a two-hander: cut");
	CutSettings bad = s;
	bad.twoHandedShare = -1.0f;
	Check(CutSpeedNeeded(bad, false, true) == s.minSpeedMetres, "a share out of 0..1: the one-hand speed");
	CutSettings kills = s;
	kills.cutKills = true;
	in = Lethal();
	in.dead = false;
	Check(DecideCut(kills, in) == CutVerdict::Cut, "CutKills=1: the living too");
	in.essential = true;
	Check(DecideCut(kills, in) == CutVerdict::Essential, "but never an essential one");
	// Nadi, 2026-11-02: essential NPCs lost parts - one at no health lies
	// unconscious and reads dead, so the essential refuses before the death.
	in = Lethal();
	in.essential = true;
	Check(DecideCut(CutSettings{}, in) == CutVerdict::Essential, "an essential one reading dead keeps their parts");
	in.alreadyCut = true;
	Check(DecideCut(CutSettings{}, in) == CutVerdict::AlreadyCut, "a part already off stays the verdict");
	Check(ActorBaseFlagsEssential(0x02) && ActorBaseFlagsEssential(0x83), "the essential flag 0x02, among others");
	Check(!ActorBaseFlagsEssential(0x81), "female and PC level offset: not essential");
	Check(CutVerdictName(CutVerdict::TooSlow)[0] != '\0', "every verdict has a name for the log");
}

void TestCutSound() {
	std::printf("the cut's sound: the blade in flesh and the bone (Nadi, 2026-11-05)\n");
	UInt32 sounds[kCutSoundsMost] = {};
	Check(CutSoundsFor(true, false, sounds) == 2 && sounds[0] == kSoundCutFlesh && sounds[1] == kSoundCutBone,
	      "a cut: WPNHitBladeFlesh and PHYDamageBone");
	Check(kSoundCutFlesh == 0x0018B9F1u && kSoundCutBone == 0x00025417u, "the forms as Oblivion.esm has them");
	Check(CutSoundsFor(false, false, sounds) == 0, "DismemberSound=0: silent");
	Check(CutSoundsFor(true, true, sounds) == 0, "a cut done again after a load: silent, as its blood");
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
	// A blunt blow - a kick, a shield bash, a pommel or a hilt - takes the
	// death from the blade (Nadi, 2026-11-01: "kicks sollten niemanden
	// enthaupten können").
	ArmPendingCut(p, 0x1234u, NiPoint3{});
	ForgetPendingCutFor(p, 0x5678u);
	Check(p.armed, "a blunt blow on someone else: the waiting cut stays");
	ForgetPendingCutFor(p, 0x1234u);
	Check(!p.armed && StepPendingCut(p, 0.1f, true) == PendingStep::None,
	      "a blunt blow on the one waiting: forgotten, their death cuts nothing");
	ForgetPendingCutFor(p, 0x1234u);
	Check(!p.armed, "nothing waiting: nothing to forget");
	Check(BluntGuardHolds(0x1234u, 0.1f, 0x1234u), "just after a blunt blow on them: their neck is not cut");
	Check(!BluntGuardHolds(0x1234u, kBluntGuardSeconds + 0.01f, 0x1234u), "half a second later: cut as any dead neck");
	Check(!BluntGuardHolds(0x1234u, 0.1f, 0x5678u), "someone else: cut as before");
	Check(!BluntGuardHolds(0u, 0.1f, 0u), "no blunt blow: nothing held");
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
	Check(CapScaleFor(CutPart::Head) == 1.0f && CapScaleFor(CutPart::LeftHand) == kWristCapScale &&
	          CapScaleFor(CutPart::RightHand) == kWristCapScale && kWristCapScale > 0.3f && kWristCapScale < 0.6f,
	      "the neck plug at its size on a neck, made a wrist's on a hand");
	Check(SwapBoneOnPartSide(true, false, false, false, false, false), "a head cut's part side: every bone not the head's goes to the part");
	Check(SwapBoneOnPartSide(false, false, false, true, false, true), "a hand cut: the cut arm's forearm goes to the part");
	Check(!SwapBoneOnPartSide(false, false, false, true, true, false), "a hand cut: the spine both hands hang from stays");
	Check(!SwapBoneOnPartSide(false, false, false, false, false, false), "a hand cut: the other hand's bones stay");
	Check(!SwapBoneOnPartSide(false, true, false, false, false, true) && !SwapBoneOnPartSide(false, false, true, true, false, true),
	      "a hand cut: the hand and its fingers stay its own");
	// Nadi, 2026-11-05: a hand off stretched to the horizon - the hand's mesh
	// weighs the wrist on Bip01 L ForearmTwist too, a child of the forearm
	// beside the hand: it neither is the hand nor holds it, so it stayed on
	// the body while the hand flew.
	Check(SwapBoneOnPartSide(false, false, false, false, false, true),
	      "a hand cut: the forearm's twist bone beside the hand goes to the part");
	Check(!SwapBoneOnPartSide(true, true, false, false, false, true) && !SwapBoneOnPartSide(false, false, true, false, false, true),
	      "the stump's chain never takes the part itself or what hangs under it");
	// The middle both limbs hang from (Nadi, 2026-11-10: arms cut at the
	// shoulder stretched): kept in a shape the other limb shares, sent with
	// the part in a shape of the cut limb alone.
	Check(!SwapBoneOnPartSide(false, false, false, true, true, false, false, true),
	      "a shared shape (handRM, boots): the spine both hang from stays");
	Check(SwapBoneOnPartSide(false, false, false, true, true, false, false, false),
	      "an arm's own shape: the spine its seam hangs from goes with the arm");
	Check(!SwapBoneOnPartSide(false, false, false, false, true, false, true, true) &&
	          !SwapBoneOnPartSide(false, false, false, false, false, false, true, false),
	      "the other limb's bones (its match, under it, holding it alone) always stay");
	Check(!SwapBoneOnPartSide(false, false, false, false, false, false, false, false),
	      "a bone of neither limb's chain (a thigh in a sleeve): stays");
	Check(SwapBoneOnPartSide(true, false, false, true, true, false, true, true),
	      "a head cut: everything not the head's goes, the limbs' inputs aside");
	std::printf("the bones a part's shape leaves on the body\n");
	Check(!PartSideBoneLeftBehind(false, false, false, true, false, false, false, false),
	      "the chain up to the cut limb's root: sent, not left");
	Check(!PartSideBoneLeftBehind(false, false, false, false, false, true, false, false), "the twist beside: sent");
	Check(!PartSideBoneLeftBehind(false, false, false, true, true, false, false, true),
	      "the shared middle of a shared shape: kept on purpose, not counted");
	Check(!PartSideBoneLeftBehind(false, false, false, true, true, false, false, false),
	      "the middle of an unshared shape: sent now, not left");
	Check(PartSideBoneLeftBehind(false, false, false, false, false, false, false, false),
	      "a bone of neither chain: left behind, counted");
	Check(!PartSideBoneLeftBehind(false, false, false, false, false, false, true, true) &&
	          !PartSideBoneLeftBehind(false, true, false, false, false, false, false, false) &&
	          !PartSideBoneLeftBehind(false, false, true, false, false, false, false, false) &&
	          !PartSideBoneLeftBehind(true, false, false, false, false, false, false, false),
	      "the other limb's, the part's own and a head cut's: never counted");
	NiPoint3 wlow{};
	NiPoint3 whigh{};
	WristSegment(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, wlow, whigh);
	Check(wlow.x > 13.9f && wlow.x < 14.1f && whigh.x > 21.9f && whigh.x < 22.1f,
	      "the wrist: the forearm's last 30 % to a tenth past the hand, not the whole forearm");

	Check(!RedoDue(0.5f) && RedoDue(1.0f) && RedoDue(3.0f), "the cut redone once its 3D has stood a second");

	// Raised again (Walkers' pool walkers, a Resurrect): the cut belongs to
	// the death and goes with it (harness dismember-resurrect, 2026-11-05: the
	// raised bandit's head cut again on its feet).
	Check(RedoFor(true, true, 1.0f) == RedoStep::Redo, "loaded, dead, its 3D stood a second: cut again");
	Check(RedoFor(true, true, 0.5f) == RedoStep::Wait, "loaded, dead, its 3D new: wait");
	Check(RedoFor(true, false, 0.0f) == RedoStep::Forget, "loaded and alive: raised again - forgotten, at once");
	Check(RedoFor(false, false, 3.0f) == RedoStep::Wait, "not among the loaded ones: wait, nothing forgotten");
	CutMemory raised;
	RememberCut(raised, 0x11u, CutPart::Head);
	RememberCut(raised, 0x11u, CutPart::LeftHand);
	RememberCut(raised, 0x22u, CutPart::Head);
	Check(ForgetCuts(raised, 0x11u) == 2 && raised.count == 1 && CutRemembered(raised, 0x22u, CutPart::Head) &&
	          !CutRemembered(raised, 0x11u, CutPart::Head),
	      "forgetting one: all its parts go, the others stay");
	Check(ForgetCuts(raised, 0x33u) == 0 && raised.count == 1, "forgetting one never cut: nothing");
}

// The limbs at their joints (Nadi, 2026-11-05: "go 4"): the forearm at the
// elbow, the whole arm at the shoulder, the lower leg at the knee.
void TestLimbs() {
	std::printf("the limbs at their joints\n");
	Check(kCutPartCount == 9, "nine parts: the head, two hands, two forearms, two arms, two lower legs");
	const CutPartBones fore = BonesOfCut(CutPart::LeftForearm);
	Check(std::strcmp(fore.part, "Bip01 L Forearm") == 0 && std::strcmp(fore.joint, "Bip01 L UpperArm") == 0 &&
	          std::strcmp(fore.stump, "Bip01 L UpperArm") == 0 && std::strcmp(fore.other, "Bip01 R Forearm") == 0 &&
	          std::strcmp(fore.next, "Bip01 L Hand") == 0,
	      "the left forearm: off the upper arm's joint, the stump on the upper arm, the right forearm the other");
	const CutPartBones arm = BonesOfCut(CutPart::RightArm);
	Check(std::strcmp(arm.part, "Bip01 R UpperArm") == 0 && std::strcmp(arm.joint, "Bip01 Spine2") == 0 &&
	          std::strcmp(arm.stump, "Bip01 R Clavicle") == 0 && std::strcmp(arm.other, "Bip01 L UpperArm") == 0 &&
	          std::strcmp(arm.next, "Bip01 R Forearm") == 0,
	      "the right arm: its joint is to Spine2 (the skeleton's ragdoll), the stump on the collarbone");
	const CutPartBones leg = BonesOfCut(CutPart::LeftLowerLeg);
	Check(std::strcmp(leg.part, "Bip01 L Calf") == 0 && std::strcmp(leg.joint, "Bip01 L Thigh") == 0 &&
	          std::strcmp(leg.stump, "Bip01 L Thigh") == 0 && std::strcmp(leg.other, "Bip01 R Calf") == 0 &&
	          std::strcmp(leg.next, "Bip01 L Foot") == 0,
	      "the left lower leg: off the thigh");
	const CutPartBones head = BonesOfCut(CutPart::Head);
	Check(std::strcmp(head.part, "Bip01 Head") == 0 && std::strcmp(head.joint, "Bip01 Spine2") == 0 &&
	          std::strcmp(head.stump, "Bip01 Neck1") == 0 && head.other == nullptr && head.next == nullptr,
	      "the head as before: off Spine2, the stump on Neck1");
	const CutPartBones hand = BonesOfCut(CutPart::RightHand);
	Check(std::strcmp(hand.part, "Bip01 R Hand") == 0 && std::strcmp(hand.joint, "Bip01 R Forearm") == 0 &&
	          std::strcmp(hand.stump, "Bip01 R Forearm") == 0 && std::strcmp(hand.other, "Bip01 L Hand") == 0 &&
	          hand.next == nullptr,
	      "a hand as before: off the forearm");
	Check(std::strcmp(BonesOfCut(static_cast<CutPart>(200)).part, "Bip01 Head") == 0, "a part past the list: the head's bones");
	Check(std::strcmp(CutPartName(CutPart::LeftForearm), "left forearm") == 0 &&
	          std::strcmp(CutPartName(CutPart::RightArm), "right arm") == 0 &&
	          std::strcmp(CutPartName(CutPart::LeftLowerLeg), "left lower leg") == 0,
	      "the limbs have names");
	Check(std::strcmp(CutJointName(CutPart::Head), "neck") == 0 && std::strcmp(CutJointName(CutPart::LeftHand), "wrist") == 0 &&
	          std::strcmp(CutJointName(CutPart::RightForearm), "elbow") == 0 &&
	          std::strcmp(CutJointName(CutPart::LeftArm), "shoulder") == 0 &&
	          std::strcmp(CutJointName(CutPart::RightLowerLeg), "knee") == 0,
	      "and their joints");
	Check(IsLimbCut(CutPart::LeftForearm) && IsLimbCut(CutPart::RightLowerLeg) && !IsLimbCut(CutPart::Head) &&
	          !IsLimbCut(CutPart::LeftHand),
	      "the limbs are the parts past the hands");
	Check(CapScaleFor(CutPart::LeftForearm) > kWristCapScale && CapScaleFor(CutPart::LeftArm) > CapScaleFor(CutPart::LeftForearm) &&
	          CapScaleFor(CutPart::LeftArm) < 1.0f && CapScaleFor(CutPart::RightLowerLeg) > CapScaleFor(CutPart::RightForearm),
	      "the caps: an elbow wider than a wrist, a shoulder wider still, a knee wider than an elbow");
	Check(LimbRadiusUnits(CutPart::LeftForearm) > kWristRadiusUnits && LimbRadiusUnits(CutPart::LeftArm) > LimbRadiusUnits(CutPart::LeftForearm) &&
	          LimbRadiusUnits(CutPart::LeftLowerLeg) > LimbRadiusUnits(CutPart::LeftForearm) &&
	          LimbRadiusUnits(CutPart::LeftHand) == kWristRadiusUnits,
	      "the reach around each joint: thicker limbs, wider");

	// One chain per side: the hand on the forearm on the arm; a lower leg alone.
	Check(CutPartsClash(CutPart::LeftHand, CutPart::LeftForearm) && CutPartsClash(CutPart::LeftForearm, CutPart::LeftArm) &&
	          CutPartsClash(CutPart::LeftArm, CutPart::LeftHand),
	      "a part of the left arm cut: no other part of the left arm");
	Check(!CutPartsClash(CutPart::LeftHand, CutPart::RightForearm) && !CutPartsClash(CutPart::LeftArm, CutPart::LeftLowerLeg) &&
	          !CutPartsClash(CutPart::Head, CutPart::LeftArm),
	      "the other arm, a leg and the head are their own");
	Check(CutPartsClash(CutPart::RightLowerLeg, CutPart::RightLowerLeg) && CutPartsClash(CutPart::Head, CutPart::Head),
	      "a part clashes with itself (cut already)");
	Check(!CutPartsClash(CutPart::LeftLowerLeg, CutPart::RightLowerLeg), "the two legs are their own");

	// The joint as a blade meets it: around the part's origin, along the part.
	NiPoint3 low{};
	NiPoint3 high{};
	JointSegment(CutPart::LeftForearm, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, low, high);
	Check(low.x < 0.0f && low.x > -5.0f && high.x > 0.0f && high.x < 6.0f && low.y == 0.0f && high.z == 0.0f,
	      "the elbow: a little up the upper arm to a little down the forearm, not the whole forearm");
	NiPoint3 wlow{};
	NiPoint3 whigh{};
	WristSegment(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{20.0f, 0.0f, 0.0f}, wlow, whigh);
	Check(high.x < wlow.x, "the elbow's stretch ends before the wrist's begins");
	JointSegment(CutPart::LeftArm, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -22.0f}, low, high);
	Check(low.z > 0.0f && high.z < -3.0f && high.z > -10.0f, "the shoulder: the upper arm's top");
	JointSegment(CutPart::LeftLowerLeg, NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, -27.0f}, low, high);
	Check(low.z > 0.0f && high.z < 0.0f && high.z > -8.0f, "the knee: around the calf's top");

	// The co-save: version 3 holds the limbs; version 2 (hands) still read.
	CutMemory limbs;
	RememberCut(limbs, 0x0001C45Bu, CutPart::RightLowerLeg);
	RememberCut(limbs, 0x0001C45Bu, CutPart::LeftArm);
	const CutHeadsRecord lr = RecordOfCuts(limbs);
	CutMemory back;
	auto same = [](UInt32 id, UInt32& out) {
		out = id;
		return true;
	};
	Check(CutsOfRecord(&lr, sizeof(lr), kCutHeadsVersion, same, back) && back.count == 2 &&
	          CutRemembered(back, 0x0001C45Bu, CutPart::RightLowerLeg) && CutRemembered(back, 0x0001C45Bu, CutPart::LeftArm),
	      "the limbs read back from a version 3 record");
	CutMemory fromV2;
	Check(kCutHeadsVersion == 3 && CutsOfRecord(&lr, sizeof(lr), kCutHeadsVersionHands, same, fromV2) &&
	          fromV2.count == 0,
	      "a version 2 record knows no limbs: a part past the hands is not read as a head");
	CutMemory handsV2;
	RememberCut(handsV2, 0x0001C45Bu, CutPart::LeftHand);
	const CutHeadsRecord hr = RecordOfCuts(handsV2);
	CutMemory handsBack;
	Check(CutsOfRecord(&hr, sizeof(hr), kCutHeadsVersionHands, same, handsBack) && handsBack.count == 1 &&
	          CutRemembered(handsBack, 0x0001C45Bu, CutPart::LeftHand),
	      "a version 2 record's hands read as before");
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

bool NearP(const NiPoint3& a, const NiPoint3& b, float within = 1e-3f) {
	const NiPoint3 d{a.x - b.x, a.y - b.y, a.z - b.z};
	return d.x * d.x + d.y * d.y + d.z * d.z <= within * within;
}

NiTransform TurnedTransform(float yawRadians, const NiPoint3& pos, float scale) {
	NiTransform t{};
	const float c = std::cos(yawRadians);
	const float s = std::sin(yawRadians);
	t.rot.data[0][0] = c;
	t.rot.data[0][1] = -s;
	t.rot.data[1][0] = s;
	t.rot.data[1][1] = c;
	t.rot.data[2][2] = 1.0f;
	t.pos = pos;
	t.scale = scale;
	return t;
}

NiPoint3 Apply(const NiTransform& t, const NiPoint3& v) { return t.pos + t.rot * (v * t.scale); }

void TestCapFacing() {
	std::printf("the wound cap's facing, read from its normals\n");
	// A disc in the bone's space at z = -5, its normals up (+z): it faces up.
	const NiPoint3 verts[] = {{1, 0, -5}, {-1, 0, -5}, {0, 1, -5}, {0, -1, -5}};
	const NiPoint3 up[] = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
	NiTransform same{};
	same.rot = NiMatrix33::Identity();
	same.scale = 1.0f;
	CapFacing f;
	AddCapFacing(f, verts, up, 4, same);
	Check(f.count == 4 && f.unitNormals == 4 && NearP(CapCentre(f), NiPoint3{0, 0, -5}), "four vertices, unit normals, the centre");
	Check(CapFacingAlong(f, NiPoint3{0, 0, -1}) < -0.99f, "normals up, asked down: facing away (-1)");
	Check(CapFacingAlong(f, NiPoint3{0, 0, 1}) > 0.99f, "asked up: facing it (+1)");
	// The skin-to-bone turn and offset carry the vertices and the normals.
	NiTransform shift{};
	shift.rot.data[0][0] = 1.0f;
	shift.rot.data[1][2] = -1.0f;
	shift.rot.data[2][1] = 1.0f;  // +z to -y
	shift.pos = NiPoint3{0, 0, 2};
	shift.scale = 1.0f;
	CapFacing g;
	AddCapFacing(g, verts, up, 4, shift);
	Check(CapFacingAlong(g, NiPoint3{0, -1, 0}) > 0.99f, "turned by the skin-to-bone: its normals turned with it");
	const NiPoint3 bad[] = {{0, 0, 0}, {0, 0, 7}, {0, 0, 1}, {0, 0, 1}};
	CapFacing h;
	AddCapFacing(h, verts, bad, 4, same);
	Check(h.unitNormals == 2, "normals not of unit length are counted apart");
	CapFacing none;
	Check(CapFacingAlong(none, NiPoint3{0, 0, 1}) == 0.0f && NearP(CapCentre(none), NiPoint3{0, 0, 0}), "nothing read: 0");
	AddCapFacing(none, verts, nullptr, 4, same);
	Check(none.count == 0, "no normals: nothing added");
}

void TestCapFlip() {
	std::printf("the head's cap turned over about its own middle\n");
	// The head's cap faced up into the head (2026-11-03, headset: the head's
	// underside see-through): the same cap turned half round about an axis
	// across `down`, through its centre, faces down out of it, in place.
	const NiTransform head = TurnedTransform(0.7f, NiPoint3{100, 50, 120}, 1.1f);
	const NiPoint3 centre{0.5f, -0.3f, -6.0f};
	const NiPoint3 down{0, 0, -1};
	const NiTransform t = FlippedCap(head, centre, down);
	Check(NearP(Apply(t, centre), Apply(head, centre)), "the centre stays where it was");
	const NiPoint3 downWorld = head.rot * down;
	const NiPoint3 turned = t.rot * down;
	Check(NearP(turned, downWorld * -1.0f), "what pointed down points up: the facing turned over");
	const NiPoint3 a{2, 1, -6};
	const NiPoint3 b{-1, 3, -4};
	const NiPoint3 da = Apply(t, a) - Apply(t, b);
	const NiPoint3 db = Apply(head, a) - Apply(head, b);
	Check(std::fabs(std::sqrt(da.LengthSquared()) - std::sqrt(db.LengthSquared())) < 1e-3f, "distances kept: no stretch");
	Check(t.scale == head.scale, "the scale kept");
	const NiTransform side = FlippedCap(head, centre, NiPoint3{1, 0, 0});
	Check(NearP(side.rot * NiPoint3{1, 0, 0}, (head.rot * NiPoint3{1, 0, 0}) * -1.0f), "any down: turned over it");
}

void TestCutWorld() {
	std::printf("a cut and the world it was made in\n");
	Check(CutInWorld(7, 7), "made in this world: read");
	Check(!CutInWorld(7, 8) && !CutInWorld(7, 6), "another world: not read as it is");
	Check(!CutInWorld(0, 7) && !CutInWorld(0, 0), "a cut from no world: not read");
	// A load frees the actors made at runtime (the crash of 2026-11-07 23:18
	// in StepCuts) and keeps others with their 3D (a load in the same cell).
	Check(CutWorldFor(7, 7, 0x1000u, 0u) == CutWorldStep::Read, "this world: read, the form table not asked");
	Check(CutWorldFor(7, 8, 0x1000u, 0x1000u) == CutWorldStep::Recheck,
	      "a load since, the table holds the same actor: taken into this world, its 3D checked as before");
	Check(CutWorldFor(7, 8, 0x1000u, 0x2000u) == CutWorldStep::Drop,
	      "a load since, the table answers with another actor: dropped unread");
	Check(CutWorldFor(7, 8, 0x1000u, 0u) == CutWorldStep::Drop, "a load since, the form id gone: dropped unread");
	Check(CutWorldFor(0, 8, 0u, 0u) == CutWorldStep::Drop, "no actor and no world: dropped");
}

void TestCutLimit() {
	std::printf("the most cuts at once ([Hands] DismemberMostCuts)\n");
	Check(CutSettings{}.most == static_cast<int>(kCutsMost), "the default: every slot");
	const CutLimit in = CutLimitFor(64);
	Check(in.most == 64 && !in.clamped, "64 asked: 64, as asked");
	const CutLimit low = CutLimitFor(3);
	Check(low.most == kCutsLeast && low.clamped, "below the least: the least, said");
	const CutLimit negative = CutLimitFor(-5);
	Check(negative.most == kCutsLeast && negative.clamped, "below zero: the least, said");
	const CutLimit high = CutLimitFor(500);
	Check(high.most == kCutsMost && high.clamped, "above the slots: all the slots, said");
	Check(CutLimitFor(static_cast<int>(kCutsLeast)).most == kCutsLeast && !CutLimitFor(static_cast<int>(kCutsLeast)).clamped,
	      "the least itself: as asked");
	Check(CutLimitFor(static_cast<int>(kCutsMost)).most == kCutsMost && !CutLimitFor(static_cast<int>(kCutsMost)).clamped,
	      "all the slots: as asked");
	Check(CutRoom(15, 16), "fewer standing than the limit: room");
	Check(!CutRoom(16, 16), "as many standing as the limit: no room");
	Check(!CutRoom(40, 16), "the limit lowered under the cuts standing: none freed, no room until they go");
	Check(CutRoom(0, kCutsLeast), "none standing: room");
}

void TestCutSlots() {
	std::printf("the cut slots and their cost\n");
	Check(kCutsMost >= kCutMemoryMost, "every cut the co-save remembers can stand again after a load");
	Check(kCutsMost >= 128, "a camp's or a gate's dead: at least 128 (Walkers' gate-guards, 90 kills)");
	CutCostWindow w;
	Check(!AddCutCost(w, 10.0, 3, 3), "the first frame: no line yet");
	Check(!AddCutCost(w, 30.0, 5, 3), "the second: no line yet");
	Check(AddCutCost(w, 20.0, 4, 3), "the third of three: a line due");
	Check(w.frames == 3 && std::fabs(w.sumMicros - 60.0) < 1e-9 && std::fabs(w.maxMicros - 30.0) < 1e-9 && w.mostLive == 5,
	      "the window holds the frames, the sum, the longest and the most cuts");
	Check(std::fabs(CutCostMean(w) - 20.0) < 1e-9, "its mean");
	w = CutCostWindow{};
	Check(CutCostMean(w) == 0.0, "an empty window: mean 0");
	Check(!AddCutCost(w, 5.0, 0, 0) && w.frames == 1, "no frames per line: counted, never due");
	w = CutCostWindow{};
	const double nan = std::numeric_limits<double>::quiet_NaN();
	Check(!AddCutCost(w, nan, 1, 1) && w.frames == 0, "a time that is no number: not counted");
	Check(!AddCutCost(w, -1.0, 1, 1) && w.frames == 0, "a negative time: not counted");
}

void TestCapEntries() {
	std::printf("the caps' engine functions\n");
	Check(CapEntryOf(true, false) == CapEntry::AsRead && CapEntryCallable(CapEntry::AsRead), "as read: called");
	Check(CapEntryOf(true, true) == CapEntry::AsRead, "as read wins over a jump read into it");
	Check(CapEntryOf(false, true) == CapEntry::Detoured && CapEntryCallable(CapEntry::Detoured),
	      "another plugin's jump at the entry: called through it");
	Check(CapEntryOf(false, false) == CapEntry::Unknown && !CapEntryCallable(CapEntry::Unknown),
	      "other bytes, no jump: not called, no caps");
	std::printf("the cut body's skins read again\n");
	float t = 0.0f;
	Check(!CutRescanDue(t, 0.011f), "a frame after the cut: not yet");
	bool due = false;
	for (int i = 0; i < 50 && !due; ++i) {
		due = CutRescanDue(t, 0.011f);
	}
	Check(due && t == 0.0f, "half a second on: due, the clock started over");
	t = 0.4f;
	Check(!CutRescanDue(t, -1.0f) && t == 0.4f, "a negative frame time: nothing counted");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!CutRescanDue(t, nan) && t == 0.4f, "not a number: nothing counted");
	Check(CutRescanDue(t, 0.2f), "a long frame over the half second: due at once");
}

int main() {
	TestCapEntries();
	TestCutSlots();
	TestCapFacing();
	TestCapFlip();
	TestCutMemory();
	TestBlood();
	TestEdge();
	TestNeckTouched();
	TestDecide();
	TestCutSound();
	TestKillingBlow();
	TestKillingStrike();
	TestKillingSweep();
	TestWalkersMessages();
	TestSides();
	TestLimbs();
	TestCutWorld();
	TestCutLimit();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
