// Checks the dialogue panel's decisions (vr/DialogPanel.h): when it is placed
// on the speaker, where, and its size while talking.

#include <cstdio>
#include <limits>

#include "vr/DialogPanel.h"

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

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

openvr::HmdMatrix34 Head() {
	openvr::HmdMatrix34 h{};
	h.m[0][0] = 1.0f;
	h.m[1][1] = 1.0f;
	h.m[2][2] = 1.0f;
	h.m[0][3] = 0.5f;
	h.m[1][3] = 1.7f;
	h.m[2][3] = 0.0f;
	return h;
}

void TestSides() {
	std::printf("The side\n");
	DialogPanelSide side = DialogPanelSide::Centre;
	Check(ParseDialogPanelSide("Right", side) && side == DialogPanelSide::Right, "\"Right\": right");
	side = DialogPanelSide::Left;
	Check(!ParseDialogPanelSide("above", side) && side == DialogPanelSide::Left, "an unknown word: refused, kept");
	Check(DialogPanelSideFromIndex(2.0f) == DialogPanelSide::Left, "row value 2: left");
	Check(DialogPanelSideFromIndex(7.0f) == DialogPanelSide::Centre, "out of range: centre");
	const DialogPanelSettings s;
	Check(s.recentre && s.side == DialogPanelSide::Centre && Near(s.scale, 0.8f),
	      "defaults: placed on the speaker, facing them, 80 %");
}

void TestDue() {
	DialogPanelPlacement placement;
	Check(!placement.Pending(false), "ordinary world is not awaiting dialogue placement");
	Check(placement.Pending(true) && placement.Pending(true), "first and delayed tracking frames remain pending");
	placement.placed = true;
	Check(!placement.Pending(true), "successful placement remains fixed through the conversation");
	Check(!placement.Pending(false) && !placement.placed && placement.Pending(true),
	      "exit resets placement so the next conversation can use a different speaker");
	std::printf("When it is placed\n");
	Check(DialogRecentreDue(true, true, true, true), "a conversation opened, menus in the room, speaker seen: placed");
	Check(!DialogRecentreDue(false, true, true, true), "the setting off: not");
	Check(!DialogRecentreDue(true, false, true, true), "already talking: not again");
	Check(!DialogRecentreDue(true, true, false, true), "menus on the head: nothing to place");
	Check(!DialogRecentreDue(true, true, true, false), "the speaker not seen: left where it was");
	bool combinations = true;
	for (int enabled = 0; enabled < 2; ++enabled)
	for (int pending = 0; pending < 2; ++pending)
	for (int room = 0; room < 2; ++room)
	for (int target = 0; target < 2; ++target)
		combinations = combinations &&
			DialogRecentreDue(enabled != 0, pending != 0, room != 0, target != 0) ==
			(enabled + pending + room + target == 4);
	Check(combinations, "all 16 placement policy combinations");
	Check(!DialogRecentreDue(true, true, true, false) && DialogRecentreDue(true, true, true, true),
	      "missing first-frame speaker retries when it becomes available");
}

void TestWidth() {
	std::printf("Its size\n");
	Check(Near(DialogPanelWidth(1.6f, true, 0.8f), 1.28f), "talking: 80 % of the menus' width");
	Check(Near(DialogPanelWidth(1.6f, false, 0.8f), 1.6f), "not talking: the menus' own");
	Check(Near(DialogPanelWidth(1.6f, true, 0.0f), 1.6f), "a scale of 0: ignored");
}

void TestAnchor() {
	std::printf("Where it is placed\n");
	const openvr::HmdMatrix34 head = Head();
	openvr::HmdMatrix34 a{};
	// The speaker 2 m straight ahead (-z) of the head.
	Check(DialogAnchor(head, 0.5f, 1.6f, -2.0f, DialogPanelSide::Centre, 25.0f, a), "a speaker ahead: placed");
	Check(Near(a.m[0][2], 0.0f) && Near(a.m[2][2], 1.0f) && Near(a.m[0][0], 1.0f),
	      "centre: facing straight at them");
	Check(Near(a.m[0][3], 0.5f) && Near(a.m[1][3], 1.7f) && Near(a.m[2][3], 0.0f), "at the head");
	Check(DialogAnchor(head, 0.5f, 1.6f, -2.0f, DialogPanelSide::Right, 30.0f, a), "right");
	Check(Near(-a.m[0][2], 0.5f) && Near(-a.m[2][2], -0.8660f), "turned 30 degrees to the right");
	Check(DialogAnchor(head, 0.5f, 1.6f, -2.0f, DialogPanelSide::Left, 30.0f, a), "left");
	Check(Near(-a.m[0][2], -0.5f), "turned 30 degrees to the left");
	// The speaker to the right (+x) of the head.
	Check(DialogAnchor(head, 3.5f, 1.6f, 0.0f, DialogPanelSide::Centre, 25.0f, a) && Near(-a.m[0][2], 1.0f) &&
	          Near(a.m[2][0], 1.0f),
	      "a speaker to the right: the heading turns to them, its x axis follows");
	openvr::HmdMatrix34 kept = head;
	Check(!DialogAnchor(head, 0.5f, 3.0f, 0.0f, DialogPanelSide::Right, 25.0f, kept) && Near(kept.m[2][2], 1.0f),
	      "straight above the head: no heading, the anchor untouched");
	Check(DialogAnchor(head, 0.5f, 0.0f, 2.0f, DialogPanelSide::Centre, 25.0f, a) && Near(a.m[2][2], -1.0f),
	      "NPC approaches from behind: panel turns to the actual speaker");
	Check(DialogAnchor(head, -2.5f, 0.0f, 0.0f, DialogPanelSide::Centre, 25.0f, a) && Near(a.m[0][2], 1.0f),
	      "NPC approaches from left: panel turns left");
	const float invalid[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
	for (float value : invalid) {
		kept = head;
		Check(!DialogAnchor(head, value, 0, -2, DialogPanelSide::Centre, 0, kept) && Near(kept.m[0][3], 0.5f),
		      "invalid tracking direction leaves anchor intact");
	}
}

}  // namespace

int main() {
	TestSides();
	TestDue();
	TestWidth();
	TestAnchor();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
