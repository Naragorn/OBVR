#include <cmath>
#include <cstdio>

#include "game/KeptOnBodyLogic.h"

using namespace obvr;
using namespace obvr::game;
using vr::HolsterSlot;
using vr::SlotItem;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

constexpr UInt32 kElven = 0x000229B7;
constexpr UInt32 kIron = 0x000229B6;

UInt32 Index(HolsterSlot slot) { return static_cast<UInt32>(slot); }

// The harness save's case: the Elven Longsword kept at the left hip, the Iron
// Longsword kept at the right hip and equipped (so the game hangs it there).
KeptOnBodyInput TwoSwords() {
	KeptOnBodyInput in;
	in.enabled = true;
	in.kept[Index(HolsterSlot::HipLeft)] = kElven;
	in.item[Index(HolsterSlot::HipLeft)] = SlotItem::OneHand;
	in.inPack[Index(HolsterSlot::HipLeft)] = true;
	in.kept[Index(HolsterSlot::HipRight)] = kIron;
	in.item[Index(HolsterSlot::HipRight)] = SlotItem::OneHand;
	in.inPack[Index(HolsterSlot::HipRight)] = true;
	in.equippedWeapon = kIron;
	in.gameSheathHip = HolsterSlot::HipRight;
	return in;
}

void TestShown() {
	std::printf("What a hip shows\n");
	const KeptOnBodyInput in = TwoSwords();
	Check(KeptShownAt(in, HolsterSlot::HipLeft) == KeptShown::Shown, "a kept one-hander, not equipped, in the pack: shown");
	Check(KeptShownAt(in, HolsterSlot::HipRight) == KeptShown::Equipped, "the equipped one: the game's own");
	const KeptOnBody out = KeptOnBodyFor(in);
	Check(out.form[0] == kElven && out.form[1] == 0, "the left hip shows the Elven sword, the right nothing of OBVR's");

	KeptOnBodyInput off = in;
	off.enabled = false;
	Check(KeptShownAt(off, HolsterSlot::HipLeft) == KeptShown::Off, "switched off: nothing");
	Check(KeptOnBodyFor(off).form[0] == 0, "switched off: no form");

	KeptOnBodyInput empty = in;
	empty.kept[Index(HolsterSlot::HipLeft)] = 0;
	Check(KeptShownAt(empty, HolsterSlot::HipLeft) == KeptShown::Empty, "an empty place: nothing");

	KeptOnBodyInput shield = in;
	shield.item[Index(HolsterSlot::HipLeft)] = SlotItem::Shield;
	Check(KeptShownAt(shield, HolsterSlot::HipLeft) == KeptShown::Shown, "a shield kept there, not worn: shown");
	Check(KeptOnBodyFor(shield).kind[0] == KeptKind::Shield, "as a shield");
	shield.worn[Index(HolsterSlot::HipLeft)] = true;
	Check(KeptShownAt(shield, HolsterSlot::HipLeft) == KeptShown::Equipped, "the shield worn: the game's own");
	KeptOnBodyInput torch = in;
	torch.item[Index(HolsterSlot::HipLeft)] = SlotItem::Torch;
	Check(KeptShownAt(torch, HolsterSlot::HipLeft) == KeptShown::Shown && KeptOnBodyFor(torch).kind[0] == KeptKind::Torch,
	      "a torch kept there, not worn: shown as a torch");
	torch.worn[Index(HolsterSlot::HipLeft)] = true;
	Check(KeptShownAt(torch, HolsterSlot::HipLeft) == KeptShown::Equipped, "the torch in the hand: nothing at the hip");
	KeptOnBodyInput torchKeptUnequippedSword = torch;
	torchKeptUnequippedSword.worn[Index(HolsterSlot::HipLeft)] = false;
	torchKeptUnequippedSword.equippedWeapon = kElven;  // the torch's id is the Elven sword's in this input
	Check(KeptShownAt(torchKeptUnequippedSword, HolsterSlot::HipLeft) == KeptShown::Shown,
	      "a torch is in use only when worn, whatever the weapon slot holds");
	KeptOnBodyInput bow = in;
	bow.item[Index(HolsterSlot::HipLeft)] = SlotItem::Bow;
	Check(KeptShownAt(bow, HolsterSlot::HipLeft) == KeptShown::NotForAHip, "a bow at a hip (it cannot be): nothing");

	KeptOnBodyInput potion = in;
	potion.kept[Index(HolsterSlot::HipLeft)] = 0;
	potion.item[Index(HolsterSlot::HipLeft)] = SlotItem::Nothing;
	potion.potion[Index(HolsterSlot::HipLeft)] = 0x00009311;
	potion.potionInPack[Index(HolsterSlot::HipLeft)] = true;
	const KeptOnBody bottle = KeptOnBodyFor(potion);
	Check(KeptShownAt(potion, HolsterSlot::HipLeft) == KeptShown::Shown && bottle.form[0] == 0x00009311 &&
	          bottle.kind[0] == KeptKind::Potion,
	      "a potion kept at a hip with no item: shown as a potion");
	potion.potionInPack[Index(HolsterSlot::HipLeft)] = false;
	Check(KeptShownAt(potion, HolsterSlot::HipLeft) == KeptShown::NotInPack, "the last of it drunk: nothing");
	KeptOnBodyInput potionUnder = in;
	potionUnder.potion[Index(HolsterSlot::HipLeft)] = 0x00009311;
	potionUnder.potionInPack[Index(HolsterSlot::HipLeft)] = true;
	Check(KeptOnBodyFor(potionUnder).form[0] == kElven && KeptOnBodyFor(potionUnder).kind[0] == KeptKind::Weapon,
	      "a potion and a weapon at one place: the weapon's");
	KeptOnBodyInput potionSheath = potion;
	potionSheath.potionInPack[Index(HolsterSlot::HipLeft)] = true;
	potionSheath.gameSheathHip = HolsterSlot::HipLeft;
	Check(KeptShownAt(potionSheath, HolsterSlot::HipLeft) == KeptShown::GameSheathThere,
	      "a potion where the game's sheath hangs: nothing");

	KeptOnBodyInput sold = in;
	sold.inPack[Index(HolsterSlot::HipLeft)] = false;
	Check(KeptShownAt(sold, HolsterSlot::HipLeft) == KeptShown::NotInPack, "no longer in the pack: nothing");

	KeptOnBodyInput unkept = in;
	unkept.kept[Index(HolsterSlot::HipRight)] = 0;  // the equipped sword kept at no place: the game's left hip
	unkept.gameSheathHip = HolsterSlot::HipLeft;
	Check(KeptShownAt(unkept, HolsterSlot::HipLeft) == KeptShown::GameSheathThere,
	      "the game's sheath at that hip (the equipped one kept nowhere): nothing");

	KeptOnBodyInput fists = in;
	fists.equippedWeapon = 0;
	fists.gameSheathHip = HolsterSlot::None;
	const KeptOnBody both = KeptOnBodyFor(fists);
	Check(both.form[0] == kElven && both.form[1] == kIron, "nothing equipped: both hips show what they keep");

	KeptOnBodyInput back = in;
	back.kept[Index(HolsterSlot::BackRight)] = 0x00012345;
	back.item[Index(HolsterSlot::BackRight)] = SlotItem::TwoHand;
	back.inPack[Index(HolsterSlot::BackRight)] = true;
	Check(KeptShownAt(back, HolsterSlot::BackRight) == KeptShown::Empty, "a shoulder place: not shown (the hips only)");
}

