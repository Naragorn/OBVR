#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The controls card (assets/menus/generic/OBVR_Controls.xml): the Full VR
// layout by role - the weapon hand and the other hand - so it reads right for
// a left-hander too (the tester, 2026-10-15: "das controller schema muss
// irgendwo dokumentiert sein. am besten im obvr setting menü und in docs und
// readme"). Opened from the settings menu's Help row "Show the controls", by
// the first start, or by a hand script's "action controls-card" (the
// harness's door). The buttons report these IDs; the hidden 9600 is the
// closing ID OBVR clicks after acting on a choice. README.md ("Controls in
// Full VR") says the same in a table; change both. The door and the chest are
// named since 2026-10-18 (a tester, 2026-10-17: "cant open doors not sure how
// to pick up items" - A takes no loose item with TakeOnlyByHand on, the grip
// holds it and the chest keeps it).
//
// Five pages since 2026-10-21, a drawing and five short lines each (Nadi:
// "die controls cards gerne mehrere statt alles auf einem haufen gepresst",
// "zeichnung der controller mappings statt text wo immer das sinn macht"):
// twelve lines a page had wrapped into each other on the card (the harness
// picture of 2026-10-21: a line of 62 characters already wraps at the card's
// 688 pixels). The drawings are tools/make-controls-drawings.py's, one per
// page and hand (a right-hander's, and the left-hander's mirrored), shown by
// their image tile's alpha. Pure, covered by native_settings_test.
constexpr int kControlsCardClose = 9600;
constexpr int kControlsCardPrimary = 9601;    // Next, or Start over on the last page
constexpr int kControlsCardSecondary = 9602;  // Close

constexpr UInt32 kControlsCardLines = 5;
// The longest a line may be and still fit the card's width on one line.
constexpr UInt32 kControlsCardLineMax = 56;

enum class ControlsCardPage : UInt8 { WeaponButtons, WeaponPlaces, OtherButtons, Holding, BothHands };
constexpr UInt32 kControlsCardPages = 5;
constexpr ControlsCardPage kControlsCardFirstPage = ControlsCardPage::WeaponButtons;

// Two drawings a page, the right-hander's and the left-hander's: the image
// tiles pic0..pic9, pic(2 * page + left-handed). With Index controllers
// (vr::RingsOnTrackpad; Nadi, 2026-10-22: "erstelle dafür die bilder mit
// index controller auch") the three pages that show the controllers - 1, 3
// and 5 - have an Index's drawings, pic10..pic15.
constexpr UInt32 kControlsCardIndexDrawings = 6;
constexpr UInt32 kControlsCardDrawings = 2 * kControlsCardPages + kControlsCardIndexDrawings;

// The image tile a page shows.
inline UInt32 ControlsCardDrawing(ControlsCardPage page, bool leftHanded, bool index) {
	const UInt32 hand = leftHanded ? 1u : 0u;
	const UInt32 p = static_cast<UInt32>(page);
	const bool showsControllers = page == ControlsCardPage::WeaponButtons || page == ControlsCardPage::OtherButtons ||
	                              page == ControlsCardPage::BothHands;
	if (index && showsControllers) {
		return 2 * kControlsCardPages + 2 * (p / 2) + hand;
	}
	return 2 * p + hand;
}

struct ControlsCardText {
	const char* title = "";
	const char* lines[kControlsCardLines] = {};
	const char* primary = "";
	const char* secondary = "";
	UInt32 drawing = 0;  // the image tile shown, 0..kControlsCardDrawings-1
};

