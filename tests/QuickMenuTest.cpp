// Checks the quick menu on the right trackpad (vr/QuickMenu.h): which slot a
// hand offset chooses, and every way the ring opens, chooses, uses, refuses
// and closes.

#include <cmath>
#include <cstdio>

#include "vr/QuickMenu.h"

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

const NiPoint3 kRight{1.0f, 0.0f, 0.0f};
const NiPoint3 kUp{0.0f, 1.0f, 0.0f};

NiPoint3 AtAngle(float degrees, float metres) {
	const float a = degrees * math::kDegreesToRadians;
	return NiPoint3{metres * std::sin(a), metres * std::cos(a), 0.0f};
}

void TestSlots() {
	std::printf("Which slot an offset chooses\n");
	Check(QuickSlotAt(NiPoint3{0.01f, 0.01f, 0.0f}, kRight, kUp, 0.04f) == -1, "inside the dead zone: none");
	Check(QuickSlotAt(AtAngle(0.0f, 0.1f), kRight, kUp, 0.04f) == 0, "up: slot 1");
	Check(QuickSlotAt(AtAngle(45.0f, 0.1f), kRight, kUp, 0.04f) == 1, "up and right: slot 2");
	Check(QuickSlotAt(AtAngle(90.0f, 0.1f), kRight, kUp, 0.04f) == 2, "right: slot 3");
	Check(QuickSlotAt(AtAngle(180.0f, 0.1f), kRight, kUp, 0.04f) == 4, "down: slot 5");
	Check(QuickSlotAt(AtAngle(270.0f, 0.1f), kRight, kUp, 0.04f) == 6, "left: slot 7");
	Check(QuickSlotAt(AtAngle(-45.0f, 0.1f), kRight, kUp, 0.04f) == 7, "up and left: slot 8");
	Check(QuickSlotAt(AtAngle(22.0f, 0.1f), kRight, kUp, 0.04f) == 0, "22 degrees still slot 1");
	Check(QuickSlotAt(AtAngle(23.0f, 0.1f), kRight, kUp, 0.04f) == 1, "23 degrees slot 2");
	Check(QuickSlotAt(AtAngle(-22.0f, 0.1f), kRight, kUp, 0.04f) == 0, "-22 degrees slot 1");
	Check(QuickSlotAt(NiPoint3{0.0f, 0.0f, -0.2f}, kRight, kUp, 0.04f) == -1,
	      "towards the ring, not across it: none");
	// A ring opened with the head turned: its right is tracking -z.
	const NiPoint3 turnedRight{0.0f, 0.0f, -1.0f};
	Check(QuickSlotAt(NiPoint3{0.0f, 0.0f, -0.1f}, turnedRight, kUp, 0.04f) == 2,
	      "the ring's own right, not tracking x");

	float u = 0.0f;
	float v = 0.0f;
	QuickSlotCentre(0, 0.1f, u, v);
	Check(Near(u, 0.0f) && Near(v, 0.1f), "slot 1 is drawn at the top");
	QuickSlotCentre(2, 0.1f, u, v);
	Check(Near(u, 0.1f) && Near(v, 0.0f), "slot 3 at the right");
	Check(QuickSlotAt(NiPoint3{u, v, 0.0f}, kRight, kUp, 0.04f) == 2, "and chosen where it is drawn");
}

void TestLevelRight() {
	std::printf("The ring stands upright\n");
	NiPoint3 r = LevelRight(NiPoint3{0.6f, 0.8f, 0.0f});
	Check(Near(r.x, 1.0f) && Near(r.y, 0.0f), "a rolled head's right laid level");
	r = LevelRight(NiPoint3{0.0f, 0.2f, -0.5f});
	Check(Near(r.z, -1.0f) && Near(r.y, 0.0f), "and made unit length");
	r = LevelRight(NiPoint3{0.0f, 1.0f, 0.0f});
	Check(Near(r.x, 1.0f), "a right pointing straight up: tracking x stands in");
}

QuickMenuInput Frame(bool pad, const NiPoint3& hand, bool allowed = true) {
	QuickMenuInput in;
	in.pad = pad;
	in.allowed = allowed;
	in.hand = hand;
	in.headRight = kRight;
	in.dt = 0.011f;
	for (int i = 0; i < kQuickSlots; ++i) {
		in.filled[i] = i != 4;  // slot 5 empty
	}
	return in;
}

