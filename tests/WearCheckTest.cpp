// Checks the worn-gear check's rules (game/WearCheckLogic.h): the finger
// bones' names, the hands a side - one, none, doubled, short of fingers, a
// glove instead of the body's hand, a garment's own hand part, one mesh for
// both hands, a collapsed arm - the worn items over the trees, and the
// paperdoll's top and bottom in the menu's view.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "game/WearCheckLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

HandShapeFingers Hand(UInt8 right, UInt8 left) {
	HandShapeFingers h;
	h.fingers[kWearRight] = right;
	h.fingers[kWearLeft] = left;
	return h;
}

void FingerNames() {
	std::printf("finger bones:\n");
	UInt8 side = 9;
	UInt8 finger = 9;
	Check(FingerOfBone("Bip01 R Finger0", side, finger) && side == kWearRight && finger == 0, "the right thumb");
	Check(FingerOfBone("Bip01 L Finger21", side, finger) && side == kWearLeft && finger == 2,
	      "a joint of the left middle finger");
	Check(FingerOfBone("Bip01 L Finger4", side, finger) && finger == 4, "the little finger");
	Check(!FingerOfBone("Bip01 L Finger5", side, finger), "no sixth finger");
	Check(!FingerOfBone("Bip01 L Hand", side, finger), "the hand bone is no finger");
	Check(!FingerOfBone("Bip01 L Forearm", side, finger), "the forearm is no finger");
	Check(!FingerOfBone("Bip01 X Finger1", side, finger), "no side");
	Check(!FingerOfBone("Bip01 R Finger", side, finger), "no finger number");
	Check(!FingerOfBone(nullptr, side, finger), "no name");
	Check(FingerCount(kAllFingers) == 5 && FingerCount(0x0F) == 4 && FingerCount(0) == 0, "fingers counted");
}

