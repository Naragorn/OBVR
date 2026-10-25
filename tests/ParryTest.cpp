// Checks the parry's decisions (game/ParryLogic.h): which actions are an
// attack, their blade from their weapon node, two moving blades meeting, the
// ledger of parried blows, and the hit handler's three answers - the block
// forced, the cone, the share - for a parried blow, another attacker's, and
// everyone else's.

#include <cstdio>
#include <limits>

#include "game/ParryLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestActions() {
	std::printf("An attack in flight\n");
	Check(IsAttackAction(2) && IsAttackAction(3), "the swing and its follow-through");
	Check(!IsAttackAction(-1) && !IsAttackAction(4) && !IsAttackAction(6) && !IsAttackAction(7),
	      "nothing, a bow, a block, a recoil: no");
}

void TestTheirBlade() {
	std::printf("Their blade\n");
	NiPoint3 a;
	NiPoint3 b;
	Check(TheirBladeFromNode(NiPoint3{100, 0, 50}, NiPoint3{0, 1, 0}, NiPoint3{100, 30, 50}, 35.0f, a, b) &&
	          Near(a.x, 100.0f) && Near(b.y, 65.0f),
	      "a sword: from the node along its axis to the bound's far end");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 2, 0}, NiPoint3{0, 30, 0}, 35.0f, a, b),
	      "an axis that is no unit vector: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 0, 0}, 0.0f, a, b),
	      "no bound (an empty hand): none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, -20, 0}, 5.0f, a, b),
	      "a bound behind the hand: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 300, 0}, 50.0f, a, b),
	      "longer than any weapon: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 30, 0}, kNaN, a, b),
	      "a bound that is not a number: none");
}

void TestMeet() {
	std::printf("Two blades meeting\n");
	float s = -1.0f;
	NiPoint3 p;
	// Ours held still across, from x -30 to 30 at y 40; theirs swinging down
	// through it.
	const NiPoint3 oa{-30, 40, 0};
	const NiPoint3 ob{30, 40, 0};
	Check(BladesMeet(oa, ob, oa, ob, NiPoint3{0, 40, 60}, NiPoint3{0, 40, 120}, NiPoint3{0, 40, -60},
	                 NiPoint3{0, 40, 0}, kParryReachUnits, s, p) &&
	          s > 0.0f && s < 1.0f && Near(p.x, 0.0f) && Near(p.y, 40.0f),
	      "theirs swung down through ours held still: met during the frame, at the middle of ours");
	Check(BladesMeet(oa, ob, oa, ob, NiPoint3{0, 41, -10}, NiPoint3{0, 41, 10}, NiPoint3{0, 41, -10},
	                 NiPoint3{0, 41, 10}, kParryReachUnits, s, p) &&
	          s == 0.0f,
	      "touching from the start: met at once");
	Check(!BladesMeet(oa, ob, oa, ob, NiPoint3{0, 60, 60}, NiPoint3{0, 60, 120}, NiPoint3{0, 60, -60},
	                  NiPoint3{0, 60, 0}, kParryReachUnits, s, p),
	      "theirs swung down 20 units beside ours: no");
	Check(BladesMeet(NiPoint3{-30, 0, 0}, NiPoint3{30, 0, 0}, NiPoint3{-30, 80, 0}, NiPoint3{30, 80, 0},
	                 NiPoint3{0, 40, -30}, NiPoint3{0, 40, 30}, NiPoint3{0, 40, -30}, NiPoint3{0, 40, 30},
	                 kParryReachUnits, s, p) &&
	          Near(s, 0.5f),
	      "ours swung through theirs held still: met half way");
}

void TestShield() {
	std::printf("The shield\n");
	float r = 0.0f;
	Check(ShieldBall(true, 30.0f, r) && Near(r, 24.0f), "a worn shield's bound: a ball a little smaller");
	Check(!ShieldBall(false, 30.0f, r), "no shield worn: none");
	Check(!ShieldBall(true, 4.0f, r), "a bound a bare forearm's size: none");
	Check(!ShieldBall(true, 200.0f, r) && !ShieldBall(true, kNaN, r), "a bound no shield has: none");
	float s = -1.0f;
	Check(BladeMeetsBall(NiPoint3{-20, 30, 60}, NiPoint3{20, 30, 60}, NiPoint3{-20, 30, -60}, NiPoint3{20, 30, -60},
	                     NiPoint3{0, 30, 0}, 24.0f, s) &&
	          Near(s, 0.5f),
	      "their blade swung down onto the shield: met half way");
	Check(!BladeMeetsBall(NiPoint3{-20, 80, 60}, NiPoint3{20, 80, 60}, NiPoint3{-20, 80, -60}, NiPoint3{20, 80, -60},
	                      NiPoint3{0, 30, 0}, 24.0f, s),
	      "swung down 50 units beside it: no");
}