void TestGameSheathHip() {
	std::printf("Where the game's sheath hangs\n");
	Check(GameSheathHip(vr::EquippedKind::OneHand, false) == HolsterSlot::HipLeft, "a one-hander: the left hip");
	Check(GameSheathHip(vr::EquippedKind::OneHand, true) == HolsterSlot::HipRight, "a one-hander mirrored: the right hip");
	Check(GameSheathHip(vr::EquippedKind::TwoHand, false) == HolsterSlot::None, "a two-hander: on the back");
	Check(GameSheathHip(vr::EquippedKind::Bow, true) == HolsterSlot::None, "a bow: on the back");
	Check(GameSheathHip(vr::EquippedKind::Nothing, false) == HolsterSlot::None, "nothing equipped: no sheath");
}

void TestNames() {
	std::printf("The log's words\n");
	const KeptShown all[] = {KeptShown::Shown,     KeptShown::Off,       KeptShown::Empty,          KeptShown::NotForAHip,
	                         KeptShown::Equipped, KeptShown::NotInPack, KeptShown::GameSheathThere};
	bool named = true;
	for (const KeptShown k : all) {
		named = named && KeptShownName(k)[0] != '?';
	}
	Check(named, "every verdict has its words");
}

// The game's sheath node at the left hip: 14.6 units to the left along the
// pelvis's local z (BodyIkLogic.h), turned a little.
NiTransform GameSheath() {
	NiTransform t;
	t.rot = NiMatrix33::Identity();
	t.rot.data[0][0] = 0.8f;
	t.rot.data[0][1] = -0.6f;
	t.rot.data[1][0] = 0.6f;
	t.rot.data[1][1] = 0.8f;
	t.pos = NiPoint3{2.0f, -3.0f, 14.6f};
	t.scale = 1.0f;
	return t;
}

bool Same(const NiTransform& a, const NiTransform& b) {
	bool same = Near(a.pos.x, b.pos.x) && Near(a.pos.y, b.pos.y) && Near(a.pos.z, b.pos.z) && Near(a.scale, b.scale);
	for (UInt32 r = 0; r < 3; ++r) {
		for (UInt32 c = 0; c < 3; ++c) {
			same = same && Near(a.rot.data[r][c], b.rot.data[r][c]);
		}
	}
	return same;
}