void Hands() {
	std::printf("hands a side:\n");
	{
		const HandsTally t = TallyHands({Hand(kAllFingers, 0), Hand(0, kAllFingers)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok && JudgeHand(t.side[kWearLeft]) == HandVerdict::Ok,
		      "one hand a side, five fingers each: ok");
	}
	{
		const HandsTally t = TallyHands({Hand(kAllFingers, kAllFingers)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok && JudgeHand(t.side[kWearLeft]) == HandVerdict::Ok,
		      "one mesh carrying both hands counts once a side");
	}
	{
		// the body's own hand drawn beside the first-person hand (the suit
		// hand not hidden, 2026-10-18)
		const HandsTally t = TallyHands({Hand(kAllFingers, kAllFingers), Hand(kAllFingers, kAllFingers)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Doubled, "two hands drawn on the right: doubled");
		Check(HandWrong(JudgeHand(t.side[kWearLeft])), "doubled is wrong");
	}
	{
		const HandsTally t = TallyHands({Hand(kAllFingers, 0)});
		Check(JudgeHand(t.side[kWearLeft]) == HandVerdict::NoneDrawn, "no left hand drawn: none drawn");
		Check(!HandWrong(JudgeHand(t.side[kWearLeft])), "none drawn alone is not wrong (out of view)");
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok, "the right one still ok");
	}
	{
		const HandsTally t = TallyHands({Hand(0x0F, kAllFingers)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::MissingFingers, "four fingers: short of fingers");
		Check(HandWrong(JudgeHand(t.side[kWearRight])), "short of fingers is wrong");
	}
	{
		// a gauntlet replaces the body's hand: the engine leaves the hand
		// mesh out, the gauntlet's shape is the one hand
		const HandsTally t = TallyHands({Hand(kAllFingers, 0), Hand(0, kAllFingers), Hand(0, 0)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok && JudgeHand(t.side[kWearLeft]) == HandVerdict::Ok,
		      "gloves instead of the body's hands, and a sleeve with no finger: ok");
	}
	{
		// a garment with its own hand part drawn while the body's hand is
		// drawn too
		const HandsTally t = TallyHands({Hand(kAllFingers, kAllFingers), Hand(0, kAllFingers)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok, "the garment's hand part on the left only: right ok");
		Check(JudgeHand(t.side[kWearLeft]) == HandVerdict::Doubled, "and the left doubled");
	}
	{
		// the body's arm collapsed for the draw: its shape is drawn with no
		// finger standing (the caller leaves collapsed fingers out)
		const HandsTally t = TallyHands({Hand(kAllFingers, kAllFingers), Hand(0, 0)});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Ok && JudgeHand(t.side[kWearLeft]) == HandVerdict::Ok,
		      "a collapsed body hand beside the drawn one: ok");
	}
	{
		const HandsTally t = TallyHands({Hand(kAllFingers, 0), Hand(0x07, 0)});
		Check(t.side[kWearRight].shapes == 2 && t.side[kWearRight].fewest == 0x07,
		      "the fewest fingers among two kept");
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::Doubled, "two of them is doubled first");
	}
	{
		const HandsTally t = TallyHands({});
		Check(JudgeHand(t.side[kWearRight]) == HandVerdict::NoneDrawn &&
		          JudgeHand(t.side[kWearLeft]) == HandVerdict::NoneDrawn,
		      "nothing drawn: none drawn both sides");
	}
}

void Worn() {
	std::printf("worn items:\n");
	const UInt32 finery = 0x0003489F;
	{
		// first person: the body shows the trunk, the first-person tree the
		// sleeves; the body's own sleeves hidden
		const std::vector<WornItemTally> t = TallyWorn({
		    {finery, "UpperBody:0", true, true},
		    {finery, "UpperBody:2", true, true},
		    {finery, "UpperBody", true, true},
		    {finery, "Arms", false, false},
		    {finery, "Arms", true, true},
		});
		Check(t.size() == 1 && t[0].shapes == 4 && t[0].shown == 4 && t[0].drawn == 4,
		      "one garment over two trees: four shapes by name, all shown and drawn");
		Check(EveryShapeDrawn(t[0]), "every shape drawn");
	}
	{
		const std::vector<WornItemTally> t = TallyWorn({
		    {finery, "UpperBody:0", true, false},
		    {finery, "Arms", true, true},
		});
		Check(t.size() == 1 && t[0].drawn == 1 && !EveryShapeDrawn(t[0]), "one shape shown but not drawn");
	}
	{
		const std::vector<WornItemTally> t = TallyWorn({
		    {finery, "UpperBody:0", false, false},
		    {finery, "Arms", false, false},
		});
		Check(t.size() == 1 && t[0].shown == 0 && t[0].drawn == 0 && !EveryShapeDrawn(t[0]),
		      "the garment hidden: none shown, none drawn");
	}
	{
		const std::vector<WornItemTally> t = TallyWorn({
		    {finery, "UpperBody", true, true},
		    {0x00000C, "Hair", true, true},
		    {0, "UpperBody", true, true},
		});
		Check(t.size() == 2 && t[0].wornId == finery && t[1].wornId == 0x00000C,
		      "two items apart, the body's own shapes left out");
	}
	Check(TallyWorn({}).empty(), "nothing worn: no items");
	Check(!EveryShapeDrawn(WornItemTally{}), "an item without shapes is not every shape drawn");
}

void Figure() {
	std::printf("the paperdoll in view:\n");
	const float tanHalf = 0.2364f;  // 13.3 degrees, the menu's view (MenuModelFit.h)
	FigureInView v = JudgeFigureInView(30.0f, 300.0f, -60.0f, 300.0f, tanHalf);
	Check(v.top && v.bottom, "head and feet inside the view");
	v = JudgeFigureInView(30.0f, 168.0f, -99.0f, 168.0f, tanHalf);
	Check(v.top && !v.bottom, "the feet below the view (the cut of 2026-10-18)");
	v = JudgeFigureInView(90.0f, 168.0f, -10.0f, 168.0f, tanHalf);
	Check(!v.top && v.bottom, "the head above the view");
	v = JudgeFigureInView(30.0f, -10.0f, -60.0f, 300.0f, tanHalf);
	Check(!v.top, "behind the camera is not in view");
	v = JudgeFigureInView(30.0f, 300.0f, -60.0f, 300.0f, 0.0f);
	Check(!v.top && !v.bottom, "no view read: not in view");
}

}  // namespace

void Moments() {
	std::printf("when the check runs:\n");
	{
		WearCheckClock c;
		Check(StepWearCheckClock(c, true, true, false, true, 0.1f) == WearCheckMoment::Mark,
		      "a hand script's mark: checked as before");
		Check(StepWearCheckClock(c, true, false, true, true, 60.0f) == WearCheckMoment::None,
		      "in a hand script, no mark: nothing - not the inventory, not the clock");
		Check(c.done == 0, "a script's marks do not count against the player's checks");
	}
	{
		WearCheckClock c;
		Check(StepWearCheckClock(c, false, false, false, false, 100.0f) == WearCheckMoment::None,
		      "no world (the main menu, a load): nothing");
		Check(StepWearCheckClock(c, false, false, false, true, 0.5f) == WearCheckMoment::None,
		      "in the world, the first seconds: nothing yet");
		Check(StepWearCheckClock(c, false, false, false, true, kWearCheckFirstSeconds) == WearCheckMoment::Timed,
		      "in the world for the first wait: the first check");
		Check(StepWearCheckClock(c, false, false, false, true, 1.0f) == WearCheckMoment::None,
		      "right after: nothing");
		Check(StepWearCheckClock(c, false, false, false, true, kWearCheckEverySeconds) == WearCheckMoment::Timed,
		      "the next interval: checked again");
		Check(c.done == 2, "two checks counted");
	}
	{
		WearCheckClock c;
		Check(StepWearCheckClock(c, false, false, true, true, 0.1f) == WearCheckMoment::PlayerMenuOpened,
		      "the inventory opened: checked (the paperdoll is drawn)");
		Check(StepWearCheckClock(c, false, false, true, true, 0.1f) == WearCheckMoment::None,
		      "the inventory still open: once an opening");
		Check(StepWearCheckClock(c, false, false, false, true, 0.1f) == WearCheckMoment::None, "closed: nothing");
		Check(StepWearCheckClock(c, false, false, true, true, 0.1f) == WearCheckMoment::PlayerMenuOpened,
		      "opened again: checked again");
	}
	{
		WearCheckClock c;
		c.since = 5.0f;
		Check(StepWearCheckClock(c, false, false, false, false, 0.1f) == WearCheckMoment::None && c.since == 0.0f,
		      "out of the world: the clock starts over");
	}
	{
		WearCheckClock c;
		c.done = kWearCheckMost;
		Check(StepWearCheckClock(c, false, false, true, true, 1000.0f) == WearCheckMoment::None,
		      "the run's checks spent: quiet, the inventory too");
	}
	{
		WearCheckClock c;
		const float nan = std::nanf("");
		Check(StepWearCheckClock(c, false, false, false, true, -5.0f) == WearCheckMoment::None && c.since == 0.0f,
		      "a negative frame time: not counted");
		Check(StepWearCheckClock(c, false, false, false, true, nan) == WearCheckMoment::None && c.since == 0.0f,
		      "not a number: not counted");
	}
	Check(std::string(WearCheckMomentName(WearCheckMoment::PlayerMenuOpened)) == "menus" &&
	          std::string(WearCheckMomentName(WearCheckMoment::Timed)) == "timed",
	      "the moments' names for the log");
	Check(PlayerMenuOnTop(kMenuIdBigFour) && PlayerMenuOnTop(kMenuIdInventory) && PlayerMenuOnTop(kMenuIdStats) &&
	          PlayerMenuOnTop(kMenuIdMagic) && PlayerMenuOnTop(kMenuIdMap),
	      "the player's pages, and their stack's own entry");
	Check(!PlayerMenuOnTop(kMenuIdContainer) && !PlayerMenuOnTop(kMenuIdNone), "a container, no menu: not");
}

int main() {
	FingerNames();
	Hands();
	Worn();
	Figure();
	Moments();
	std::printf("%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures, g_failures == 1 ? "" : "s");
	return g_failures == 0 ? 0 : 1;
}
