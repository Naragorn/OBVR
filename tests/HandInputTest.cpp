// Checks the pure half of the hand-tracked mode: button bits in the legacy
// mask, and the shortest turn between two headings across the seam.

#include <cstdio>

#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"

namespace {

using obvr::vr::ButtonDown;
using obvr::vr::ShortestTurn;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 0.0001f && b - a < 0.0001f; }

void TestButtons() {
	std::printf("Buttons\n");

	const UInt64 trigger = 1ull << obvr::vr::openvr::kButtonTrigger;
	const UInt64 grip = 1ull << obvr::vr::openvr::kButtonGrip;
	Check(ButtonDown(trigger, obvr::vr::openvr::kButtonTrigger), "the trigger bit reads");
	Check(!ButtonDown(trigger, obvr::vr::openvr::kButtonGrip), "and is not the grip");
	Check(ButtonDown(trigger | grip, obvr::vr::openvr::kButtonGrip), "two down read both");
	Check(!ButtonDown(0, obvr::vr::openvr::kButtonApplicationMenu), "nothing down reads nothing");
	Check(!ButtonDown(1ull << 63, obvr::vr::openvr::kButtonTrigger),
	      "a high bit is not the trigger");
}

void TestShortestTurn() {
	std::printf("Shortest turn\n");

	const float pi = obvr::math::kPi;
	Check(Near(ShortestTurn(0.0f, 0.5f), 0.5f), "a small turn is itself");
	Check(Near(ShortestTurn(0.5f, 0.0f), -0.5f), "and back is its negative");
	Check(Near(ShortestTurn(pi - 0.1f, -pi + 0.1f), 0.2f),
	      "across the seam the turn is the short way round");
	Check(Near(ShortestTurn(-pi + 0.1f, pi - 0.1f), -0.2f), "in both directions");
	Check(Near(ShortestTurn(1.0f, 1.0f), 0.0f), "no difference is no turn");
	Check(Near(ShortestTurn(0.0f, 2.0f * pi), 0.0f), "a full circle is no turn");
}

}  // namespace

int main() {
	TestButtons();
	TestShortestTurn();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
