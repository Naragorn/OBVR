// Checks arrows by the shield's contact rule (game/ArrowShieldLogic.h): the
// impact's verdict against the disc - the point on it, beside it, before or
// behind its plane, the flight's segment crossing or missing it, no usable
// point - the verdicts kept per projectile until the hit handler takes them,
// the stub's bytes at 0x0060AD01, and the hit handler's cone taking the kept
// verdict (game/ParryLogic.h, ShieldConeFor).

#include <cstdio>
#include <limits>

#include "game/ArrowShieldLogic.h"
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

// The shield's disc 20 units round, 12 thick, its face ahead (+y), at chest
// height 10 units before the player; an arrow comes from ahead.
ArrowShieldInput AtShield() {
	ArrowShieldInput in;
	in.targetIsPlayer = true;
	in.byContact = true;
	in.discValid = true;
	in.disc.centre = NiPoint3{0.0f, 10.0f, 100.0f};
	in.disc.normal = NiPoint3{0.0f, 2.0f, 0.0f};  // any length
	in.disc.radius = 20.0f;
	in.disc.halfThick = 6.0f;
	in.pointValid = true;
	in.point = NiPoint3{5.0f, 14.0f, 104.0f};  // on the face
	in.fromValid = true;
	in.from = NiPoint3{5.0f, 60.0f, 104.0f};
	return in;
}

void TestPoint() {
	std::printf("The arrow's point against the disc\n");
	ArrowShieldInput in = AtShield();
	ArrowShieldVerdict v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Blocked && !v.bySegment, "the point on the disc's face: blocked");
	in.point = NiPoint3{24.0f, 10.0f, 100.0f};  // 24 out: past the rim, within its 6 margin
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Blocked, "4 past the rim: within the margin, blocked");
	in.point = NiPoint3{0.0f, 10.0f + 6.0f + 9.0f, 100.0f};  // 9 before the face, margin 10
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Blocked, "9 before the face: within the margin, blocked");
	in.from = NiPoint3{60.0f, 10.0f, 30.0f};  // a flight that never passes the disc
	in.point = NiPoint3{40.0f, 10.0f, 100.0f};  // beside it
	v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Through && v.bySegment && v.radialUnits > 39.0f,
	      "the point beside the disc, its flight too: through");
	in.point = NiPoint3{0.0f, 10.0f - 6.0f - 30.0f, 100.0f};  // 30 behind the back face
	in.from = NiPoint3{0.0f, -60.0f, 100.0f};                 // from behind: never through the disc
	v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Through && v.alongUnits > 35.0f, "behind the plane beyond the margin: through");
	in.point = NiPoint3{0.0f, 10.0f + 6.0f + 30.0f, 100.0f};  // 30 before the face, from ahead, short of it
	in.from = NiPoint3{0.0f, 90.0f, 100.0f};
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Through, "before the plane beyond the margin, short of it: through");
}

void TestSegment() {
	std::printf("The flight's segment as the fallback\n");
	ArrowShieldInput in = AtShield();
	// The contact on the body behind the shield: 40 behind its plane, the
	// arrow's last position 60 before it - the flight crossed the disc.
	in.point = NiPoint3{2.0f, -30.0f, 102.0f};
	in.from = NiPoint3{2.0f, 70.0f, 102.0f};
	ArrowShieldVerdict v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Blocked && v.bySegment, "the point on the body behind, the flight through the disc: blocked");
	in.from = NiPoint3{60.0f, 70.0f, 102.0f};  // in from the side, past the rim
	in.point = NiPoint3{30.0f, -30.0f, 102.0f};
	v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Through && v.bySegment, "the flight beside the disc: through");
	in.from = in.point;  // no direction
	v = ArrowAtShieldFor(in);
	Check(v.answer == ArrowAtShield::Through && !v.bySegment, "no flight to follow, the point beside: through");
	in.fromValid = false;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Through, "the position not read, the point beside: through");
}

void TestFallbacks() {
	std::printf("When the disc or the point cannot be judged\n");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	ArrowShieldInput in = AtShield();
	in.point = NiPoint3{nan, 0.0f, 0.0f};
	in.raised = true;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::AsRaised, "no usable point, the hand raised: the vanilla test as raised");
	in.raised = false;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Through, "no usable point, the hand low: the body");
	in = AtShield();
	in.pointValid = false;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Through, "no point at all, the hand low: the body");
	in = AtShield();
	in.discValid = false;
	in.raised = true;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::AsRaised, "the disc lost this frame, raised: as raised");
	in.disc.normal = NiPoint3{0.0f, 0.0f, 0.0f};
	in.discValid = true;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::AsRaised, "a disc without a normal: as raised");
	in = AtShield();
	in.byContact = false;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Vanilla,
	      "the setting off or the disc unread (not by contact): vanilla, the original call");
	in = AtShield();
	in.targetIsPlayer = false;
	Check(ArrowAtShieldFor(in).answer == ArrowAtShield::Vanilla, "an arrow at someone else: vanilla");
}

