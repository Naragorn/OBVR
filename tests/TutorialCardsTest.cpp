// The tutorial cards in Full VR's words (ui/TutorialCards.h): every card
// matched whole for every way the bow is shot, nothing else matched, the
// cards' tokens all controls OBVR has words for, no side named.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ui/ControlPhrase.h"
#include "ui/TutorialCards.h"

using namespace obvr::ui;

int failures = 0;
void Check(bool yes, const char* label) {
	if (!yes) {
		++failures;
		std::printf("FAIL %s\n", label);
	}
}

const BowWords kBows[3] = {BowWords::TriggerDraws, BowWords::HandTrigger, BowWords::HandGrip};

// The checks every text of a card has to pass.
void CheckWords(const TutorialCardText& card, const char* words, const char* label) {
	Check(words != nullptr && words[0] != '\0', label);
	Check(std::strcmp(card.vanilla, words) != 0, "the card says something else");
	Check(std::strlen(words) <= 320, "the card fits a window");

	// Every "&name;" in the card is a control OBVR has words for.
	for (const char* c = words; *c != '\0'; ++c) {
		if (*c != '&') {
			continue;
		}
		char name[48];
		UInt32 at = 0;
		const char* e = c + 1;
		while (*e != '\0' && *e != ';' && at + 1 < sizeof(name)) {
			name[at++] = *e++;
		}
		name[at] = '\0';
		const UInt32 control = ControlIndexOfToken(name);
		char out[kControlPhraseCapacity];
		Check(*e == ';' && control < kControlCount &&
		          ControlPhrase(control, false, BowWords::HandTrigger, out, sizeof(out)),
		      "a token in the card is a control with words");
	}

	// No side named: either hand's words come from the tokens.
	for (const char* side : {"left stick", "right stick", "left hand", "right hand", "left controller",
	                         "right controller", "left trigger", "right trigger", "left grip", "right grip"}) {
		Check(std::strstr(words, side) == nullptr, "the card names no side");
	}
}

int main() {
	std::printf("Every card, every bow\n");
	UInt32 withBowVariants = 0;
	for (UInt32 i = 0; i < kTutorialCardCount; ++i) {
		const TutorialCardText& card = TutorialCardAt(i);
		char label[64];
		std::snprintf(label, sizeof(label), "card %u", i);
		Check(card.vanilla[0] != '\0', label);
		Check((card.cardHandTrigger == nullptr) == (card.cardHandGrip == nullptr),
		      "a card has both bow variants or neither");
		if (card.cardHandTrigger != nullptr) {
			++withBowVariants;
		}
		for (BowWords bow : kBows) {
			const char* words = TutorialCard(card.vanilla, bow);
			Check(words == TutorialCardWords(card, bow), "the vanilla text is answered with the card's words for the bow");
			CheckWords(card, words, label);
		}
		if (card.cardHandTrigger == nullptr) {
			Check(TutorialCardWords(card, BowWords::HandTrigger) == card.card &&
			          TutorialCardWords(card, BowWords::HandGrip) == card.card,
			      "a card without bow variants says the same for every bow");
		} else {
			Check(std::strcmp(card.card, card.cardHandTrigger) != 0 &&
			          std::strcmp(card.cardHandTrigger, card.cardHandGrip) != 0,
			      "the bow variants differ");
		}

		// A text one character off is no card: the last character changed,
		// one more at the end, one fewer.
		for (BowWords bow : kBows) {
			char off[400];
			std::strcpy(off, card.vanilla);
			const size_t n = std::strlen(off);
			off[n - 1] = off[n - 1] == 'x' ? 'y' : 'x';
			Check(TutorialCard(off, bow) == nullptr, "a changed last character matches nothing");
			std::strcpy(off, card.vanilla);
			off[n] = ' ';
			off[n + 1] = '\0';
			Check(TutorialCard(off, bow) == nullptr, "one character more matches nothing");
			std::strcpy(off, card.vanilla);
			off[n - 1] = '\0';
			Check(TutorialCard(off, bow) == nullptr, "one character fewer matches nothing");
		}
	}
	Check(withBowVariants == 1, "one card - the bow's - has the bow variants");

	std::printf("The words\n");
	const char* walk = TutorialCard(
		"To move forward, &sUActnForward;, &sUActnBack; to move back. To move left, &sUActnSldleft;. To move "
		"right, &sUActnSldright;. The mouse turns you. To run, &sUActnRun;.",
		BowWords::HandTrigger);
	Check(walk != nullptr && std::strstr(walk, "Your weapon hand's stick turns you") != nullptr &&
	          std::strstr(walk, "mouse") == nullptr && std::strstr(walk, "&sUActnRun;") != nullptr,
	      "the walk: the stick turns, the mouse is gone, the run's token kept");
	const char* drop = TutorialCard(
		"Click on an item to equip it. Click again to unequip it. Shift-click to drop an item.", BowWords::HandTrigger);
	Check(drop != nullptr && std::strstr(drop, "hold A on your other hand") != nullptr &&
	          std::strstr(drop, "Shift") == nullptr,
	      "the drop: A on the other hand, no Shift");
	const char* const bowEntry =
		"Now you're ready to practice with your bow. The bucket near the well will make a good target. To draw "
		"and aim, &sUActnUse;. Release it when you're ready to fire.";
	const char* bow = TutorialCard(bowEntry, BowWords::TriggerDraws);
	Check(bow != nullptr && std::strstr(bow, "Hold the trigger of your weapon hand to draw") != nullptr &&
	          std::strstr(bow, "&sUActnUse;") == nullptr,
	      "the bow drawn by the trigger: the weapon hand's trigger, the attack's token gone");
	bow = TutorialCard(bowEntry, BowWords::HandTrigger);
	Check(bow != nullptr && std::strstr(bow, "hold the trigger to take an arrow from the quiver") != nullptr &&
	          std::strstr(bow, "string behind the bow") != nullptr,
	      "the bow by hand: the arrow by the trigger, nocked behind the bow");
	bow = TutorialCard(bowEntry, BowWords::HandGrip);
	Check(bow != nullptr && std::strstr(bow, "squeeze the grip to take an arrow from the quiver") != nullptr &&
	          std::strstr(bow, "trigger") == nullptr,
	      "the bow by hand: the arrow by the grip, no trigger named");

	std::printf("Nothing else\n");
	for (BowWords b : kBows) {
		Check(TutorialCard(nullptr, b) == nullptr, "no text");
		Check(TutorialCard("", b) == nullptr, "an empty text");
		Check(TutorialCard("To move forward", b) == nullptr, "a beginning alone");
		Check(TutorialCard("Use the left stick to move around. The right stick turns you.", b) == nullptr,
		      "the console variant is not swapped");
		Check(TutorialCard("Your Health is low and you are close to death. Increase it by drinking potions, casting "
		                   "heal spells, or sleeping. The red bar at the bottom left displays your current Health.",
		                   b) == nullptr,
		      "a card without a control keeps its words");
	}
	Check(TutorialCardAt(kTutorialCardCount).vanilla[0] == '\0', "one past the last card is empty");

	if (failures == 0) {
		std::printf("tutorial_cards_test: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
