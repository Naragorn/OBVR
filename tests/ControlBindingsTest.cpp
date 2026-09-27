// Checks the reading of Oblivion.ini's [Controls] bindings and the verdict on
// whether the inventory drop (Shift + click) can work with them.

#include <cstdio>

#include "game/ControlBindings.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestParse() {
	std::printf("Parsing\n");
	ControlBinding b = ParseControlBinding("002AFFFF");
	Check(b.valid && b.key == 0x2A && b.mouse == 0xFF && b.joystick == 0xFF, "Run: Left Shift only");
	b = ParseControlBinding("003801FF");
	Check(b.valid && b.key == 0x38 && b.mouse == 0x01, "Block: Left Alt and the right button");
	b = ParseControlBinding("00ff00ff");
	Check(b.valid && b.key == 0xFF && b.mouse == 0x00, "Use, lower case: the left button");
	Check(!ParseControlBinding("002AFF").valid, "too short: refused");
	Check(!ParseControlBinding("00G2FFFF").valid, "not hex: refused");
	Check(!ParseControlBinding(nullptr).valid, "nothing: refused");
	Check(!ParseControlBinding("").valid, "empty: refused");
}

void TestJudge() {
	std::printf("The drop's verdict\n");
	const ControlBinding shift = ParseControlBinding("002AFFFF");
	Check(JudgeDropBinding(shift, 0x2A) == DropBindingVerdict::Fine, "vanilla: fine");
	Check(JudgeDropBinding(ControlBinding{}, 0x2A) == DropBindingVerdict::Unknown,
	      "unreadable: unknown");
	const ControlBinding rightShift = ParseControlBinding("0036FFFF");
	Check(JudgeDropBinding(rightShift, 0x36) == DropBindingVerdict::RunNotOnShift,
	      "Run moved to another key: the drop may fail");
	Check(JudgeDropBinding(shift, 0x1D) == DropBindingVerdict::RunKeyDiffers,
	      "OBVR's RunKey on another key: it presses the wrong one");
}

}  // namespace

int main() {
	TestParse();
	TestJudge();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