void TestLedger() {
	std::printf("The parried\n");
	ParryLedger l;
	Check(l.Add(7, kParryWindowSeconds) && l.Has(7) && !l.Add(7, kParryWindowSeconds), "parried once");
	l.Step(0.5f);
	Check(l.Has(7), "half a second on: still");
	l.Step(0.4f);
	Check(!l.Has(7), "past the window: no longer");
	l.Add(7, kParryWindowSeconds);
	l.Add(8, kParryWindowSeconds);
	Check(l.Take(7) && !l.Has(7) && l.Has(8), "their blow came: taken, the other kept");
	Check(!l.Take(7), "taken twice: no");
	ParryLedger full;
	for (UInt32 a = 1; a <= kParryLedgerMax; ++a) {
		full.Add(a, 1.0f);
	}
	Check(!full.Add(99, 1.0f) && !full.Has(99), "never more than eight");
	l.Step(kNaN);
	Check(l.Has(8), "a time that is not a number takes nothing off");
}

void TestHitHandler() {
	std::printf("The hit handler's answers\n");
	Check(ForceBlockForParry(true, false, true), "the player, not blocking, someone parried: forced");
	Check(!ForceBlockForParry(true, true, true), "the player blocking already: the engine's yes");
	Check(!ForceBlockForParry(true, false, false), "nobody parried: the engine's no");
	Check(!ForceBlockForParry(false, false, true), "someone else hit: never");
	Check(ConeForParry(true, true, true) == ParryCone::Blocked, "the parried attacker: blocked");
	Check(ConeForParry(true, true, false) == ParryCone::Blocked,
	      "the parried attacker while the player blocks too: blocked whatever the cone");
	Check(ConeForParry(true, false, true) == ParryCone::NotBlocked,
	      "another attacker under a block forced for a parry: a hit");
	Check(ConeForParry(true, false, false) == ParryCone::Original, "a real block, nobody parried: the cone decides");
	Check(ConeForParry(false, true, true) == ParryCone::Original, "not the player: the cone decides");
	Check(Near(ParriedShare(true, true, 0.25f), 1.0f), "a parried blow: all of it blocked");
	Check(Near(ParriedShare(true, false, 0.25f), 0.25f), "with ParryStopsAll off: the engine's share");
	Check(Near(ParriedShare(false, true, 0.25f), 0.25f), "any other blow: the engine's share");
	Check(Near(ParryFatigueCost(10.0f), 10.0f) && ParryFatigueCost(-1.0f) == 0.0f && ParryFatigueCost(kNaN) == 0.0f &&
	          ParryFatigueCost(5000.0f) == 0.0f,
	      "the cost: as set, nothing for one out of reason");
}

void TestHeld() {
	std::printf("Someone held by the off hand\n");
	const UInt32 held = 0x1000;
	const UInt32 other = 0x2000;
	BlowStop s = BlowStopFor(true, false, held, held);
	Check(s.held && !s.parried, "the one held strikes: stopped as held");
	s = BlowStopFor(false, false, held, held);
	Check(s.held && !s.parried, "held with the parries off or no weapon drawn: still stopped");
	s = BlowStopFor(true, true, held, held);
	Check(s.held && !s.parried, "held and in a parry's window too: held (nothing owed for a parry)");
	s = BlowStopFor(true, true, held, other);
	Check(!s.held && s.parried, "another, parried, while someone is held: parried");
	s = BlowStopFor(false, true, held, other);
	Check(!s.held && !s.parried, "another in a window with the parries off: not stopped");
	s = BlowStopFor(true, false, 0, other);
	Check(!s.held && !s.parried, "nobody held, nobody parried: not stopped");
	Check(AnyBlowStopped(true, 1, 0), "a parry in its window: a block forced");
	Check(!AnyBlowStopped(false, 1, 0), "a window left over with the parries off: nothing forced");
	Check(AnyBlowStopped(false, 0, held), "someone held, the parries off: a block forced");
	Check(!AnyBlowStopped(true, 0, 0), "nothing at all: nothing forced");
	BlowStop h;
	h.held = true;
	BlowStop p;
	p.parried = true;
	Check(Near(StoppedShare(h, false, 0.25f), 1.0f), "held: all of the blow, ParryStopsAll off or not");
	Check(Near(StoppedShare(p, true, 0.25f), 1.0f), "parried with ParryStopsAll: all of it");
	Check(Near(StoppedShare(p, false, 0.25f), 0.25f), "parried without: the engine's share");
	Check(Near(StoppedShare(BlowStop{}, true, 0.25f), 0.25f), "neither: the engine's share");
}