// `index`: Index controllers, whose rings open on the trackpads held.
// armSwing: running by swung arms ([Hands] ArmSwingRun) instead of the left
// stick pushed in - the card says the one that is set.
inline ControlsCardText ControlsCardFor(ControlsCardPage page, bool leftHanded, bool index = false,
                                        bool armSwing = false) {
	ControlsCardText t;
	t.secondary = "Close";
	t.primary = page == ControlsCardPage::BothHands ? "Start over" : "Next";
	t.drawing = ControlsCardDrawing(page, leftHanded, index);
	const char* const* lines = nullptr;
	switch (page) {
	case ControlsCardPage::WeaponButtons: {
		t.title = leftHanded ? "Weapon hand (left): buttons 1/5" : "Weapon hand (right): buttons 1/5";
		static const char* const l[kControlsCardLines] = {
			"Trigger: draw the bow while held, loose on release.",
			"A: open doors, talk, use - it never picks up.",
			"B: the pause menu.",
			"Stick: turn. Hold it up to aim a teleport, let go.",
			"Flick it up: jump. Flick down, or crouch: sneak.",
		};
		lines = l;
		break;
	}
	case ControlsCardPage::WeaponPlaces: {
		t.title = leftHanded ? "Weapon hand (left): weapons 2/5" : "Weapon hand (right): weapons 2/5";
		static const char* const l[kControlsCardLines] = {
			"Grip at a hip: draw or put away a one-hander.",
			"Grip behind a shoulder: a two-hander or a staff.",
			"Swing to hit. Long and fast: a power attack.",
			"Make a fist or squeeze the grip: raise your fists.",
			"A second grip on a two-hander: hold it two-handed.",
		};
		lines = l;
		break;
	}
	case ControlsCardPage::OtherButtons: {
		t.title = leftHanded ? "Other hand (right): buttons 3/5" : "Other hand (left): buttons 3/5";
		static const char* const l[kControlsCardLines] = {
			"Trigger: cast your spell.",
			"Grip at a hip or shoulder: bow, shield, torch.",
			"A / X: drop the item under the cursor (inventory).",
			"B / Y: the Tab menus here; the other hand clicks.",
			"Stick: walk. Pushed in: run.",
		};
		static const char* const swing[kControlsCardLines] = {
			"Trigger: cast your spell.",
			"Grip at a hip or shoulder: bow, shield, torch.",
			"A / X: drop the item under the cursor (inventory).",
			"B / Y: the Tab menus here; the other hand clicks.",
			"Stick: walk. Swing both arms: run.",
		};
		lines = armSwing ? swing : l;
		break;
	}
	case ControlsCardPage::Holding: {
		t.title = "Holding and opening 4/5";
		static const char* const l[kControlsCardLines] = {
			"Grip an item to hold it. Let go at your chest: kept.",
			"Let go mid-swing and it flies.",
			"Reach into a chest, a body or a lock: it opens.",
			"Raise the arm with a shield on it: block.",
			"Both palms out, rocked gently side to side: yield.",
		};
		lines = l;
		break;
	}
	default: {
		t.title = "Both hands: rings and settings 5/5";
		static const char* const sticks[kControlsCardLines] = {
			"Weapon stick clicked: the hotkey ring. Held: rest.",
			"Other stick clicked: the spell ring.",
			"In a ring: move the hand there, click again.",
			"Trigger in a ring: next page. Other stick in: run.",
			"Both sticks clicked: settings. Held 3 s: recenter.",
		};
		static const char* const sticksSwing[kControlsCardLines] = {
			"Weapon stick clicked: the hotkey ring. Held: rest.",
			"Other stick clicked: the spell ring.",
			"In a ring: move the hand there, click again.",
			"Trigger in a ring: next page.",
			"Both sticks clicked: settings. Held 3 s: recenter.",
		};
		static const char* const pads[kControlsCardLines] = {
			"Weapon trackpad held: the hotkey ring.",
			"Other trackpad held: the spell ring.",
			"In a ring: move the hand there and let go.",
			"Trigger in a ring: next page. Weapon stick held: rest.",
			"Both sticks clicked: settings. Held 3 s: recenter.",
		};
		lines = index ? pads : (armSwing ? sticksSwing : sticks);
		break;
	}
	}
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

// The next page; the last turns back to the first.
inline ControlsCardPage ControlsCardTurned(ControlsCardPage page) {
	const UInt32 next = static_cast<UInt32>(page) + 1;
	return next >= kControlsCardPages ? kControlsCardFirstPage : static_cast<ControlsCardPage>(next);
}

// Writes `head`, the number, then `tail` into `out`; false, and `out`
// emptied, when the buffer is too small.
inline bool ControlsCardTrait(const char* head, UInt32 number, const char* tail, char* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0) {
		return false;
	}
	UInt32 at = 0;
	bool fits = true;
	const auto put = [&](char c) {
		if (at + 1 >= capacity) {
			fits = false;
			return;
		}
		out[at++] = c;
	};
	for (const char* c = head; *c != '\0'; ++c) {
		put(*c);
	}
	if (number >= 10) {
		put(static_cast<char>('0' + (number / 10) % 10));
	}
	put(static_cast<char>('0' + number % 10));
	for (const char* c = tail; *c != '\0'; ++c) {
		put(*c);
	}
	out[fits ? at : 0] = '\0';
	return fits;
}

// The trait of a line's text element, "parchment\lineN\string"; false when
// the index is past the card or the buffer too small.
inline bool ControlsCardLineTrait(UInt32 line, char* out, UInt32 capacity) {
	if (line >= kControlsCardLines) {
		if (out != nullptr && capacity > 0) {
			out[0] = '\0';
		}
		return false;
	}
	return ControlsCardTrait("parchment\\line", line, "\\string", out, capacity);
}

// The alpha of a drawing's image tile, "parchment\picN\alpha"; false past the
// drawings or with the buffer too small.
inline bool ControlsCardDrawingTrait(UInt32 drawing, char* out, UInt32 capacity) {
	if (drawing >= kControlsCardDrawings) {
		if (out != nullptr && capacity > 0) {
			out[0] = '\0';
		}
		return false;
	}
	return ControlsCardTrait("parchment\\pic", drawing, "\\alpha", out, capacity);
}

}  // namespace obvr::ui
