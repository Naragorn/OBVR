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
	in.rightRelative = kSettings.swordZone;
	in.leftRelative = NiPoint3{-0.2f, 0.3f, -0.4f};  // resting in front
	in.rightGrip = grip;
	in.equipped = equipped;
	in.seen = seen;
	in.haveSword = true;
	in.haveBow = true;
	in.dt = 0.011f;
	return in;
}

void TestSword() {
	std::printf("The sword from the left hip\n");
	{
		HolsterState s;
		StepHolster(s, AtHip(false, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
		Check(v.readyClick && v.gesture == HolsterKind::Sword && v.rightClaimed && v.rightInZone,
		      "sheathed, a grip at the hip: drawn, and the grip is the holster's");
		HolsterVerdict held =
			StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Drawn), kSettings);
		Check(!held.readyClick && held.rightClaimed, "held on: one click, still claimed");
		HolsterInput away = AtHip(true, EquippedKind::Melee, WeaponSeen::Drawn);
		away.rightRelative = NiPoint3{0.2f, 0.4f, -0.3f};
		held = StepHolster(s, away, kSettings);
		Check(held.rightClaimed && !held.rightInZone, "carried out of the zone: claimed till let go");
		held = StepHolster(s, AtHip(false, EquippedKind::Melee, WeaponSeen::Drawn), kSettings);
		Check(!held.rightClaimed, "let go: the grip grabs again");
		const HolsterVerdict again =
			StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Drawn), kSettings);
		Check(again.readyClick, "drawn, the same reach: sheathed");
	}
	{
		HolsterState s;
		HolsterInput outside = AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed);
		outside.rightRelative = NiPoint3{0.3f, 0.4f, -0.2f};
		StepHolster(s, outside, kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
		Check(!v.readyClick && !v.rightClaimed && v.rightInZone,
		      "a grip closed elsewhere and brought to the hip: still a grab, nothing drawn");
	}
	{
		HolsterState s;
		HolsterVerdict v = StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Drawn), kSettings);
		Check(v.equip == HolsterKind::Sword && !v.readyClick,
		      "the bow in hand: the remembered sword is equipped first");
		v = StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Drawn), kSettings);
		Check(!v.readyClick && v.equip == HolsterKind::None, "until it shows, nothing more");
		v = StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
		Check(v.readyClick, "the sword shows sheathed: drawn");
		v = StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
		Check(!v.readyClick, "once");
	}
	{
		HolsterState s;
		StepHolster(s, AtHip(true, EquippedKind::Bow, WeaponSeen::Drawn), kSettings);
		const HolsterVerdict v =
			StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Drawn), kSettings);
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
		none.haveSword = false;
		const HolsterVerdict v = StepHolster(s, none, kSettings);
		Check(v.refused && !v.readyClick && v.equip == HolsterKind::None && v.rightClaimed,
		      "fists and no sword remembered: refused, the grip still not a grab");
	}
	{
		HolsterState s;
		HolsterInput fists = AtHip(true, EquippedKind::Nothing, WeaponSeen::Sheathed);
		const HolsterVerdict v = StepHolster(s, fists, kSettings);
		Check(v.equip == HolsterKind::Sword, "fists, a sword remembered: it is equipped");
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
	in.equipped = EquippedKind::Melee;
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

void TestGates() {
	std::printf("When it does nothing\n");
	HolsterSettings off = kSettings;
	off.enabled = false;
	HolsterState s;
	HolsterVerdict v = StepHolster(s, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), off);
	Check(!v.readyClick && !v.rightClaimed, "switched off: the grip only grabs");
	HolsterState t;
	HolsterInput menu = AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed);
	menu.allowed = false;
	v = StepHolster(t, menu, kSettings);
	Check(!v.readyClick, "in a menu: nothing");
	HolsterState u;
	StepHolster(u, AtHip(true, EquippedKind::Bow, WeaponSeen::Sheathed), kSettings);
	StepHolster(u, menu, kSettings);
	v = StepHolster(u, AtHip(true, EquippedKind::Melee, WeaponSeen::Sheathed), kSettings);
	Check(!v.readyClick, "a menu in between drops a pending draw");

	HolsterInput mirrored = AtHip(false, EquippedKind::Melee, WeaponSeen::Sheathed);
	mirrored.leftHanded = true;
	mirrored.rightRelative =
		NiPoint3{-kSettings.swordZone.x, kSettings.swordZone.y, kSettings.swordZone.z};
	HolsterState w;
	StepHolster(w, mirrored, kSettings);
	mirrored.rightGrip = true;
	v = StepHolster(w, mirrored, kSettings);
	Check(v.readyClick, "left-handed: the sword hangs at the right hip");
	HolsterState x;
	HolsterInput notMirrored = AtHip(false, EquippedKind::Melee, WeaponSeen::Sheathed);
	notMirrored.leftHanded = true;
	StepHolster(x, notMirrored, kSettings);
	notMirrored.rightGrip = true;
	v = StepHolster(x, notMirrored, kSettings);
	Check(!v.readyClick && !v.rightInZone, "and not at the left");

	HolsterState both;
	HolsterInput two = AtHip(false, EquippedKind::Melee, WeaponSeen::Sheathed);
	two.leftRelative = kSettings.bowZone;
	StepHolster(both, two, kSettings);
	two.rightGrip = true;
	two.leftGrip = true;
	v = StepHolster(both, two, kSettings);
	Check(v.gesture == HolsterKind::Sword && !v.leftClaimed,
	      "both reaches in one frame: the sword's; the left grip stays a grab");
}

}  // namespace

int main() {
	TestBodyFrame();
	TestSword();
	TestBow();
	TestGates();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
