// Checks drawing by reaching (vr/Holster.h): the body frame, the zones, and
// every way a reach draws, sheathes, equips first, refuses or gives up.

#include <cmath>
#include <cstdio>

#include "vr/Holster.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

void TestBodyFrame() {
	std::printf("The body's frame\n");
	const NiPoint3 eyes{0.0f, 1.6f, 0.0f};
	// Tracking axes: x right, y up, z back. A hand 0.6 m below the eyes,
	// 0.2 m left and 0.1 m ahead.
	const NiPoint3 hand{-0.2f, 1.0f, -0.1f};
	NiPoint3 r = BodyRelative(Quaternion::Identity(), eyes, hand);
	Check(Near(r.x, -0.2f) && Near(r.y, 0.1f) && Near(r.z, -0.6f), "game axes: right, forward, up");
	const Quaternion left90 = FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	// Facing -x: the hand at -x is ahead, at +z (back in tracking) is left.
	r = BodyRelative(left90, eyes, NiPoint3{-0.3f, 1.6f, 0.2f});
	Check(Near(r.y, 0.3f) && Near(r.x, -0.2f), "turned with the head's heading");
	const Quaternion lookDown = FromAxisAngle(1.0f, 0.0f, 0.0f, -60.0f);
	r = BodyRelative(lookDown, eyes, hand);
	Check(Near(r.x, -0.2f) && Near(r.y, 0.1f) && Near(r.z, -0.6f),
	      "looking down at the hip does not move the hip");
	const Quaternion straightDown = FromAxisAngle(1.0f, 0.0f, 0.0f, -90.0f);
	r = BodyRelative(straightDown, eyes, hand);
	Check(std::isfinite(r.x) && Near(r.z, -0.6f), "a head straight down still gives a frame");
	Check(InZone(NiPoint3{0.1f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.2f) &&
	          !InZone(NiPoint3{0.3f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.2f),
	      "in a zone within its radius, not beyond");
}

const HolsterSettings kSettings;

HolsterInput AtHip(bool grip, EquippedKind equipped, WeaponSeen seen) {
	HolsterInput in;
	in.allowed = true;
	in.rightValid = true;
	in.leftValid = true;
	in.rightRelative = kSettings.oneHandZone;
	in.leftRelative = NiPoint3{-0.2f, 0.3f, -0.4f};  // resting in front
	in.rightGrip = grip;
	in.equipped = equipped;
	in.seen = seen;
	in.haveOneHand = true;
	in.haveBow = true;
	in.dt = 0.011f;
	return in;
}

void TestSword() {
	std::printf("The sword from the left hip\n");
	{
		HolsterState s;
		StepHolster(s, AtHip(false, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
		Check(v.readyClick && v.gesture == HolsterKind::OneHand && v.rightClaimed && v.rightInZone,
		      "sheathed, a grip at the hip: drawn, and the grip is the holster's");
		HolsterVerdict held =
			StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Drawn), kSettings);
		Check(!held.readyClick && held.rightClaimed, "held on: one click, still claimed");
		HolsterInput away = AtHip(true, EquippedKind::OneHand, WeaponSeen::Drawn);
		away.rightRelative = NiPoint3{0.2f, 0.4f, -0.3f};
		held = StepHolster(s, away, kSettings);
		Check(held.rightClaimed && !held.rightInZone, "carried out of the zone: claimed till let go");
		held = StepHolster(s, AtHip(false, EquippedKind::OneHand, WeaponSeen::Drawn), kSettings);
		Check(!held.rightClaimed, "let go: the grip grabs again");
		const HolsterVerdict again =
			StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Drawn), kSettings);
		Check(again.readyClick, "drawn, the same reach: sheathed");
	}
	{
		HolsterState s;
		HolsterInput outside = AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed);
		outside.rightRelative = NiPoint3{0.3f, 0.4f, -0.2f};
		StepHolster(s, outside, kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
		Check(!v.readyClick && !v.rightClaimed && v.rightInZone,
		      "a grip closed elsewhere and brought to the hip: still a grab, nothing drawn");
	}
	{
		HolsterState s;
		HolsterVerdict v = StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
		Check(v.equip == HolsterKind::OneHand && !v.readyClick,
		      "the bow sheathed: the remembered sword is equipped first");
		v = StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
		Check(!v.readyClick && v.equip == HolsterKind::None, "until it shows, nothing more");
		v = StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
		Check(v.readyClick, "the sword shows sheathed: drawn");
		v = StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
		Check(!v.readyClick, "once");
	}
	{
		HolsterState s;
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Drawn), kSettings);
		Check(v.otherDrawn && !v.readyClick && v.equip == HolsterKind::None && v.rightClaimed,
		      "the bow drawn: the sword stays put until the bow is back");
	}
	{
		HolsterState s;
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::Nothing, WeaponSeen::Drawn), kSettings);
		Check(v.equip == HolsterKind::OneHand && !v.otherDrawn, "fists up are not in the way");
	}
	{
		HolsterState s;
		StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Drawn), kSettings);
		Check(!v.readyClick && !v.gaveUp, "the sword shows already drawn: nothing to click");
	}
	{
		HolsterState s;
		StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
		HolsterInput waiting = AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed);
		waiting.dt = 2.5f;
		const HolsterVerdict v = StepHolster(s, waiting, kSettings);
		Check(v.gaveUp && !v.readyClick, "the sword never shows: given up");
	}
	{
		HolsterState s;
		HolsterInput none = AtHip(true, EquippedKind::Nothing, WeaponSeen::Sheathed);
		none.haveOneHand = false;
		const HolsterVerdict v = StepHolster(s, none, kSettings);
		Check(v.refused && !v.readyClick && v.equip == HolsterKind::None && v.rightClaimed,
		      "fists and no sword remembered: refused, the grip still not a grab");
	}
	{
		HolsterState s;
		HolsterInput fists = AtHip(true, EquippedKind::Nothing, WeaponSeen::Sheathed);
		const HolsterVerdict v = StepHolster(s, fists, kSettings);
		Check(v.equip == HolsterKind::OneHand, "fists, a sword remembered: it is equipped");
	}
}

