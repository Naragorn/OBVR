// Checks drawing by reaching (vr/Holster.h): the body frame, the zones, and
// every way a reach draws, sheathes, equips first, refuses or gives up.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "vr/Holster.h"
#include "vr/SheathMirror.h"
#include "vr/SheathReach.h"

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
	{
		const NiPoint3 there = BodyToTracking(left90, eyes, NiPoint3{0.18f, -0.12f, -0.15f});
		const NiPoint3 back = BodyRelative(left90, eyes, there);
		Check(Near(back.x, 0.18f) && Near(back.y, -0.12f) && Near(back.z, -0.15f), "the way back to tracking space and again");
	}
	Check(InZone(NiPoint3{0.1f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.2f) &&
	          !InZone(NiPoint3{0.3f, 0.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 0.0f}, 0.2f),
	      "in a zone within its radius, not beyond");
}

const HolsterSettings kSettings;

constexpr UInt32 kHipL = static_cast<UInt32>(HolsterSlot::HipLeft);
constexpr UInt32 kHipR = static_cast<UInt32>(HolsterSlot::HipRight);
constexpr UInt32 kBackL = static_cast<UInt32>(HolsterSlot::BackLeft);
constexpr UInt32 kBackR = static_cast<UInt32>(HolsterSlot::BackRight);

NiPoint3 PlaceAt(HolsterSlot slot) { return SlotZonesFor(kSettings).at[static_cast<UInt32>(slot)]; }

// Both hands resting in front, nothing equipped, nothing kept anywhere.
HolsterInput Rest() {
	HolsterInput in;
	in.allowed = true;
	in.rightValid = true;
	in.leftValid = true;
	in.rightRelative = NiPoint3{0.2f, 0.3f, -0.4f};
	in.leftRelative = NiPoint3{-0.2f, 0.3f, -0.4f};
	in.seen = WeaponSeen::Sheathed;
	in.dt = 0.02f;
	return in;
}

// A hand's grip closing at a place: a frame open there, then a frame closed.
HolsterVerdict Press(HolsterState& s, HolsterInput in, bool leftHand, HolsterSlot slot,
                     const HolsterSettings& settings = kSettings) {
	(leftHand ? in.leftRelative : in.rightRelative) = PlaceAt(slot);
	(leftHand ? in.leftGrip : in.rightGrip) = false;
	StepHolster(s, in, settings);
	(leftHand ? in.leftGrip : in.rightGrip) = true;
	return StepHolster(s, in, settings);
}

void TestPlaces() {
	std::printf("The four places and what each takes\n");
	const SlotZones z = SlotZonesFor(kSettings);
	Check(z.at[kHipL].x < 0.0f && z.at[kHipR].x > 0.0f && Near(z.at[kHipL].z, z.at[kHipR].z),
	      "the hips: the one-hander's place on both sides");
	Check(z.at[kBackL].x < 0.0f && z.at[kBackR].x > 0.0f, "the back: the bow's place left, the two-hander's right");
	Check(SlotAt(z, z.at[kHipR]) == HolsterSlot::HipRight && SlotAt(z, NiPoint3{0.0f, 0.5f, 0.0f}) == HolsterSlot::None,
	      "a hand at a place finds it, in front of the chest none");
	Check(SlotAccepts(HolsterSlot::HipLeft, SlotItem::OneHand) && SlotAccepts(HolsterSlot::HipRight, SlotItem::Torch) &&
	          !SlotAccepts(HolsterSlot::HipLeft, SlotItem::TwoHand) && !SlotAccepts(HolsterSlot::HipRight, SlotItem::Bow),
	      "the hips: one-handers and the torch, no two-hander, no bow");
	Check(SlotAccepts(HolsterSlot::BackLeft, SlotItem::TwoHand) && SlotAccepts(HolsterSlot::BackRight, SlotItem::Bow) &&
	          SlotAccepts(HolsterSlot::BackLeft, SlotItem::OneHand) && SlotAccepts(HolsterSlot::BackRight, SlotItem::OneHand) &&
	          !SlotAccepts(HolsterSlot::BackRight, SlotItem::Torch) && !SlotAccepts(HolsterSlot::BackLeft, SlotItem::Torch),
	      "the shoulders: two-handers, staffs, the bow and one-handers (2026-11-26); no torch");
	Check(!SlotAccepts(HolsterSlot::None, SlotItem::OneHand), "no place: no one-hander");
	bool shieldEverywhere = true;
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		shieldEverywhere = shieldEverywhere && SlotAccepts(static_cast<HolsterSlot>(i), SlotItem::Shield);
	}
	Check(shieldEverywhere && !SlotAccepts(HolsterSlot::None, SlotItem::Shield) &&
	          !SlotAccepts(HolsterSlot::HipLeft, SlotItem::Nothing),
	      "the shield at all four; no place for nothing");
	Check(WeaponHandItem(SlotItem::OneHand) && WeaponHandItem(SlotItem::TwoHand) && !WeaponHandItem(SlotItem::Bow) &&
	          !WeaponHandItem(SlotItem::Shield) && !WeaponHandItem(SlotItem::Torch),
	      "the weapon hand's: the one- and two-handers; the bow, shield and torch the other hand's");
	Check(SlotItemOf(EquippedKind::TwoHand) == SlotItem::TwoHand && SlotItemOf(EquippedKind::Nothing) == SlotItem::Nothing &&
	          SlotItemOf(OffHandItem::Torch) == SlotItem::Torch && SlotItemOf(OffHandItem::Nothing) == SlotItem::Nothing,
	      "the kinds as the places know them");
	Check(DefaultSlotFor(SlotItem::OneHand, kSettings, false) == HolsterSlot::HipLeft &&
	          DefaultSlotFor(SlotItem::TwoHand, kSettings, false) == HolsterSlot::BackRight &&
	          DefaultSlotFor(SlotItem::Bow, kSettings, false) == HolsterSlot::BackLeft &&
	          DefaultSlotFor(SlotItem::Shield, kSettings, false) == HolsterSlot::None,
	      "a right-hander's defaults: sword left hip, claymore right shoulder, bow left; a shield none");
	Check(DefaultSlotFor(SlotItem::OneHand, kSettings, true) == HolsterSlot::HipRight &&
	          DefaultSlotFor(SlotItem::Bow, kSettings, true) == HolsterSlot::BackRight,
	      "left-handed: mirrored");
	HolsterSettings chosen = kSettings;
	chosen.oneHandSide = HolsterSide::Right;
	Check(DefaultSlotFor(SlotItem::OneHand, chosen, false) == HolsterSlot::HipRight, "a side chosen: that side");
	{
		SlotItem kept[kHolsterSlotCount] = {};
		Check(PlaceForUnkept(SlotItem::OneHand, HolsterSlot::HipLeft, kept) == HolsterSlot::HipLeft,
		      "a sword equipped in the inventory: its home when free");
		kept[kHipL] = SlotItem::OneHand;
		Check(PlaceForUnkept(SlotItem::OneHand, HolsterSlot::HipLeft, kept) == HolsterSlot::HipRight,
		      "its home keeps another sword: the other hip, the first one stays kept");
		kept[kHipR] = SlotItem::Torch;
		Check(PlaceForUnkept(SlotItem::OneHand, HolsterSlot::HipLeft, kept) == HolsterSlot::HipLeft,
		      "both hips taken: its home, in the place of what was kept there - not a free shoulder (2026-11-26)");
		kept[kBackR] = SlotItem::OneHand;
		Check(PlaceForUnkept(SlotItem::TwoHand, HolsterSlot::BackRight, kept) == HolsterSlot::BackLeft,
		      "a two-hander whose home keeps a one-hander: the other free shoulder");
		kept[kBackL] = SlotItem::Bow;
		Check(PlaceForUnkept(SlotItem::TwoHand, HolsterSlot::BackRight, kept) == HolsterSlot::BackRight,
		      "both shoulders taken: its home, in the place of the one-hander");
		kept[kBackR] = SlotItem::Nothing;
		kept[kBackL] = SlotItem::Nothing;
		Check(PlaceForUnkept(SlotItem::TwoHand, HolsterSlot::BackRight, kept) == HolsterSlot::BackRight &&
		          PlaceForUnkept(SlotItem::TwoHand, HolsterSlot::HipLeft, kept) == HolsterSlot::None &&
		          PlaceForUnkept(SlotItem::OneHand, HolsterSlot::None, kept) == HolsterSlot::None,
		      "a home that does not take it, or none: nowhere");
	}
	Check(SideOfSlot(HolsterSlot::BackLeft) == HolsterSide::Left && SideOfSlot(HolsterSlot::HipRight) == HolsterSide::Right &&
	          SideOfSlot(HolsterSlot::None) == HolsterSide::Auto,
	      "the side of each place, for the sheaths");
}