void TestFlows() {
	std::printf("Opening, choosing, using\n");
	const QuickMenuSettings settings;
	const NiPoint3 hand{0.2f, 1.0f, -0.3f};

	{
		QuickMenuSettings off;
		off.enabled = false;
		QuickMenuState s;
		const QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand), off);
		Check(!v.visible && !v.opened && !s.open, "switched off: the trackpad opens nothing");
	}
	{
		QuickMenuState s;
		const QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand, false), settings);
		Check(!v.visible && !s.open, "not allowed (a menu, not in the world): nothing");
	}
	{
		QuickMenuState s;
		QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand), settings);
		Check(v.visible && v.opened && v.highlighted == -1 && v.key == 0,
		      "pressed: the ring opens where the hand is, nothing chosen");
		Check(Near(s.anchor.x, 0.2f) && Near(s.anchor.z, -0.3f), "anchored at the hand");
		v = StepQuickMenu(s, Frame(true, hand), settings);
		Check(v.visible && !v.opened, "held: shown, opened only once");
		v = StepQuickMenu(s, Frame(false, hand), settings);
		Check(!v.visible && v.cancelled && v.key == 0 && v.used == -1,
		      "let go in the middle: nothing used");
	}
	{
		QuickMenuState s;
		StepQuickMenu(s, Frame(true, hand), settings);
		QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand + NiPoint3{0.0f, 0.08f, 0.0f}), settings);
		Check(v.highlighted == 0, "the hand moved up: slot 1 lit");
		v = StepQuickMenu(s, Frame(true, hand + NiPoint3{0.08f, 0.0f, 0.0f}), settings);
		Check(v.highlighted == 2, "moved on to the right: slot 3 lit");
		v = StepQuickMenu(s, Frame(false, hand + NiPoint3{0.08f, 0.0f, 0.0f}), settings);
		Check(!v.visible && v.used == 2 && v.key == 3, "let go there: slot 3 used, key 3 down");
		v = StepQuickMenu(s, Frame(false, hand), settings);
		Check(v.key == 3 && v.used == -1, "the key stays down for the tap");
		QuickMenuVerdict press = StepQuickMenu(s, Frame(true, hand), settings);
		Check(!press.opened && press.key == 3, "a press during the tap does not open the ring");
		for (int i = 0; i < 10; ++i) {
			v = StepQuickMenu(s, Frame(false, hand), settings);
		}
		Check(v.key == 0, "then it comes up: a tap, not a hold");
		v = StepQuickMenu(s, Frame(true, hand), settings);
		Check(v.opened, "after the tap a press opens it again");
	}
	{
		QuickMenuState s;
		StepQuickMenu(s, Frame(true, hand), settings);
		QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand + NiPoint3{0.0f, -0.08f, 0.0f}), settings);
		Check(v.highlighted == 4, "down: the empty slot 5 lit");
		v = StepQuickMenu(s, Frame(false, hand + NiPoint3{0.0f, -0.08f, 0.0f}), settings);
		Check(v.cancelled && v.key == 0 && v.used == -1, "let go on an empty slot: nothing");
	}
	{
		QuickMenuState s;
		StepQuickMenu(s, Frame(true, hand), settings);
		QuickMenuVerdict v = StepQuickMenu(s, Frame(true, hand, false), settings);
		Check(v.cancelled && !v.visible && !s.open, "a menu comes up while it is open: closed");
		v = StepQuickMenu(s, Frame(true, hand), settings);
		Check(!v.visible && !v.opened, "still held when allowed again: stays closed");
		StepQuickMenu(s, Frame(false, hand), settings);
		v = StepQuickMenu(s, Frame(true, hand), settings);
		Check(v.opened, "a fresh press opens it");
	}
	{
		// The ring keeps the axes it opened with while the head turns.
		QuickMenuState s;
		QuickMenuInput in = Frame(true, hand);
		in.headRight = NiPoint3{0.0f, 0.0f, -1.0f};
		StepQuickMenu(s, in, settings);
		in.headRight = kRight;
		in.hand = hand + NiPoint3{0.0f, 0.0f, -0.08f};
		const QuickMenuVerdict v = StepQuickMenu(s, in, settings);
		Check(v.highlighted == 2, "the ring's right is the head's at opening");
	}
}

}  // namespace

int main() {
	TestSlots();
	TestLevelRight();
	TestFlows();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
