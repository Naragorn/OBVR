// Checks the grip toggle (vr/GripToggle.h): with [Hands] GripToggle on, a
// grip that took what a hand keeps - a drawn weapon, the bow, a torch, a
// weapon's handle - leaves the hand's hold closed until the next grip, which
// lets go; off, the hold is the grip as before.

#include <cstdio>

#include "vr/GripToggle.h"

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

constexpr float kFrame = 1.0f / 72.0f;

GripToggleInput Hand(bool hold, bool keeps, float dt = kFrame) {
	GripToggleInput in;
	in.hold = hold;
	in.keeps = keeps;
	in.valid = true;
	in.world = true;
	in.dt = dt;
	return in;
}

// Frames of one input; true when the toggle's hold read `want` in every one.
bool Frames(GripToggleState& t, const GripToggleInput& in, int frames, bool want, bool on = true) {
	bool all = true;
	for (int i = 0; i < frames; ++i) {
		all = all && StepGripToggle(t, in, on).hold == want;
	}
	return all;
}

void TestOff() {
	std::printf("Off: the hold is the grip\n");
	GripToggleState t;
	Check(!StepGripToggle(t, Hand(false, true), false).hold, "open grip, a drawn weapon: open");
	const GripToggleOut closed = StepGripToggle(t, Hand(true, true), false);
	Check(closed.hold && !closed.takesClick && closed.event == GripToggleEvent::None, "closed: closed, the click left alone");
	Check(!StepGripToggle(t, Hand(false, true), false).hold, "opened: open at once - Blade & Sorcery's hold");
	Check(Frames(t, Hand(true, false), 3, true, false) && Frames(t, Hand(false, false), 40, false, false),
	      "a tap on nothing: closed while closed, then open - no tap time");
	Check(!t.latched && !t.swallow, "nothing kept by the toggle");
}

void TestPressKeepsAndNextLetsGo() {
	std::printf("A grip with a drawn weapon keeps it; the next grip lets go\n");
	GripToggleState t;
	Check(!StepGripToggle(t, Hand(false, true), true).hold, "the weapon drawn, the hand open: open (nothing taken yet)");
	const GripToggleOut press = StepGripToggle(t, Hand(true, true), true);
	Check(press.hold && press.event == GripToggleEvent::Kept && press.takesClick, "the grip closes: held at once, said once, no grab from it");
	Check(Frames(t, Hand(false, true), 200, true), "the grip opened, nearly 3 s: still held");
	Check(StepGripToggle(t, Hand(false, true), true).takesClick, "held by the toggle: the click grabs nothing");
	const GripToggleOut letGo = StepGripToggle(t, Hand(true, true), true);
	Check(!letGo.hold && letGo.event == GripToggleEvent::LetGo && letGo.takesClick, "the next grip: open at once, said, no grab from it");
	Check(Frames(t, Hand(true, true), 30, false), "the grip still closed after the let go: stays open (that grip is spent)");
	Check(StepGripToggle(t, Hand(true, true), true).takesClick, "and its click grabs nothing either");
	Check(Frames(t, Hand(false, true), 30, false), "the grip opened: open - the weapon still drawn keeps nothing by itself");
	Check(!StepGripToggle(t, Hand(false, true), true).takesClick, "open and free: the click is the hand's again");
	Check(StepGripToggle(t, Hand(true, true), true).event == GripToggleEvent::Kept, "a new grip takes it again");
}

void TestTapWhoseTakeShowsLater() {
	std::printf("A short grip whose take shows a few frames later\n");
	GripToggleState t;
	Check(StepGripToggle(t, Hand(true, false), true).hold, "the grip closes at a weapon's grip: closed");
	Check(StepGripToggle(t, Hand(true, false), true).hold, "a frame later: closed");
	Check(Frames(t, Hand(false, false), 3, true), "the grip opened after two frames, nothing in the hand yet: kept closed for the take");
	const GripToggleOut kept = StepGripToggle(t, Hand(false, true), true);
	Check(kept.hold && kept.event == GripToggleEvent::Kept, "the weapon shows drawn: held, never read open on the way");
	Check(Frames(t, Hand(false, true), 100, true), "and held on");
}

void TestTapOnNothing() {
	std::printf("A short grip that takes nothing\n");
	GripToggleState t;
	StepGripToggle(t, Hand(true, false), true);
	Check(Frames(t, Hand(false, false), 34, true), "the grip opened a frame later: closed for the take's time (0.47 s)");
	Frames(t, Hand(false, false), 3, false);
	Check(Frames(t, Hand(false, false), 6, false), "past half a second with nothing taken: open");
	Check(!t.latched, "nothing kept");
	GripToggleState held;
	Check(Frames(held, Hand(true, false), 60, true) && Frames(held, Hand(false, false), 1, false),
	      "a grip held on nothing for 0.8 s and opened: open at once (the take's time is past)");
}

