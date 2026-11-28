// Checks the draw's sound (game/DrawSoundLogic.h): a weapon that comes to the
// hand without the engine's Equip group (equipped while the fists are up,
// taken straight into the hand) gets the engine's own draw sound from OBVR,
// once; a draw by its animation keeps the engine's; never twice - a second
// engine sound for the same draw or sheathe is kept quiet; one record per
// draw and sheathe, for the log (Nadi, 2026-11-28: "manchmal keine equip
// sounds obwohl ich die ziehen konnte").

#include <cstdio>
#include <cstring>
#include <limits>

#include "game/DrawSoundLogic.h"

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

constexpr UInt32 kAxe = 0x000229B8;
constexpr UInt32 kSword = 0x00025A6A;
constexpr float kFrame = 1.0f / 90.0f;

DrawSoundFrame Frame(UInt32 onHand, bool out, bool menuUp = false, bool staff = false) {
	DrawSoundFrame f;
	f.onHand = onHand;
	f.out = out;
	f.menuUp = menuUp;
	f.staff = staff;
	f.dt = kFrame;
	return f;
}

// Steps until the open record is said, at most two seconds; the report.
DrawSoundStep RunOut(DrawSoundState& s, const DrawSoundFrame& f, UInt32* plays = nullptr) {
	for (int i = 0; i < 200; ++i) {
		const DrawSoundStep r = StepDrawSound(s, f);
		if (plays != nullptr && r.playEquip) {
			++*plays;
		}
		if (r.report) {
			return r;
		}
	}
	return DrawSoundStep{};
}

void TestRule() {
	std::printf("Which way owes OBVR's sound\n");
	Check(!OwesOwnDrawSound(DrawWay::Animation, true, false), "drawn by its animation: the engine's key plays it");
	Check(OwesOwnDrawSound(DrawWay::EquippedWhileOut, false, false), "equipped while out: no Equip group, OBVR's");
	Check(OwesOwnDrawSound(DrawWay::TakenToHand, true, false), "taken straight into the hand: OBVR's");
	Check(!OwesOwnDrawSound(DrawWay::TakenToHand, true, true),
	      "a staff taken into the hand as the weapon comes out: the staff's own watch plays it");
	Check(OwesOwnDrawSound(DrawWay::EquippedWhileOut, false, true), "a staff equipped while out: OBVR's");
	Check(!OwesOwnDrawSound(DrawWay::Menu, false, false), "equipped in a menu: as the game, no draw sound");
}

void TestAnimationDraw() {
	std::printf("A draw by its animation (the ready click)\n");
	DrawSoundState s;
	Check(!StepDrawSound(s, Frame(0, false)).playEquip, "the first frame: nothing");
	const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true));
	Check(!r.playEquip && s.rec.open && s.rec.draw && s.rec.way == DrawWay::Animation && s.rec.flipped,
	      "out: a draw opened, by its animation, nothing played by OBVR");
	Check(EngineSoundAllowed(s, DrawSoundKind::Equip), "the engine's Equip key plays");
	Check(!EngineSoundAllowed(s, DrawSoundKind::Equip), "a second Equip for the same draw is kept quiet");
	UInt32 plays = 0;
	const DrawSoundStep said = RunOut(s, Frame(kAxe, true), &plays);
	Check(said.report && said.said.draw && said.said.form == kAxe && said.said.engine == 1 && said.said.quieted == 1 &&
	          !said.said.ownPlayed && plays == 0,
	      "said once after the record's time: the engine's played once, one kept quiet, OBVR's none");
	Check(!RunOut(s, Frame(kAxe, true)).report, "and said once");
	DrawSoundState silent;
	StepDrawSound(silent, Frame(0, false));
	StepDrawSound(silent, Frame(kAxe, true));
	const DrawSoundStep none = RunOut(silent, Frame(kAxe, true));
	Check(none.report && none.said.engine == 0 && !none.said.ownPlayed,
	      "a draw by its animation whose key never fired: said with the engine's none (the instrument)");
}

void TestEquippedWhileOut() {
	std::printf("Equipped while the fists are up (the holster's equip, then draw)\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(0, true));
	const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true));
	Check(r.playEquip && s.rec.way == DrawWay::EquippedWhileOut && s.rec.ownPlayed, "on the hand: OBVR plays it at once");
	Check(!StepDrawSound(s, Frame(kAxe, true)).playEquip, "once");
	Check(!EngineSoundAllowed(s, DrawSoundKind::Equip), "an Equip group after all: its sound kept quiet - never twice");
	const DrawSoundStep said = RunOut(s, Frame(kAxe, true));
	Check(said.said.ownPlayed && said.said.engine == 0 && said.said.quieted == 1, "said: OBVR's played, the engine's quiet");
	DrawSoundState first;
	StepDrawSound(first, Frame(0, true));
	Check(EngineSoundAllowed(first, DrawSoundKind::Equip), "the engine's Equip just before the weapon shows");
	Check(!StepDrawSound(first, Frame(kAxe, true)).playEquip && first.rec.engine == 1,
	      "taken as this draw's: OBVR plays nothing - never twice");
}