void TestWeaponHand() {
	std::printf("The weapon hand at a place\n");
	// The sword kept at the right hip, equipped and sheathed: the reach draws it.
	HolsterInput in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.slotItem[kHipR] = SlotItem::OneHand;
	in.slotWorn[kHipR] = true;
	in.equippedSlot = HolsterSlot::HipRight;
	HolsterState s;
	HolsterVerdict v = Press(s, in, false, HolsterSlot::HipRight);
	Check(v.readyClick && v.outcome == SlotOutcome::Draw && v.rightClaimed && v.gesture == HolsterSlot::HipRight,
	      "the equipped sword kept there: drawn, the grip the holster's");
	// Drawn, at the left hip: sheathed and kept there.
	in.seen = WeaponSeen::Drawn;
	HolsterState t;
	v = Press(t, in, false, HolsterSlot::HipLeft);
	Check(v.readyClick && v.outcome == SlotOutcome::Sheathe && v.assign == HolsterSlot::HipLeft,
	      "drawn, at the other hip: sheathed there, kept there");
	// Drawn, over the shoulder: a one-hander goes there too (2026-11-26).
	HolsterState u;
	v = Press(u, in, false, HolsterSlot::BackRight);
	Check(v.readyClick && v.outcome == SlotOutcome::Sheathe && v.assign == HolsterSlot::BackRight && v.rightClaimed,
	      "a sword at a free shoulder: sheathed there, kept there");
	// Drawn, at a shoulder keeping a two-hander: it goes back first, as at a hip.
	in.slotItem[kBackR] = SlotItem::TwoHand;
	HolsterState u2;
	v = Press(u2, in, false, HolsterSlot::BackRight);
	Check(!v.readyClick && v.outcome == SlotOutcome::OtherDrawn, "a sword at a shoulder keeping a two-hander: put it away first");
	in.slotItem[kBackR] = SlotItem::Nothing;
	// Drawn, at a hip that keeps a torch: taken.
	in.slotItem[kHipL] = SlotItem::Torch;
	HolsterState w;
	v = Press(w, in, false, HolsterSlot::HipLeft);
	Check(!v.readyClick && v.outcome == SlotOutcome::Full, "a hip with a torch kept: the place is taken");
	in.slotItem[kBackR] = SlotItem::TwoHand;
	HolsterState w2;
	v = Press(w2, in, false, HolsterSlot::BackRight);
	Check(!v.readyClick && v.equip == HolsterSlot::None && v.outcome == SlotOutcome::OtherDrawn,
	      "drawn, at the back keeping a claymore: the sword goes back first");
	in.slotItem[kHipL] = SlotItem::OneHand;
	HolsterState w3;
	v = Press(w3, in, false, HolsterSlot::HipLeft);
	Check(v.outcome == SlotOutcome::OtherDrawn, "drawn, at a hip keeping another sword: the same");
	in.slotItem[kHipL] = SlotItem::Torch;
	in.slotItem[kBackR] = SlotItem::Nothing;
	// Sheathed, at the right hip again: back where it is kept, the draw.
	in.seen = WeaponSeen::Sheathed;
	HolsterState x;
	v = Press(x, in, false, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::Draw, "sheathed: drawn from its own place");

	// A claymore kept over the right shoulder, the sword equipped: equipped,
	// then drawn once the game shows it.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.equippedSlot = HolsterSlot::HipLeft;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.slotItem[kBackR] = SlotItem::TwoHand;
	HolsterState y;
	v = Press(y, in, false, HolsterSlot::BackRight);
	Check(v.equip == HolsterSlot::BackRight && !v.readyClick && v.outcome == SlotOutcome::EquipDraw,
	      "the claymore kept at the back, the sword in the slot: equipped first");
	in.rightGrip = true;
	in.rightRelative = PlaceAt(HolsterSlot::BackRight);
	v = StepHolster(y, in, kSettings);
	Check(!v.readyClick && v.equip == HolsterSlot::None, "not shown yet: waits");
	in.equipped = EquippedKind::TwoHand;
	in.slotWorn[kHipL] = false;
	in.slotWorn[kBackR] = true;
	in.equippedSlot = HolsterSlot::BackRight;
	v = StepHolster(y, in, kSettings);
	Check(v.readyClick, "shown sheathed: the draw");
	v = StepHolster(y, in, kSettings);
	Check(!v.readyClick, "once");
	// Equipped, never shown: given up, the place said.
	in = Rest();
	in.slotItem[kBackR] = SlotItem::TwoHand;
	HolsterState z;
	v = Press(z, in, false, HolsterSlot::BackRight);
	Check(v.equip == HolsterSlot::BackRight, "nothing equipped, a claymore kept: equipped");
	bool gaveUp = false;
	HolsterSlot gaveUpAt = HolsterSlot::None;
	for (int i = 0; i < 200 && !gaveUp; ++i) {
		v = StepHolster(z, in, kSettings);
		gaveUp = v.gaveUp;
		gaveUpAt = v.gaveUpAt;
	}
	Check(gaveUp && gaveUpAt == HolsterSlot::BackRight, "never shown (sold since?): given up, at that place");

	// An empty place: the equipped weapon kept nowhere is drawn from there
	// and kept there; kept elsewhere, nothing.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	HolsterState a;
	v = Press(a, in, false, HolsterSlot::HipRight);
	Check(v.readyClick && v.outcome == SlotOutcome::Draw && v.assign == HolsterSlot::HipRight,
	      "a sword equipped in the inventory, kept nowhere: drawn from the hip reached, kept there");
	HolsterState b;
	v = Press(b, in, false, HolsterSlot::BackLeft);
	Check(v.readyClick && v.outcome == SlotOutcome::Draw && v.assign == HolsterSlot::BackLeft,
	      "the same at a shoulder: drawn from there, kept there (2026-11-26)");
	in.equippedSlot = HolsterSlot::HipLeft;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	HolsterState c;
	v = Press(c, in, false, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::Empty && !v.readyClick, "kept at the other hip: this one is empty");

	// The other hand's things, reached for with the weapon hand: a hint.
	in = Rest();
	in.slotItem[kBackL] = SlotItem::Bow;
	in.slotItem[kHipR] = SlotItem::Shield;
	HolsterState d;
	v = Press(d, in, false, HolsterSlot::BackLeft);
	Check(v.outcome == SlotOutcome::EquipDraw && v.equip == HolsterSlot::BackLeft && v.switchSides && v.rightClaimed,
	      "the bow kept there, nothing drawn: equipped and drawn with the weapon hand, the sides switch (5.16)");
	HolsterState e;
	v = Press(e, in, false, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::WrongHand && v.offHandTake == HolsterSlot::None, "a shield kept there: the same");

	// The bow drawn: the weapon hand's reach is the quiver's.
	in = Rest();
	in.equipped = EquippedKind::Bow;
	in.seen = WeaponSeen::Drawn;
	in.slotItem[kHipR] = SlotItem::OneHand;
	HolsterState f;
	v = Press(f, in, false, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::Quiver && !v.rightClaimed && v.equip == HolsterSlot::None,
	      "the bow drawn, a sword kept at the hip: the arrow's reach, the grip not claimed");

	// A raised fist (nothing equipped, drawn): not in the way.
	in = Rest();
	in.seen = WeaponSeen::Drawn;
	in.slotItem[kHipL] = SlotItem::OneHand;
	HolsterState g;
	v = Press(g, in, false, HolsterSlot::HipLeft);
	Check(v.equip == HolsterSlot::HipLeft, "fists up: the sword kept there is equipped");
	// A grip already closed when the hand arrives: no reach.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.equippedSlot = HolsterSlot::HipLeft;
	in.rightGrip = true;
	HolsterState h;
	StepHolster(h, in, kSettings);
	in.rightRelative = PlaceAt(HolsterSlot::HipLeft);
	v = StepHolster(h, in, kSettings);
	Check(!v.readyClick && !v.rightClaimed && v.rightInZone && v.rightSlot == HolsterSlot::HipLeft,
	      "a grip closed on the way: no draw, still a grab, the place known");
}