void TestHeldGripThenKeeps() {
	std::printf("A grip held closed when the hand comes to keep something\n");
	GripToggleState t;
	Check(Frames(t, Hand(true, false), 72, true), "the grip held a second on nothing: closed");
	const GripToggleOut kept = StepGripToggle(t, Hand(true, true), true);
	Check(kept.hold && kept.event == GripToggleEvent::Kept, "a pulled weapon arrives in the closed hand: held");
	Check(Frames(t, Hand(false, true), 10, true), "the grip opened: still held");
}

void TestLoss() {
	std::printf("What the hand kept goes without a grip\n");
	GripToggleState t;
	StepGripToggle(t, Hand(true, true), true);
	Check(Frames(t, Hand(false, false), 10, true), "the weapon reads gone for 10 frames (0.14 s): still held - a reading's gap");
	Check(Frames(t, Hand(false, true), 10, true), "and back: held");
	Check(Frames(t, Hand(false, false), 14, true), "gone for 14 frames (under 0.2 s): held");
	const GripToggleOut lost = StepGripToggle(t, Hand(false, false), true);
	Check(!lost.hold && lost.event == GripToggleEvent::Lost, "the fifteenth (0.21 s): open, said - sheathed by a key, taken in a menu");
	Check(!t.latched && Frames(t, Hand(false, false), 5, false), "and open after");
	GripToggleState closed;
	StepGripToggle(closed, Hand(true, true), true);
	Check(Frames(closed, Hand(true, false), 15, true) && !closed.latched && StepGripToggle(closed, Hand(true, false), true).hold,
	      "lost with the grip still closed: the hold is the grip's again, closed");
}

void TestPausedInMenus() {
	std::printf("A menu up: the toggle waits\n");
	GripToggleState t;
	StepGripToggle(t, Hand(true, true), true);
	StepGripToggle(t, Hand(false, true), true);
	GripToggleInput menu = Hand(false, false);
	menu.paused = true;
	Check(Frames(t, menu, 200, true), "a menu up for nearly 3 s with the weapon unread: still held, no loss counted");
	Check(!StepGripToggle(t, menu, true).takesClick, "in the menu the click is the menu's");
	menu.hold = true;
	Check(Frames(t, menu, 20, true) && t.latched, "a grip in the menu: no let go");
	GripToggleInput back = Hand(true, true);
	Check(StepGripToggle(t, back, true).hold && t.latched, "the menu closed with the grip still closed: no press, still held");
	Check(StepGripToggle(t, Hand(false, true), true).hold, "opened: held");
	Check(!StepGripToggle(t, Hand(true, true), true).hold, "the next grip lets go");
	GripToggleState open;
	GripToggleInput openMenu = Hand(true, false);
	openMenu.paused = true;
	Check(StepGripToggle(open, openMenu, true).hold && !open.latched, "nothing kept, a grip in a menu: the grip as it is");
}

void TestOutOfTheWorldAndUntracked() {
	std::printf("Out of the world, untracked\n");
	GripToggleState t;
	StepGripToggle(t, Hand(true, true), true);
	GripToggleInput gone = Hand(false, true);
	gone.valid = false;
	Check(Frames(t, gone, 30, true) && t.latched, "the controller untracked for 0.4 s: still held");
	gone.hold = true;
	StepGripToggle(t, gone, true);
	Check(StepGripToggle(t, Hand(true, true), true).hold && t.latched, "tracked again with the grip closed: no press from the gap");
	GripToggleInput load = Hand(false, true);
	load.world = false;
	Check(!StepGripToggle(t, load, true).hold && !t.latched, "a load or the main menu: open, nothing kept");
	Check(!StepGripToggle(t, Hand(false, true), true).hold, "back in the world: open until a grip");
}

void TestKeepsOf() {
	std::printf("What a hand keeps by its hold\n");
	const GripKeeps sword = GripKeepsOf(EquippedKind::OneHand, WeaponSeen::Drawn, OffHandItem::Nothing, false, false);
	Check(sword.right && !sword.left, "a drawn one-hander: the weapon hand's");
	Check(GripKeepsOf(EquippedKind::TwoHand, WeaponSeen::Drawn, OffHandItem::Nothing, false, false).right,
	      "a drawn two-hander or staff: the weapon hand's");
	Check(!GripKeepsOf(EquippedKind::OneHand, WeaponSeen::Sheathed, OffHandItem::Nothing, false, false).right,
	      "a sheathed one: nobody's");
	Check(!GripKeepsOf(EquippedKind::OneHand, WeaponSeen::Unknown, OffHandItem::Nothing, false, false).right,
	      "unread (a menu): nobody's");
	Check(!GripKeepsOf(EquippedKind::Nothing, WeaponSeen::Drawn, OffHandItem::Nothing, false, false).right,
	      "the fists: nothing to keep");
	const GripKeeps bow = GripKeepsOf(EquippedKind::Bow, WeaponSeen::Drawn, OffHandItem::Nothing, false, false);
	Check(!bow.right && bow.left, "a drawn bow: the other hand's - the weapon hand draws the string by its own grip");
	Check(GripKeepsOf(EquippedKind::OneHand, WeaponSeen::Drawn, OffHandItem::Torch, false, false).left,
	      "a torch beside a one-hander: the other hand's");
	Check(GripKeepsOf(EquippedKind::Nothing, WeaponSeen::Sheathed, OffHandItem::Torch, false, false).left,
	      "a torch alone: the other hand's");
	Check(!GripKeepsOf(EquippedKind::TwoHand, WeaponSeen::Drawn, OffHandItem::Torch, false, false).left,
	      "a torch worn with a two-hander drawn: the game took it off the hand");
	Check(GripKeepsOf(EquippedKind::TwoHand, WeaponSeen::Drawn, OffHandItem::Torch, true, false).left,
	      "...unless it is kept beside the two-hander (TorchWithTwoHander)");
	Check(!GripKeepsOf(EquippedKind::Bow, WeaponSeen::Drawn, OffHandItem::Torch, true, true).right,
	      "never the weapon hand's with a bow");
	Check(!GripKeepsOf(EquippedKind::Nothing, WeaponSeen::Sheathed, OffHandItem::Shield, false, false).left,
	      "a shield: nothing - it stays on the arm whatever the grip does");
	Check(GripKeepsOf(EquippedKind::TwoHand, WeaponSeen::Drawn, OffHandItem::Nothing, false, true).left,
	      "the other hand on the weapon's handle or blade: that hand's");
}