void TestBow() {
	std::printf("The bow from the left shoulder\n");
	HolsterInput in = AtHip(false, EquippedKind::Bow, WeaponSeen::Sheathed);
	in.rightRelative = NiPoint3{0.2f, 0.3f, -0.4f};
	in.leftRelative = kSettings.bowZone;
	HolsterState s;
	StepHolster(s, in, kSettings);
	in.leftGrip = true;
	HolsterVerdict v = StepHolster(s, in, kSettings);
	Check(v.readyClick && v.gesture == HolsterKind::Bow && v.leftClaimed && !v.rightClaimed,
	      "the left hand at the shoulder: the bow drawn");
	HolsterState t;
	in.leftGrip = false;
	in.equipped = EquippedKind::OneHand;
	StepHolster(t, in, kSettings);
	in.leftGrip = true;
	v = StepHolster(t, in, kSettings);
	Check(v.equip == HolsterKind::Bow, "a sword in hand: the remembered bow equipped first");
	HolsterState u;
	HolsterInput wrong = AtHip(false, EquippedKind::Bow, WeaponSeen::Sheathed);
	wrong.rightRelative = kSettings.bowZone;
	StepHolster(u, wrong, kSettings);
	wrong.rightGrip = true;
	v = StepHolster(u, wrong, kSettings);
	Check(v.gesture == HolsterKind::None, "the right hand at the left shoulder draws nothing");
}