void TestOtherHand() {
	std::printf("The other hand at a place\n");
	// The bow kept behind the left shoulder, equipped and sheathed: drawn.
	HolsterInput in = Rest();
	in.equipped = EquippedKind::Bow;
	in.equippedSlot = HolsterSlot::BackLeft;
	in.slotItem[kBackL] = SlotItem::Bow;
	in.slotWorn[kBackL] = true;
	HolsterState s;
	HolsterVerdict v = Press(s, in, true, HolsterSlot::BackLeft);
	Check(v.readyClick && v.outcome == SlotOutcome::Draw && v.leftClaimed && v.gestureLeft, "the bow kept there: drawn");
	// Drawn, at the right shoulder: sheathed there, kept there.
	in.seen = WeaponSeen::Drawn;
	HolsterState t;
	v = Press(t, in, true, HolsterSlot::BackRight);
	Check(v.readyClick && v.outcome == SlotOutcome::Sheathe && v.assign == HolsterSlot::BackRight,
	      "the bow drawn, at the other shoulder: sheathed there");
	HolsterState u;
	v = Press(u, in, true, HolsterSlot::HipLeft);
	Check(!v.readyClick && v.outcome == SlotOutcome::DoesNotFit, "the bow at a hip: does not fit");
	in.slotItem[kBackR] = SlotItem::TwoHand;
	HolsterState w;
	v = Press(w, in, true, HolsterSlot::BackRight);
	Check(!v.readyClick && v.outcome == SlotOutcome::Full, "the bow at a shoulder with a claymore kept: taken");

	// A bow kept, a sword drawn: it goes back first.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.seen = WeaponSeen::Drawn;
	in.slotItem[kBackL] = SlotItem::Bow;
	HolsterState x;
	v = Press(x, in, true, HolsterSlot::BackLeft);
	Check(v.outcome == SlotOutcome::OtherDrawn && v.equip == HolsterSlot::None, "a sword drawn: the bow waits");
	in.seen = WeaponSeen::Sheathed;
	HolsterState y;
	v = Press(y, in, true, HolsterSlot::BackLeft);
	Check(v.equip == HolsterSlot::BackLeft && v.outcome == SlotOutcome::EquipDraw, "sheathed: the bow equipped, then drawn");
	// The bow equipped in the inventory, kept nowhere: drawn from a back place.
	in = Rest();
	in.equipped = EquippedKind::Bow;
	HolsterState z;
	v = Press(z, in, true, HolsterSlot::BackRight);
	Check(v.readyClick && v.assign == HolsterSlot::BackRight, "the bow kept nowhere: drawn from the shoulder reached, kept there");

	// The shield on the arm: stowed at any free place, not at a taken one.
	in = Rest();
	in.offHand = OffHandItem::Shield;
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		HolsterState a;
		v = Press(a, in, true, static_cast<HolsterSlot>(i));
		if (!(v.offHandStow == static_cast<HolsterSlot>(i) && v.outcome == SlotOutcome::Stow && v.leftClaimed)) {
			Check(false, "the shield stowed at every free place");
		}
	}
	Check(true, "the shield stowed at every free place");
	in.slotItem[kHipR] = SlotItem::OneHand;
	HolsterState b;
	v = Press(b, in, true, HolsterSlot::HipRight);
	Check(v.offHandStow == HolsterSlot::None && v.outcome == SlotOutcome::Full, "at a hip with a sword kept: taken");
	in.slotItem[kHipR] = SlotItem::Shield;
	in.slotWorn[kHipR] = true;
	HolsterState b2;
	v = Press(b2, in, true, HolsterSlot::HipRight);
	Check(v.offHandStow == HolsterSlot::HipRight, "at the hip keeping this very shield (worn again): its own, stowed");
	in.slotWorn[kHipR] = false;
	// The torch: the hips only.
	in = Rest();
	in.offHand = OffHandItem::Torch;
	HolsterState c;
	v = Press(c, in, true, HolsterSlot::HipRight);
	Check(v.offHandStow == HolsterSlot::HipRight, "the torch at the right hip: stowed");
	Check(v.leftClaimed && !v.leftTook, "a stow claims the grip but takes nothing into the hand");
	HolsterState d;
	v = Press(d, in, true, HolsterSlot::BackLeft);
	Check(v.offHandStow == HolsterSlot::None && v.outcome == SlotOutcome::DoesNotFit, "the torch at the back: does not fit");
	// The torch held, the bow drawn: the bow is in that hand, the torch hidden.
	in.equipped = EquippedKind::Bow;
	in.seen = WeaponSeen::Drawn;
	HolsterState e;
	v = Press(e, in, true, HolsterSlot::BackLeft);
	Check(v.offHandStow == HolsterSlot::None && v.outcome == SlotOutcome::Sheathe, "the bow drawn: its reach, not the torch's");

	// The hand empty: what is kept is taken back.
	in = Rest();
	in.slotItem[kBackR] = SlotItem::Shield;
	HolsterState f;
	v = Press(f, in, true, HolsterSlot::BackRight);
	Check(v.offHandTake == HolsterSlot::BackRight && v.outcome == SlotOutcome::Take, "a shield kept on the back: taken back");
	Check(v.leftClaimed && v.leftTook, "the hand that took it: claimed, and said to have taken it");
	in.leftGrip = true;
	in.leftRelative = NiPoint3{-0.2f, 0.3f, -0.4f};
	v = StepHolster(f, in, kSettings);
	Check(v.leftClaimed && v.leftTook, "still closed, carried away: still the take's");
	in.leftGrip = false;
	v = StepHolster(f, in, kSettings);
	Check(!v.leftClaimed && !v.leftTook, "opened: neither");
	in.equipped = EquippedKind::TwoHand;
	in.seen = WeaponSeen::Drawn;
	HolsterState g;
	v = Press(g, in, true, HolsterSlot::BackRight);
	Check(v.offHandRefused == HolsterSlot::BackRight && v.offHandTake == HolsterSlot::None &&
	          v.outcome == SlotOutcome::OtherDrawn,
	      "a two-hander drawn: the shield stays, it goes back first");
	in.equipped = EquippedKind::OneHand;
	HolsterState h;
	v = Press(h, in, true, HolsterSlot::BackRight);
	Check(v.offHandTake == HolsterSlot::BackRight, "a sword drawn: the shield taken back");
	// A torch with a two-hander drawn: kept in the other hand by OBVR
	// ([Hands] TorchWithTwoHander) - so taken back (Nadi, Windows 2026-10-27:
	// eight reaches for the torch at the hip refused with the claymore out).
	in = Rest();
	in.equipped = EquippedKind::TwoHand;
	in.seen = WeaponSeen::Drawn;
	in.slotItem[kHipL] = SlotItem::Torch;
	in.torchBesideTwoHander = true;
	HolsterState h2;
	v = Press(h2, in, true, HolsterSlot::HipLeft);
	Check(v.offHandTake == HolsterSlot::HipLeft && v.outcome == SlotOutcome::Take,
	      "a claymore drawn, the torch kept beside it: the torch taken back");
	in.torchBesideTwoHander = false;
	HolsterState h3;
	v = Press(h3, in, true, HolsterSlot::HipLeft);
	Check(v.offHandRefused == HolsterSlot::HipLeft, "a staff (or the setting off): the torch waits");
	in.torchBesideTwoHander = true;
	in.slotItem[kHipL] = SlotItem::Shield;
	HolsterState h4;
	v = Press(h4, in, true, HolsterSlot::HipLeft);
	Check(v.offHandRefused == HolsterSlot::HipLeft, "a shield with the claymore drawn: still waits");
	in.slotItem[kHipL] = SlotItem::Torch;
	in.equipped = EquippedKind::Bow;
	HolsterState h5;
	v = Press(h5, in, true, HolsterSlot::HipLeft);
	Check(v.offHandTake == HolsterSlot::None, "the bow drawn: the bow's hand, no torch");
	// A sword kept there, the other hand empty and nothing drawn: drawn with
	// this hand, the sides switch (free-weapons-spec 5.16, step 4).
	in = Rest();
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.equipped = EquippedKind::OneHand;
	in.equippedSlot = HolsterSlot::HipLeft;
	HolsterState k;
	v = Press(k, in, true, HolsterSlot::HipLeft);
	Check(v.outcome == SlotOutcome::Draw && v.readyClick && v.switchSides && v.leftClaimed,
	      "the equipped sword kept there, the other hand empty: drawn with it, the sides switch");
	in.slotWorn[kHipL] = false;
	in.equipped = EquippedKind::Nothing;
	in.equippedSlot = HolsterSlot::None;
	HolsterState k2;
	v = Press(k2, in, true, HolsterSlot::HipLeft);
	Check(v.outcome == SlotOutcome::EquipDraw && v.equip == HolsterSlot::HipLeft && v.switchSides,
	      "a sword kept there not equipped: equipped, drawn with this hand, the sides switch");
	in.slotWorn[kHipL] = true;
	in.equipped = EquippedKind::OneHand;
	in.equippedSlot = HolsterSlot::HipLeft;
	in.offHand = OffHandItem::Shield;
	HolsterState k3;
	v = Press(k3, in, true, HolsterSlot::HipLeft);
	Check(!v.switchSides && !v.readyClick, "the other hand with its shield: no draw, one hand one thing");
	in.offHand = OffHandItem::Nothing;
	in.slotItem[kHipR] = SlotItem::OneHand;
	in.seen = WeaponSeen::Drawn;
	HolsterState k4;
	v = Press(k4, in, true, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::WrongHand && !v.switchSides && !v.readyClick,
	      "a sword drawn in the weapon hand, the other hand at another sword: the wrong hand (stage 2)");
	in = Rest();
	in.slotItem[kHipL] = SlotItem::OneHand;
	HolsterState m;
	v = Press(m, in, true, HolsterSlot::HipRight);
	Check(v.outcome == SlotOutcome::Empty && !v.leftClaimed, "an empty place: nothing, the grip left to grab");
	// The bow kept at the back, the weapon hand empty and nothing drawn: the
	// bow drawn with the weapon hand, the sides switch (Nadi, 2026-11-20: the
	// bow in the right hand as well).
	in = Rest();
	in.slotItem[kBackL] = SlotItem::Bow;
	in.slotWorn[kBackL] = true;
	in.equipped = EquippedKind::Bow;
	in.equippedSlot = HolsterSlot::BackLeft;
	HolsterState b1;
	v = Press(b1, in, false, HolsterSlot::BackLeft);
	Check(v.outcome == SlotOutcome::Draw && v.readyClick && v.switchSides && v.rightClaimed,
	      "the bow kept at the back, the weapon hand empty: drawn with it, the sides switch");
	// HolsterShieldTorch off: the shield on the arm stows nothing.
	HolsterSettings off = kSettings;
	off.offHandItems = false;
	in = Rest();
	in.offHand = OffHandItem::Shield;
	HolsterState n;
	v = Press(n, in, true, HolsterSlot::HipRight, off);
	Check(v.offHandStow == HolsterSlot::None && v.outcome == SlotOutcome::Empty, "shields and torches off: nothing stowed");
	in.offHand = OffHandItem::Nothing;
	in.slotItem[kHipR] = SlotItem::Torch;
	HolsterState o;
	v = Press(o, in, true, HolsterSlot::HipRight, off);
	Check(v.offHandTake == HolsterSlot::None, "and nothing taken back");

	// Both hands in one frame: each its own place; the weapon hand's said.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.equippedSlot = HolsterSlot::HipLeft;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.offHand = OffHandItem::Shield;
	in.rightRelative = PlaceAt(HolsterSlot::HipLeft);
	in.leftRelative = PlaceAt(HolsterSlot::BackRight);
	HolsterState p;
	StepHolster(p, in, kSettings);
	in.rightGrip = true;
	in.leftGrip = true;
	v = StepHolster(p, in, kSettings);
	Check(v.readyClick && v.offHandStow == HolsterSlot::BackRight && v.gesture == HolsterSlot::HipLeft && !v.gestureLeft,
	      "the sword drawn and the shield stowed in one frame");
}

