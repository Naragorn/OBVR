#pragma once

#include "core/Types.h"
#include "ui/ControlPhrase.h"

namespace obvr::ui {

// The tutorial cards said in Full VR's words (docs/onboarding-spec.md 3.4,
// step 2; the tester, 2026-10-16). The Tutorial quest's stage entries
// (MQ01 in Oblivion.esm, the PC variants, read 2026-10-16) are matched
// whole and swapped for OBVR's text where a sentence of theirs is bound to
// the mouse or the keyboard beyond what a key name's token carries - "The
// mouse turns you", "Click on the fist icon", "Shift-click to drop". The
// cards keep vanilla's "&sUActn…;" tokens where a control is named, so
// ui/ControlPhrase.h gives them either hand's words at layout; what the
// cards say themselves names no side ("your weapon hand", "your other
// hand"), so one text serves both hands. The bow's card follows how the bow
// is shot (BowWords). An entry of another language, or one a mod changed,
// matches nothing and keeps its own words (the tokens in it still get the
// mode's phrases). Pure, covered by tutorial_cards_test.

struct TutorialCardText {
	const char* vanilla;         // the esm's entry, matched whole
	const char* card;            // OBVR's words (the bow drawn by the trigger)
	const char* cardHandTrigger; // the bow by hand, the arrow by the trigger; nullptr: as `card`
	const char* cardHandGrip;    // the bow by hand, the arrow by the grip; nullptr: as `card`
};

constexpr UInt32 kTutorialCardCount = 9;

inline const TutorialCardText& TutorialCardAt(UInt32 index) {
	static const TutorialCardText kCards[kTutorialCardCount] = {
		// Stage 10, the walk.
		{"To move forward, &sUActnForward;, &sUActnBack; to move back. To move left, &sUActnSldleft;. To "
		 "move right, &sUActnSldright;. The mouse turns you. To run, &sUActnRun;.",
		 "To move forward, &sUActnForward;, &sUActnBack; to move back. To move left, &sUActnSldleft;. To "
		 "move right, &sUActnSldright;. Your weapon hand's stick turns you; hold it up to aim a teleport "
		 "and let go to go there. To run, &sUActnRun;.",
		 nullptr, nullptr},
		// Stage 15, the first container.
		{"You need a weapon. Check Captain Renault's body. To open any container, approach it and "
		 "&sUActnActivate; when its name appears.",
		 "You need a weapon. Check Captain Renault's body. To open any container, approach it and "
		 "&sUActnActivate; when its name appears, or just reach into it.",
		 nullptr, nullptr},
		// Stage 22, the inventory tab.
		{"Click on the fist icon at the bottom of the journal to bring up the inventory tab. From here, "
		 "you can equip armor and weapons by clicking on them.",
		 "The journal hangs on your other wrist. Point your weapon hand's laser at it and press A to "
		 "click. Click the fist icon at the bottom to bring up the inventory tab. From here, you can "
		 "equip armor and weapons by clicking on them.",
		 nullptr, nullptr},
		// Stage 26, equip and drop.
		{"Click on an item to equip it. Click again to unequip it. Shift-click to drop an item.",
		 "Click an item with the laser to equip it. Click again to unequip it. To drop an item, hold A "
		 "on your other hand over it.",
		 nullptr, nullptr},
		// Stage 36, the magic tab.
		{"Click on the spell icon at the bottom of the journal to bring up the magic tab.",
		 "Click the spell icon at the bottom of the journal with the laser to bring up the magic tab. "
		 "Out in the world, your other hand's stick clicked opens a spell ring with every spell you "
		 "know.",
		 nullptr, nullptr},
		// Stage 56, the bow - by how it is shot.
		{"Now you're ready to practice with your bow. The bucket near the well will make a good target. "
		 "To draw and aim, &sUActnUse;. Release it when you're ready to fire.",
		 "Now you're ready to practice with your bow. The bucket near the well will make a good target. "
		 "Hold the trigger of your weapon hand to draw, aim with the bow, and let go to loose the "
		 "arrow.",
		 "Now you're ready to practice with your bow. The bucket near the well will make a good target. "
		 "Reach over your weapon hand's shoulder or to a hip and hold the trigger to take an arrow, "
		 "bring it to the string behind the bow, pull it back and release to shoot.",
		 "Now you're ready to practice with your bow. The bucket near the well will make a good target. "
		 "Reach over your weapon hand's shoulder or to a hip and squeeze the grip to take an arrow, "
		 "bring it to the string behind the bow, pull it back and let go to shoot."},
		// Stage 76, the hotkeys.
		{"You can hotkey items and spells from your journal by holding down a number key and then "
		 "clicking the item you want in that slot. Pressing this number while exploring will equip that "
		 "item instantly.",
		 "You can hotkey items and spells from your journal: with the laser on the item, "
		 "&sUActnQuickmenu; to open the hotkey ring, move your hand to a slot and click again. While "
		 "exploring, the same ring equips that item instantly.",
		 nullptr, nullptr},
		// Stage 78, over the carry weight.
		{"Your current encumbrance exceeds the maximum you can carry (displayed in the lower left of the "
		 "inventory page). Shift-click to drop items you don't need.",
		 "Your current encumbrance exceeds the maximum you can carry (displayed in the lower left of the "
		 "inventory page). To drop items you don't need, hold A on your other hand over them.",
		 nullptr, nullptr},
		// Stage 94, the map tab.
		{"Click on the compass at the bottom of the journal to bring up the quest and map tab.",
		 "Click the compass at the bottom of the journal with the laser to bring up the quest and map "
		 "tab.",
		 nullptr, nullptr},
	};
	static const TutorialCardText kNone = {"", "", nullptr, nullptr};
	return index < kTutorialCardCount ? kCards[index] : kNone;
}

// The card's words for how the bow is shot: the variant where the card has
// one, its one text otherwise.
inline const char* TutorialCardWords(const TutorialCardText& card, BowWords bow) {
	if (bow == BowWords::HandTrigger && card.cardHandTrigger != nullptr) {
		return card.cardHandTrigger;
	}
	if (bow == BowWords::HandGrip && card.cardHandGrip != nullptr) {
		return card.cardHandGrip;
	}
	return card.card;
}

// OBVR's card for the entry's text, matched whole; nullptr for no text and
// for any text that is not one of the entries above.
inline const char* TutorialCard(const char* vanilla, BowWords bow) {
	if (vanilla == nullptr || vanilla[0] == '\0') {
		return nullptr;
	}
	for (UInt32 i = 0; i < kTutorialCardCount; ++i) {
		const char* a = TutorialCardAt(i).vanilla;
		const char* b = vanilla;
		while (*a != '\0' && *a == *b) {
			++a;
			++b;
		}
		if (*a == '\0' && *b == '\0') {
			return TutorialCardWords(TutorialCardAt(i), bow);
		}
	}
	return nullptr;
}

}  // namespace obvr::ui