void TestTwoHanded() {
	std::printf("Two-handed weapons and staffs over the right shoulder\n");
	Check(KindOfWeaponType(0) == EquippedKind::OneHand && KindOfWeaponType(2) == EquippedKind::OneHand,
	      "one-handed blade and blunt: the hip");
	Check(KindOfWeaponType(1) == EquippedKind::TwoHand && KindOfWeaponType(3) == EquippedKind::TwoHand &&
	          KindOfWeaponType(4) == EquippedKind::TwoHand,
	      "two-handed blade and blunt, and the staff: the right shoulder");
	Check(KindOfWeaponType(5) == EquippedKind::Bow, "the bow: the left shoulder");
	Check(KindOfWeaponType(-1) == EquippedKind::Nothing && KindOfWeaponType(9) == EquippedKind::Nothing,
	      "no weapon, or a type the game does not have: nothing");

	HolsterInput in = AtHip(false, EquippedKind::TwoHand, WeaponSeen::Sheathed);
	in.rightRelative = kSettings.twoHandZone;
	in.haveTwoHand = true;
	HolsterState s;
	StepHolster(s, in, kSettings);
	in.rightGrip = true;
	HolsterVerdict v = StepHolster(s, in, kSettings);
	Check(v.readyClick && v.gesture == HolsterKind::TwoHand && v.rightClaimed,
	      "the weapon hand over the right shoulder: the two-handed weapon drawn");

	HolsterState t;
	in.rightGrip = false;
	in.equipped = EquippedKind::OneHand;
	StepHolster(t, in, kSettings);
	in.rightGrip = true;
	v = StepHolster(t, in, kSettings);
	Check(v.equip == HolsterKind::TwoHand, "a sheathed sword in the slot: the two-hander equipped first");
	in.rightGrip = false;
	StepHolster(t, in, kSettings);
	in.equipped = EquippedKind::TwoHand;
	v = StepHolster(t, in, kSettings);
	Check(v.readyClick, "and drawn once it shows");

	HolsterState u;
	HolsterInput drawn = in;
	drawn.equipped = EquippedKind::OneHand;
	drawn.seen = WeaponSeen::Drawn;
	drawn.rightGrip = false;
	StepHolster(u, drawn, kSettings);
	drawn.rightGrip = true;
	v = StepHolster(u, drawn, kSettings);
	Check(v.otherDrawn && !v.readyClick, "a sword drawn: the two-hander waits until it is back");

	HolsterState w;
	HolsterInput none = in;
	none.equipped = EquippedKind::Nothing;
	none.haveTwoHand = false;
	none.rightGrip = false;
	StepHolster(w, none, kSettings);
	none.rightGrip = true;
	v = StepHolster(w, none, kSettings);
	Check(v.refused && v.gesture == HolsterKind::TwoHand, "no two-hander seen: refused");

	HolsterState x;
	HolsterInput leftAtBack = AtHip(false, EquippedKind::TwoHand, WeaponSeen::Sheathed);
	leftAtBack.rightRelative = NiPoint3{0.2f, 0.3f, -0.4f};
	leftAtBack.leftRelative = kSettings.twoHandZone;
	StepHolster(x, leftAtBack, kSettings);
	leftAtBack.leftGrip = true;
	v = StepHolster(x, leftAtBack, kSettings);
	Check(v.gesture == HolsterKind::None, "the other hand over the right shoulder draws nothing");

	HolsterState y;
	HolsterInput mirrored = AtHip(false, EquippedKind::TwoHand, WeaponSeen::Sheathed);
	mirrored.leftHanded = true;
	mirrored.rightRelative =
		NiPoint3{-kSettings.twoHandZone.x, kSettings.twoHandZone.y, kSettings.twoHandZone.z};
	StepHolster(y, mirrored, kSettings);
	mirrored.rightGrip = true;
	v = StepHolster(y, mirrored, kSettings);
	Check(v.gesture == HolsterKind::TwoHand, "left-handed: over the left shoulder");
}