void TestWatches() {
	std::printf("The stow and the take watched\n");
	HolsterInput in = Rest();
	in.offHand = OffHandItem::Shield;
	HolsterState s;
	HolsterVerdict v = Press(s, in, true, HolsterSlot::BackLeft);
	Check(v.offHandStow == HolsterSlot::BackLeft, "stowed");
	in.slotItem[kBackL] = SlotItem::Shield;
	in.slotWorn[kBackL] = true;
	v = StepHolster(s, in, kSettings);
	Check(v.offHandDone == HolsterSlot::None, "still worn: waits");
	in.slotWorn[kBackL] = false;
	in.offHand = OffHandItem::Nothing;
	v = StepHolster(s, in, kSettings);
	Check(v.offHandDone == HolsterSlot::BackLeft && !v.offHandDoneTaking, "taken off: the stow done");
	// The take, never worn: given up.
	in.leftGrip = false;
	v = Press(s, in, true, HolsterSlot::BackLeft);
	Check(v.offHandTake == HolsterSlot::BackLeft, "taken back");
	bool gaveUp = false;
	for (int i = 0; i < 200 && !gaveUp; ++i) {
		gaveUp = StepHolster(s, in, kSettings).offHandGaveUp == HolsterSlot::BackLeft;
	}
	Check(gaveUp, "never worn again: given up");
	// The glue forgot the place: nothing to watch.
	in.leftGrip = false;
	Press(s, in, true, HolsterSlot::BackLeft);
	in.slotItem[kBackL] = SlotItem::Nothing;
	v = StepHolster(s, in, kSettings);
	Check(v.offHandDone == HolsterSlot::None && v.offHandGaveUp == HolsterSlot::None && s.offHandWatch == HolsterSlot::None,
	      "the place forgotten: the watch ends quietly");
}

