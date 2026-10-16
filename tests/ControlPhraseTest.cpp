// The words OBVR gives Oblivion's controls in Full VR (ui/ControlPhrase.h):
// every control index with either hand, the controls left to the engine,
// the edges of the buffer.
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
bool Has(const char* text, const char* piece) { return std::strstr(text, piece) != nullptr; }

int main() {
	std::printf("Every control, either hand\n");
	const bool noWords[kControlCount] = {false, false, false, false, false, false, false, false, false, false,
	                                     false, true,  true,  false, true,  false, false, false, false, false,
	                                     false, false, false, false, false, false, true,  true,  false};
	for (UInt32 hand = 0; hand < 2; ++hand) {
		const bool leftHanded = hand == 1;
		for (UInt32 control = 0; control < kControlCount; ++control) {
			char out[kControlPhraseCapacity];
			std::strcpy(out, "untouched");
			const bool answered = ControlPhrase(control, leftHanded, out, sizeof(out));
			char label[96];
			std::snprintf(label, sizeof(label), "control %u (%s), %s-handed", control, ControlName(control),
			              leftHanded ? "left" : "right");
			if (noWords[control]) {
				Check(!answered && Same(out, "untouched"), label);
				continue;
			}
			Check(answered && out[0] != '\0' && out[std::strlen(out) - 1] != '.' && out[0] >= 'a' && out[0] <= 'z',
			      label);
			Check(std::strlen(out) + 1 <= kControlPhraseCapacity, "the phrase fits the capacity");
		}
	}

	std::printf("The words, right-handed\n");
	char out[kControlPhraseCapacity];
	Check(ControlPhrase(kControlForward, false, out, sizeof(out)) && Same(out, "push the left stick forward"),
	      "forward: the other hand's stick");
	Check(ControlPhrase(kControlBack, false, out, sizeof(out)) && Same(out, "pull the left stick back"), "back");
	Check(ControlPhrase(kControlStrafeLeft, false, out, sizeof(out)) && Same(out, "push the left stick left"),
	      "strafe left");
	Check(ControlPhrase(kControlStrafeRight, false, out, sizeof(out)) && Same(out, "push the left stick right"),
	      "strafe right");
	Check(ControlPhrase(kControlUseEquipped, false, out, sizeof(out)) &&
	          Same(out, "swing the weapon - with a bow, hold the right trigger"),
	      "attack: the swing, the bow by the weapon hand's trigger");
	Check(ControlPhrase(kControlActivate, false, out, sizeof(out)) && Same(out, "press A on the right controller"),
	      "activate: A on the weapon hand");
	Check(ControlPhrase(kControlBlock, false, out, sizeof(out)) &&
	          Same(out, "raise your left hand with the shield on its arm, or hold the blade across the blow"),
	      "block");
	Check(ControlPhrase(kControlCast, false, out, sizeof(out)) && Same(out, "pull the left trigger"),
	      "cast: the other hand's trigger");
	Check(ControlPhrase(kControlDrawSheatheWeapon, false, out, sizeof(out)) &&
	          Same(out, "reach to your left hip and squeeze the right grip, or make a fist"),
	      "ready weapon: the holster on the other side, or a fist");
	Check(ControlPhrase(kControlSneak, false, out, sizeof(out)) && Same(out, "flick the right stick down, or crouch"),
	      "sneak");
	Check(ControlPhrase(kControlRun, false, out, sizeof(out)) && Same(out, "hold the left stick clicked in"), "run");
	Check(ControlPhrase(kControlJump, false, out, sizeof(out)) && Same(out, "flick the right stick up"), "jump");
	Check(ControlPhrase(kControlMenuMode, false, out, sizeof(out)) && Same(out, "press B on the left controller"),
	      "journal: B on the other hand");
	Check(ControlPhrase(kControlRest, false, out, sizeof(out)) && Same(out, "hold the right stick clicked in"),
	      "rest: the weapon hand's stick held");
	Check(ControlPhrase(kControlQuickMenu, false, out, sizeof(out)) && Same(out, "click the right stick"),
	      "quick menu: the weapon hand's stick clicked");
	Check(ControlPhrase(kControlQuick1, false, out, sizeof(out)) && Same(out, "pick slot 1 on the hotkey ring"),
	      "hotkey 1");
	Check(ControlPhrase(kControlQuick1 + 2, false, out, sizeof(out)) && Same(out, "pick slot 3 on the hotkey ring"),
	      "hotkey 3");
	Check(ControlPhrase(kControlQuick8, false, out, sizeof(out)) && Same(out, "pick slot 8 on the hotkey ring"),
	      "hotkey 8");
	Check(ControlPhrase(kControlGrab, false, out, sizeof(out)) && Same(out, "squeeze the grip on it"), "grab");

	std::printf("The hands swapped\n");
	Check(ControlPhrase(kControlForward, true, out, sizeof(out)) && Same(out, "push the right stick forward"),
	      "forward, left-handed: the right stick walks");
	Check(ControlPhrase(kControlActivate, true, out, sizeof(out)) && Same(out, "press A on the left controller"),
	      "activate, left-handed: the weapon hand is the left");
	Check(ControlPhrase(kControlDrawSheatheWeapon, true, out, sizeof(out)) &&
	          Same(out, "reach to your right hip and squeeze the left grip, or make a fist"),
	      "ready weapon, left-handed: the right hip, the left grip");
	Check(ControlPhrase(kControlJump, true, out, sizeof(out)) && Same(out, "flick the left stick up"),
	      "jump, left-handed: the weapon hand's stick is the left");
	Check(ControlPhrase(kControlGrab, true, out, sizeof(out)) && Same(out, "squeeze the grip on it"),
	      "grab names no hand");

	std::printf("The controls left to the engine\n");
	for (UInt32 control : {kControlAutoRun, kControlAutoMove, kControlTogglePov, kControlQuickSave, kControlQuickLoad}) {
		std::strcpy(out, "untouched");
		Check(!ControlPhrase(control, false, out, sizeof(out)) && Same(out, "untouched"), ControlName(control));
	}

	std::printf("The edges\n");
	std::strcpy(out, "untouched");
	Check(!ControlPhrase(kControlCount, false, out, sizeof(out)) && Same(out, "untouched"), "one past the last");
	Check(!ControlPhrase(1000u, false, out, sizeof(out)) && Same(out, "untouched"), "far past the last");
	Check(!ControlPhrase(kControlForward, false, nullptr, 64), "no buffer");
	Check(!ControlPhrase(kControlForward, false, out, 0) && Same(out, "untouched"), "no room");
	Check(!ControlPhrase(kControlForward, false, out, 10) && Same(out, "untouched"), "too little room: untouched");
	Check(ControlPhrase(kControlForward, false, out, 28) && Same(out, "push the left stick forward"),
	      "exactly the room, the terminator included");
	Check(!ControlPhrase(kControlForward, false, out, 27), "one byte short");
	Check(Same(ControlName(0), "Forward") && Same(ControlName(28), "Grab") && Same(ControlName(29), "?"),
	      "the names and the one past them");

	if (failures == 0) {
		std::printf("control_phrase_test: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