void TestBothHands() {
	std::printf("Both hands in one step\n");
	GripToggleState right;
	GripToggleState left;
	GripToggleFrame f;
	f.rightValid = true;
	f.leftValid = true;
	f.world = true;
	f.dt = kFrame;
	f.keeps.right = true;
	bool rightHold = true;
	bool leftHold = true;
	bool rightClick = true;
	bool leftClick = true;
	GripToggleSeen seen = StepGripToggles(right, left, rightHold, leftHold, rightClick, leftClick, f, true);
	Check(rightHold && seen.right == GripToggleEvent::Kept && !rightClick, "the weapon hand closes on its drawn sword: held, its click taken");
	Check(leftHold && seen.left == GripToggleEvent::None && leftClick, "the other hand closes on nothing: closed, its click its own");
	rightHold = false;
	leftHold = false;
	rightClick = false;
	leftClick = false;
	for (int i = 0; i < 40; ++i) {
		bool r = false;
		bool l = false;
		bool rc = false;
		bool lc = false;
		StepGripToggles(right, left, r, l, rc, lc, f, true);
		rightHold = r;
		leftHold = l;
	}
	Check(rightHold && !leftHold, "both opened for 0.55 s: the sword held, the other hand open");
	bool off = false;
	bool offLeft = true;
	bool offClick = true;
	bool offLeftClick = true;
	GripToggleState a;
	GripToggleState b;
	StepGripToggles(a, b, off, offLeft, offClick, offLeftClick, f, false);
	Check(!off && offLeft && offClick && offLeftClick, "off: the holds and clicks untouched");
}

// The hand mode's frame as far as the toggle reads it.
struct FakeFrame {
	bool inWorld = true;
	bool menuMode = false;
	bool settingsMenuOpen = false;
	float dtSeconds = kFrame;
	EquippedKind equipped = EquippedKind::Nothing;
	WeaponSeen weaponSeen = WeaponSeen::Unknown;
	OffHandItem offHand = OffHandItem::Nothing;
	bool torchBesideTwoHander = false;
	bool leftGripOnHandle = false;
	bool bothHandsOnHandle = false;
};

void TestFrameOf() {
	std::printf("The hand mode's frame read for the toggle\n");
	FakeFrame f;
	f.equipped = EquippedKind::OneHand;
	f.weaponSeen = WeaponSeen::Drawn;
	GripToggleFrame g = GripToggleFrameOf(f, true, false);
	Check(g.rightValid && !g.leftValid && g.world && !g.paused && g.keeps.right && !g.keeps.left,
	      "a drawn sword in the world: the weapon hand keeps it, the hands' tracking as given");
	f.menuMode = true;
	Check(GripToggleFrameOf(f, true, true).paused, "a menu up: paused");
	f.menuMode = false;
	f.settingsMenuOpen = true;
	Check(GripToggleFrameOf(f, true, true).paused, "OBVR's own menu up: paused");
	f.settingsMenuOpen = false;
	f.inWorld = false;
	Check(!GripToggleFrameOf(f, true, true).world, "no game loaded: out of the world");
	f.inWorld = true;
	f.bothHandsOnHandle = true;
	Check(GripToggleFrameOf(f, true, true).keeps.left, "both hands on the handle: the other hand keeps it too");
	f.bothHandsOnHandle = false;
	f.leftGripOnHandle = true;
	Check(GripToggleFrameOf(f, true, true).keeps.left, "the other hand's grip on the handle: kept");
	Check(GripToggleFrameOf(f, true, true).dt == kFrame, "the frame's time");
}

}  // namespace

int main() {
	TestOff();
	TestPressKeepsAndNextLetsGo();
	TestTapWhoseTakeShowsLater();
	TestTapOnNothing();
	TestHeldGripThenKeeps();
	TestLoss();
	TestPausedInMenus();
	TestOutOfTheWorldAndUntracked();
	TestKeepsOf();
	TestBothHands();
	TestFrameOf();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