void TestGates() {
	std::printf("When it does nothing\n");
	HolsterInput in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.equippedSlot = HolsterSlot::HipLeft;
	in.allowed = false;
	HolsterState s;
	HolsterVerdict v = Press(s, in, false, HolsterSlot::HipLeft);
	Check(!v.readyClick && !v.rightClaimed, "not allowed (a menu): nothing");
	HolsterSettings off = kSettings;
	off.enabled = false;
	in.allowed = true;
	HolsterState t;
	v = Press(t, in, false, HolsterSlot::HipLeft, off);
	Check(!v.readyClick && !v.rightClaimed, "the holsters off: nothing");
	in.rightValid = false;
	HolsterState u;
	v = Press(u, in, false, HolsterSlot::HipLeft);
	Check(!v.readyClick && !v.rightInZone, "the hand untracked: nothing");
	// A miss: the nearest place and how far.
	in = Rest();
	HolsterState w;
	in.rightRelative = NiPoint3{0.0f, 0.5f, -0.3f};
	StepHolster(w, in, kSettings);
	in.rightGrip = true;
	v = StepHolster(w, in, kSettings);
	Check(v.rightMissed && v.missedNearest != HolsterSlot::None && v.missedOff > 0.0f, "a grip at no place: the nearest said");
	// The claim holds while the grip stays closed and ends when it opens.
	in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.equippedSlot = HolsterSlot::HipLeft;
	HolsterState x;
	Press(x, in, false, HolsterSlot::HipLeft);
	in.rightGrip = true;
	in.rightRelative = NiPoint3{0.2f, 0.3f, -0.4f};
	v = StepHolster(x, in, kSettings);
	Check(v.rightClaimed && !v.readyClick, "the grip still closed away from the hip: still the holster's, no second draw");
	in.rightGrip = false;
	v = StepHolster(x, in, kSettings);
	Check(!v.rightClaimed, "opened: no longer");
}

void TestSign() {
	std::printf("The sign's hint before a reach\n");
	Check(SignOutcomeFor(false, HolsterSlot::BackRight, SlotItem::OneHand, SlotItem::Nothing, false) == SlotOutcome::None,
	      "a drawn sword at a free shoulder: fine");
	Check(SignOutcomeFor(false, HolsterSlot::BackLeft, SlotItem::OneHand, SlotItem::Shield, false) == SlotOutcome::Full,
	      "a drawn sword at a shoulder keeping a shield: taken");
	Check(SignOutcomeFor(false, HolsterSlot::HipLeft, SlotItem::OneHand, SlotItem::Torch, false) == SlotOutcome::Full,
	      "at a hip keeping a torch: taken");
	Check(SignOutcomeFor(false, HolsterSlot::BackRight, SlotItem::OneHand, SlotItem::TwoHand, false) ==
	          SlotOutcome::OtherDrawn &&
	          SignOutcomeFor(false, HolsterSlot::HipRight, SlotItem::OneHand, SlotItem::OneHand, false) ==
	              SlotOutcome::OtherDrawn,
	      "a sword drawn at a place keeping another weapon: put it away first, as the reach says");
	Check(SignOutcomeFor(false, HolsterSlot::HipLeft, SlotItem::OneHand, SlotItem::OneHand, true) == SlotOutcome::None,
	      "at its own place: fine");
	Check(SignOutcomeFor(false, HolsterSlot::HipRight, SlotItem::OneHand, SlotItem::Nothing, false) == SlotOutcome::None,
	      "at a free hip: fine");
	Check(SignOutcomeFor(false, HolsterSlot::BackLeft, SlotItem::Nothing, SlotItem::Bow, false) == SlotOutcome::None &&
	          SignOutcomeFor(true, HolsterSlot::HipLeft, SlotItem::Nothing, SlotItem::OneHand, false) == SlotOutcome::None,
	      "an empty hand at the other hand's weapon or bow: fine, it draws with this hand (5.16)");
	Check(SignOutcomeFor(false, HolsterSlot::HipLeft, SlotItem::Nothing, SlotItem::Shield, false) == SlotOutcome::WrongHand,
	      "the weapon hand empty at a shield kept: still the wrong hand");
	Check(SignOutcomeFor(true, HolsterSlot::BackLeft, SlotItem::Nothing, SlotItem::Bow, false) == SlotOutcome::None &&
	          SignOutcomeFor(false, HolsterSlot::HipLeft, SlotItem::Nothing, SlotItem::OneHand, false) == SlotOutcome::None &&
	          SignOutcomeFor(true, HolsterSlot::HipRight, SlotItem::Nothing, SlotItem::Nothing, false) == SlotOutcome::None,
	      "the right hand, or an empty place: fine");
	Check(SignOutcomeFor(true, HolsterSlot::None, SlotItem::Torch, SlotItem::Nothing, false) == SlotOutcome::None,
	      "at no place: nothing");
	Check(SignOutcomeFor(true, HolsterSlot::BackRight, SlotItem::Torch, SlotItem::Nothing, false) ==
	          SlotOutcome::DoesNotFit,
	      "a torch at the back: does not fit");
}

void TestWords() {
	std::printf("The words for the log and the panel\n");
	Check(SlotWords(HolsterSlot::HipRight)[0] != '\0' && SlotWords(HolsterSlot::None)[0] != '\0', "every place has words");
	bool every = true;
	for (UInt8 o = 0; o <= static_cast<UInt8>(SlotOutcome::Quiver); ++o) {
		every = every && SlotOutcomeWords(static_cast<SlotOutcome>(o))[0] != '\0';
	}
	Check(every, "every outcome has words");
	Check(SlotHint(SlotOutcome::WrongHand, SlotItem::OneHand, SlotItem::Nothing) != nullptr &&
	          SlotHint(SlotOutcome::WrongHand, SlotItem::Shield, SlotItem::Nothing) != nullptr &&
	          std::strcmp(SlotHint(SlotOutcome::WrongHand, SlotItem::OneHand, SlotItem::Nothing), "Use your sword hand") == 0 &&
	          std::strcmp(SlotHint(SlotOutcome::WrongHand, SlotItem::Torch, SlotItem::Nothing), "Use your other hand") == 0,
	      "the wrong hand: which hand it is");
	Check(std::strcmp(SlotHint(SlotOutcome::DoesNotFit, SlotItem::Nothing, SlotItem::TwoHand), "This goes on your back") == 0 &&
	          std::strcmp(SlotHint(SlotOutcome::DoesNotFit, SlotItem::Nothing, SlotItem::Torch), "This goes on your hip") == 0,
	      "the wrong place: where it goes");
	Check(SlotHint(SlotOutcome::Full, SlotItem::Bow, SlotItem::Shield) != nullptr &&
	          SlotHint(SlotOutcome::OtherDrawn, SlotItem::Bow, SlotItem::Nothing) != nullptr &&
	          SlotHint(SlotOutcome::Draw, SlotItem::OneHand, SlotItem::Nothing) == nullptr &&
	          SlotHint(SlotOutcome::DoesNotFit, SlotItem::Nothing, SlotItem::Shield) != nullptr,
	      "a full place and a weapon in the way hinted, a draw not");
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
}

}  // namespace

