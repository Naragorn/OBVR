#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The controls card (assets/menus/generic/OBVR_Controls.xml): the Full VR
// layout on two pages, by role - the weapon hand and the other hand - so it
// reads right for a left-hander too (the tester, 2026-10-15: "das controller
// schema muss irgendwo dokumentiert sein. am besten im obvr setting menü und
// in docs und readme"). Opened from the settings menu's Help row "Show the
// controls", by the first start, or by a hand script's "action controls-card" (the
// harness's door). The buttons report these IDs; the hidden 9600 is the
// closing ID OBVR clicks after acting on a choice. The same words stand in
// README.md ("Controls in Full VR"); change both. Pure, covered by
// native_settings_test.
constexpr int kControlsCardClose = 9600;
constexpr int kControlsCardPrimary = 9601;    // Next, or Back on the second page
constexpr int kControlsCardSecondary = 9602;  // Close

constexpr UInt32 kControlsCardLines = 12;

enum class ControlsCardPage { WeaponHand, OtherHand };

struct ControlsCardText {
	const char* title = "";
	const char* lines[kControlsCardLines] = {};
	const char* primary = "";
	const char* secondary = "";
};

inline ControlsCardText ControlsCardFor(ControlsCardPage page, bool leftHanded) {
	ControlsCardText t;
	t.secondary = "Close";
	if (page == ControlsCardPage::WeaponHand) {
		t.title = leftHanded ? "Controls: the weapon hand (left)" : "Controls: the weapon hand (right)";
		t.primary = "Next";
		const char* const lines[kControlsCardLines] = {
			"Grip at the hip on your other side: draw or sheathe a one-hander.",
			"Grip over this shoulder: draw a two-hander or a staff.",
			"Swing: the blade hits where it touches. Long and fast: a power attack.",
			"Trigger: draw the bow while held, loose on release.",
			"Grip on an item: hold it. Let go mid-swing to throw it.",
			"A: activate what the laser points at.",
			"B: the pause menu (Escape).",
			"Stick: turn. Held up: aim a teleport, let go to go. Flick up: jump.",
			"Stick flicked down: sneak. Crouching sneaks too.",
			"Stick button short: rest menu. Held: the hotkey ring, move, let go.",
			"Index: the trackpad held opens the ring too. Trigger: next page.",
			"Make a fist, or squeeze the grip hard: raise your fists.",
		};
		for (UInt32 i = 0; i < kControlsCardLines; ++i) {
			t.lines[i] = lines[i];
		}
		return t;
	}
	t.title = leftHanded ? "Controls: the other hand (right)" : "Controls: the other hand (left)";
	t.primary = "Back";
	const char* const lines[kControlsCardLines] = {
		"Trigger: cast your spell.",
		"Grip: grab or hold an item. Grip behind this shoulder: draw the bow.",
		"Raise the hand with a shield on the arm: block.",
		"Both hands up, palms forward, held still: yield to end a fight.",
		"A: in the inventory, drop the item under the cursor.",
		"B: the Tab menus on this wrist; the other hand's laser clicks them.",
		"Stick: walk. Pressed in: run.",
		"Index: the trackpad's click opens the journal.",
		"Both stick buttons clicked together: OBVR's settings (or Insert).",
		"Both stick buttons held three seconds: recenter.",
		"Reach into a chest, a body or a lock to open it.",
		"A second grip on a two-hander's handle holds it with both hands.",
	};
	for (UInt32 i = 0; i < kControlsCardLines; ++i) {
		t.lines[i] = lines[i];
	}
	return t;
}

enum class ControlsCardChoice { None, Turn, Close };

inline ControlsCardChoice ChooseControlsCard(int button) {
	if (button == kControlsCardPrimary) {
		return ControlsCardChoice::Turn;
	}
	if (button == kControlsCardSecondary) {
		return ControlsCardChoice::Close;
	}
	return ControlsCardChoice::None;
}

inline ControlsCardPage ControlsCardTurned(ControlsCardPage page) {
	return page == ControlsCardPage::WeaponHand ? ControlsCardPage::OtherHand
	                                            : ControlsCardPage::WeaponHand;
}

// The trait of a line's text element, "parchment\lineN\string"; false when
// the index is past the card or the buffer too small.
inline bool ControlsCardLineTrait(UInt32 line, char* out, UInt32 capacity) {
	if (line >= kControlsCardLines || out == nullptr || capacity < 28) {
		return false;
	}
	const char* const head = "parchment\\line";
	UInt32 at = 0;
	for (const char* c = head; *c != '\0'; ++c) {
		out[at++] = *c;
	}
	if (line >= 10) {
		out[at++] = static_cast<char>('0' + line / 10);
	}
	out[at++] = static_cast<char>('0' + line % 10);
	const char* const tail = "\\string";
	for (const char* c = tail; *c != '\0'; ++c) {
		out[at++] = *c;
	}
	out[at] = '\0';
	return true;
}

}  // namespace obvr::ui
