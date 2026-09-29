// Checks the sound a heard teleport lands with (vr/TeleportSound.h): the
// setting's words and row values, the form each plays, and when it plays.

#include <cstdio>

#include "vr/TeleportSound.h"

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

void TestWords() {
	std::printf("The INI's words\n");
	TeleportSound s = TeleportSound::Swish;
	Check(ParseTeleportSound("dodge", s) && s == TeleportSound::Dodge, "dodge");
	Check(ParseTeleportSound("DodgeBack", s) && s == TeleportSound::DodgeBackward, "any case: DodgeBack");
	Check(ParseTeleportSound("landing", s) && s == TeleportSound::Landing, "landing");
	Check(ParseTeleportSound("SWISH", s) && s == TeleportSound::Swish, "SWISH");
	Check(!ParseTeleportSound("roll", s) && s == TeleportSound::Swish, "not a word of it: refused, left alone");
	Check(!ParseTeleportSound("dodg", s) && !ParseTeleportSound("dodgebackward", s), "shorter or longer: refused");
	Check(!ParseTeleportSound("", s) && !ParseTeleportSound(nullptr, s), "empty or none: refused");
	Check(kTeleportSoundDefault == TeleportSound::Landing, "the default is the landing (the tester, 2026-09-29)");
}

void TestRow() {
	std::printf("The settings row\n");
	Check(TeleportSoundFromIndex(0.0f) == TeleportSound::Dodge && TeleportSoundFromIndex(1.0f) == TeleportSound::DodgeBackward &&
	          TeleportSoundFromIndex(2.0f) == TeleportSound::Landing && TeleportSoundFromIndex(3.0f) == TeleportSound::Swish,
	      "0..3 in the order of the words");
	Check(TeleportSoundFromIndex(1.6f) == TeleportSound::Landing, "a value between rounds");
	Check(TeleportSoundFromIndex(-1.0f) == kTeleportSoundDefault && TeleportSoundFromIndex(4.0f) == kTeleportSoundDefault,
	      "out of range: the default");
	volatile float zero = 0.0f;
	Check(TeleportSoundFromIndex(zero / zero) == kTeleportSoundDefault, "not a number: the default");
}

void TestForms() {
	std::printf("The forms\n");
	Check(TeleportSoundForm(TeleportSound::Dodge) == 0x000CBA79, "dodge: FSTDodge");
	Check(TeleportSoundForm(TeleportSound::DodgeBackward) == 0x000CBA7A, "dodgeback: FSTDodgeBackward");
	Check(TeleportSoundForm(TeleportSound::Swish) == 0x00088834, "swish: WPNSwishHand (the PC's, not the 360lofi one)");
	Check(TeleportSoundForm(TeleportSound::Landing) == 0, "landing: none - the engine picks it by the ground");
	Check(TeleportSoundForm(static_cast<TeleportSound>(9)) == 0, "a value that is none of them: no form");
}

void TestDue() {
	std::printf("When it plays\n");
	Check(TeleportSoundDue(true, true), "heard, and just landed: played");
	Check(!TeleportSoundDue(true, false), "heard, still moving (or idle): not yet");
	Check(!TeleportSoundDue(false, true), "not heard: silent");
	Check(!TeleportSoundDue(false, false), "neither: silent");
}

}  // namespace

int main() {
	TestWords();
	TestRow();
	TestForms();
	TestDue();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
