// The words OBVR gives Oblivion's controls in Full VR (ui/ControlPhrase.h):
// every control index with either hand, the controls left to the engine,
// the attack's words by how the bow is shot, the edges of the buffer.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ui/ControlPhrase.h"

using namespace obvr::ui;

int failures = 0;
void Check(bool yes, const char* label) {
	if (!yes) {
		++failures;
		std::printf("FAIL %s\n", label);
	}
}

bool Same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

constexpr BowWords kBow = BowWords::HandTrigger;  // the shipped defaults

int main() {
	std::printf("Every control, either hand, every bow\n");
	const bool noWords[kControlCount] = {false, false, false, false, false, false, false, false, false, false,
	                                     false, true,  true,  false, true,  false, false, false, false, false,
	                                     false, false, false, false, false, false, true,  true,  false};
	for (BowWords bow : {BowWords::TriggerDraws, BowWords::HandTrigger, BowWords::HandGrip}) {
		for (UInt32 hand = 0; hand < 2; ++hand) {
			const bool leftHanded = hand == 1;
			for (UInt32 control = 0; control < kControlCount; ++control) {
				char out[kControlPhraseCapacity];
				std::strcpy(out, "untouched");
				const bool answered = ControlPhrase(control, leftHanded, bow, out, sizeof(out));
				char label[96];
				std::snprintf(label, sizeof(label), "control %u (%s), %s-handed, bow %d", control,
				              ControlName(control), leftHanded ? "left" : "right", static_cast<int>(bow));
				if (noWords[control]) {
					Check(!answered && Same(out, "untouched"), label);
					continue;
				}
				Check(answered && out[0] != '\0' && out[std::strlen(out) - 1] != '.' && out[0] >= 'a' &&
				          out[0] <= 'z',
				      label);
				Check(std::strlen(out) + 1 <= kControlPhraseCapacity, "the phrase fits the capacity");
			}
		}
	}

	std::printf("The words, right-handed\n");
	char out[kControlPhraseCapacity];
	Check(ControlPhrase(kControlForward, false, kBow, out, sizeof(out)) && Same(out, "push the left stick forward"),
	      "forward: the other hand's stick");
	Check(ControlPhrase(kControlBack, false, kBow, out, sizeof(out)) && Same(out, "pull the left stick back"),
	      "back");
	Check(ControlPhrase(kControlStrafeLeft, false, kBow, out, sizeof(out)) && Same(out, "push the left stick left"),
	      "strafe left");
	Check(ControlPhrase(kControlStrafeRight, false, kBow, out, sizeof(out)) &&
	          Same(out, "push the left stick right"),
	      "strafe right");
	Check(ControlPhrase(kControlActivate, false, kBow, out, sizeof(out)) &&
	          Same(out, "press A on the right controller"),
	      "activate: A on the weapon hand");
	Check(ControlPhrase(kControlBlock, false, kBow, out, sizeof(out)) &&
	          Same(out, "raise your left hand with the shield on its arm, or hold the blade across the blow"),
	      "block");
	Check(ControlPhrase(kControlCast, false, kBow, out, sizeof(out)) && Same(out, "pull the left trigger"),
	      "cast: the other hand's trigger");
	Check(ControlPhrase(kControlDrawSheatheWeapon, false, kBow, out, sizeof(out)) &&
	          Same(out, "reach to your left hip and squeeze the right grip, or make a fist"),
	      "ready weapon: the holster on the other side, or a fist");
	Check(ControlPhrase(kControlSneak, false, kBow, out, sizeof(out)) &&
	          Same(out, "flick the right stick down, or crouch"),
	      "sneak");
	Check(ControlPhrase(kControlRun, false, kBow, out, sizeof(out)) && Same(out, "hold the left stick clicked in"),
	      "run");
	Check(ControlPhrase(kControlJump, false, kBow, out, sizeof(out)) && Same(out, "flick the right stick up"),
	      "jump");
	Check(ControlPhrase(kControlMenuMode, false, kBow, out, sizeof(out)) &&
	          Same(out, "press B on the left controller"),
	      "journal: B on the other hand");
	Check(ControlPhrase(kControlRest, false, kBow, out, sizeof(out)) && Same(out, "hold the right stick clicked in"),
	      "rest: the weapon hand's stick held");
	Check(ControlPhrase(kControlQuickMenu, false, kBow, out, sizeof(out)) && Same(out, "click the right stick"),
	      "quick menu: the weapon hand's stick clicked");
	Check(ControlPhrase(kControlQuick1, false, kBow, out, sizeof(out)) &&
	          Same(out, "pick slot 1 on the hotkey ring"),
	      "hotkey 1");
	Check(ControlPhrase(kControlQuick1 + 2, false, kBow, out, sizeof(out)) &&
	          Same(out, "pick slot 3 on the hotkey ring"),
	      "hotkey 3");
	Check(ControlPhrase(kControlQuick8, false, kBow, out, sizeof(out)) &&
	          Same(out, "pick slot 8 on the hotkey ring"),
	      "hotkey 8");
	Check(ControlPhrase(kControlGrab, false, kBow, out, sizeof(out)) && Same(out, "squeeze the grip on it"),
	      "grab");

	std::printf("The attack, by how the bow is shot\n");
	Check(ControlPhrase(kControlUseEquipped, false, BowWords::HandTrigger, out, sizeof(out)) &&
	          Same(out, "swing the weapon - with a bow, hold the right trigger to grab an arrow and release to shoot"),
	      "the bow by hand, the arrow by the trigger (the default)");
	Check(ControlPhrase(kControlUseEquipped, false, BowWords::HandGrip, out, sizeof(out)) &&
	          Same(out, "swing the weapon - with a bow, hold the right grip to grab an arrow and release to shoot"),
	      "the bow by hand, the arrow by the grip");
	Check(ControlPhrase(kControlUseEquipped, false, BowWords::TriggerDraws, out, sizeof(out)) &&
	          Same(out, "swing the weapon - with a bow, hold the right trigger to draw and let go to shoot"),
	      "the bow drawn by the trigger");
	Check(ControlPhrase(kControlUseEquipped, true, BowWords::HandGrip, out, sizeof(out)) &&
	          Same(out, "swing the weapon - with a bow, hold the left grip to grab an arrow and release to shoot"),
	      "left-handed: the left grip");
	Check(BowWordsFor(false, true) == BowWords::TriggerDraws && BowWordsFor(false, false) == BowWords::TriggerDraws,
	      "BowByHand off: the trigger draws, whatever takes the arrow");
	Check(BowWordsFor(true, true) == BowWords::HandTrigger, "BowByHand on, the arrow by the trigger");
	Check(BowWordsFor(true, false) == BowWords::HandGrip, "BowByHand on, the arrow by the grip");

	std::printf("The hands swapped\n");
	Check(ControlPhrase(kControlForward, true, kBow, out, sizeof(out)) && Same(out, "push the right stick forward"),
	      "forward, left-handed: the right stick walks");
	Check(ControlPhrase(kControlActivate, true, kBow, out, sizeof(out)) &&
	          Same(out, "press A on the left controller"),
	      "activate, left-handed: the weapon hand is the left");
	Check(ControlPhrase(kControlDrawSheatheWeapon, true, kBow, out, sizeof(out)) &&
	          Same(out, "reach to your right hip and squeeze the left grip, or make a fist"),
	      "ready weapon, left-handed: the right hip, the left grip");
	Check(ControlPhrase(kControlJump, true, kBow, out, sizeof(out)) && Same(out, "flick the left stick up"),
	      "jump, left-handed: the weapon hand's stick is the left");
	Check(ControlPhrase(kControlGrab, true, kBow, out, sizeof(out)) && Same(out, "squeeze the grip on it"),
	      "grab names no hand");

	std::printf("The controls left to the engine\n");
	for (UInt32 control : {kControlAutoRun, kControlAutoMove, kControlTogglePov, kControlQuickSave, kControlQuickLoad}) {
		std::strcpy(out, "untouched");
		Check(!ControlPhrase(control, false, kBow, out, sizeof(out)) && Same(out, "untouched"), ControlName(control));
	}

	std::printf("The tokens\n");
	for (UInt32 control = 0; control < kControlCount; ++control) {
		Check(ControlToken(control)[0] == 's' && ControlIndexOfToken(ControlToken(control)) == control,
		      "every token names its own control");
	}
	Check(ControlIndexOfToken("sUActnForward") == kControlForward && ControlIndexOfToken("sUActnGrab") == kControlGrab,
	      "the first and the last token");
	Check(ControlIndexOfToken("sUActnForwar") == kControlCount && ControlIndexOfToken("sUActnForwardX") == kControlCount &&
	          ControlIndexOfToken("SUACTNFORWARD") == kControlCount && ControlIndexOfToken("") == kControlCount &&
	          ControlIndexOfToken(nullptr) == kControlCount,
	      "a token cut, lengthened, in other case, empty or missing names none");
	Check(ControlToken(kControlCount)[0] == '\0', "no token past the last");

	std::printf("The edges\n");
	std::strcpy(out, "untouched");
	Check(!ControlPhrase(kControlCount, false, kBow, out, sizeof(out)) && Same(out, "untouched"), "one past the last");
	Check(!ControlPhrase(1000u, false, kBow, out, sizeof(out)) && Same(out, "untouched"), "far past the last");
	Check(!ControlPhrase(kControlForward, false, kBow, nullptr, 64), "no buffer");
	Check(!ControlPhrase(kControlForward, false, kBow, out, 0) && Same(out, "untouched"), "no room");
	Check(!ControlPhrase(kControlForward, false, kBow, out, 10) && Same(out, "untouched"),
	      "too little room: untouched");
	Check(ControlPhrase(kControlForward, false, kBow, out, 28) && Same(out, "push the left stick forward"),
	      "exactly the room, the terminator included");
	Check(!ControlPhrase(kControlForward, false, kBow, out, 27), "one byte short");
	Check(Same(ControlName(0), "Forward") && Same(ControlName(28), "Grab") && Same(ControlName(29), "?"),
	      "the names and the one past them");

	if (failures == 0) {
		std::printf("control_phrase_test: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