// Which sheathed-weapon nodes hang mirrored (vr/SheathMirror.h).
void TestSheathMirror() {
	std::printf("The sheaths for a left-hander\n");
	HolsterSettings h;
	const float quiverX = 0.15f;
	const EquippedKind kinds[] = {EquippedKind::Nothing, EquippedKind::OneHand, EquippedKind::TwoHand,
	                              EquippedKind::Bow};
	bool none = true;
	for (EquippedKind kind : kinds) {
		const SheathMirror m = SheathMirrorFor(h, quiverX, false, kind);
		none = none && !m.side && !m.back && !m.quiver;
	}
	Check(none, "right-handed: every node where the game hangs it, whatever is equipped");

	// Left-handed with every place on auto: the one-hander at the right hip
	// (the game's left mirrored), the bow behind the right shoulder (mirrored),
	// the two-hander over the left shoulder where the game hangs it (kept),
	// the quiver at the left (mirrored).
	SheathMirror m = SheathMirrorFor(h, quiverX, true, EquippedKind::OneHand);
	Check(m.side && !m.back && m.quiver, "left-handed, a one-hander: the hip's and the quiver's nodes mirrored");
	Check(SheathMirrorFor(h, quiverX, true, EquippedKind::Bow).back, "left-handed, a bow: the back's node mirrored");
	Check(!SheathMirrorFor(h, quiverX, true, EquippedKind::TwoHand).back,
	      "left-handed, a two-hander: drawn over the left shoulder, where the game hangs it - kept");
	Check(!SheathMirrorFor(h, quiverX, true, EquippedKind::Nothing).back, "nothing equipped: the back's node kept");
	Check(SheathMirrorFor(h, quiverX, true, EquippedKind::Nothing).side,
	      "the hip's node follows its place whatever is in hand - a drawn sword's scabbard stays at the hip");

	// Sides chosen in the settings win over the hand.
	HolsterSettings chosen;
	chosen.oneHandSide = HolsterSide::Left;
	chosen.twoHandSide = HolsterSide::Right;
	chosen.bowSide = HolsterSide::Left;
	m = SheathMirrorFor(chosen, quiverX, true, EquippedKind::TwoHand);
	Check(!m.side, "the one-hander chosen at the left hip: the game's node, kept");
	Check(m.back, "the two-hander chosen over the right shoulder: mirrored");
	Check(!SheathMirrorFor(chosen, quiverX, true, EquippedKind::Bow).back, "the bow chosen behind the left shoulder: kept");

	// The quiver's place set to the right-hander's left (a negative QuiverX):
	// a left-hander has it at the right, where the game's node is.
	Check(!SheathMirrorFor(h, -0.15f, true, EquippedKind::Bow).quiver, "a quiver place mirrored to the right: kept");
	Check(SheathMirrorFor(h, 0.0f, true, EquippedKind::Bow).quiver, "a quiver place on the middle counts as not right: mirrored");

	// The places (vr/Holster.h): a one-hander kept at a hip hangs at that hip.
	Check(SheathMirrorFor(h, quiverX, false, EquippedKind::OneHand, HolsterSlot::HipRight).side &&
	          !SheathMirrorFor(h, quiverX, false, EquippedKind::OneHand, HolsterSlot::HipLeft).side,
	      "right-handed, the sword kept at the right hip: mirrored there; at the left: the game's own");
	Check(!SheathMirrorFor(h, quiverX, true, EquippedKind::OneHand, HolsterSlot::HipLeft).side &&
	          SheathMirrorFor(h, quiverX, true, EquippedKind::OneHand, HolsterSlot::HipRight).side,
	      "left-handed, the same by the place, not the hand");
	Check(!SheathMirrorFor(h, quiverX, false, EquippedKind::TwoHand, HolsterSlot::HipRight).side &&
	          !SheathMirrorFor(h, quiverX, false, EquippedKind::OneHand, HolsterSlot::BackRight).side,
	      "only a one-hander at a hip moves the hip's node");
}

// The hip's sheath hidden while the equipped one-hander is kept at a shoulder (2026-11-26).
void TestHideHipSheath() {
	std::printf("The hip's sheath while the one-hander is kept at a shoulder\n");
	const HolsterSlot places[] = {HolsterSlot::None, HolsterSlot::HipLeft, HolsterSlot::HipRight, HolsterSlot::BackLeft,
	                              HolsterSlot::BackRight};
	const EquippedKind kinds[] = {EquippedKind::Nothing, EquippedKind::OneHand, EquippedKind::TwoHand, EquippedKind::Bow};
	bool onlyShoulderOneHander = true;
	for (EquippedKind kind : kinds) {
		for (HolsterSlot place : places) {
			const bool want = kind == EquippedKind::OneHand && SlotIsBack(place);
			onlyShoulderOneHander = onlyShoulderOneHander && HideHipSheath(kind, place) == want;
		}
	}
	Check(onlyShoulderOneHander, "hidden for a one-hander at a shoulder, in no other case");
	Check(HideHipSheath(EquippedKind::OneHand, HolsterSlot::BackLeft) && HideHipSheath(EquippedKind::OneHand, HolsterSlot::BackRight),
	      "either shoulder");
	Check(!HideHipSheath(EquippedKind::OneHand, HolsterSlot::None), "kept at no place: the game's sheath shows");
	HolsterSettings h;
	for (bool leftHanded : {false, true}) {
		const SheathMirror m = SheathMirrorFor(h, 0.15f, leftHanded, EquippedKind::OneHand, HolsterSlot::BackRight);
		const SheathMirror hip = SheathMirrorFor(h, 0.15f, leftHanded, EquippedKind::OneHand, HolsterSlot::HipLeft);
		const SheathMirror bow = SheathMirrorFor(h, 0.15f, leftHanded, EquippedKind::Bow, HolsterSlot::BackRight);
		Check(m.hideSide && !hip.hideSide && !bow.hideSide,
		      leftHanded ? "left-handed: the mirror carries the hide for a shoulder only"
		                 : "right-handed: the mirror carries the hide for a shoulder only");
	}
	Check(!SheathMirrorFor(h, 0.15f, false, EquippedKind::OneHand, HolsterSlot::BackRight).side,
	      "a hidden sheath is not mirrored on top (right-handed)");
}

// Nadi, 2026-10-28: "die waffen tafeln sollen nicht kommen wenn sie eh leer
// sind" - and asked, an empty place that would take the drawn weapon still
// says "Holster weapon", as B&S's prompt.
void TestEmptySign() {
	std::printf("The sign at an empty place\n");
	Check(EmptyPlaceSign(SlotItem::OneHand, SlotOutcome::None) != nullptr &&
	          std::strcmp(EmptyPlaceSign(SlotItem::OneHand, SlotOutcome::None), "Holster weapon") == 0 &&
	          std::strcmp(EmptyPlaceSign(SlotItem::TwoHand, SlotOutcome::None), "Holster weapon") == 0,
	      "a drawn weapon the place would take: \"Holster weapon\"");
	Check(std::strcmp(EmptyPlaceSign(SlotItem::Bow, SlotOutcome::None), "Holster bow") == 0 &&
	          std::strcmp(EmptyPlaceSign(SlotItem::Shield, SlotOutcome::None), "Holster shield") == 0 &&
	          std::strcmp(EmptyPlaceSign(SlotItem::Torch, SlotOutcome::None), "Holster torch") == 0,
	      "the other hand's bow, shield or torch: said what it is");
	Check(EmptyPlaceSign(SlotItem::Nothing, SlotOutcome::None) == nullptr, "nothing in the hand: nothing to say");
	Check(EmptyPlaceSign(SlotItem::OneHand, SlotOutcome::DoesNotFit) == nullptr,
	      "a weapon the place does not take: not offered (the hint says why)");
	Check(HolsterSignWanted(true, nullptr, nullptr), "a place that keeps something: shown, its name");
	Check(!HolsterSignWanted(false, nullptr, nullptr), "an empty place, nothing to put there, no hint: no sign");
	Check(HolsterSignWanted(false, "This goes on your hip", nullptr), "an empty place with a hint: shown for the hint");
	Check(HolsterSignWanted(false, nullptr, "Holster weapon"), "an empty place that would take the weapon: shown");
}