void TestVerdicts() {
	std::printf("The verdicts kept per projectile\n");
	ArrowVerdicts k;
	bool blocked = false;
	k.Add(0x1000, true, 5);
	k.Add(0x2000, false, 5);  // two arrows in one frame, different verdicts
	Check(k.Take(0x2000, blocked) && !blocked, "the second arrow: its own verdict, through");
	Check(k.Take(0x1000, blocked) && blocked, "the first arrow: its own verdict, blocked");
	Check(!k.Take(0x1000, blocked), "taken once");
	k.Add(0x1000, true, 6);
	Check(!k.Take(0x3000, blocked) && k.count == 1, "a melee attacker the same frame: no verdict of an arrow's");
	k.Add(0, true, 6);
	Check(k.count == 1, "no projectile: nothing kept");
	k.Add(0x1000, false, 7);
	Check(k.count == 1 && k.Take(0x1000, blocked) && !blocked, "the same projectile again: renewed, not doubled");
	for (UInt32 i = 0; i < kArrowVerdictsMax + 2; ++i) {
		k.Add(0x4000 + i, true, 10 + i);
	}
	Check(k.count == kArrowVerdictsMax && !k.Take(0x4000, blocked) && k.Take(0x4009, blocked),
	      "full: the oldest gives way");
	ArrowVerdicts e;
	e.Add(0x1000, true, 10);
	e.Add(0x2000, true, 50);
	Check(e.Expire(10 + kArrowVerdictFrames) == 0 && e.Expire(11 + kArrowVerdictFrames) == 1 && e.count == 1,
	      "an entry the hit handler never asked for runs out");
}

void TestHitHandlerCone() {
	std::printf("The hit handler's cone takes the kept verdict\n");
	// The red case against f067265: an arrow at the disc with the hand low
	// landed there (the blow not placed, the hand not raised).
	Check(ShieldConeFor(true, true, BlockAngle::Engine, false, KeptArrow::Blocked) == ShieldCone::Blocked,
	      "an arrow judged on the disc, the hand low: blocked (f067265: it landed)");
	Check(ShieldConeFor(true, true, BlockAngle::Engine, true, KeptArrow::Through) == ShieldCone::NotBlocked,
	      "an arrow judged beside the disc, the hand raised: it lands");
	Check(ShieldConeFor(true, true, BlockAngle::Engine, true, KeptArrow::None) == ShieldCone::Original,
	      "no kept verdict (the impact as raised): the old fallback, the game's cone");
	Check(ShieldConeFor(true, true, BlockAngle::Engine, false, KeptArrow::None) == ShieldCone::NotBlocked,
	      "no kept verdict, the hand low: it lands, as before");
	Check(ShieldConeFor(true, false, BlockAngle::Engine, false, KeptArrow::Blocked) == ShieldCone::Original,
	      "not the shield's force: the kept verdict has no say");
	Check(ShieldConeFor(false, true, BlockAngle::Engine, false, KeptArrow::Blocked) == ShieldCone::Original,
	      "not the player: the cone");
	// An arrow and a melee blow the same frame: the melee attacker has no
	// kept verdict and is judged by its blade.
	ArrowVerdicts k;
	k.Add(0x5000, true, 1);
	bool b = false;
	const KeptArrow melee = k.Take(0x6000, b) ? (b ? KeptArrow::Blocked : KeptArrow::Through) : KeptArrow::None;
	Check(ShieldConeFor(true, true, BlockAngle::Through, false, melee) == ShieldCone::NotBlocked,
	      "a melee blade beside the shield the same frame: its own verdict, it lands");
	const KeptArrow arrow = k.Take(0x5000, b) ? (b ? KeptArrow::Blocked : KeptArrow::Through) : KeptArrow::None;
	Check(ShieldConeFor(true, true, BlockAngle::Engine, false, arrow) == ShieldCone::Blocked,
	      "and the arrow at the disc: blocked");
}

void TestStub() {
	std::printf("The stub at 0x0060AD01\n");
	UInt8 code[kArrowShieldStubSize + 4] = {};
	Check(BuildArrowShieldStub(code, 8, 0x10000000, 0, 0) == 0, "no room: nothing written");
	const UInt32 base = 0x20000000;
	const UInt32 callback = 0x20001000;
	Check(BuildArrowShieldStub(code, sizeof(code), base, callback, 0x005E5670) == kArrowShieldStubSize, "written whole");
	const auto rel = [&](UInt32 at) {
		return static_cast<UInt32>(code[at]) | (static_cast<UInt32>(code[at + 1]) << 8) |
		       (static_cast<UInt32>(code[at + 2]) << 16) | (static_cast<UInt32>(code[at + 3]) << 24);
	};
	Check(code[6] == 0xE8 && base + 11 + rel(7) == callback, "the call lands on the callback");
	Check(code[19] == 0xE9 && base + 24 + rel(20) == 0x005E5670, "the vanilla way jumps to the original check");
	Check(code[17] == 0x75 && 19 + code[18] == 24 && code[27] == 0x75 && 29 + code[28] == 32 && code[41] == 0x75 &&
	          43 + code[42] == 49,
	      "the short jumps land on their instructions");
	Check(code[43] == 0x68 && rel(44) == kArrowShieldBranch && code[48] == 0xC3, "Blocked: to 0x0060AD80, al = 1");
	Check(code[49] == 0x68 && rel(50) == kArrowBodyBranch && code[54] == 0xC3, "Through: to 0x0060AD84, the body");
	Check(code[29] == 0xB0 && code[30] == 0x01 && code[31] == 0xC3, "AsRaised: Block's yes back to 0x0060AD06");
	Check(code[32] == 0x83 && code[33] == 0xC4 && code[34] == 0x04 && code[35] == 0x8B && code[36] == 0x7D &&
	          code[37] == 0x08,
	      "the decided ways drop the return address and load the point into edi");
}

}  // namespace

int main() {
	TestPoint();
	TestSegment();
	TestFallbacks();
	TestVerdicts();
	TestHitHandlerCone();
	TestStub();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