// A foe swinging down through x 0, y 40, from z 60 to -60 during the frame.
BladeFoe Swinging() {
	BladeFoe foe;
	foe.actor = 0x1A000000;
	foe.action = 2;
	foe.bladeValid = true;
	foe.lastA = NiPoint3{0, 40, 60};
	foe.lastB = NiPoint3{0, 40, 120};
	foe.a = NiPoint3{0, 40, -60};
	foe.b = NiPoint3{0, 40, 0};
	return foe;
}

// A guard held still across their swing at y 40, or 30 units beside it.
void HoldStill(bool met, NiPoint3& lastA, NiPoint3& lastB, NiPoint3& a, NiPoint3& b) {
	const float y = met ? 40.0f : 70.0f;
	lastA = NiPoint3{-30, y, 0};
	lastB = NiPoint3{30, y, 0};
	a = lastA;
	b = lastB;
}

void TestWhatMeets() {
	std::printf("What of the player's meets their blade\n");
	const BladeFoe foe = Swinging();
	ParryGuards blade;
	blade.blade = true;
	HoldStill(true, blade.bladeLastA, blade.bladeLastB, blade.bladeA, blade.bladeB);
	ParryMeet m = WhatMeetsTheirBlade(blade, foe);
	Check(m.by == ParryBy::Blade && m.share > 0.0f && m.share < 1.0f && Near(m.point.y, 40.0f),
	      "the blade in their swing: the blades met");

	ParryGuards torch;
	torch.torch = true;
	HoldStill(true, torch.torchLastA, torch.torchLastB, torch.torchA, torch.torchB);
	m = WhatMeetsTheirBlade(torch, foe);
	Check(m.by == ParryBy::Torch && Near(m.point.x, 0.0f) && Near(m.point.y, 40.0f),
	      "the torch in their swing: their blade met the torch, on its shaft");
	ParryGuards torchBeside = torch;
	HoldStill(false, torchBeside.torchLastA, torchBeside.torchLastB, torchBeside.torchA, torchBeside.torchB);
	Check(WhatMeetsTheirBlade(torchBeside, foe).by == ParryBy::None, "the torch 30 units beside it: nothing met");
	ParryGuards torchNotHeld = torch;
	torchNotHeld.torch = false;
	Check(WhatMeetsTheirBlade(torchNotHeld, foe).by == ParryBy::None,
	      "a torch where their blade goes but none held: nothing met");

	ParryGuards shield;
	shield.shield = true;
	shield.shieldCentre = NiPoint3{0, 40, 0};
	shield.shieldRadius = 20.0f;
	m = WhatMeetsTheirBlade(shield, foe);
	Check(m.by == ParryBy::Shield && Near(m.point.y, 40.0f), "the shield in their swing: met at its centre");

	ParryGuards both = blade;
	both.torch = true;
	HoldStill(true, both.torchLastA, both.torchLastB, both.torchA, both.torchB);
	Check(WhatMeetsTheirBlade(both, foe).by == ParryBy::Blade, "the blade and the torch both in it: the blade's");
	ParryGuards bladeBeside = both;
	HoldStill(false, bladeBeside.bladeLastA, bladeBeside.bladeLastB, bladeBeside.bladeA, bladeBeside.bladeB);
	Check(WhatMeetsTheirBlade(bladeBeside, foe).by == ParryBy::Torch, "the blade beside it, the torch in it: the torch's");

	BladeFoe idle = foe;
	idle.action = 0;
	Check(WhatMeetsTheirBlade(both, idle).by == ParryBy::None, "their blade where ours are, no attack: nothing");
	BladeFoe bare = foe;
	bare.bladeValid = false;
	Check(WhatMeetsTheirBlade(both, bare).by == ParryBy::None, "attacking without a blade read: nothing");
	Check(WhatMeetsTheirBlade(ParryGuards{}, foe).by == ParryBy::None, "nothing held: nothing");

	Check(ParryFeltInWeaponHand(ParryBy::Blade) && !ParryFeltInWeaponHand(ParryBy::Shield) &&
	          !ParryFeltInWeaponHand(ParryBy::Torch),
	      "felt in the hand that met it: the weapon hand for the blade, the other for the shield and the torch");
	Check(ParrySoundForm(ParryBy::Blade, false) == kParrySoundBlade && ParrySoundForm(ParryBy::Blade, true) == kParrySoundBlunt,
	      "the blade's clash by the weapon: an edge's, a mace's");
	Check(ParrySoundForm(ParryBy::Shield, false) == kParrySoundShield, "the shield's knock");
	Check(ParrySoundForm(ParryBy::Torch, false) == kParrySoundBlunt, "the torch's, wood: the blunt clash");
	Check(ParrySoundForm(ParryBy::None, false) == 0, "nothing met: no sound");
}

