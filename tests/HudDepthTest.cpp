// Checks when the interface pass gets a depth-stencil of its own
// (render/HudDepth.h).

#include <cstdio>

#include "render/HudDepth.h"

using namespace obvr;
using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	std::printf("The interface pass's depth\n");
	Check(HudNeedsOwnDepth(8, 0, 4028, 3380, 3924, 2207), "8 samples against the layer's 1: its own");
	Check(!HudNeedsOwnDepth(0, 0, 4028, 3380, 3924, 2207), "both single-sample and large enough: the game's");
	Check(HudNeedsOwnDepth(0, 0, 1920, 1080, 3924, 2207), "smaller than the layer: its own");
	Check(HudNeedsOwnDepth(0, 0, 4028, 1000, 3924, 2207), "too short: its own");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