// The seen weapon's own reach (vr/SheathReach.h): the hilt on the drawn body, wherever the places are.
HolsterInput SheathedSword() {
	HolsterInput in = Rest();
	in.equipped = EquippedKind::OneHand;
	in.slotItem[kHipL] = SlotItem::OneHand;
	in.slotWorn[kHipL] = true;
	in.equippedSlot = HolsterSlot::HipLeft;
	return in;
}

// A grip closing with the hand at `at`: a frame open there, then a frame closed.
HolsterVerdict PressAt(HolsterState& s, HolsterInput in, bool leftHand, const NiPoint3& at, const SheathReach& sheath) {
	(leftHand ? in.leftRelative : in.rightRelative) = at;
	(leftHand ? in.leftGrip : in.rightGrip) = false;
	StepHolsterAndSheath(s, in, kSettings, sheath);
	(leftHand ? in.leftGrip : in.rightGrip) = true;
	return StepHolsterAndSheath(s, in, kSettings, sheath);
}

void TestSheathReach() {
	std::printf("The seen weapon is always taken at its hilt\n");
	// The body turned: the sword hangs far from every place, in front and to the right.
	SheathReach sheath;
	sheath.valid = true;
	sheath.relative = NiPoint3{0.45f, 0.30f, -0.55f};
	const NiPoint3 atHilt{0.50f, 0.30f, -0.50f};
	Check(SheathReached(sheath, true, atHilt) && !SheathReached(sheath, false, atHilt), "the hand at the hilt, tracked");
	Check(!SheathReached(sheath, true, NiPoint3{0.45f, 0.30f, -0.55f + kSheathReachMetres + 0.01f}), "a little past the reach: not");
	SheathReach none;
	Check(!SheathReached(none, true, NiPoint3{0.0f, 0.0f, 0.0f}), "no hilt read: never");

	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), false, atHilt, sheath);
		Check(v.fromBody && v.readyClick && v.outcome == SlotOutcome::Draw && v.rightClaimed &&
		          !v.rightMissed && v.gesture == HolsterSlot::HipLeft && !v.gestureLeft,
		      "at no place: the grip at the hilt draws, the grip claimed, no miss said");
	}
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), false, NiPoint3{0.9f, 0.9f, 0.0f}, sheath);
		Check(!v.fromBody && !v.readyClick && v.rightMissed, "away from the hilt and from every place: a miss, as before");
	}
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), false, atHilt, none);
		Check(!v.fromBody && !v.readyClick, "the body not drawn (no hilt read): nothing");
	}
	{
		HolsterState s;
		HolsterInput in = SheathedSword();
		in.seen = WeaponSeen::Drawn;
		const HolsterVerdict v = PressAt(s, in, false, atHilt, sheath);
		Check(!v.fromBody && !v.readyClick, "the weapon drawn already: the hilt is not on the body");
	}
	{
		HolsterState s;
		HolsterInput in = SheathedSword();
		in.allowed = false;
		const HolsterVerdict v = PressAt(s, in, false, atHilt, sheath);
		Check(!v.fromBody && !v.readyClick, "a menu up: nothing");
	}
	{
		HolsterState s;
		HolsterSettings off;
		off.enabled = false;
		HolsterInput in = SheathedSword();
		in.rightRelative = atHilt;
		StepHolsterAndSheath(s, in, off, sheath);
		in.rightGrip = true;
		const HolsterVerdict v = StepHolsterAndSheath(s, in, off, sheath);
		Check(!v.fromBody && !v.readyClick, "the holsters switched off: nothing");
	}
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), true, atHilt, sheath);
		Check(!v.fromBody && !v.readyClick, "the other hand at a sword's hilt: not its weapon");
	}
	{
		HolsterState s;
		HolsterInput in = Rest();
		in.equipped = EquippedKind::Nothing;
		const HolsterVerdict v = PressAt(s, in, false, atHilt, sheath);
		Check(!v.fromBody && !v.readyClick, "nothing equipped: nothing to take");
	}
	std::printf("The hilt inside a place\n");
	{
		// The hilt hangs in the empty right hip's circle (the sword is kept at the left): Empty yields.
		SheathReach atRightHip;
		atRightHip.valid = true;
		atRightHip.relative = PlaceAt(HolsterSlot::HipRight);
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), false, PlaceAt(HolsterSlot::HipRight), atRightHip);
		Check(v.fromBody && v.readyClick && v.outcome == SlotOutcome::Draw && v.rightClaimed,
		      "an empty place around the hilt: the seen weapon is drawn");
	}
	{
		// The right hip keeps a shield (the other hand's): WrongHand yields to the hilt.
		SheathReach atRightHip;
		atRightHip.valid = true;
		atRightHip.relative = PlaceAt(HolsterSlot::HipRight);
		HolsterInput in = SheathedSword();
		in.slotItem[kHipR] = SlotItem::Shield;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, false, PlaceAt(HolsterSlot::HipRight), atRightHip);
		Check(v.fromBody && v.readyClick && v.outcome == SlotOutcome::Draw,
		      "a place keeping the other hand's shield around the hilt: the seen weapon is drawn");
	}
	{
		// The place keeps the equipped sword itself: its own draw, not the hilt's.
		SheathReach atLeftHip;
		atLeftHip.valid = true;
		atLeftHip.relative = PlaceAt(HolsterSlot::HipLeft);
		HolsterState s;
		const HolsterVerdict v = PressAt(s, SheathedSword(), false, PlaceAt(HolsterSlot::HipLeft), atLeftHip);
		Check(!v.fromBody && v.readyClick && v.outcome == SlotOutcome::Draw && v.gesture == HolsterSlot::HipLeft,
		      "the place that keeps it: drawn by the place, once");
	}
	{
		// The right hip keeps another sword (not worn): the place equips and draws that one.
		SheathReach atRightHip;
		atRightHip.valid = true;
		atRightHip.relative = PlaceAt(HolsterSlot::HipRight);
		HolsterInput in = SheathedSword();
		in.slotItem[kHipR] = SlotItem::OneHand;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, false, PlaceAt(HolsterSlot::HipRight), atRightHip);
		Check(!v.fromBody && v.outcome == SlotOutcome::EquipDraw && v.equip == HolsterSlot::HipRight && !v.readyClick,
		      "a place keeping another weapon: that one is equipped and drawn, the place's word stands");
	}
	std::printf("The bow on the back\n");
	{
		SheathReach back;
		back.valid = true;
		back.relative = NiPoint3{-0.40f, -0.30f, 0.05f};
		HolsterInput in = Rest();
		in.equipped = EquippedKind::Bow;
		in.slotItem[kBackL] = SlotItem::Bow;
		in.slotWorn[kBackL] = true;
		in.equippedSlot = HolsterSlot::BackLeft;
		HolsterState s;
		HolsterVerdict v = PressAt(s, in, true, back.relative, back);
		Check(v.fromBody && v.readyClick && v.leftClaimed && v.leftTook && v.gestureLeft &&
		          v.outcome == SlotOutcome::Draw && !v.leftMissed,
		      "the other hand at the seen bow: drawn, the grip claimed and taken");
		HolsterState s2;
		v = PressAt(s2, in, false, back.relative, back);
		Check(!v.fromBody && !v.readyClick, "the weapon hand at the bow: not its weapon");
	}
	{
		// The grip held: no second draw the frame after.
		HolsterState s;
		HolsterInput in = SheathedSword();
		in.rightRelative = atHilt;
		StepHolsterAndSheath(s, in, kSettings, sheath);
		in.rightGrip = true;
		StepHolsterAndSheath(s, in, kSettings, sheath);
		const HolsterVerdict v = StepHolsterAndSheath(s, in, kSettings, sheath);
		Check(!v.fromBody && !v.readyClick && v.rightClaimed, "the grip held on: one draw, the grip still the holster's");
	}
}