void TestHandToHandStub() {
	std::printf("The hand-to-hand branch's stub\n");
	UInt8 code[kHandToHandStubSize + 4] = {};
	const UInt32 flag = 0x12345678;
	Check(BuildHandToHandStub(code, sizeof(code), flag) == kHandToHandStubSize, "built");
	const UInt8 expected[kHandToHandStubSize] = {
		0x80, 0x3D, 0x78, 0x56, 0x34, 0x12, 0x00,  // cmp byte [flag], 0
		0x75, 0x06,                                // jne stopped
		0x80, 0x7C, 0x24, 0x78, 0x00,              // cmp byte [esp+78h], 0 - the handler's [esp+74h]
		0xC3,                                      // ret
		0x39, 0xE4,                                // stopped: cmp esp, esp - equal
		0xC3,                                      // ret
	};
	bool same = true;
	for (UInt32 i = 0; i < kHandToHandStubSize; ++i) {
		same = same && code[i] == expected[i];
	}
	Check(same, "the flag looked at first, else the handler's own compare one return address further down");
	Check(code[kHandToHandStubSize] == 0, "nothing written past it");
	UInt8 small[kHandToHandStubSize - 1] = {};
	Check(BuildHandToHandStub(small, sizeof(small), flag) == 0, "no room: nothing built");
	Check(BuildHandToHandStub(nullptr, 64, flag) == 0, "no buffer: nothing built");
	Check(kHandToHandCompareBytes[0] == 0x80 && kHandToHandCompareBytes[3] == 0x74 && kHandToHandCompareBytes[5] == 0x74,
	      "the site as read: cmp byte [esp+74h], 0, then je");
}

}  // namespace

void TestFoeStoppers() {
	std::printf("The foes' blades and shields as stoppers\n");
	BladeFoes foes;
	foes.count = 3;
	foes.foe[0].actor = 0x1A000000;
	foes.foe[0].bladeValid = true;
	foes.foe[0].a = NiPoint3{0, 0, 0};
	foes.foe[0].b = NiPoint3{0, 60, 0};
	foes.foe[0].shieldValid = true;
	foes.foe[0].shieldCentre = NiPoint3{10, 10, 10};
	foes.foe[0].shieldNormal = NiPoint3{0, 1, 0};
	foes.foe[0].shieldRadius = 20.0f;
	foes.foe[0].shieldHalfThick = 6.0f;
	foes.foe[1].actor = 0x1B000000;
	foes.foe[1].bladeValid = false;  // a shield alone
	foes.foe[1].shieldValid = true;
	foes.foe[1].shieldCentre = NiPoint3{0, 0, 0};
	foes.foe[1].shieldNormal = NiPoint3{1, 0, 0};
	foes.foe[1].shieldRadius = 15.0f;
	foes.foe[1].shieldHalfThick = 4.0f;
	foes.foe[2].actor = 0;  // nobody
	foes.foe[2].bladeValid = true;
	BladeBodies stoppers;
	BladeDiscs discs;
	Check(AddFoeStoppers(foes, stoppers, discs) == 3, "a blade and two shields added, nobody's skipped");
	Check(stoppers.count == 1 && stoppers.cap[0].actor == FoeBladeId(0x1A000000) &&
	          Near(stoppers.cap[0].radius, kFoeBladeStopRadiusUnits) && Near(stoppers.cap[0].b.y, 60.0f),
	      "the blade's capsule by the foe's blade id");
	Check(discs.count == 2 && discs.disc[0].id == FoeShieldId(0x1A000000) && discs.disc[1].id == FoeShieldId(0x1B000000) &&
	          Near(discs.disc[1].radius, 15.0f) && Near(discs.disc[1].halfThick, 4.0f),
	      "the shields' discs by the foes' shield ids");
	BladeFoes none;
	Check(AddFoeStoppers(none, stoppers, discs) == 0 && stoppers.count == 1, "no foes: nothing added");
	BladeFoes bad;
	bad.count = 1;
	bad.foe[0].actor = 0x1C000000;
	bad.foe[0].bladeValid = false;
	bad.foe[0].shieldValid = true;
	bad.foe[0].shieldNormal = NiPoint3{0, 0, 0};
	bad.foe[0].shieldRadius = 15.0f;
	bad.foe[0].shieldHalfThick = 4.0f;
	Check(AddFoeStoppers(bad, stoppers, discs) == 0 && discs.count == 2, "a shield with no normal: refused");
}

int main() {
	TestFoeStoppers();
	TestActions();
	TestTheirBlade();
	TestMeet();
	TestShield();
	TestLedger();
	TestHitHandler();
	TestHeld();
	TestWhatMeets();
	TestHandToHandStub();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
