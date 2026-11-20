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
	Check(KeptShownAt(shield, HolsterSlot::HipLeft) == KeptShown::NotAOneHander, "a shield kept there: nothing yet");
	KeptOnBodyInput torch = in;
	torch.item[Index(HolsterSlot::HipLeft)] = SlotItem::Torch;
	Check(KeptShownAt(torch, HolsterSlot::HipLeft) == KeptShown::NotAOneHander, "a torch kept there: nothing yet");

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
	const KeptShown all[] = {KeptShown::Shown,     KeptShown::Off,       KeptShown::Empty,          KeptShown::NotAOneHander,
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
}

}  // namespace

int main() {
	TestShown();
	TestGameSheathHip();
	TestNames();
	TestHipLocal();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