void TestSwap() {
	std::printf("Another weapon while one is out\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(kAxe, true));
	StepDrawSound(s, Frame(kAxe, true));
	const DrawSoundStep r = StepDrawSound(s, Frame(kSword, true));
	Check(r.playEquip && s.rec.form == kSword, "the sword hung on the hand in the axe's place: its sound");
	DrawSoundState open;
	StepDrawSound(open, Frame(0, true));
	StepDrawSound(open, Frame(kAxe, true));
	const DrawSoundStep swap = StepDrawSound(open, Frame(kSword, true));
	Check(swap.report && swap.said.form == kAxe && swap.playEquip, "the axe's record said as the sword comes");
}

void TestTaken() {
	std::printf("Taken straight into the hand (the grab's hang)\n");
	{
		DrawSoundState s;
		StepDrawSound(s, Frame(0, false));
		NoteTakenToHand(s);
		const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true));
		Check(r.playEquip && s.rec.way == DrawWay::TakenToHand, "the note, then the weapon out on the hand: played");
	}
	{
		DrawSoundState s;
		StepDrawSound(s, Frame(0, false));
		StepDrawSound(s, Frame(kAxe, true));
		NoteTakenToHand(s);
		Check(s.rec.way == DrawWay::TakenToHand, "the weapon seen first, the note in the same frame after: relabelled");
		Check(StepDrawSound(s, Frame(kAxe, true)).playEquip, "played the next frame");
		NoteTakenToHand(s);
		Check(!StepDrawSound(s, Frame(kAxe, true)).playEquip, "a second hang of the same take: not again");
	}
	{
		DrawSoundState s;
		StepDrawSound(s, Frame(0, false));
		NoteTakenToHand(s);
		for (int i = 0; i < 30; ++i) {
			StepDrawSound(s, Frame(0, false));
		}
		const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true));
		Check(!r.playEquip && s.rec.way == DrawWay::Animation, "a note a third of a second old: not this draw's");
	}
	{
		DrawSoundState s;
		StepDrawSound(s, Frame(0, false));
		NoteTakenToHand(s);
		const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true, false, true));
		Check(!r.playEquip, "a staff taken as it comes out: the staff's own watch plays it, not OBVR's here too");
	}
}

void TestMenu() {
	std::printf("Equipped in a menu\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(0, true, true));
	const DrawSoundStep r = StepDrawSound(s, Frame(kAxe, true, true));
	Check(!r.playEquip && s.rec.way == DrawWay::Menu, "with the fists up: no draw sound, as the game");
}

void TestSheathe() {
	std::printf("A sheathe\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(kAxe, true));
	StepDrawSound(s, Frame(kAxe, true));
	const DrawSoundStep r = StepDrawSound(s, Frame(0, false));
	Check(!r.playEquip && s.rec.open && !s.rec.draw && s.rec.form == kAxe, "the weapon put away: a sheathe opened");
	Check(EngineSoundAllowed(s, DrawSoundKind::Unequip), "the engine's Unequip plays");
	Check(!EngineSoundAllowed(s, DrawSoundKind::Unequip), "a second Unequip of the same sheathe kept quiet");
	Check(EngineSoundAllowed(s, DrawSoundKind::Equip), "an Equip during a sheathe is not this record's: let through");
	const DrawSoundStep said = RunOut(s, Frame(0, false));
	Check(said.report && !said.said.draw && said.said.engine == 1 && said.said.quieted == 1, "said: sheathed, once");
	DrawSoundState loose;
	StepDrawSound(loose, Frame(kAxe, true));
	Check(EngineSoundAllowed(loose, DrawSoundKind::Unequip), "an Unequip a frame before the flag drops");
	StepDrawSound(loose, Frame(0, false));
	Check(loose.rec.engine == 1 && !EngineSoundAllowed(loose, DrawSoundKind::Unequip),
	      "taken as the sheathe's: the next one is the second");
}

void TestNoRecord() {
	std::printf("No draw near\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(kAxe, true));
	Check(EngineSoundAllowed(s, DrawSoundKind::Equip) && EngineSoundAllowed(s, DrawSoundKind::Equip),
	      "the engine's sounds with no draw of OBVR's watch open: all let through");
	for (int i = 0; i < 40; ++i) {
		StepDrawSound(s, Frame(kAxe, true));
	}
	Check(s.looseEquips == 0, "and forgotten after a quarter second");
}

void TestNotANumber() {
	std::printf("A frame time not a number\n");
	DrawSoundState s;
	StepDrawSound(s, Frame(0, false));
	StepDrawSound(s, Frame(kAxe, true));
	DrawSoundFrame bad = Frame(kAxe, true);
	bad.dt = std::numeric_limits<float>::quiet_NaN();
	StepDrawSound(s, bad);
	Check(s.rec.age == s.rec.age, "the record's age stays a number");
	Check(RunOut(s, Frame(kAxe, true)).report, "and the record still ends");
}

void TestWords() {
	std::printf("The words\n");
	Check(std::strcmp(DrawWayWords(DrawWay::Animation), "by its animation") == 0, "animation");
	Check(std::strcmp(DrawWayWords(DrawWay::EquippedWhileOut), "equipped while out (no Equip group)") == 0, "out");
	Check(std::strcmp(DrawWayWords(DrawWay::TakenToHand), "taken straight into the hand") == 0, "taken");
	Check(std::strcmp(DrawWayWords(DrawWay::Menu), "equipped in a menu") == 0, "menu");
}

}  // namespace

int main() {
	TestRule();
	TestAnimationDraw();
	TestEquippedWhileOut();
	TestSwap();
	TestTaken();
	TestMenu();
	TestSheathe();
	TestNoRecord();
	TestNotANumber();
	TestWords();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all draw sound checks passed\n");
	return 0;
}