void TestGates() {
	std::printf("When it does nothing\n");
	HolsterSettings off = kSettings;
	off.enabled = false;
	HolsterState s;
	HolsterVerdict v = StepHolster(s, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), off);
	Check(!v.readyClick && !v.rightClaimed, "switched off: the grip only grabs");
	HolsterState t;
	HolsterInput menu = AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed);
	menu.allowed = false;
	v = StepHolster(t, menu, kSettings);
	Check(!v.readyClick, "in a menu: nothing");
	HolsterState u;
	StepHolster(u, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
	StepHolster(u, menu, kSettings);
	v = StepHolster(u, AtHip(true, EquippedKind::OneHand, WeaponSeen::Sheathed), kSettings);
	Check(!v.readyClick, "a menu in between drops a pending draw");

	HolsterInput mirrored = AtHip(false, EquippedKind::OneHand, WeaponSeen::Sheathed);
	mirrored.leftHanded = true;
	mirrored.rightRelative =
		NiPoint3{-kSettings.oneHandZone.x, kSettings.oneHandZone.y, kSettings.oneHandZone.z};
	HolsterState w;
	StepHolster(w, mirrored, kSettings);
	mirrored.rightGrip = true;
	v = StepHolster(w, mirrored, kSettings);
	Check(v.readyClick, "left-handed: the sword hangs at the right hip");
	HolsterState x;
	HolsterInput notMirrored = AtHip(false, EquippedKind::OneHand, WeaponSeen::Sheathed);
	notMirrored.leftHanded = true;
	StepHolster(x, notMirrored, kSettings);
	notMirrored.rightGrip = true;
	v = StepHolster(x, notMirrored, kSettings);
	Check(!v.readyClick && !v.rightInZone, "and not at the left");

	HolsterState both;
	HolsterInput two = AtHip(false, EquippedKind::OneHand, WeaponSeen::Sheathed);
	two.leftRelative = kSettings.bowZone;
	StepHolster(both, two, kSettings);
	two.rightGrip = true;
	two.leftGrip = true;
	v = StepHolster(both, two, kSettings);
	Check(v.gesture == HolsterKind::OneHand && !v.leftClaimed,
	      "both reaches in one frame: the sword's; the left grip stays a grab");
}

void TestSides() {
	std::printf("Which side each place is on\n");
	const NiPoint3 hip = kSettings.oneHandZone;  // the left hip, a right-hander's
	Check(Near(ZoneFor(hip, HolsterSide::Auto, false).x, hip.x), "auto, right-handed: as set");
	Check(Near(ZoneFor(hip, HolsterSide::Auto, true).x, -hip.x), "auto, left-handed: mirrored");
	const NiPoint3 right = ZoneFor(hip, HolsterSide::Right, false);
	Check(Near(right.x, 0.18f) && Near(right.y, hip.y) && Near(right.z, hip.z),
	      "right: on the right, only the side changes");
	Check(Near(ZoneFor(hip, HolsterSide::Right, true).x, 0.18f), "right stays right when left-handed");
	Check(Near(ZoneFor(kSettings.twoHandZone, HolsterSide::Left, false).x, -0.18f),
	      "left: the right shoulder's place put on the left");
	Check(Near(ZoneFor(kSettings.twoHandZone, HolsterSide::Left, true).x, -0.18f), "left stays left when left-handed");

	HolsterSide side = HolsterSide::Auto;
	Check(ParseHolsterSide("Left", side) && side == HolsterSide::Left, "the INI's word, any case");
	Check(!ParseHolsterSide("middle", side) && side == HolsterSide::Left, "another word: kept");
	Check(HolsterSideFromIndex(1.0f) == HolsterSide::Right && HolsterSideFromIndex(7.0f) == HolsterSide::Auto &&
	          HolsterSideFromIndex(-1.0f) == HolsterSide::Auto,
	      "the settings row's value, out of range auto");

	// Mix and match: a right-hander's two-hander over the left shoulder.
	HolsterSettings mixed = kSettings;
	mixed.twoHandSide = HolsterSide::Left;
	HolsterInput in = AtHip(false, EquippedKind::TwoHand, WeaponSeen::Sheathed);
	in.haveTwoHand = true;
	in.rightRelative = NiPoint3{-0.18f, kSettings.twoHandZone.y, kSettings.twoHandZone.z};
	HolsterState s;
	StepHolster(s, in, mixed);
	in.rightGrip = true;
	HolsterVerdict v = StepHolster(s, in, mixed);
	Check(v.readyClick && v.gesture == HolsterKind::TwoHand, "the two-hander set left: drawn over the left shoulder");
	HolsterState t;
	in.rightGrip = false;
	in.rightRelative = kSettings.twoHandZone;
	StepHolster(t, in, mixed);
	in.rightGrip = true;
	v = StepHolster(t, in, mixed);
	Check(!v.rightInZone && !v.readyClick, "and no longer over the right");
}

}  // namespace

int main() {
	TestSides();
	TestBodyFrame();
	TestSword();
	TestBow();
	TestGates();
	TestTwoHanded();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