// A weapon kept at a hip and hung there by OBVR (Model B at the hips): taken at its hilt.
void TestKeptHilt() {
	std::printf("A weapon kept at a hip is taken at its hilt\n");
	// The body turned: the left hip's copy hangs away from every place.
	SheathReach seen;
	seen.keptValid[0] = true;
	seen.kept[0] = NiPoint3{-0.45f, 0.30f, -0.55f};
	const NiPoint3 atKept{-0.42f, 0.30f, -0.52f};
	HolsterInput keeps = Rest();
	keeps.slotItem[kHipL] = SlotItem::OneHand;  // kept, not worn
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, keeps, false, atKept, seen);
		Check(v.fromBody && v.outcome == SlotOutcome::EquipDraw && v.equip == HolsterSlot::HipLeft && !v.readyClick &&
		          v.rightClaimed && !v.rightMissed && v.gesture == HolsterSlot::HipLeft,
		      "nothing equipped, the grip at the kept sword's hilt: equipped there, to be drawn");
		Check(s.drawAfterEquip == HolsterSlot::HipLeft && s.drawWait > 0.0f, "the draw waits for the equip, as at the place");
	}
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, keeps, false, NiPoint3{0.9f, 0.9f, 0.0f}, seen);
		Check(!v.fromBody && v.equip == HolsterSlot::None && v.rightMissed, "away from it and from every place: a miss");
	}
	{
		HolsterState s;
		HolsterInput in = keeps;
		in.equipped = EquippedKind::OneHand;
		in.slotItem[kHipR] = SlotItem::OneHand;
		in.slotWorn[kHipR] = true;
		in.equippedSlot = HolsterSlot::HipRight;
		in.seen = WeaponSeen::Drawn;
		const HolsterVerdict v = PressAt(s, in, false, atKept, seen);
		Check(!v.fromBody && v.equip == HolsterSlot::None, "another sword drawn: it goes back first, nothing taken");
	}
	{
		HolsterState s;
		HolsterInput in = keeps;
		in.equipped = EquippedKind::OneHand;
		in.slotItem[kHipR] = SlotItem::OneHand;
		in.slotWorn[kHipR] = true;
		in.equippedSlot = HolsterSlot::HipRight;
		const HolsterVerdict v = PressAt(s, in, false, atKept, seen);
		Check(v.fromBody && v.outcome == SlotOutcome::EquipDraw && v.equip == HolsterSlot::HipLeft,
		      "the other sword sheathed at the right hip: the kept one equipped and drawn");
	}
	{
		HolsterState s;
		HolsterInput in = keeps;
		in.slotItem[kHipL] = SlotItem::Nothing;
		const HolsterVerdict v = PressAt(s, in, false, atKept, seen);
		Check(!v.fromBody && v.equip == HolsterSlot::None, "the place no longer keeps it: nothing");
	}
	{
		HolsterState s;
		const HolsterVerdict v = PressAt(s, keeps, true, atKept, seen);
		Check(!v.fromBody && v.equip == HolsterSlot::None, "the other hand at the hilt: not its weapon");
	}
	{
		SheathReach two = seen;
		two.keptValid[1] = true;
		two.kept[1] = NiPoint3{-0.35f, 0.30f, -0.55f};
		HolsterInput in = keeps;
		in.slotItem[kHipR] = SlotItem::OneHand;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, false, NiPoint3{-0.36f, 0.30f, -0.55f}, two);
		Check(v.fromBody && v.equip == HolsterSlot::HipRight, "two hilts in reach: the nearer one");
	}
	{
		// The hand inside the right hip's place, which keeps another sword: the place's word stands.
		SheathReach atRight;
		atRight.keptValid[0] = true;
		atRight.kept[0] = PlaceAt(HolsterSlot::HipRight);
		HolsterInput in = keeps;
		in.slotItem[kHipR] = SlotItem::OneHand;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, false, PlaceAt(HolsterSlot::HipRight), atRight);
		Check(!v.fromBody && v.equip == HolsterSlot::HipRight, "a place keeping a weapon around the hilt: the place's own");
	}
	{
		HolsterState s;
		HolsterInput in = keeps;
		in.allowed = false;
		const HolsterVerdict v = PressAt(s, in, false, atKept, seen);
		Check(!v.fromBody && v.equip == HolsterSlot::None, "a menu up: nothing");
	}
	std::printf("A shield or torch kept at a hip is taken back where it hangs\n");
	{
		HolsterInput in = Rest();
		in.slotItem[kHipL] = SlotItem::Shield;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, true, atKept, seen);
		Check(v.fromBody && v.outcome == SlotOutcome::Take && v.offHandTake == HolsterSlot::HipLeft && v.leftTook &&
		          v.leftClaimed && !v.leftMissed && v.gestureLeft,
		      "the other hand empty at the kept shield: taken back, the grip claimed and taken");
		Check(s.offHandWatch == HolsterSlot::HipLeft && s.offHandTaking, "watched until the game shows it worn");
		HolsterState s2;
		const HolsterVerdict w = PressAt(s2, in, false, atKept, seen);
		Check(!w.fromBody && w.offHandTake == HolsterSlot::None && w.equip == HolsterSlot::None,
		      "the weapon hand at the shield: not its item");
	}
	{
		HolsterInput in = Rest();
		in.slotItem[kHipL] = SlotItem::Torch;
		in.offHand = OffHandItem::Shield;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, true, atKept, seen);
		Check(!v.fromBody && v.offHandTake == HolsterSlot::None, "the other hand carrying a shield: the torch stays");
	}
	{
		HolsterInput in = Rest();
		in.slotItem[kHipL] = SlotItem::Shield;
		in.equipped = EquippedKind::TwoHand;
		in.seen = WeaponSeen::Drawn;
		HolsterState s;
		const HolsterVerdict v = PressAt(s, in, true, atKept, seen);
		Check(!v.fromBody && v.offHandTake == HolsterSlot::None, "a two-hander drawn: the shield waits");
		in.slotItem[kHipL] = SlotItem::Torch;
		in.torchBesideTwoHander = true;
		HolsterState s2;
		const HolsterVerdict w = PressAt(s2, in, true, atKept, seen);
		Check(w.fromBody && w.offHandTake == HolsterSlot::HipLeft, "the torch beside the two-hander: taken");
	}
	{
		HolsterInput in = Rest();
		in.slotItem[kHipL] = SlotItem::Shield;
		HolsterSettings noItems = kSettings;
		noItems.offHandItems = false;
		HolsterState s;
		in.leftRelative = atKept;
		StepHolsterAndSheath(s, in, noItems, seen);
		in.leftGrip = true;
		const HolsterVerdict v = StepHolsterAndSheath(s, in, noItems, seen);
		Check(!v.fromBody && v.offHandTake == HolsterSlot::None, "shields and torches at the places off: nothing");
	}
	{
		BodyFrame body;
		body.sheathValid = true;
		body.keptValid[1] = true;
		body.kept[1] = NiPoint3{0.2f, 0.0f, -0.6f};
		const SheathReach r = SheathReachOf(body);
		Check(r.valid && !r.keptValid[0] && r.keptValid[1] && r.kept[1].z == -0.6f, "the body frame's hilts carried over");
	}
}

int main() {
	TestSheathReach();
	TestKeptHilt();
	TestEmptySign();
	TestSides();
	TestBodyFrame();
	TestPlaces();
	TestWeaponHand();
	TestOtherHand();
	TestWatches();
	TestGates();
	TestSign();
	TestWords();
	TestSheathMirror();
	TestHideHipSheath();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