void TestHipLocal() {
	std::printf("Where a hip's weapon hangs\n");
	const NiTransform game = GameSheath();
	const NiTransform mirrored = MirroredAcrossLocalZ(game);
	Check(Same(KeptHipLocal(game, -14.6f, HolsterSlot::HipLeft), game), "read at the left: the left hip is the node's own");
	Check(Same(KeptHipLocal(game, -14.6f, HolsterSlot::HipRight), mirrored), "read at the left: the right hip its mirror");
	Check(Same(KeptHipLocal(mirrored, 14.6f, HolsterSlot::HipLeft), game),
	      "read mirrored (the body moved it right): the left hip turned back to the game's");
	Check(Same(KeptHipLocal(mirrored, 14.6f, HolsterSlot::HipRight), mirrored), "read mirrored: the right hip as read");
	Check(Same(KeptHipLocal(game, 0.0f, HolsterSlot::HipLeft), game), "on the middle: taken as the game's");
	Check(Same(KeptHipLocalFor(KeptKind::Weapon, game, -14.6f, HolsterSlot::HipRight), mirrored) &&
	          Same(KeptHipLocalFor(KeptKind::Torch, game, -14.6f, HolsterSlot::HipLeft), game),
	      "a weapon and a torch: as the game's sheathed weapon");
}

NiPoint3 Column(const NiTransform& t, UInt32 c) { return NiPoint3{t.rot.data[0][c], t.rot.data[1][c], t.rot.data[2][c]}; }
float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Det(const NiMatrix33& m) {
	return m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
	       m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
	       m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
}

void TestPoses() {
	std::printf("How a shield and a potion hang\n");
	const NiTransform game = GameSheath();
	const NiPoint3 down = Column(game, 1);  // the sword's way down, here square to the body's left-right already
	const NiTransform shield = KeptHipLocalFor(KeptKind::Shield, game, -14.6f, HolsterSlot::HipLeft);
	Check(Near(Det(shield.rot), 1.0f), "the shield's turn is a turn (no mirror image)");
	Check(Near(Column(shield, 1).z, -1.0f), "its face (-y) to the body's left, outward at the left hip");
	Check(Near(Dot(Column(shield, 0), down), 1.0f), "its x along the sword's way down");
	Check(Near(shield.pos.z, 14.6f + kKeptShieldOutUnits) && Near(shield.pos.x, 2.0f), "at the sheath's spot, a little out");
	const NiTransform shieldRight = KeptHipLocalFor(KeptKind::Shield, game, -14.6f, HolsterSlot::HipRight);
	Check(Near(Det(shieldRight.rot), 1.0f) && Near(Column(shieldRight, 1).z, 1.0f) &&
	          Near(shieldRight.pos.z, -(14.6f + kKeptShieldOutUnits)),
	      "the right hip: mirrored, its face to the body's right");
	const NiTransform potion = KeptHipLocalFor(KeptKind::Potion, game, -14.6f, HolsterSlot::HipLeft);
	Check(Near(Det(potion.rot), 1.0f), "the potion's turn is a turn");
	Check(Near(Dot(Column(potion, 2), down), -1.0f), "it stands up, against the sword's way down");
	Check(Near(potion.pos.z, 14.6f + kKeptPotionOutUnits), "beside the sheath's spot");
	// The world's up in the pelvis's axes: along its x, the sword's way down slanting back from it.
	const NiPoint3 up{1.0f, 0.0f, 0.0f};
	const NiTransform upright = KeptHipLocalFor(KeptKind::Potion, game, -14.6f, HolsterSlot::HipLeft, up);
	Check(Near(Column(upright, 2).x, 1.0f) && Near(Det(upright.rot), 1.0f), "given the world's up: the potion stands along it");
	const NiTransform uprightRight = KeptHipLocalFor(KeptKind::Potion, game, -14.6f, HolsterSlot::HipRight, up);
	Check(Near(Column(uprightRight, 2).x, 1.0f) && Near(Det(uprightRight.rot), 1.0f) &&
	          Near(uprightRight.pos.z, -(14.6f + kKeptPotionOutUnits)) && Near(Column(uprightRight, 0).z, -1.0f),
	      "at the right hip too: up, not upside down (a mirror would turn it over), beside the right sheath");
	const NiPoint3 tiltedUp{0.8f, 0.0f, 0.6f};  // a lean to the side is taken out
	Check(Near(Column(KeptHipLocalFor(KeptKind::Potion, game, -14.6f, HolsterSlot::HipLeft, tiltedUp), 2).z, 0.0f),
	      "the world's up leaning sideways: the potion still square to the body's left-right");
	const NiTransform fromMirrored = KeptHipLocalFor(KeptKind::Shield, MirroredAcrossLocalZ(game), 14.6f, HolsterSlot::HipLeft);
	Check(Same(fromMirrored, shield), "read mirrored by the body: the same pose");
}

}  // namespace

int main() {
	TestShown();
	TestGameSheathHip();
	TestNames();
	TestHipLocal();
	TestPoses();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
